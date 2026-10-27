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
| Left stick / D-pad | walk                                         |
| Right stick        | move the cursor                              |
| R1 / R2            | use (left click)                             |
| L1 / L2            | look (right click)                           |
| A (hold)           | skip text                                    |
| B / Start          | skip scene (Esc)                             |
| Select             | options menu: save, load, settings, quit     |
| Start + Select     | quit (gptokeyb)                              |

In the options menu: D-pad up/down to select, left/right or A to change,
B goes back, Select or Start closes it.

Save games and logs stay under the port directory on the SD card.

## Build

```sh
nix build .#moas-r36s-portmaster-zip
```
