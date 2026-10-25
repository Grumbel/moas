# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Cross-compile MOAS for MS-DOS with Open Watcom (DOS/4GW).
#
#   nix build .#moas-dos
#   nix build .#moas-dos-zip
#   nix run .#moas-dos            # Watcom rebuild under DOSBox-X
#   nix run .#moas-dos-original   # historical manick/GAME.EXE under DOSBox-X
#
# Open Watcom is Watcom-1.0 (redistributable; marked unfree in nixpkgs).
# The flake allows that package only via allowUnfreePredicate in flake.nix.

{ pkgs
, version
, gamedata
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
      cp ${gamedata}/* $out/dos/
      if [ -f manick/DOS4GW.EXE ]; then
        cp manick/DOS4GW.EXE $out/dos/
      fi
      cp COPYING $out/dos/COPYING.txt
      cat > $out/dos/README.txt <<'EOR'
MOAS — MS-DOS build (Open Watcom, DOS/4GW)

  nix run .#moas-dos              # this rebuild
  nix run .#moas-dos-original     # historical manick/GAME.EXE
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

  /*
   * Shared DOSBox-X launcher.
   * dosDir: store path containing EXE + GAME.* + DOS4GW.EXE
   * exeName: MOAS.EXE or GAME.EXE
   * title / workTag: window title and temp dir name
   */
  mkDosboxRun = {
    name,
    dosDir,
    exeName,
    title,
    workTag,
    stateSubdir,
  }:
    pkgs.writeShellApplication {
      inherit name;
      runtimeInputs = [ pkgs.dosbox-x ];
      text = ''
        set -euo pipefail
        DOSDIR="${dosDir}"
        # Persist GAME*.SAV on the host (nix store is read-only; WORK is tmp).
        # XDG state: $XDG_STATE_HOME/${stateSubdir} or ~/.local/state/${stateSubdir}
        if [ -n "''${XDG_STATE_HOME:-}" ]; then
          STATEDIR="$XDG_STATE_HOME/${stateSubdir}"
        else
          STATEDIR="''${HOME:-/tmp}/.local/state/${stateSubdir}"
        fi
        mkdir -p "$STATEDIR"
        BASE="''${XDG_RUNTIME_DIR:-/tmp}"
        WORK="$BASE/${workTag}-$$"
        rm -rf "$WORK"
        mkdir -p "$WORK"
        CONF="$WORK/dosbox-moas.conf"
        cleanup() {
          # Copy saves back before deleting WORK
          shopt -s nullglob
          for f in "$WORK"/GAME*.SAV "$WORK"/game*.sav; do
            [ -f "$f" ] || continue
            cp -f "$f" "$STATEDIR"/
          done
          shopt -u nullglob
          rm -rf "$WORK"
        }
        trap cleanup EXIT

        cp -a --no-preserve=mode "$DOSDIR"/. "$WORK"/
        # Seed prior saves into the DOS C: drive
        shopt -s nullglob
        for f in "$STATEDIR"/GAME*.SAV "$STATEDIR"/game*.sav; do
          [ -f "$f" ] || continue
          cp -f "$f" "$WORK"/
        done
        shopt -u nullglob
        echo "${name}: state directory $STATEDIR (GAME*.SAV persisted here)"

        cat > "$CONF" <<EOF
[sdl]
fullscreen=false
output=texture
autolock=true

[dosbox]
machine=svga_s3
memsize=16
title=${title}
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
sensitivity=55
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
echo Starting ${exeName}...
${exeName}
exit
EOF

        echo "${name}: game directory $WORK"
        echo "${name}: launching DOSBox-X (close the window or exit the game to quit)"
        cd "$WORK"
        dosbox-x -defaultdir "$WORK" -conf "$CONF" -exit "$@"
      '';
    };

  moasDosRun = mkDosboxRun {
    name = "moas-dos";
    dosDir = "${moasDos}/dos";
    exeName = "MOAS.EXE";
    title = "MOAS (Open Watcom rebuild)";
    workTag = "moas-dos";
    stateSubdir = "moas-dos";
  };

  # Historical binary from manick/ — no rebuild, just stage data + GAME.EXE
  moasOriginalDos = pkgs.runCommand "moas-dos-original-${version}" { } ''
    mkdir -p $out/dos
    for f in ${../manick}/GAME.* ${../manick}/DOS4GW.EXE ${../manick}/LICENCE.DAT; do
      [ -f "$f" ] || continue
      cp "$f" $out/dos/
    done
    # Normalise licence name if only mixed-case exists
    for f in ${../manick}/Licence.Dat ${../manick}/licence.dat; do
      [ -f "$f" ] || continue
      cp "$f" $out/dos/
    done
    cat > $out/dos/README.txt <<'EOR'
MOAS — original DOS binary (manick/GAME.EXE + DOS4GW.EXE)

  nix run .#moas-dos-original
EOR
  '';

  moasDosOriginalRun = mkDosboxRun {
    name = "moas-dos-original";
    dosDir = "${moasOriginalDos}/dos";
    exeName = "GAME.EXE";
    title = "MOAS (original GAME.EXE)";
    workTag = "moas-dos-original";
    stateSubdir = "moas-dos-original";
  };

in {
  moas-dos = moasDos;
  moas-dos-zip = moasDosZip;
  moas-dos-run = moasDosRun;
  moas-dos-original = moasOriginalDos;
  moas-dos-original-run = moasDosOriginalRun;
}
