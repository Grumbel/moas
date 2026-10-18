{
  description = "MOAS - Mouse Orientiertes Adventure System (SDL2 Linux port)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f {
        inherit system;
        pkgs = import nixpkgs { inherit system; };
      });
    in
    {
      packages = forAllSystems ({ pkgs, system }: {
        default = pkgs.stdenv.mkDerivation {
          pname = "moas";
          version = "0.1.0-sdl";

          src = ./.;

          nativeBuildInputs = with pkgs; [
            cmake
            pkg-config
          ];

          buildInputs = with pkgs; [
            SDL2
          ];

          cmakeFlags = [
            "-DCMAKE_BUILD_TYPE=Release"
            # Bake the store path so the binary finds data without env vars
            "-DMOAS_DATADIR=${placeholder "out"}/share/moas"
          ];

          meta = with pkgs.lib; {
            description = "Point-and-click adventure engine (DOS port to SDL2)";
            license = licenses.gpl2Plus;
            platforms = platforms.linux;
            mainProgram = "moas";
          };
        };
      });

      apps = forAllSystems ({ pkgs, system }: {
        default = {
          type = "app";
          program = "${self.packages.${system}.default}/bin/moas";
          meta = {
            description = "Run MOAS (Mänick Mänschion) with bundled data";
          };
        };
        moas = {
          type = "app";
          program = "${self.packages.${system}.default}/bin/moas";
        };
      });

      devShells = forAllSystems ({ pkgs, system }: {
        default = pkgs.mkShell {
          packages = with pkgs; [
            cmake
            pkg-config
            SDL2
            gdb
            clang-tools
          ];
          # Local runs from the repo: prefer ./manick
          shellHook = ''
            export MOAS_DATADIR="''${MOAS_DATADIR:-$PWD/manick}"
          '';
        };
      });
    };
}
