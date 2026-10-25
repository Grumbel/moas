# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Windows builds, cross-compiled with MinGW (pkgsCross). SDL2 is the official
# prebuilt MinGW package (SDL2-win32 flake), same approach as Kurvenrausch/Pingus.
#
#   nix build .#moas-win64
#   nix build .#moas-win64-zip
#   nix build .#moas-win32
#   nix build .#moas-win32-zip

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
    in pkgs.runCommand "moas-${arch}-${version}" { } ''
      mkdir -p $out
      cp ${game}/bin/moas.exe $out/
      cp ${sdl2}/bin/SDL2.dll $out/
      # Game data beside the binary
      cp ${gamedata}/* $out/
      mkdir -p $out/lang
      cp ${../lang}/*.po $out/lang/
      # per-language pictures: lang/<code>/<kind>/*.png
      for d in ${../lang}/*/; do cp -r "$d" $out/lang/; done
      cp ${../COPYING} $out/COPYING.txt
      cp ${../data/icons/hicolor/256x256/apps/moas.png} $out/moas.png 2>/dev/null || true
      cat > $out/README.txt <<EOF
      MOAS ${version}
      Mouse Orientiertes Adventure System — Mänick Mänschion

      Start moas.exe. Point and click with the mouse; Space/Enter also click.
      F10 opens the file menu. Esc aborts dialogs. Q or Alt+F4 quits.
      Use --fullscreen for a borderless fullscreen window.

      Game data files (GAME.*) must stay next to the executable.

      Licence: GPL-2.0-or-later (COPYING.txt).
      Source: https://github.com/Grumbel/moas
      EOF
      sed -i 's/^      //; s/$/\r/' $out/README.txt
    '';

  mkZip = { folder, arch }:
    pkgs.runCommand "moas-${version}-${arch}.zip" { nativeBuildInputs = [ pkgs.zip ]; } ''
      mkdir -p $out
      work=$(mktemp -d)
      cp -r --no-preserve=mode ${folder} "$work/moas-${version}-${arch}"
      cd "$work"
      zip -r -9 $out/moas-${version}-${arch}.zip moas-${version}-${arch}
    '';

  win64 = mkFolder { crossPkgs = pkgs.pkgsCross.mingwW64; sdl2 = sdl2Win64; arch = "win64"; };
  win32 = mkFolder { crossPkgs = pkgs.pkgsCross.mingw32; sdl2 = sdl2Win32; arch = "win32"; };

in {
  inherit win64 win32;
  win64Zip = mkZip { folder = win64; arch = "win64"; };
  win32Zip = mkZip { folder = win32; arch = "win32"; };
}
