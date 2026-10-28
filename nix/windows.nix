# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Windows builds, cross-compiled with MinGW (pkgsCross). SDL2 is the official
# prebuilt MinGW package (SDL2-win32 flake), same approach as Kurvenrausch/Pingus.
#
#   nix build .#manick-win64
#   nix build .#manick-win64-zip
#   nix build .#manick-win32
#   nix build .#manick-win32-zip
#   nix run .#manick-win64     (under Wine)
#   nix run .#manick-win32     (under Wine)

{ pkgs
, sdl2Win64
, sdl2Win32
, version
, gamedata
}:

let
  lib = pkgs.lib;

  mkGame = { crossPkgs, sdl2, arch }:
    crossPkgs.stdenv.mkDerivation {
      pname = "moas-${arch}-build";
      inherit version;
      src = lib.cleanSource ../.;
      nativeBuildInputs = [ pkgs.cmake ];
      buildInputs = [ sdl2 ];
      cmakeFlags = [
        "-DCMAKE_BUILD_TYPE=Release"
        "-DSDL2_DIR=${sdl2}/lib/cmake/SDL2"
        # Data lives next to the .exe in the shipped folder
        "-DMOAS_DATADIR=."
        "-DPROJECT_VERSION_FULL=${version}"
      ];
    };

  mkFolder = { crossPkgs, sdl2, arch }:
    let game = mkGame { inherit crossPkgs sdl2 arch; };
    in pkgs.runCommand "manick-${arch}-${version}" { } ''
      mkdir -p $out
      cp ${game}/bin/manick.exe $out/
      cp ${sdl2}/bin/SDL2.dll $out/
      # Game data beside the binary
      cp ${gamedata}/* $out/
      mkdir -p $out/lang
      cp ${../lang}/*.po $out/lang/
      # per-language pictures: lang/<code>/<kind>/*.png
      for d in ${../lang}/*/; do cp -r "$d" $out/lang/; done
      cp ${../COPYING} $out/COPYING.txt
      cp ${../data/icons/hicolor/256x256/apps/manick.png} $out/manick.png 2>/dev/null || true
      cat > $out/README.txt <<EOF
      Mänick Mänschion ${version}
      Point-and-click adventure on MOAS (Mouse Orientiertes Adventure System)

      Start manick.exe. Point and click with the mouse; Space/Enter also click.
      F10 opens the file menu. Esc aborts dialogs. Q or Alt+F4 quits.
      Use --fullscreen for a borderless fullscreen window.

      Game data files (GAME.*) must stay next to the executable.

      Licence: GPL-2.0-or-later (COPYING.txt).
      Source: https://github.com/Grumbel/moas
      EOF
      sed -i 's/^      //; s/$/\r/' $out/README.txt
    '';

  mkZip = { folder, arch }:
    pkgs.runCommand "manick-${version}-${arch}.zip" { nativeBuildInputs = [ pkgs.zip ]; } ''
      mkdir -p $out
      work=$(mktemp -d)
      cp -r --no-preserve=mode ${folder} "$work/manick-${version}-${arch}"
      cd "$work"
      zip -r -9 $out/manick-${version}-${arch}.zip manick-${version}-${arch}
    '';

  # Run a Windows folder under Wine. The game itself stays in the read-only
  # store (it only reads from its own directory); saves and settings go to
  # %LOCALAPPDATA% / %APPDATA% inside a persistent WINEPREFIX. WoW64 Wine runs
  # both the win64 and win32 builds from one 64-bit prefix, so they share saves.
  mkWineRun = { folder, arch }:
    let wine = pkgs.wineWow64Packages.stable;
    in pkgs.writeShellApplication {
      name = "manick-${arch}";
      runtimeInputs = [ wine pkgs.coreutils ];
      text = ''
        set -euo pipefail
        GAMEDIR="${folder}"
        say() { echo "manick-${arch}: $*"; }

        # WINEPREFIX: explicit > MANICK_WINE_TEMP=1 (throwaway) > XDG state
        TEMP_PREFIX=0
        if [ -n "''${WINEPREFIX:-}" ]; then
          say "using WINEPREFIX from the environment"
        elif [ -n "''${MANICK_WINE_TEMP:-}" ]; then
          WINEPREFIX="$(mktemp -d "''${XDG_RUNTIME_DIR:-/tmp}/manick-wine.XXXXXX")"
          TEMP_PREFIX=1
          say "using a temporary WINEPREFIX (MANICK_WINE_TEMP set; deleted on exit)"
        else
          WINEPREFIX="''${XDG_STATE_HOME:-''${HOME:-/tmp}/.local/state}/manick-wine"
          say "using the persistent WINEPREFIX (set MANICK_WINE_TEMP=1 for a throwaway one)"
        fi
        export WINEPREFIX
        # A WoW64 prefix is always 64-bit; win32 executables run inside it.
        export WINEARCH=win64
        # No Mono/Gecko install prompts; prefer the shipped SDL2.dll.
        export WINEDLLOVERRIDES="SDL2=n,''${WINEDLLOVERRIDES:-mscoree,mshtml=}"
        export WINEDEBUG="''${WINEDEBUG:--all}"

        if [ "$TEMP_PREFIX" -eq 1 ]; then
          # shellcheck disable=SC2329
          cleanup() {
            wineserver -k >/dev/null 2>&1 || true
            rm -rf "$WINEPREFIX"
          }
          trap cleanup EXIT
          trap 'exit 130' INT TERM
        fi

        say "Wine:       ${wine}/bin/wine (${wine.version})"
        say "WINEPREFIX: $WINEPREFIX"
        say "WINEARCH:   $WINEARCH"
        say "WINEDLLOVERRIDES: $WINEDLLOVERRIDES"

        mkdir -p "$WINEPREFIX"
        if [ ! -f "$WINEPREFIX/system.reg" ]; then
          say "creating a new Wine prefix (wineboot --init, takes a moment)..."
          wineboot --init >/dev/null 2>&1 || true
          wineserver -w || true
        else
          say "prefix already initialised (system.reg present)"
        fi

        WINUSER="''${USER:-$(id -un)}"
        USERDIR="$WINEPREFIX/drive_c/users/$WINUSER"
        say "directory layout:"
        say "  game (read-only):  $GAMEDIR"
        say "                     = $(winepath -w "$GAMEDIR" 2>/dev/null || echo "Z:$GAMEDIR")  (Wine maps / to Z:)"
        say "  saves (state):     $USERDIR/AppData/Local/manick/state"
        say "                     = %LOCALAPPDATA%\\manick\\state"
        say "  settings (config): $USERDIR/AppData/Roaming/manick"
        say "                     = %APPDATA%\\manick"
        say "  drive C:           $WINEPREFIX/drive_c"
        say "  drive mappings:    $WINEPREFIX/dosdevices"
        if [ -d "$WINEPREFIX/dosdevices" ]; then
          # Drive letters only; "d::" style entries are raw devices.
          for d in "$WINEPREFIX"/dosdevices/?:; do
            [ -L "$d" ] || continue
            say "    $(basename "$d") -> $(readlink "$d")"
          done
        fi

        # Data is found relative to the working directory (MOAS_DATADIR=.)
        cd "$GAMEDIR"
        say "working directory: $GAMEDIR"
        say "launching: wine manick.exe $*"
        wine ./manick.exe "$@"
      '';
    };

  win64 = mkFolder { crossPkgs = pkgs.pkgsCross.mingwW64; sdl2 = sdl2Win64; arch = "win64"; };
  win32 = mkFolder { crossPkgs = pkgs.pkgsCross.mingw32; sdl2 = sdl2Win32; arch = "win32"; };

in {
  inherit win64 win32;
  win64Run = mkWineRun { folder = win64; arch = "win64"; };
  win32Run = mkWineRun { folder = win32; arch = "win32"; };
  win64Zip = mkZip { folder = win64; arch = "win64"; };
  win32Zip = mkZip { folder = win32; arch = "win32"; };
}
