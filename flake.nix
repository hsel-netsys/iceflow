{
  inputs = {
    nixpkgs.url = "nixpkgs/nixos-24.11";
    systems.url = "github:nix-systems/default";
    devenv.url = "github:cachix/devenv";
    devenv.inputs.nixpkgs.follows = "nixpkgs";
  };

  outputs = { self, nixpkgs, devenv, systems, ... } @ inputs:
    let
      forEachSystem = nixpkgs.lib.genAttrs (import systems);
      # Define build dependencies for IceFlow (will be added both to the devShell and to the package build).
      iceflowDependencies = ["yaml-cpp" "nlohmann_json" "boost" "ndn-svs" "ndn-cxx" "grpc" "openssl" "protobuf"];
    in {

      overlays.default = final: prev: let
         lib = nixpkgs.lib;
         pkgs = prev;
      in rec {
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
          buildInputs = [ prev.ndn-cxx prev.sphinx prev.openssl ];

          wafConfigureFlags = [
            "--boost-includes=${prev.boost179.dev}/include"
            "--boost-libs=${prev.boost179.out}/lib"
            "--with-tests"
          ];
          dontAddWafCrossFlags = true;

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
        lib = nixpkgs.lib;
        iceflowPackage = ({enableExamples ? false, crossTarget ? "${system}"}:  let
          pkgs = (import nixpkgs { localSystem = system; crossSystem = crossTarget; }).extend self.overlays.default;
        in lib.makeOverridable pkgs.stdenv.mkDerivation {
          name = "iceflow";
          src = ./.;

          # Build using cmake, pkg-config and gnumake, add doxygen for docs.
          nativeBuildInputs = with pkgs; [ cmake pkg-config gnumake doxygen ];
          buildInputs = map (x: pkgs."${x}") iceflowDependencies;

          cmakeFlags = [ "-DBUILD_APPS=${if enableExamples then "ON" else "OFF"}" ];
        });
        genIceflowExampleCtrImage = {example_name, args ? [], crossTarget ? "${system}"}: let
          pkgs = (import nixpkgs { localSystem = system; crossSystem = crossTarget; }).extend self.overlays.default;
          crossTargetContainer =
            if crossTarget == "x86_64-linux" then "amd64"
            else if crossTarget == "aarch64-linux" then "arm64"
            else throw "unknown target architecture, please specify docker equivalent architecture for Nix target \"${crossTarget}\" in flake.nix variable \"crossTargetContainer\"";
        in pkgs.dockerTools.buildLayeredImage {
          name = "iceflow-${example_name}";
          tag = "latest";

          contents = [ self.packages."${system}".iceflow-with-examples-cross."${crossTarget}" pkgs.busybox ];
          architecture = crossTargetContainer;
          config = {
            Cmd = ["sh" "-c" (lib.concatStringsSep " " (["/bin/${example_name}" ] ++ args))];
            Env = [ "ICEFLOW_CONFIG_FILE=/data/${example_name}.yaml" "ICEFLOW_METRICS_FILE=/data/${example_name}.metrics" ];
            Volume = "/data";
          };
        };
      in rec {
        default = iceflow;

        # IceFlow package
        iceflow = iceflow-cross."${system}";

        # IceFlow package including examples
        iceflow-with-examples = iceflow-with-examples-cross."${system}";

        docker-text2lines = docker-text2lines-cross."${system}";
        docker-lines2words = docker-lines2words-cross."${system}";
        docker-wordcount = docker-wordcount-cross."${system}";

        # IceFlow package
        iceflow-cross = forEachSystem (crossTarget: iceflowPackage {enableExamples = false; crossTarget = crossTarget;});

        # IceFlow examples
        iceflow-with-examples-cross = forEachSystem (crossTarget: iceflowPackage {enableExamples = true; crossTarget = crossTarget;});

        docker-text2lines-cross = forEachSystem (crossTarget: genIceflowExampleCtrImage {
          example_name = "text2lines";
          args = ["$ICEFLOW_CONFIG_FILE" "$ICEFLOW_INPUT_FILE" "$ICEFLOW_METRICS_FILE"];
          crossTarget = crossTarget;
        });

        docker-lines2words-cross = forEachSystem (crossTarget: genIceflowExampleCtrImage {
          example_name = "lines2words";
          args = ["$ICEFLOW_CONFIG_FILE" "$ICEFLOW_METRICS_FILE"];
          crossTarget = crossTarget;
        });

        docker-wordcount-cross = forEachSystem (crossTarget: genIceflowExampleCtrImage {
          example_name = "wordcount";
          args = ["$ICEFLOW_CONFIG_FILE" "$ICEFLOW_METRICS_FILE"];
          crossTarget = crossTarget;
        });

        devenv-up = self.devShells.${system}.default.config.procfileScript;
      });

      devShells = forEachSystem
        (system:
          let
            pkgs = nixpkgs.legacyPackages.${system}.extend self.overlays.default;
            lib = nixpkgs.lib;
            # Keep debug symbols disabled for very large packages to avoid long compilation times.
            keepDebuggingDisabledFor = [];
            additionalShellPackages = with pkgs; [nfd cppcheck manifest-tool];
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
                      files = "(examples|include)\\\/\.\*\\\/\.\*\\\.(c|h)pp";
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
