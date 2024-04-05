# IceFlow Nix Development Environment Guide
Nix is a declarative package manager that allows for easily reproducible development environments created from a
description file written in the Nix programming language.
For more detailed information regarding Nix, see https://nixos.org/.

This document provides documentation regarding the use of Nix for IceFlow development, as well as instructions on 
how to change or update dependencies and some explanations regarding the configuration file.

## Using Nix for IceFlow development
IceFlow provides a Nix Flake, which provides a development environment using the Nix-based tool 
[devenv.sh](https://devenv.sh).

In order to compile IceFlow in the Nix environment, you may follow the 
[Nix section of the installation instructions](../Install.md#using-nix-and-devenvsh).

Some IDEs may allow for direct integration with devenv.sh, see 
[the devenv.sh documentation](https://devenv.sh/getting-started/), most notably the section on "Editor Support".
For VSCode, refer to https://devenv.sh/editor-support/vscode/.
If you are using JetBrains IDEs (CLion), refer to the following section.

### Using the Nix environment in CLion
A plugin for adding syntax highlighting and linting to .nix files can be found [here](https://github.com/NixOS/nix-idea).

Unfortunately, CLion does not have direct support for Nix integration (i.e. automatically enabling a Nix environment 
configured for a project and using its compiler, libraries and debugger), even though this feature has been requested 
both for [the IDE itself](https://intellij-support.jetbrains.com/hc/en-us/community/posts/360008227939-How-to-configure-a-Nix-based-remote-interpreter)
as well as for [the nix-idea plugin](https://github.com/NixOS/nix-idea/issues/1).

So far, the easiest way to use Nix in CLion is by simply starting it from the command line while inside the Nix 
development shell.
This allows you to configure a toolchain in CLion that uses the Nix environment by simply specifying the command names 
without an absolute path:
![Nix toolchain configuration in CLion](nix_clion.png)


## Changing the Nix environment
TODO

### Updating dependencies

For updates from the Nix package repositories:
`nix flake update` (will update flake.lock)

For updating packages that we defined ourselves:
1. Update the `src` attribute of the package in the `flake.nix` file as follows:
    - Update the `rev` to the git revision (commit hash or tag) you want to switch to.
    - Change a single character of the `hash` attribute (otherwise Nix will think that it doesn't have to redownload)
2. Save and close the file and reload the Nix environment (will happen automatically when using direnv). 
    This *will* fail, as the downloaded hash doesn't match, but the error message will provide you with the correct 
    hash of the downloaded new revision.
3. Update the `hash` attribute of the package again, this time using the hash given in the error message.
4. Save and close the file again. This time, the build should succeed (assuming there are no actualy compilation issues).

TODO expand

### Defining a dependency
TODO

#### Defining a dependency available in the repositories
TODO

#### Defining a dependency not available in the repositories
TODO

### Overriding packages (e.g. change versions)
TODO

### Adding command line tools
TODO
