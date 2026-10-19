# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Cross-compile MOAS for MS-DOS with Open Watcom (DOS/4GW).
#
#   nix build .#moas-dos
#   nix build .#moas-dos-zip
#   nix run .#moas-dos          # DOSBox-X
#
# Open Watcom is Watcom-1.0 (redistributable; marked unfree in nixpkgs).
# The flake allows that package only via allowUnfreePredicate in flake.nix.

{ pkgs
, version
}:

let
  lib = pkgs.lib;

  watcom = pkgs.open-watcom-bin;

  moasDos = pkgs.stdenvNoCC.mkDerivation {
    pname = "moas-dos";
    inherit version;
    src = lib.cleanSource ../.;

    nativeBuildInputs = [ watcom pkgs.gnumake ];

    buildPhase = ''
      runHook preBuild
      if [ -z "''${WATCOM:-}" ] || [ ! -d "''${WATCOM}/h" ]; then
        WCC=$(command -v wcc386)
        REAL=$(readlink -f "$WCC")
        for d in "$(dirname "$REAL")/.." "$(dirname "$REAL")/../.." \
                 "${watcom}" "$(dirname $(dirname "$REAL"))"; do
          d=$(cd "$d" 2>/dev/null && pwd || true)
          if [ -n "$d" ] && [ -d "$d/h" ]; then
            export WATCOM="$d"
            break
          fi
        done
      fi
      if [ -z "''${WATCOM:-}" ] || [ ! -d "$WATCOM/h" ]; then
        echo "Could not locate Open Watcom headers (WATCOM/h)" >&2
        exit 1
      fi
      export INCLUDE="$WATCOM/h"
      echo "Building DOS target with WATCOM=$WATCOM"
      make -f mk/dos/Makefile -j"$NIX_BUILD_CORES"
      runHook postBuild
    '';

    installPhase = ''
      runHook preInstall
      mkdir -p $out/dos
      cp build-dos/moas.exe $out/dos/
      for f in manick/GAME.* manick/LICENCE.DAT manick/Licence.Dat manick/licence.dat; do
        [ -f "$f" ] || continue
        case "$f" in
          *.EXE|*.exe) continue ;;
        esac
        cp "$f" $out/dos/
      done
      if [ -f manick/DOS4GW.EXE ]; then
        cp manick/DOS4GW.EXE $out/dos/
      fi
      cp COPYING $out/dos/COPYING.txt
      cat > $out/dos/README.txt <<'EOR'
MOAS — MS-DOS build (Open Watcom, DOS/4GW)

  nix run .#moas-dos

Uses the original #ifdef DOS hardware paths, not the SDL2 port.
EOR
      runHook postInstall
    '';

    meta = with lib; {
      description = "MOAS MS-DOS executable (Open Watcom / DOS/4GW)";
      license = licenses.gpl2Plus;
      platforms = platforms.linux;
      hydraPlatforms = [ ];
    };
  };

  moasDosZip = pkgs.runCommand "moas-${version}-dos.zip" {
    nativeBuildInputs = [ pkgs.zip ];
  } ''
    mkdir -p $out
    work=$(mktemp -d)
    cp -r --no-preserve=mode ${moasDos}/dos "$work/moas-${version}-dos"
    cd "$work"
    zip -r -9 $out/moas-${version}-dos.zip moas-${version}-dos
  '';

  moasDosRun = pkgs.writeShellApplication {
    name = "moas-dos";
    runtimeInputs = [ pkgs.dosbox-x ];
    text = ''
      set -euo pipefail
      DOSDIR="${moasDos}/dos"
      BASE="''${XDG_RUNTIME_DIR:-/tmp}"
      WORK="$BASE/moas-dos-$$"
      rm -rf "$WORK"
      mkdir -p "$WORK"
      CONF="$WORK/dosbox-moas.conf"
      cleanup() { rm -rf "$WORK"; }
      trap cleanup EXIT

      cp -a --no-preserve=mode "$DOSDIR"/. "$WORK"/

      # Generate conf with absolute paths (DOSBox-X primary folder = WORK)
      cat > "$CONF" <<EOF
[sdl]
fullscreen=false
output=texture
autolock=true

[dosbox]
machine=svga_s3
memsize=16
title=MOAS (DOS/4GW)
working directory=$WORK

[cpu]
core=auto
cycles=max

[mixer]
nosound=false
rate=44100

[mouse]
# Original MOAS pins INT33 at (100,100) and scales deltas — high host
# sensitivity collapses that to 1:1 and feels unusable under DOSBox.
sensitivity=40
# Keep host pointer captured so INT 33 gets relative motion
mouse_emulation=locked

[sblaster]
sbtype=sb16
sbbase=220
irq=7
dma=1
hdma=5
oplmode=auto

[autoexec]
@echo off
mount C "$WORK"
C:
echo Starting MOAS (Open Watcom DOS/4GW rebuild)...
MOAS.EXE
exit
EOF

      echo "moas-dos: game directory $WORK"
      echo "moas-dos: launching DOSBox-X (close the window or exit the game to quit)"
      cd "$WORK"
      # -defaultdir avoids the "You have not selected a valid path" dialog
      dosbox-x -defaultdir "$WORK" -conf "$CONF" -exit "$@"
    '';
  };


in {
  moas-dos = moasDos;
  moas-dos-zip = moasDosZip;
  moas-dos-run = moasDosRun;
}
