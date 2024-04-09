{
  inputs = {
    nixpkgs.url = "nixpkgs/release-23.11";
    systems.url = "github:nix-systems/default";
    devenv.url = "github:cachix/devenv";
    devenv.inputs.nixpkgs.follows = "nixpkgs";
  };

  outputs = { self, nixpkgs, devenv, systems, ... } @ inputs:
    let
      forEachSystem = nixpkgs.lib.genAttrs (import systems);
      # Define build dependencies for IceFlow (will be added both to the devShell and to the package build).
      iceflowDependencies = ["yaml-cpp" "nlohmann_json" "boost179" "psync" "ndn-svs" "ndn-cxx"];
    in {

      overlays.default = final: prev: let
         lib = nixpkgs.lib;
         pkgs = prev;
      in rec {
        # Update ndn-cxx to specific commit (required by ndn-svs).
        ndn-cxx = prev.ndn-cxx.overrideAttrs (old: rec {
          src = prev.fetchFromGitHub {
            owner = "named-data";
            repo = "ndn-cxx";
            rev = "18ccbb3b1f600d913dd42dd5c462afdac77e37e0";
            hash = "sha256-yHsp6dBq2kMsubJrn77qeQ9Ah+Udy7nE9eWBX2smemA=";
            fetchSubmodules = true;
          };

        });

        # Update nfd to specific commit.
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

        # Add psync build dependency.
        psync = lib.makeOverridable prev.stdenv.mkDerivation rec {
          pname = "psync";
          version = "8c7c22804c2437166af14156b6681a245e8724fa";

          src = prev.fetchFromGitHub {
            owner = "named-data";
            repo = "PSync";
            rev = "${version}";
            sha256 = "sha256-IY3hq06l4MYpNw2GKY9g9nLyY/yvuNKl10BYz/CJJyg=";
          };

          nativeBuildInputs = with prev; [ pkg-config wafHook python3 ];
          buildInputs = [ ndn-cxx prev.sphinx prev.openssl ];

          wafConfigureFlags = [
            "--boost-includes=${prev.boost179.dev}/include"
            "--boost-libs=${prev.boost179.out}/lib"
            #"--without-tests"
          ];

          # Tests currently fail on macOS (https://github.com/hsel-netsys/iceflow/pull/45#discussion_r1555717705)
          doCheck = false;
          checkPhase = ''
            runHook preCheck
            # this line fixes a bug where value of $HOME is set to a non-writable /homeless-shelter dir
            # see https://github.com/NixOS/nix/issues/670#issuecomment-1211700127
            export HOME=$(pwd)
            LD_LIBRARY_PATH=build/:$LD_LIBRARY_PATH build/unit-tests
            runHook postCheck
          '';
        };

        # Add ndn-svs build dependency.
        ndn-svs = lib.makeOverridable prev.stdenv.mkDerivation rec {
          pname = "ndn-svs";
          version = "dev";

          src = prev.fetchFromGitHub {
            owner = "named-data";
            repo = "ndn-svs";
            rev = "ec54124d79fcb1d4ee00038c8ed3a0cdd9ad4e8b";
            sha256 = "sha256-BoT3G4CAhoq7CerZoOPgGlqRkVY6Arnax834YDAeohk=";
          };

          nativeBuildInputs = with prev; [ pkg-config wafHook python3 ];
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
      };

      packages = forEachSystem (system: let
        pkgs = nixpkgs.legacyPackages.${system}.extend self.overlays.default;
        lib = nixpkgs.lib;
      in rec {
        default = iceflow;

        # IceFlow package
        iceflow = lib.makeOverridable pkgs.stdenv.mkDerivation {
            name = "iceflow";
            src = ./.;

            # Build using cmake, pkg-config and gnumake, add doxygen for docs.
            nativeBuildInputs = with pkgs; [ cmake pkg-config gnumake doxygen ];
            #
            buildInputs = map (x: pkgs."${x}") iceflowDependencies;
        };

        devenv-up = self.devShells.${system}.default.config.procfileScript;
      });

      devShells = forEachSystem
        (system:
          let
            pkgs = nixpkgs.legacyPackages.${system}.extend self.overlays.default;
            lib = nixpkgs.lib;
            # Keep debug symbols disabled for very large packages to avoid long compilation times.
            keepDebuggingDisabledFor = [];
            additionalShellPackages = with pkgs; [nfd cppcheck];
          in rec {
            default = lib.makeOverridable devenv.lib.mkShell {
              inherit inputs pkgs;
              modules = [
                ({config, ...}: {
                  # https://devenv.sh/reference/options/

                  # Add to the dev shell:
                  # - the build system (nativeBuildInputs)
                  # - build dependencies (iceflowDependencies)
                  # - additional software
                  # Unfortunately, we need a separate iceflowDependencies variable and cant use iceflow.buildInputs
                  # directly, as it wouldn't allow enabling debugging.
                  packages = with pkgs; self.packages.${system}.iceflow.nativeBuildInputs
                    ++ (map (x: if builtins.elem x keepDebuggingDisabledFor then pkgs."${x}" else enableDebugging pkgs."${x}") iceflowDependencies)
                    ++ additionalShellPackages;

                  languages.cplusplus.enable = true;

                  pre-commit.hooks = {
                    clang-format = {
                      enable = true;
                      types_or = lib.mkForce ["c" "c++"];
                      files = "(apps|include)\\\/\.\*\\\/\.\*\\\.(c|h)pp";
                    };
                  };
                })
              ];
            };

            debugall = default.override (old: {
              modules = old.modules ++ [
                ({config, ...}: {
                  packages = lib.mkForce (with pkgs; self.packages.${system}.iceflow.nativeBuildInputs
                    ++ (map (x: enableDebugging pkgs."${x}") iceflowDependencies)
                    ++ additionalShellPackages);
                })
              ];
            });

            nodebug = default.override (old: {
              modules = old.modules ++ [
                ({config, ...}: {
                  packages = lib.mkForce (with pkgs; self.packages.${system}.iceflow.nativeBuildInputs
                    ++ (map (x: pkgs."${x}") iceflowDependencies)
                    ++ additionalShellPackages);
                })
              ];
            });

            ci = nodebug;
          });
    };
}
