<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# MOAS for the R36S (ArkOS) and other PortMaster handhelds

Point-and-click adventure system (Mänick Mänschion). Native aarch64 build,
linked against an ArkOS-era (Ubuntu 19.10) userland; it uses the device's
SDL2. The original 320×200 picture is letterboxed to fill the screen.

## Install

Copy `Moas.sh` and the `moas/` folder to `/roms/ports/`, or put the zip into
`ports/PortMaster/autoinstall/` and start PortMaster once.

## Controls

| Button / input     | Action                                       |
|--------------------|----------------------------------------------|
| Left / right stick | move the cursor (both together: faster)      |
| D-pad              | walk                                         |
| A / R1             | use (left click)                             |
| B / L1             | look (right click)                           |
| X (hold)           | show hotspots                                |
| Y (hold)           | skip text                                    |
| Select             | skip scene (Esc)                             |
| Start              | options menu: save, load, settings, quit     |
| R2 / L2            | save / load                                  |
| R3 / L3            | game speed up / down                         |
| Start + Select     | quit (gptokeyb)                              |

In the options menu: D-pad up/down to select, left/right or A to change,
B goes back, Select or Start closes it.

Save games and logs stay under the port directory on the SD card.

## Build

```sh
nix build .#moas-r36s-portmaster-zip
```
