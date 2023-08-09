[![Build Status](https://github.com/hsel-netsys/iceflow/actions/workflows/build.yml/badge.svg)](https://github.com/hsel-netsys/iceflow/actions/workflows/build.yml)

# IceFlow

This repository contains IceFlow, a stream processing library based on
Named-Data Networking (NDN) written in C++.
Besides the actual library, a number of applications are included in the
repository illustrating the use of IceFlow for a video processing use case.

<!-- TODO: Expand README -->

## Installation

Detailed installation instructions can be found in the file [Install.md](Install.md).

## Code Style

In order to achieve a consistent style, IceFlow's codebase is formatted using
`clang-format`.
After installing it (e.g., by using `brew install clang-format` on macOS) you
can format all source files by invoking the following command:

```sh
clang-format -i src/**/*.cpp src/**/*.hpp
```
The CI pipeline will assert that the codebase is always correctly
formatted and will fail otherwise.
