#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
game=${1:-all}

if command -v nproc >/dev/null 2>&1; then
	jobs=$(nproc)
elif command -v getconf >/dev/null 2>&1; then
	jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)
else
	jobs=4
fi

check_dep()
{
	pkg-config --exists "$1" && return 0
	[ -n "${CMAKE_PREFIX_PATH:-}" ] && return 0
	echo "Missing development package for $1." >&2
	echo "On Debian/Ubuntu: sudo apt install build-essential cmake pkg-config libglfw3-dev libglew-dev libopenal-dev libmpg123-dev libgl1-mesa-dev" >&2
	exit 2
}

command -v cmake >/dev/null 2>&1 || {
	echo "cmake was not found." >&2
	exit 2
}
command -v pkg-config >/dev/null 2>&1 || {
	echo "pkg-config was not found." >&2
	exit 2
}

check_dep glfw3
check_dep glew
check_dep openal
check_dep libmpg123

cmake_flags="-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE:-RelWithDebInfo} -DLIBRW_PLATFORM=GL3 -DLIBRW_GL3_GFXLIB=GLFW -DLIBRW_TOOLS=OFF"
if [ -n "${CMAKE_PREFIX_PATH:-}" ]; then
	cmake_flags="$cmake_flags -DCMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH"
fi

build_one()
{
	src=
	case "$1" in
		re3)
			src="$project_root/III"
			;;
		revc)
			src="$project_root/miami"
			;;
		relcs)
			src="$project_root/stories"
			;;
		*)
			echo "Usage: scripts/build-linux.sh [all|re3|revc|relcs]" >&2
			exit 3
			;;
	esac

	build_dir="$src/build-linux"
	mkdir -p "$build_dir"
	cmake -S "$src" -B "$build_dir" $cmake_flags
	cmake --build "$build_dir" -j"$jobs"
}

case "$game" in
	all)
		build_one re3
		build_one revc
		build_one relcs
		;;
	re3|III|iii) build_one re3 ;;
	revc|vc|miami) build_one revc ;;
	relcs|lcs|stories) build_one relcs ;;
	*) build_one "$game" ;;
esac
