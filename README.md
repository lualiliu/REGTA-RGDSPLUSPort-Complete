<p align="center">
  <img src="logo.png" alt="REGTA logo" width="280">
</p>

<p align="center">
  <a href="III/README.md"><img src="III/logo.png" alt="re3 — GTA III" width="150"></a>
  <a href="miami/README.md"><img src="miami/logo.png" alt="reVC — Vice City" width="150"></a>
  <a href="stories/README.md"><img src="stories/logo.png" alt="reLCS — Liberty City Stories" width="150"></a>
</p>

# REGTA-3DSPort-Complete

GTA III, Vice City, and Liberty City Stories for **New Nintendo 3DS**. This tree also builds a dual-screen Linux preview, and GTA III for the **Anbernic RG DS Plus**.

Bring your own game data. Not affiliated with Rockstar or Take-Two.

Trailer: https://youtu.be/fBzzLx0BX5M

| | Data | 3DS | Linux | RG DS Plus |
| --- | --- | --- | --- | --- |
| GTA III (`III`) | PC | `/3ds/re3/` | `III/build-linux/src/re3` | yes |
| Vice City (`miami`) | PC | `/3ds/miami/` | `miami/build-linux/src/reVC` | — |
| LCS (`stories`) | converted PS2 | `/3ds/relcs/` | `stories/build-linux/src/reLCS` | — |

Keep `vendor` symlinks into `common`. Per-game notes: [III](III/README.md), [VC](miami/README.md), [LCS](stories/README.md).

## Linux

2048×768 window: 3D on the left, radar/HUD on the right.

```sh
sudo apt install build-essential cmake pkg-config \
    libglfw3-dev libglew-dev libopenal-dev libmpg123-dev libgl1-mesa-dev

./scripts/build-linux.sh re3    # or revc / relcs / all
cd /path/to/full/game/data
../III/build-linux/src/re3
```

## RG DS Plus (GTA III)

Two 1024×768 screens. Look by dragging the right screen.

```sh
./scripts/build-rgds.sh
./scripts/deploy-rgds.sh
```

Needs an aarch64 cross compiler, `sshpass`, and SSH to the device. Put PC GTA III data in `/mnt/sdcard/Ports/gta3`. Override host/user with `RGDS_HOST`, `RGDS_USER`, `SSHPASS`.

<a id="building-from-source"></a>
<a id="preparing-game-data"></a>
<a id="cia-packaging"></a>
<a id="changelog"></a>
<a id="grand-theft-auto-iii--re3"></a>
<a id="grand-theft-auto-vice-city--revc"></a>
<a id="grand-theft-auto-liberty-city-stories--relcs"></a>
<a id="credits-and-legal-notice"></a>

## 3DS

New 3DS family only. Compiler: **devkitARM r55**.

```sh
export DEVKITPRO=/opt/devkitpro
export DEVKITARM=/path/to/devkitARM-r55

./scripts/setup-game.sh re3 "/path/to/GTA III" "/Volumes/SD/3ds"
./scripts/build.sh re3
./scripts/install-3dsx.sh re3 "/Volumes/SD/3ds"
```

Same for `revc` and `relcs`. LCS data must be converted first ([reLCS Asset Converter](https://github.com/knackers4/res/releases/tag/relcs)). CIA: `./packaging/production_cia/build_production.sh`.

Based on re3/reVC, the community 3DS port, [reStories](https://github.com/knackers4/res), librw, and SDL2.
