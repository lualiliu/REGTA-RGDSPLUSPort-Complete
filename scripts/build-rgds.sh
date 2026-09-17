#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
sysroot=${RGDS_SYSROOT:-/tmp/rgds-sysroot}
device=${RGDS_HOST:-192.168.0.118}
user=${RGDS_USER:-root}
export SSHPASS=${SSHPASS:-root}

ssh_cmd()
{
	sshpass -e ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "$user@$device" "$@"
}

mkdir -p "$sysroot/usr/lib" "$sysroot/usr/include"
# Host SDL2 headers, but replace Debian's arch trampoline with the real config.
rm -rf "$sysroot/usr/include/SDL2"
cp -a /usr/include/SDL2 "$sysroot/usr/include/"
if [ -f /usr/include/x86_64-linux-gnu/SDL2/_real_SDL_config.h ]; then
	cp /usr/include/x86_64-linux-gnu/SDL2/_real_SDL_config.h "$sysroot/usr/include/SDL2/SDL_config.h"
	# Host SDL was built for x86_64; strip Intel-only CPU config for aarch64.
	sed -i \
		-e 's/#define HAVE_IMMINTRIN_H 1/#undef HAVE_IMMINTRIN_H/' \
		-e 's/#define HAVE_MMINTRIN_H 1/#undef HAVE_MMINTRIN_H/' \
		-e 's/#define HAVE_XMMINTRIN_H 1/#undef HAVE_XMMINTRIN_H/' \
		-e 's/#define HAVE_EMMINTRIN_H 1/#undef HAVE_EMMINTRIN_H/' \
		-e 's/#define HAVE_PMMINTRIN_H 1/#undef HAVE_PMMINTRIN_H/' \
		"$sysroot/usr/include/SDL2/SDL_config.h"
	printf '\n#undef HAVE_IMMINTRIN_H\n#define SDL_DISABLE_IMMINTRIN_H 1\n' >> "$sysroot/usr/include/SDL2/SDL_config.h"
fi

mkdir -p "$sysroot/usr/include/GLES3" "$sysroot/usr/include/GLES2" "$sysroot/usr/include/KHR"
cp -a /usr/include/GLES3/. "$sysroot/usr/include/GLES3/"
cp -a /usr/include/GLES2/. "$sysroot/usr/include/GLES2/"
cp /usr/include/mpg123.h "$sysroot/usr/include/" 2>/dev/null || true
if [ -d /usr/include/AL ]; then
	rm -rf "$sysroot/usr/include/AL"
	cp -a /usr/include/AL "$sysroot/usr/include/"
fi

echo "Copying link libraries from $device ..."
ssh_cmd 'cd /usr/lib && tar cf - \
	libSDL2.so libSDL2-2.0.so.0 libSDL2-2.0.so.0.3200.10 \
	libGLESv2.so libGLESv2.so.2 \
	libEGL.so libEGL.so.1 \
	libmali.so libmali.so.1 libmali.so.1.9.0 \
	libmali-hook.so libmali-hook.so.1 libmali-hook.so.1.9.0 \
	libdrm.so libdrm.so.2 libdrm.so.2.124.0 \
	libwayland-client.so libwayland-client.so.0 libwayland-client.so.0.23.1 \
	libwayland-server.so libwayland-server.so.0 libwayland-server.so.0.23.1 \
	libffi.so libffi.so.8 libffi.so.8.1.2 \
	libopenal.so libopenal.so.1 libopenal.so.1.22.0 \
	libmpg123.so libmpg123.so.0 libmpg123.so.0.48.1 \
	libatomic.so libatomic.so.1 libatomic.so.1.2.0' \
	| tar -C "$sysroot/usr/lib" -xf -

if command -v nproc >/dev/null 2>&1; then
	jobs=$(nproc)
else
	jobs=4
fi

build_dir="$project_root/III/build-rgds"
mkdir -p "$build_dir"

cmake -S "$project_root/III" -B "$build_dir" \
	-DCMAKE_TOOLCHAIN_FILE="$script_dir/toolchain-aarch64-rgds.cmake" \
	-DRGDS_SYSROOT="$sysroot" \
	-DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-RelWithDebInfo}" \
	-DRE3_RGDS_PLUS=ON \
	-DLIBRW_PLATFORM=GL3 \
	-DLIBRW_GL3_GFXLIB=SDL2 \
	-DLIBRW_GLES=ON \
	-DLIBRW_TOOLS=OFF \
	-DSDL2_INCLUDE_DIR="$sysroot/usr/include/SDL2" \
	-DSDL2_LIBRARY="$sysroot/usr/lib/libSDL2.so" \
	-DOPENAL_INCLUDE_DIR="$sysroot/usr/include/AL" \
	-DOPENAL_LIBRARY="$sysroot/usr/lib/libopenal.so" \
	-Dmpg123_INCLUDE_DIR="$sysroot/usr/include" \
	-Dmpg123_LIBRARIES="$sysroot/usr/lib/libmpg123.so" \
	-DGLES3_INCLUDE_DIR="$sysroot/usr/include" \
	-DCMAKE_EXE_LINKER_FLAGS="-Wl,-rpath-link,$sysroot/usr/lib"

cmake --build "$build_dir" -j"$jobs"

echo "Built: $build_dir/src/re3"
