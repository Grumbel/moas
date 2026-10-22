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
        # The SDL3 that sdl2-compat was built against (on its RUNPATH).
        # ASan intercepts dlopen(), so the RUNPATH no longer applies when
        # sdl2-compat loads SDL3; the develop shell puts this one on
        # LD_LIBRARY_PATH instead. pkgs.sdl3 can be a different build.
        compatSdl3 = pkgs.lib.findFirst (p: pkgs.lib.hasPrefix "sdl3" (p.pname or p.name or ""))
          null pkgs.SDL2.buildInputs;
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
          # nixpkgs SDL2 is sdl2-compat; it needs matching SDL3 on the loader path.
          buildInputs = with pkgs; [ SDL2 sdl3 ];

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
        # Out-of-tree Debug develop loop (biltoo/Pingus pattern).
        # Package builds stay `nix build` (RelWithDebInfo); develop uses Debug.
        moasPreamble = ''
          set -euo pipefail
          if [ -z "''${MOAS_SOURCE:-}" ]; then
            echo "$0: MOAS_SOURCE is not set (enter the shell with: nix develop)" >&2
            exit 1
          fi
          MOAS_BUILD_DIR="''${MOAS_BUILD_DIR:-/tmp/moas-build}"
        '';

        moasConfigure = pkgs.writeShellScriptBin "moas-configure" ''
          ${moasPreamble}
          mkdir -p "$MOAS_BUILD_DIR"
          cmake -S "$MOAS_SOURCE" -B "$MOAS_BUILD_DIR" \
            -DCMAKE_BUILD_TYPE="''${CMAKE_BUILD_TYPE:-Debug}" \
            -DMOAS_SANITIZE="''${MOAS_SANITIZE-address,undefined}" \
            -DMOAS_DATADIR="''${MOAS_DATADIR:-$MOAS_SOURCE/manick}"
          echo "moas-configure: $MOAS_BUILD_DIR (CMAKE_BUILD_TYPE=''${CMAKE_BUILD_TYPE:-Debug}, MOAS_SANITIZE=''${MOAS_SANITIZE-address,undefined})"
        '';

        moasBuild = pkgs.writeShellScriptBin "moas-build" ''
          ${moasPreamble}
          need_cfg=0
          if [ ! -f "$MOAS_BUILD_DIR/CMakeCache.txt" ]; then
            need_cfg=1
          else
            cached=$(grep -E '^CMAKE_HOME_DIRECTORY:' "$MOAS_BUILD_DIR/CMakeCache.txt" | head -1 | cut -d= -f2- || true)
            if [ -z "$cached" ] || [ "$cached" != "$MOAS_SOURCE" ]; then
              echo "moas-build: CMake source changed ($cached -> $MOAS_SOURCE); reconfiguring" >&2
              need_cfg=1
            elif [ ! -d "$cached" ]; then
              echo "moas-build: cached source missing; reconfiguring" >&2
              need_cfg=1
            else
              san=$(grep -E '^MOAS_SANITIZE:' "$MOAS_BUILD_DIR/CMakeCache.txt" | head -1 | cut -d= -f2- || true)
              if [ "$san" != "''${MOAS_SANITIZE-address,undefined}" ]; then
                echo "moas-build: MOAS_SANITIZE changed ($san -> ''${MOAS_SANITIZE-address,undefined}); reconfiguring" >&2
                need_cfg=1
              fi
            fi
          fi
          if [ "$need_cfg" -eq 1 ]; then
            moas-configure
          fi
          cmake --build "$MOAS_BUILD_DIR" -j"''${NIX_BUILD_CORES:-$(nproc)}"
        '';

        moasRun = pkgs.writeShellScriptBin "moas-run" ''
          ${moasPreamble}
          moas-build
          exec "$MOAS_BUILD_DIR/moas" \
            --datadir="''${MOAS_DATADIR:-$MOAS_SOURCE/manick}" \
            "$@"
        '';

        moasRunGdb = pkgs.writeShellScriptBin "moas-run-gdb" ''
          ${moasPreamble}
          moas-build
          exec gdb -q \
            -ex "set pagination off" \
            -ex "set confirm off" \
            -ex "set debuginfod enabled off" \
            -ex run \
            -ex 'python
try:
  ec = gdb.parse_and_eval("$_exitcode")
  if int(ec) == 0:
    gdb.execute("quit")
except Exception:
  pass
' \
            --args "$MOAS_BUILD_DIR/moas" \
            --datadir="''${MOAS_DATADIR:-$MOAS_SOURCE/manick}" \
            "$@"
        '';

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
          moas-dos-original = dos.moas-dos-original;
          moas-dos-original-run = dos.moas-dos-original-run;
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
          # Historical manick/GAME.EXE under DOSBox-X
          moas-dos-original = {
            type = "app";
            program = "${dos.moas-dos-original-run}/bin/moas-dos-original";
          };
          install-android-moas = android.installApp;
          moas-wasm = wasm.serveApp;
        };

        devShells.default = pkgs.mkShell.override { stdenv = pkgs.ccacheStdenv; } {
          inputsFrom = [ moas ];
          packages = with pkgs; [
            cmake
            ninja
            pkg-config
            SDL2
            sdl3
            gdb
            ccache
            clang-tools
            moasConfigure
            moasBuild
            moasRun
            moasRunGdb
          ];
          CMAKE_BUILD_TYPE = "Debug";
          shellHook = ''
            export MOAS_SOURCE="$PWD"
            export MOAS_BUILD_DIR="''${MOAS_BUILD_DIR:-/tmp/moas-build}"
            export MOAS_DATADIR="''${MOAS_DATADIR:-$MOAS_SOURCE/manick}"
            # The engine never frees its resource buffers; leak reports at
            # exit would bury real ASan errors. UBSan: print a stack trace.
            export ASAN_OPTIONS="''${ASAN_OPTIONS:-detect_leaks=0}"
            export UBSAN_OPTIONS="''${UBSAN_OPTIONS:-print_stacktrace=1}"
            # sdl2-compat dlopen()s SDL3 at runtime. Replace (not extend) the
            # host LD_LIBRARY_PATH: a foreign one breaks sdl2-compat's dllinit().
            # Not pkgs.sdl3: it is a different build than the one sdl2-compat
            # finds via its RUNPATH, and would shadow it. compatSdl3 is that
            # exact build; needed with ASan (see above).
            export LD_LIBRARY_PATH="${pkgs.lib.makeLibraryPath ((with pkgs; [ SDL2 libGL libx11 ]) ++ pkgs.lib.optional (compatSdl3 != null) (compatSdl3.lib or compatSdl3))}''${MOAS_EXTRA_LD_LIBRARY_PATH:+:$MOAS_EXTRA_LD_LIBRARY_PATH}"
            echo "MOAS develop shell"
            echo "  MOAS_SOURCE=$MOAS_SOURCE"
            echo "  MOAS_BUILD_DIR=$MOAS_BUILD_DIR"
            echo "  MOAS_DATADIR=$MOAS_DATADIR"
            echo "  CMAKE_BUILD_TYPE=''${CMAKE_BUILD_TYPE:-Debug}"
            echo "  MOAS_SANITIZE=''${MOAS_SANITIZE-address,undefined}  (MOAS_SANITIZE= to disable)"
            echo "Commands: moas-configure | moas-build | moas-run | moas-run-gdb"
            echo "  moas-run --load=0   # example CLI after a save exists"
          '';
        };


      });
}
