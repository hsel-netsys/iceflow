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

cmake_minimum_required(VERSION 3.21)
project(
  IceFlow
  VERSION 0.1.0
  DESCRIPTION "NDN-based stream processing library written in C++.")
set(CMAKE_CXX_STANDARD 20)

include(cmake/formatting.cmake)

add_subdirectory(include)

option(BUILD_APPS "Build IceFlow applications" ON)
option(BUILD_TESTS "Build library tests" OFF)

if(BUILD_APPS)
  message(STATUS "Building IceFlow applications...")
  add_subdirectory(apps)
endif()

if(BUILD_TESTS)
  message(STATUS "Building tests...")
  add_subdirectory(tests)
  enable_testing()
endif()

install(FILES cmake/iceflow-config.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/iceflow/cmake)

install(EXPORT ${CMAKE_PROJECT_NAME}
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/iceflow/cmake)
