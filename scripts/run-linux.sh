#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
game=${1:-re3}

case "$game" in
	re3|III|iii)
		bin="$project_root/III/build-linux/src/re3"
		data="$project_root/gamefiles/re3"
		;;
	revc|vc|miami)
		bin="$project_root/miami/build-linux/src/reVC"
		data="$project_root/gamefiles/revc"
		;;
	relcs|lcs|stories)
		bin="$project_root/stories/build-linux/src/reLCS"
		data="$project_root/gamefiles/relcs"
		;;
	*)
		echo "Usage: scripts/run-linux.sh [re3|revc|relcs]" >&2
		exit 2
		;;
esac

[ -x "$bin" ] || {
	echo "Linux binary not found: $bin" >&2
	echo "Build it first with: ./scripts/build-linux.sh $game" >&2
	exit 3
}
[ -d "$data" ] || {
	echo "Game data directory not found: $data" >&2
	exit 4
}

if [ -d /tmp/regta-linux-deps/root/usr/lib/x86_64-linux-gnu ]; then
	LD_LIBRARY_PATH="/tmp/regta-linux-deps/root/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
	export LD_LIBRARY_PATH
fi

cd "$data"
mkdir -p userfiles
exec "$bin"
