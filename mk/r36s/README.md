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

| Button / input | Action                          |
|----------------|---------------------------------|
| Touch / mouse  | cursor and left click           |
| D-pad / stick  | move cursor (via gptokeyb map) |
| A              | left click                      |
| B / Select     | right click / cancel            |
| Start + Select | quit (gptokeyb)                 |

Save games and logs stay under the port directory on the SD card.

## Build

```sh
nix build .#moas-r36s-portmaster-zip
```
