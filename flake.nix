{
  inputs = {
    nixpkgs.url = "nixpkgs/release-23.11";
    systems.url = "github:nix-systems/default";
    devenv.url = "github:cachix/devenv";
    devenv.inputs.nixpkgs.follows = "nixpkgs";
  };

  nixConfig = {
    extra-trusted-public-keys = "devenv.cachix.org-1:w1cLUi8dv3hnoSPGAuibQv+f9TZLr6cv/Hm9XgU50cw=";
    extra-substituters = "https://devenv.cachix.org";
  };

  outputs = { self, nixpkgs, devenv, systems, ... } @ inputs:
    let
      pkg-overlay = (final: prev: rec {
        ndn-cxx = prev.ndn-cxx.overrideAttrs (old: rec {
          src = prev.fetchFromGitHub {
            owner = "named-data";
            repo = "ndn-cxx";
            rev = "18ccbb3b1f600d913dd42dd5c462afdac77e37e0";
            hash = "sha256-yHsp6dBq2kMsubJrn77qeQ9Ah+Udy7nE9eWBX2smemA="; 
            fetchSubmodules = true; 
          };

        });

        nfd = prev.nfd.overrideAttrs (old: {
          src = prev.fetchFromGitHub {
            owner = "named-data";
            repo = "nfd";
            rev = "95d63b113219d1f6bed9fb8f0fff86c9b24db422";
            hash = "sha256-3TxX9cscbysDRVE5Zr8KHYwlrbI0gkuMldnFLVk9L48=";
            fetchSubmodules = true;
          };

          wafConfigureFlags = [
            "--boost-includes=${prev.boost179.dev}/include"
            "--boost-libs=${prev.boost179.out}/lib"
          ];
          
          doCheck = false;
        });
        psync = prev.stdenv.mkDerivation rec {
          pname = "psync";
          version = "8c7c22804c2437166af14156b6681a245e8724fa";

          src = prev.fetchFromGitHub {
            owner = "named-data";
            repo = "PSync";
            rev = "${version}";
            sha256 = "sha256-IY3hq06l4MYpNw2GKY9g9nLyY/yvuNKl10BYz/CJJyg=";
          };

          nativeBuildInputs = [ prev.pkg-config prev.wafHook prev.python3 ];
          buildInputs = [ ndn-cxx prev.sphinx prev.openssl ];

          wafConfigureFlags = [
            "--boost-includes=${prev.boost179.dev}/include"
            "--boost-libs=${prev.boost179.out}/lib"
            "--with-tests"
          ];

          doCheck = true;
          checkPhase = ''
            runHook preCheck
            # this line removes a bug where value of $HOME is set to a non-writable /homeless-shelter dir
            # see https://github.com/NixOS/nix/issues/670#issuecomment-1211700127
            export HOME=$(pwd)
            LD_LIBRARY_PATH=build/:$LD_LIBRARY_PATH build/unit-tests
            runHook postCheck
          '';
        };
        ndn-svs = prev.stdenv.mkDerivation rec {
          pname = "ndn-svs";
          version = "dev";

          src = prev.fetchFromGitHub {
            owner = "named-data";
            repo = "ndn-svs";
            rev = "ec54124d79fcb1d4ee00038c8ed3a0cdd9ad4e8b";
            sha256 = "sha256-BoT3G4CAhoq7CerZoOPgGlqRkVY6Arnax834YDAeohk=";
          };

          nativeBuildInputs = [ prev.pkg-config prev.wafHook prev.python3 ];
          buildInputs = [ ndn-cxx prev.sphinx prev.openssl ];

          wafConfigureFlags = [
            "--boost-includes=${prev.boost179.dev}/include"
            "--boost-libs=${prev.boost179.out}/lib"
            "--with-tests"
          ];

          # Tests currently fail
          doCheck = false;
          checkPhase = ''
            runHook preCheck
            # this line removes a bug where value of $HOME is set to a non-writable /homeless-shelter dir
            # see https://github.com/NixOS/nix/issues/670#issuecomment-1211700127
            export HOME=$(pwd)
            LD_LIBRARY_PATH=build/:$LD_LIBRARY_PATH build/unit-tests
            runHook postCheck
          '';
        };        
      });
      forEachSystem = nixpkgs.lib.genAttrs (import systems);
    in {
      packages = forEachSystem (system: {
        devenv-up = self.devShells.${system}.default.config.procfileScript;
                
      });

      devShells = forEachSystem
        (system:
          let
            pkgs = nixpkgs.legacyPackages.${system}.extend pkg-overlay;
            lib = nixpkgs.lib;
          in
          {
            default = devenv.lib.mkShell {
              inherit inputs pkgs;
              modules = [
                ({config, ...}: {
                  # https://devenv.sh/reference/options/
                  packages = with pkgs; [ 
                    ndn-cxx
                    nfd
                    psync
                    ndn-svs
                    yaml-cpp
                    nlohmann_json
                    boost179
                    opencv
                    gnumake
                    cmake
                  ];

                  languages.cplusplus.enable = true;

                  pre-commit.hooks = {
                    clang-format = {
                      enable = true;
                      types_or = lib.mkForce ["c" "c++"];
                      files = "(apps|include)\\\/\.\*\\\/\.\*\\\.(c|h)pp";
                      #entry = lib.mkForce "${pkgs.clang-tools}/bin/clang-format -style=file -i src/**/*.cpp src/**/*.hpp";
                    };
                  };
                })
              ];
            };
          });
    };
}
