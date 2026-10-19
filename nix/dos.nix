# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Cross-compile MOAS for MS-DOS with Open Watcom (DOS/4GW).
#
#   nix build .#moas-dos
#   nix build .#moas-dos-zip
#
# Open Watcom uses the Sybase Open Watcom Public License (redistributable but
# marked unfree in nixpkgs). The flake allows that package only via
# allowUnfreePredicate in flake.nix.

{ pkgs
, version
}:

let
  lib = pkgs.lib;

  # Binary package is much faster than building open-watcom-v2 from source.
  watcom = pkgs.open-watcom-bin;

  moasDos = pkgs.stdenvNoCC.mkDerivation {
    pname = "moas-dos";
    inherit version;
    src = lib.cleanSource ../.;

    nativeBuildInputs = [ watcom pkgs.gnumake ];

    # Wrapped wcc386 already sets WATCOM; export for the Makefile and INCLUDE.
    buildPhase = ''
      runHook preBuild
      # Resolve the Open Watcom root (headers + wstub live here)
      if [ -z "''${WATCOM:-}" ] || [ ! -d "''${WATCOM}/h" ]; then
        # Probe from a wrapped tool's store path
        WCC=$(command -v wcc386)
        # wrapper → real binary under .../binl/wcc386 or similar
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

Run under DOSBox, FreeDOS, or real MS-DOS with the data files beside the EXE:
  MOAS.EXE
(DOS/4GW is invoked automatically via the EXE stub when DOS4GW.EXE is present.)

This binary uses the original #ifdef DOS hardware paths (VGA Mode 13h,
INT 33h mouse, keyboard IRQ, SoundBlaster), not the SDL2 port.
EOR
      runHook postInstall
    '';

    meta = with lib; {
      description = "MOAS MS-DOS executable (Open Watcom / DOS/4GW)";
      license = licenses.gpl2Plus;
      platforms = platforms.linux;
      hydraPlatforms = [];
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

in {
  moas-dos = moasDos;
  moas-dos-zip = moasDosZip;
}
