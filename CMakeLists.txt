# Copyright 2023 The IceFlow Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.18)
project(
  IceFlow
  VERSION 0.1.0
  DESCRIPTION "NDN-based stream processing library written in C++.")
set(CMAKE_CXX_STANDARD 20)

find_package(Boost)
find_package(PkgConfig)
pkg_check_modules(NDN_CXX REQUIRED libndn-cxx)
pkg_check_modules(SVS REQUIRED libndn-svs)

include(cmake/docs.cmake)
include(cmake/format.cmake)
include(cmake/lint.cmake)

include(GNUInstallDirs)

set(SOURCE_FILES src/iceflow.cpp src/consumer.cpp src/producer.cpp
                 src/measurements.cpp)

add_library(iceflow SHARED ${SOURCE_FILES})

target_include_directories(iceflow PUBLIC include/iceflow)
target_compile_options(iceflow PRIVATE -D_GNU_SOURCE -DBOOST_LOG_DYN_LINK)
target_link_libraries(
  iceflow
  ${Boost_LIBRARIES}
  -lboost_log
  -lboost_system
  -lpthread
  -lndn-cxx
  -lndn-svs)

option(BUILD_APPS "Build IceFlow examples" ON)
option(BUILD_TESTS "Build library tests" OFF)

if(BUILD_APPS)
  message(STATUS "Building IceFlow examples...")
  add_subdirectory(examples)
endif()

if(BUILD_TESTS)
  message(STATUS "Building tests...")
  add_subdirectory(tests)
  enable_testing()
endif()

install(TARGETS iceflow DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/iceflow DESTINATION include)
