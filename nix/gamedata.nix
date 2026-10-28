# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Game data files (GAME.*, LICENCE.DAT) packed from manick-src/ with a host
# moas-util. manick/ is the reference for unchanged graphics, so an unedited
# tree gives files identical to the original. Used by every target, including
# cross builds that can't run moas-util themselves.
#
#   nix build .#manick-gamedata

# Not versioned: it only rebuilds when its sources change.
{ pkgs }:

let
  src = pkgs.lib.fileset.toSource {
    root = ../.;
    fileset = pkgs.lib.fileset.unions [
      ../tools
      ../src/pngread.c
      ../src/pngread.h
      ../src/pngwrite.c
      ../src/pngwrite.h
      ../manick-src
      ../manick
    ];
  };
in
pkgs.stdenv.mkDerivation {
  name = "manick-gamedata";
  inherit src;
  dontConfigure = true;
  buildPhase = ''
    runHook preBuild
    $CC -O2 -D_GNU_SOURCE -Isrc -o moas-util \
      tools/moas-util.c tools/mu_common.c tools/mu_data.c tools/mu_gfx.c \
      tools/mu_png.c tools/mu_script.c src/pngread.c src/pngwrite.c
    ./moas-util --srcdir manick-src --outdir $out --reference manick pack
    runHook postBuild
  '';
  dontInstall = true;
  meta = {
    description = "Mänick Mänschion game data packed from manick-src/";
    platforms = pkgs.lib.platforms.linux;
  };
}
