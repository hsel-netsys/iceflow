# IceFlow Installation Instructions

In this document, you can find a detailed walkthrough on how to build and install IceFlow and its dependencies for
Ubuntu and macOS.

## Requirements

In order to compile and run IceFlow, we need the following dependencies installed on our system:

1. ndn-cxx (C++ library for Named Data Networking)
2. NFD (NDN Forwarding Daemon)
3. PSync (Synchronization Library for NDN)
4. nlohman-json (for handling JSON)
5. yaml-cpp (for handling YAML)

## Install ndn-cxx from the source

The ndn-cxx library is the most important dependency for IceFlow, as it provides the fundamental NDN capabilities.

### Requirements
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

### Download ndn-cxx from Git and build from the source files

```sh
git clone https://github.com/named-data/ndn-cxx
cd ndn-cxx
./waf configure
./waf
sudo ./waf install
sudo ldconfig  # on Linux only
```

## Download the NFD from Git and build from the source files

```sh
git clone --recursive https://github.com/named-data/NFD
cd NFD
./waf configure
./waf
sudo ./waf install
sudo ldconfig
sudo cp /usr/local/etc/ndn/nfd.conf.sample /usr/local/etc/ndn/nfd.conf
```

To check if the NFD is running, type:

```sh
nfd-start
```

If it runs successfully, you can stop it with

```sh
nfd-stop
```

## Download PSync from Git and build from the source files

```sh
git clone https://github.com/named-data/PSync.git
cd PSync
./waf configure
./waf
sudo ./waf install
```

## Install nlohman-json

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

Under macOS, you can use Homebrew:

```sh
brew install nlohmann-json
```

## Install yaml-cpp

Under Ubuntu, you need to build yaml-cpp from its source files:

```sh
git clone https://github.com/jbeder/yaml-cpp.git
cd yaml-cpp
mkdir build
cd build
cmake -DYAML_BUILD_SHARED_LIBS=on ..
make
sudo make install
```

Under macOS, you can install it using Homebrew:

```sh
brew install yaml-cpp
```

## Check the shared library packages

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

## Compile the available IceFlow applications

To compile all the IceFlow applications that are currently defined in the
`src` directory, run the following commands:

```sh
cmake .
make
```
