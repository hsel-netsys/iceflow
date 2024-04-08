# IceFlow Installation Instructions

IceFlow can be built either using a manually configured build environment, see 
[Using systemwide libraries](#using-systemwide-libraries), or using the preconfigured Nix environment, see 
[Using Nix](#using-nix-and-devenvsh).


## Using systemwide libraries

In this document, you can find a detailed walkthrough on how to build and install IceFlow and its dependencies for
Ubuntu and macOS.

### Requirements

In order to compile and run IceFlow, we need the following dependencies installed on our system:

1. ndn-cxx (C++ library for Named Data Networking)
2. NFD (NDN Forwarding Daemon)
3. PSync (Synchronization Library for NDN)
4. nlohman-json (for handling JSON)
5. yaml-cpp (for handling YAML)

#### Install ndn-cxx from source

The ndn-cxx library is the most important dependency for IceFlow, as it provides the fundamental NDN capabilities.

#### Requirements
- GCC >= 10 or clang >= 4.0
- Xcode >= 9.0 (on macOS)
- Python >= 3.6
- pkg-config
- Boost >= 1.71.0
- OpenSSL >= 1.1.1
- SQLite 3.x
- CMake >= 3.18

Under Ubuntu, you can install the requirements from the terminal using the following command:

```sh
sudo apt install build-essential pkg-config python3-minimal libssl-dev libsqlite3-dev libpcap-dev libboost-all-dev cmake software-properties-common
```

Under macOS, you can also install the requirements from the terminal using homebrew:

<!-- TODO: Check which packages need to be installed here. -->
```sh
brew install python boost
```

Now we are set for building ndn-cxx.

#### Download ndn-cxx from Git and build from the source files

```sh
git clone https://github.com/named-data/ndn-cxx
```

You can then follow the [instructions](https://docs.named-data.net/ndn-cxx/current/INSTALL.html)
for building ndn-cxx as a shared library.


#### Install the NFD from source

After the ndn-cxx installation is complete, you can install the NFD by following
its [build instructions](https://docs.named-data.net/NFD/current/INSTALL.html#building-from-source).

After the installation is complete, you can check if the NFD is running by
typing:

```sh
nfd-start
```

If it runs successfully, you can stop it with

```sh
nfd-stop
```

#### Install PSync from source

To install PSync, first clone its Git repository using

```sh
git clone https://github.com/named-data/PSync.git
```

Then you can perform the installation by following the respective
[instructions](https://docs.named-data.net/PSync/current/install.html).

#### Install nlohman-json

To install nlohmann-json using a package manager like homebrew, you can follow
the [official instructions](https://json.nlohmann.me/integration/package_managers/).

On Ubuntu, you can perform the installation via a PPA.

To add it, type

```sh
sudo add-apt-repository ppa:team-xbmc/ppa
sudo apt-get update
```

Afterward, you can install the `nlohmann-json3-dev` deb package as follows:

```sh
sudo apt-get install nlohmann-json3-dev
```

#### Install yaml-cpp

Under most Linux distributions, you will need to
[build yaml-cpp from source](https://github.com/jbeder/yaml-cpp#how-to-build).

Under macOS, you can install it using Homebrew:

```sh
brew install yaml-cpp
```

### Check the shared library packages

To verify that all dependencies are in place, you can use the following commands under Ubuntu:

```sh
# Approach 1 using ldconfig
ldconfig -p | grep libndn
ldconfig -p | grep libyaml
ldconfig -p | grep libboost
ldconfig -p | grep libPSync

# Approach 2 using ldd
ldd /usr/local/bin/nfd
ldd /usr/local/lib/libPSync.so
ldd /usr/local/lib/libyaml-cpp.so
```

Under macOS, you can use the following equivalent commands:

```sh
otool -L /usr/local/bin/nfd
otool -L /usr/local/lib/libPSync.dylib
otool -L /usr/local/lib/libyaml-cpp.a
```

After asserting that all dependencies are available, you may continue with 
[building and installing IceFlow](#building-and-installing).

## Using Nix (and devenv.sh)

Nix is a declarative package manager that allows for easily reproducible development environments created from a
description file written in the Nix programming language.
For more detailed information regarding Nix, see https://nixos.org/.
IceFlow developers might also want to refer to our developer documentation regarding the nix configuration, which can
be found in [Nix-Developer-Guide.md](doc/Nix-Developer-Guide.md).

For the purposes of IceFlow development, the IceFlow Nix environment can be considered as a rough equivalent to
virtualenvs in Python.

### Requirements

In order to compile and run IceFlow using Nix, the following system requirements must be fulfilled:
1. A Linux or macOS based system (Windows might work through the WSL, but is untested, refer to
   [the NixOS documentation regarding WSL](https://wiki.nixos.org/wiki/WSL) if necessary).
2. The Nix package manager must be installed, see [the Nix download page](https://nixos.org/download/) for installation
   instructions.
3. Running the NDN Forwarding Daemon (NFD), which is required for IceFlow components, requires root privileges by
   default.

### Enter the IceFlow Nix development shell

There are two ways that you can enter the IceFlow Nix environment:
You may either enter the Nix environment manually, or automatically using direnv (if installed).

#### Enter the Nix environment manually

To enter the IceFlow development environment, you can run the following command from the root directory of the IceFlow
repository:
```sh
nix --extra-experimental-features nix-command --extra-experimental-features flakes  develop --impure .
```

If you already enabled the `nix-command`  and `flakes` experimental features in your `nix.conf`, this command can be
shortened to:
```sh
nix develop --impure .
```

Note: Using `--impure` is [required by devenv.sh](https://devenv.sh/guides/using-with-flakes/), which is the Nix-based
tool used to manage the development shell.

Entering the environment for the first time might take a while, as it will download (and compile) necessary dependencies
as well as required tools (compiler, linker, etc.).

After entering the Nix shell, you may continue with [building and installing IceFlow](#building-and-installing).

#### Enter the Nix environment using direnv

Assuming that you have already installed and configured [direnv](https://direnv.net/), you may also use the IceFlow
`.envrc`.
On entering the root directory of the IceFlow repository, you will be prompted to allow using the `.envrc` file
(after checking its content for maliciousness), which can be done using the `direnv allow` command.

After doing so, the Nix environment will be entered automatically when entering the directory.
Entering the environment for the first time might take a while, as it will download (and compile) necessary dependencies
as well as required tools (compiler, linker, etc.).

After entering the Nix shell, you may continue with [building and installing IceFlow](#building-and-installing).

## Building and Installing

Before building and installing IceFlow, you will need to generate the build
files using CMake.
To do so, simply enter

```sh
cmake .
```

in the repository's root directory.

### Compile the available IceFlow applications

To compile all the IceFlow applications that are currently defined in the
`src` directory, run the following commands:

```sh
make
```

### Installing IceFlow

To install IceFlow as a library on your system, run

```sh
sudo make install
```

after generating the build files.
For now, IceFlow only supports static linking, as it is not compiled to a shared
library.
This will probably change in future IceFlow versions.

<!-- TODO some instructions on running -->
