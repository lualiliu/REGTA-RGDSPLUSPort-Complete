#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwengine.h"
#include "rw3ds.h"
#include "tex/rg_etc1.h"

#define PLUGIN_ID ID_DRIVER

namespace rw {
namespace c3d {

static Image*
createRgbaImage(int32 width, int32 height)
{
	Image *img = Image::create(width, height, 32);
	if(img == nil)
		return nil;
	img->allocate();
	if(img->pixels == nil){
		img->destroy();
		return nil;
	}
	memset(img->pixels, 0, img->stride * img->height);
	return img;
}

static Raster*
rasterFromRgbaImage(Image *img, int32 numLevels)
{
	int32 width, height, depth, format;
	if(!Raster::imageFindRasterFormat(img, Raster::TEXTURE,
	                                  &width, &height, &depth, &format))
		return nil;
	if(numLevels > 1)
		format |= Raster::MIPMAP | Raster::AUTOMIPMAP;
	Raster *ras = Raster::create(width, height, depth, format);
	if(ras == nil)
		return nil;
	if(ras->setFromImage(img) == nil){
		ras->destroy();
		return nil;
	}
	return ras;
}

static void
storeRgba(uint8 *dst, int32 stride, int32 x, int32 y,
          uint8 r, uint8 g, uint8 b, uint8 a)
{
	uint8 *px = dst + y * stride + x * 4;
	px[0] = r;
	px[1] = g;
	px[2] = b;
	px[3] = a;
}

static bool
decodeEtcLevel(uint8 *dst, int32 stride, const uint8 *src,
               int32 width, int32 height, bool hasAlpha)
{
	if(width < 8 || height < 8 || (width & 7) || (height & 7))
		return false;

	const uint8 *p = src;
	for(int32 ty = height - 8; ty >= 0; ty -= 8){
		for(int32 tx = 0; tx < width; tx += 8){
			for(int32 j = 0; j < 8; j += 4){
				for(int32 i = 0; i < 8; i += 4){
					uint8 alpha[8];
					if(hasAlpha){
						memcpy(alpha, p, 8);
						p += 8;
					}

					uint8 etcBe[8];
					for(int32 b = 0; b < 8; b++)
						etcBe[b] = p[7 - b];
					p += 8;

					unsigned int block[16];
					rg_etc1::unpack_etc1_block(etcBe, block, false);
					for(int32 y = 0; y < 4; y++){
						for(int32 x = 0; x < 4; x++){
							uint8 *c = (uint8*)&block[y * 4 + x];
							uint8 a = 255;
							if(hasAlpha){
								uint8 nibble = alpha[2 * x + y / 2];
								uint8 a4 = (y & 1) ? (nibble >> 4) : (nibble & 0xF);
								a = (uint8)(a4 * 17);
							}
							int32 gx = tx + i + x;
							int32 gy = ty + (7 - (j + y));
							storeRgba(dst, stride, gx, gy, c[0], c[1], c[2], a);
						}
					}
				}
			}
		}
	}
	return true;
}

struct Color4 {
	uint8 data[4];
};

static void
swizzle8x8(Color4 cache[64], bool reverse)
{
	static const unsigned char table[][4] = {
		{  2,  8, 16,  4, },
		{  3,  9, 17,  5, },
		{  6, 10, 24, 20, },
		{  7, 11, 25, 21, },
		{ 14, 26, 28, 22, },
		{ 15, 27, 29, 23, },
		{ 34, 40, 48, 36, },
		{ 35, 41, 49, 37, },
		{ 38, 42, 56, 52, },
		{ 39, 43, 57, 53, },
		{ 46, 58, 60, 54, },
		{ 47, 59, 61, 55, },
	};

	if(!reverse){
		for(const auto &entry : table){
			Color4 tmp = cache[entry[0]];
			cache[entry[0]] = cache[entry[1]];
			cache[entry[1]] = cache[entry[2]];
			cache[entry[2]] = cache[entry[3]];
			cache[entry[3]] = tmp;
		}
	}else{
		for(const auto &entry : table){
			Color4 tmp = cache[entry[3]];
			cache[entry[3]] = cache[entry[2]];
			cache[entry[2]] = cache[entry[1]];
			cache[entry[1]] = cache[entry[0]];
			cache[entry[0]] = tmp;
		}
	}

	Color4 tmp;
#define SWAP(i, j) do { tmp = cache[i]; cache[i] = cache[j]; cache[j] = tmp; } while(0)
	SWAP(12, 18);
	SWAP(13, 19);
	SWAP(44, 50);
	SWAP(45, 51);
#undef SWAP
}

static bool
unswizzleToLinear(uint8 *dst, const uint8 *src, int32 width, int32 height, int32 bpp)
{
	if(width < 8 || height < 8 || (width & 7) || (height & 7) || bpp <= 0 || bpp > 4)
		return false;

	int32 stride = width * bpp;
	const uint8 *tile = src;
	for(int32 ty = 0; ty < height; ty += 8){
		for(int32 tx = 0; tx < width; tx += 8){
			Color4 cache[64];
			memset(cache, 0, sizeof(cache));
			for(int32 i = 0; i < 64; i++){
				memcpy(cache[i].data, tile, bpp);
				tile += bpp;
			}
			swizzle8x8(cache, true);
			for(int32 i = 0; i < 64; i++){
				int32 x = tx + (i & 7);
				int32 y = height - 1 - ty - (i >> 3);
				memcpy(dst + y * stride + x * bpp, cache[i].data, bpp);
			}
		}
	}
	return true;
}

static bool
linearToRgba(Image *img, const uint8 *src, int32 bpp, uint32 format)
{
	int32 w = img->width;
	int32 h = img->height;
	uint32 col = format & 0xF00;
	for(int32 y = 0; y < h; y++){
		for(int32 x = 0; x < w; x++){
			const uint8 *in = src + y * w * bpp + x * bpp;
			uint8 r = 0, g = 0, b = 0, a = 255;
			switch(col){
			case Raster::C8888:
				/* stored as ABGR after conv_ABGR8888_from_RGBA8888 */
				a = in[0];
				b = in[1];
				g = in[2];
				r = in[3];
				break;
			case Raster::C888:
				b = in[0];
				g = in[1];
				r = in[2];
				break;
			case Raster::C1555: {
				uint16 v = (uint16)in[0] | ((uint16)in[1] << 8);
				a = (v & 1) ? 255 : 0;
				b = (uint8)(((v >> 1) & 0x1F) * 255 / 31);
				g = (uint8)(((v >> 6) & 0x1F) * 255 / 31);
				r = (uint8)(((v >> 11) & 0x1F) * 255 / 31);
				break;
			}
			case Raster::C565: {
				uint16 v = (uint16)in[0] | ((uint16)in[1] << 8);
				b = (uint8)((v & 0x1F) * 255 / 31);
				g = (uint8)(((v >> 5) & 0x3F) * 255 / 63);
				r = (uint8)(((v >> 11) & 0x1F) * 255 / 31);
				break;
			}
			case Raster::C4444: {
				uint16 v = (uint16)in[0] | ((uint16)in[1] << 8);
				a = (uint8)((v & 0xF) * 17);
				b = (uint8)(((v >> 4) & 0xF) * 17);
				g = (uint8)(((v >> 8) & 0xF) * 17);
				r = (uint8)(((v >> 12) & 0xF) * 17);
				break;
			}
			case Raster::LUM8:
				r = g = b = in[0];
				break;
			default:
				return false;
			}
			storeRgba(img->pixels, img->stride, x, y, r, g, b, a);
		}
	}
	return true;
}

static int32
uncompressedBpp(uint32 format)
{
	switch(format & 0xF00){
	case Raster::C8888: return 4;
	case Raster::C888:  return 3;
	case Raster::C1555:
	case Raster::C565:
	case Raster::C4444: return 2;
	case Raster::LUM8:  return 1;
	default: return 0;
	}
}

Texture*
readNativeTextureHost(Stream *stream)
{
	uint32 platform;
	if(!findChunk(stream, ID_STRUCT, nil, nil)){
		RWERROR((ERR_CHUNK, "STRUCT"));
		return nil;
	}
	platform = stream->readU32();
	if(platform != PLATFORM_3DS){
		RWERROR((ERR_PLATFORM, platform));
		return nil;
	}

	Texture *tex = Texture::create(nil);
	if(tex == nil)
		return nil;

	tex->filterAddressing = stream->readU32();
	stream->read8(tex->name, 32);
	stream->read8(tex->mask, 32);

	uint32 format = stream->readU32();
	int32 width = stream->readI32();
	int32 height = stream->readI32();
	stream->readI32(); /* depth */
	int32 numLevels = stream->readI32();
	int32 flags = stream->readI32();
	uint32 size = stream->readU32();

	if(width <= 0 || height <= 0 || numLevels <= 0 || size == 0){
		tex->destroy();
		return nil;
	}

	uint8 *data = (uint8*)rwMalloc(size, MEMDUR_EVENT | ID_DRIVER);
	if(data == nil){
		tex->destroy();
		return nil;
	}
	stream->read8(data, size);

	bool compressed = (flags & 2) != 0;
	bool hasAlpha = (flags & 1) != 0;
	Image *img = createRgbaImage(width, height);
	bool ok = false;

	if(img){
		if(compressed){
			uint32 bpp = hasAlpha ? 8 : 4;
			uint32 baseSize = (uint32)width * (uint32)height * bpp / 8;
			if(baseSize <= size)
				ok = decodeEtcLevel(img->pixels, img->stride, data, width, height, hasAlpha);
		}else{
			int32 bpp = uncompressedBpp(format);
			uint32 baseSize = (uint32)width * (uint32)height * (uint32)bpp;
			uint8 *linear = (uint8*)rwMalloc(baseSize, MEMDUR_EVENT | ID_DRIVER);
			if(linear && bpp && baseSize <= size &&
			   unswizzleToLinear(linear, data, width, height, bpp))
				ok = linearToRgba(img, linear, bpp, format);
			rwFree(linear);
		}
	}

	rwFree(data);

	if(!ok){
		if(img)
			img->destroy();
		tex->destroy();
		return nil;
	}

	Raster *ras = rasterFromRgbaImage(img, numLevels);
	img->destroy();
	if(ras == nil){
		tex->destroy();
		return nil;
	}
	tex->raster = ras;
	return tex;
}

}
}
