#pragma once

#ifdef RW_GLES
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>

#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER GL_CLAMP_TO_EDGE
#endif

#ifndef GL_COMPRESSED_RGB_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT 0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#endif

#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif
#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif

#ifndef GL_CLAMP
#define GL_CLAMP GL_CLAMP_TO_EDGE
#endif

#ifndef GLEW_OK
#define GLEW_OK 0
#endif
#ifndef GLEW_VERSION_3_3
#define GLEW_VERSION_3_3 1
#endif

static int glewExperimental __attribute__((unused));
inline unsigned glewInit(void) { return GLEW_OK; }
inline const char *glewGetErrorString(unsigned) { return "gles"; }

#ifndef APIENTRY
#define APIENTRY
#endif

#else
#include <GL/glew.h>
#endif
