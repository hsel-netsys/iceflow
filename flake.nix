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
      iceflowBaseDependencies = ["yaml-cpp" "nlohmann_json" "boost" "ndn-svs" "ndn-cxx" "openssl"];
      iceflowGrpcDependencies = ["protobuf" "grpc"];
      iceflowDependencies = iceflowBaseDependencies ++ iceflowGrpcDependencies;
    in rec {
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
        iceflowPackage = ({crossTarget ? "${system}"}:  let
          pkgs = (import nixpkgs { localSystem = system; crossSystem = crossTarget; }).extend self.overlays.default;
        in
          pkgs.callPackage ({enableExamples ? false, enableGRPC ? false, stdenv, pkgs}: stdenv.mkDerivation {
            name = "iceflow";
            src = ./.;

            # Build using cmake, pkg-config and gnumake, add doxygen for docs.
            # In order to compile the protobuf files, we need the GRPC dependencies as native build inputs as well.
            nativeBuildInputs = (with pkgs; [ cmake pkg-config gnumake doxygen ]) ++ (if enableGRPC then map (x: pkgs."${x}") iceflowGrpcDependencies else []);
            buildInputs = map (x: pkgs."${x}") (iceflowBaseDependencies ++ (if enableGRPC then iceflowGrpcDependencies else []));

            cmakeFlags = [ "-DBUILD_APPS=${if enableExamples then "ON" else "OFF"}" "-DUSE_GRPC=${if enableGRPC then "1" else "0"}" ];
          }) {}
        );
        genIceflowExampleCtrImage = {example_name, args ? [], crossTarget ? "${system}"}: let
          pkgs = (import nixpkgs { localSystem = system; crossSystem = crossTarget; }).extend self.overlays.default;
          crossTargetContainer =
            if crossTarget == "x86_64-linux" then "amd64"
            else if crossTarget == "aarch64-linux" then "arm64"
            else throw "unknown target architecture, please specify docker equivalent architecture for Nix target \"${crossTarget}\" in flake.nix variable \"crossTargetContainer\"";
        in pkgs.dockerTools.buildLayeredImage {
          name = "iceflow-${example_name}";
          tag = "latest";

          contents = [ (self.packages."${system}".iceflow-cross."${crossTarget}".override {enableExamples = true; enableGRPC = true;}) pkgs.busybox pkgs.libgcc];
          architecture = crossTargetContainer;
          config = {
            Cmd = ["sh" "-c" (lib.concatStringsSep " " (["/bin/${example_name}" ] ++ args))];
            Env = [ "ICEFLOW_CONFIG_FILE=/dag.json" ];
            Volume = "/data";
          };
        };
      in rec {
        default = iceflow;

        # IceFlow package
        iceflow = iceflow-cross."${system}";

        # IceFlow package including examples
        iceflow-with-examples = iceflow-cross."${system}".override {enableExamples = true;};

        docker-text2lines = docker-text2lines-cross."${system}";
        docker-lines2words = docker-lines2words-cross."${system}";
        docker-wordcount = docker-wordcount-cross."${system}";

        # IceFlow package
        iceflow-cross = forEachSystem (crossTarget: iceflowPackage {crossTarget = crossTarget;});

        docker-text2lines-cross = forEachSystem (crossTarget: genIceflowExampleCtrImage {
          example_name = "text2lines";
          args = ["$ICEFLOW_CONFIG_FILE"];
          crossTarget = crossTarget;
        });

        docker-lines2words-cross = forEachSystem (crossTarget: genIceflowExampleCtrImage {
          example_name = "lines2words";
          args = ["$ICEFLOW_CONFIG_FILE" "$ICEFLOW_INSTANCE_NUMBER" "$ICEFLOW_INSTANCE_COUNT"];
          crossTarget = crossTarget;
        });

        docker-wordcount-cross = forEachSystem (crossTarget: genIceflowExampleCtrImage {
          example_name = "wordcount";
          args = ["$ICEFLOW_CONFIG_FILE"];
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
            # Also, keep it disabled for libraries that do not support overriding (like libgcc).
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
