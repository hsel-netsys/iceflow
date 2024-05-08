[![Build Status](https://github.com/hsel-netsys/iceflow/actions/workflows/build.yml/badge.svg)](https://github.com/hsel-netsys/iceflow/actions/workflows/build.yml)
[![Documentation Status](https://img.shields.io/github/actions/workflow/status/hsel-netsys/iceflow/doxygen-gh-pages.yml?label=docs&link=https%3A%2F%2Fhsel-netsys.github.io%2Ficeflow)](https://hsel-netsys.github.io/iceflow)
![License](https://img.shields.io/github/license/hsel-netsys/iceflow)

# IceFlow

This repository contains IceFlow, a stream processing library based on
Named-Data Networking (NDN) written in C++.
Besides the actual library, a simple word counting example application is included
in the repository to illustrate the use of IceFlow for stream processing use
cases.

## Pre-Requisites and Installation

Detailed installation instructions for installing the required dependencies and IceFlow itself can be found in the file [Install.md](Install.md).

## Generating Build Files

To set up the build system for the project, you need to have CMake (with a minimum version of 3.18) installed on your system.
With CMake installed, you can generate the build files using the command

```sh
cmake .
```

in the repository's root directory.

## Documentation

IceFlow's documentation can be found [here](https://hsel-netsys.github.io/iceflow).

If you have Doxygen installed, you can generate the documentation locally
(after generating the build files) using the following command:

```sh
make docs
```

## Code Style

In order to achieve a consistent style, IceFlow's codebase is formatted using
`clang-format`.
After installing it (e.g., by using `brew install clang-format` on macOS) and generating the build files, you can format all source files by invoking

```sh
make format
```

The CI pipeline will assert that the codebase is always correctly
formatted and will fail otherwise.

## Static Analysis

IceFlow uses cppcheck to lint its source code.
To perform the linting step locally, use the following command (after generating the build files):

```sh
make lint
```
