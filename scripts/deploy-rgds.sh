#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
bin="$project_root/III/build-rgds/src/re3"
device=${RGDS_HOST:-192.168.0.118}
user=${RGDS_USER:-root}
gamedir=${RGDS_GAMEDIR:-/mnt/sdcard/Ports/gta3}
portsdir=$(dirname "$gamedir")
export SSHPASS=${SSHPASS:-root}

anbernic_map='1900b655010000000100000000010000,ANBERNIC-rk3568-keys,platform:Linux,a:b1,b:b0,x:b2,y:b3,back:b6,guide:b13,start:b7,leftstick:b9,rightstick:b12,leftshoulder:b4,rightshoulder:b5,dpup:h0.1,dpleft:h0.8,dpdown:h0.4,dpright:h0.2,leftx:a0,lefty:a1,lefttrigger:b10,righttrigger:b11,'

if [ ! -x "$bin" ]; then
	echo "Missing $bin — run scripts/build-rgds.sh first." >&2
	exit 2
fi

ssh_cmd()
{
	sshpass -e ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "$user@$device" "$@"
}

scp_cmd()
{
	sshpass -e scp -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "$@"
}

echo "Deploying re3_rgds to $user@$device:$gamedir"
ssh_cmd "mkdir -p '$gamedir'"
ssh_cmd "killall re3_rgds 2>/dev/null || true"
if ssh_cmd "test -f '$gamedir/re3' && test ! -f '$gamedir/re3.portmaster'"; then
	ssh_cmd "cp -a '$gamedir/re3' '$gamedir/re3.portmaster'"
fi
scp_cmd "$bin" "$user@$device:$gamedir/re3_rgds.new"
ssh_cmd "mv -f '$gamedir/re3_rgds.new' '$gamedir/re3_rgds' && chmod 755 '$gamedir/re3_rgds'"

db_host=$(mktemp)
printf '%s\n' "$anbernic_map" > "$db_host"
scp_cmd "$db_host" "$user@$device:$gamedir/gamecontrollerdb.txt"
rm -f "$db_host"

launcher_host=$(mktemp)
cat > "$launcher_host" << EOF
#!/bin/bash
export HOME=/root
if [ -f "/mnt/vendor/notADC.ini" ]; then
	echo 2 > /sys/class/anbernic_misc/nds_pwrkey 2>/dev/null || true
fi
export XDG_RUNTIME_DIR="\${XDG_RUNTIME_DIR:-/var/run}"
export WAYLAND_DISPLAY="\${WAYLAND_DISPLAY:-wayland-0}"
export SDL_VIDEODRIVER=wayland
export SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS=0
export SDL_TOUCH_MOUSE_EVENTS=0
export SDL_MOUSE_TOUCH_EVENTS=0
export LIBGL_ALWAYS_SOFTWARE=0
export vblank_mode=0
export WESTON_OUTPUT_FLOW=horizontal
export WESTON_DEFAULT_POSITION=left-top
printf '%s\n' 'compositor:output:unpin' 'output:DSI-1:pos=0,0' 'output:DSI-2:pos=1024,0' 'calibration:gt9xx-0:0.5 0 0.5 0 1 0' > /tmp/.weston_drm.conf
echo 0 > /sys/class/anbernic_misc/tpctrl 2>/dev/null || true
# Mali G52 sits at 200MHz under simple_ondemand; lock 800MHz while the game runs.
GPU_GOV=\$(cat /sys/class/devfreq/fde60000.gpu/governor 2>/dev/null || echo simple_ondemand)
GPU_MIN=\$(cat /sys/class/devfreq/fde60000.gpu/min_freq 2>/dev/null || echo 200000000)
echo performance > /sys/class/devfreq/fde60000.gpu/governor 2>/dev/null || true
echo 800000000 > /sys/class/devfreq/fde60000.gpu/min_freq 2>/dev/null || true
export SDL_GAMECONTROLLERCONFIG="${anbernic_map}"

SHDIR=\$(dirname "\$0")
GAMEDIR="\$SHDIR/gta3"
cd "\$GAMEDIR" || exit 1

chmod 666 /dev/input/event* /dev/input/js0 2>/dev/null || true

DISPLAY_ID="\$(cat /sys/class/power_supply/axp2202-battery/display_id 2>/dev/null || echo 0)"
if [ "\$DISPLAY_ID" = "1" ]; then
	export AUDIODEV=hw:2,0
fi

./re3_rgds "\$@" 2>&1 | tee log-rgds.txt
echo "\$GPU_GOV" > /sys/class/devfreq/fde60000.gpu/governor 2>/dev/null || true
echo "\$GPU_MIN" > /sys/class/devfreq/fde60000.gpu/min_freq 2>/dev/null || true
EOF

for launcher_name in "Grand Theft Auto 3-rgds.sh" "侠盗猎车 3-rgds.sh"; do
	scp_cmd "$launcher_host" "$user@$device:$portsdir/$launcher_name"
	ssh_cmd "chmod 755 '$portsdir/$launcher_name'"
done
rm -f "$launcher_host"

patch_host=$(mktemp)
cat > "$patch_host" << 'PY'
from pathlib import Path
import os

gamedir = Path(os.environ.get('RGDS_GAMEDIR', '/mnt/sdcard/Ports/gta3'))
portsdir = gamedir.parent
anbernic = os.environ.get('RGDS_PADMAP', '')

block = f'''# BEGIN RGDS_PLUS
if [ -x ./re3_rgds ]; then
  if [ -f "/mnt/vendor/notADC.ini" ]; then
    echo 2 > /sys/class/anbernic_misc/nds_pwrkey 2>/dev/null || true
  fi
  export XDG_RUNTIME_DIR="${{XDG_RUNTIME_DIR:-/var/run}}"
  export WAYLAND_DISPLAY="${{WAYLAND_DISPLAY:-wayland-0}}"
  export SDL_VIDEODRIVER=wayland
  export SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS=0
  export SDL_TOUCH_MOUSE_EVENTS=0
  export SDL_MOUSE_TOUCH_EVENTS=0
  export vblank_mode=0
  export WESTON_OUTPUT_FLOW=horizontal
  export WESTON_DEFAULT_POSITION=left-top
  printf '%s\\n' 'compositor:output:unpin' 'output:DSI-1:pos=0,0' 'output:DSI-2:pos=1024,0' 'calibration:gt9xx-0:0.5 0 0.5 0 1 0' > /tmp/.weston_drm.conf
  echo 0 > /sys/class/anbernic_misc/tpctrl 2>/dev/null || true
  echo performance > /sys/class/devfreq/fde60000.gpu/governor 2>/dev/null || true
  echo 800000000 > /sys/class/devfreq/fde60000.gpu/min_freq 2>/dev/null || true
  export SDL_GAMECONTROLLERCONFIG="{anbernic}"
  chmod 666 /dev/input/event* /dev/input/js0 2>/dev/null || true
  RE3=re3_rgds
  LIBS=""
  export LD_LIBRARY_PATH="/usr/lib:/lib${{LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}}"
  GPTOKEYB=true
fi
# END RGDS_PLUS
'''

for name in ('Grand Theft Auto 3.sh', '侠盗猎车 3.sh'):
    p = portsdir / name
    if not p.exists():
        print('launcher missing', p)
        continue
    t = p.read_text()
    begin, end = '# BEGIN RGDS_PLUS', '# END RGDS_PLUS'
    if begin in t and end in t:
        pre, rest = t.split(begin, 1)
        _, post = rest.split(end, 1)
        t = pre + block + post.lstrip('\n')
    else:
        needle = '$GPTOKEYB "${RE3}" &'
        if needle not in t:
            print('unexpected launcher format', p)
            continue
        t = t.replace(needle, block + needle, 1)
    p.write_text(t)
    print('patched', p)

ini = gamedir / 're3.ini'
if ini.exists():
    import re
    repls = {'VSync': '0', 'FrameLimiter': '0', 'Trails': '0', 'DrawDistance': '0.65'}
    out = []
    in_bindings = False
    for line in ini.read_text().splitlines(True):
        stripped = line.strip()
        if stripped.startswith('[') and stripped.endswith(']'):
            in_bindings = stripped.lower() == '[bindings]'
        key = line.lstrip().split('=', 1)[0].strip() if '=' in line else ''
        if key in repls:
            nl = '\n' if line.endswith('\n') else ''
            line = f'{key} = {repls.pop(key)}{nl}'
        elif in_bindings and '=' in line:
            left, right = line.split('=', 1)
            nl = '\n' if line.endswith('\n') else ''
            right = re.sub(r'(^|,)\s*joy:\d+', r'\1', right)
            right = re.sub(r',\s*,', ',', right)
            right = right.strip().strip(',').strip()
            line = f'{left.rstrip()} = {right}{nl}' if right else f'{left.rstrip()}{nl}'
        out.append(line)
    text = ''.join(out)
    for k, v in repls.items():
        text += ('' if text.endswith('\n') else '\n') + f'{k} = {v}\n'
    ini.write_text(text)
    print('patched', ini)
else:
    print('re3.ini missing')
PY
scp_cmd "$patch_host" "$user@$device:/tmp/patch-rgds-launcher.py"
rm -f "$patch_host"
ssh_cmd "RGDS_GAMEDIR='$gamedir' RGDS_PADMAP='$anbernic_map' python3 /tmp/patch-rgds-launcher.py"

echo "Deployed to $gamedir. Launch Grand Theft Auto 3.sh from Ports"
