# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
{
  description = "MOAS - Mouse Orientiertes Adventure System (SDL2 port + DOS Watcom target)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";

    # Official prebuilt MinGW SDL2 (same as Kurvenrausch / Pingus)
    SDL2-win32.url = "github:grumnix/SDL2-win32";
    SDL2-win32.inputs.nixpkgs.follows = "nixpkgs";
    SDL2-win32.inputs.flake-utils.follows = "flake-utils";

    # SDL2 sources for the Android NDK build
    sdl2-src = {
      url = "https://github.com/libsdl-org/SDL/releases/download/release-2.30.3/SDL2-2.30.3.tar.gz";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, flake-utils, SDL2-win32, sdl2-src }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
        # Open Watcom is Watcom-1.0 (redistributable; marked unfree in nixpkgs)
        dosPkgs = import nixpkgs {
          inherit system;
          config.allowUnfreePredicate = pkg:
            builtins.elem (pkgs.lib.getName pkg) [
              "open-watcom-bin"
              "open-watcom-bin-unwrapped"
              "open-watcom-v2"
              "open-watcom-v2-unwrapped"
              "open-watcom-v2-full"
              "open-watcom-v2-full-unwrapped"
            ];
        };

        version = "0.1.0-sdl";

        moas = pkgs.stdenv.mkDerivation {
          pname = "moas";
          inherit version;
          src = ./.;

          dontStrip = true;

          nativeBuildInputs = with pkgs; [ cmake pkg-config ];
          buildInputs = with pkgs; [ SDL2 ];

          cmakeFlags = [
            "-DCMAKE_BUILD_TYPE=RelWithDebInfo"
            "-DMOAS_DATADIR=${placeholder "out"}/share/moas"
          ];

          meta = with pkgs.lib; {
            description = "Point-and-click adventure engine (DOS port to SDL2)";
            license = licenses.gpl2Plus;
            platforms = platforms.linux;
            mainProgram = "moas";
          };
        };

        r36s = import ./nix/r36s.nix { inherit pkgs version; };

        dos = import ./nix/dos.nix { pkgs = dosPkgs; inherit version; };

        androidPkgs = import nixpkgs {
          inherit system;
          config.allowUnfree = true;
          config.android_sdk.accept_license = true;
        };
        android = import ./nix/android.nix {
          pkgs = androidPkgs;
          sdlSrc = sdl2-src;
          sdlVersion = "2.30.3";
          buildToolsVersion = "30.0.3";
          packagePlatform = "22";
          compilePlatform = "33";
          targetAbis = [ "armeabi-v7a" "arm64-v8a" "x86_64" ];
          androidSdk = (androidPkgs.androidenv.composeAndroidPackages {
            platformVersions = [ "22" "33" ];
            buildToolsVersions = [ "30.0.3" ];
            includeNDK = true;
            ndkVersion = "29.0.14206865";
            includeEmulator = false;
            includeSources = false;
          }).androidsdk;
          inherit version;
          versionCode = self.revCount or 1;
        };

        windows = import ./nix/windows.nix {
          inherit pkgs version;
          sdl2Win64 = SDL2-win32.packages.${system}.SDL2-win64;
          sdl2Win32 = SDL2-win32.packages.${system}.SDL2-win32;
        };

        wasm = import ./nix/wasm.nix {
          inherit pkgs version;
          sdlSrc = sdl2-src;
          sdlVersion = "2.30.3";
        };
      in {
        packages = {
          default = moas;
          moas = moas;

          moas-win64 = windows.win64;
          moas-win64-zip = windows.win64Zip;
          moas-win32 = windows.win32;
          moas-win32-zip = windows.win32Zip;

          moas-r36s = r36s.r36s;
          moas-r36s-portmaster = r36s.portmaster;
          moas-r36s-portmaster-zip = r36s.portmasterZip;
          arkos-sysroot = r36s.sysroot;

          moas-android = android.apk;
          sdl2-android = android.sdlAndroidLibs;

          moas-wasm = wasm.moasWasm;
          sdl2-wasm = wasm.sdl2Wasm;

          moas-dos = dos.moas-dos;
          moas-dos-zip = dos.moas-dos-zip;
          moas-dos-run = dos.moas-dos-run;
        };

        apps = {
          default = {
            type = "app";
            program = "${moas}/bin/moas";
          };
          moas = {
            type = "app";
            program = "${moas}/bin/moas";
          };
          # Open Watcom DOS rebuild under DOSBox-X
          moas-dos = {
            type = "app";
            program = "${dos.moas-dos-run}/bin/moas-dos";
          };
          install-android-moas = android.installApp;
          moas-wasm = wasm.serveApp;
        };

        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            cmake
            pkg-config
            SDL2
            gdb
            clang-tools
          ];
          shellHook = ''
            export MOAS_DATADIR="''${MOAS_DATADIR:-$PWD/manick}"
          '';
        };
      });
}
