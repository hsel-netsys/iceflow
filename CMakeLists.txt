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
set(version 0.1.0)
set(so_version 0)

project(
  IceFlow
  VERSION ${version}
  DESCRIPTION "NDN-based stream processing library written in C++.")
set(CMAKE_CXX_STANDARD 20)

option(USE_GRPC "Enable gRPC support" ON)

find_package(Boost CONFIG)
find_package(PkgConfig)
find_package(OpenSSL REQUIRED)

if(USE_GRPC)
  add_definitions(-DUSE_GRPC=1)
  find_package(gRPC CONFIG REQUIRED)
  message(STATUS "Using gRPC ${gRPC_VERSION}")
  find_program(gRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
endif()

pkg_check_modules(NDN_CXX REQUIRED libndn-cxx)
pkg_check_modules(SVS REQUIRED libndn-svs)

include(cmake/docs.cmake)
include(cmake/format.cmake)
include(cmake/lint.cmake)

include(GNUInstallDirs)

add_library(iceflow SHARED src/iceflow.cpp src/consumer.cpp src/producer.cpp
                           src/measurements.cpp src/dag-parser.cpp)

if(USE_GRPC)
  target_sources(
    iceflow
    PRIVATE src/services/node-instance-service.cpp
            src/services/node-executor-service.cpp
            src/scaler.cpp
            src/executor.cpp
            proto/node-executor.proto
            proto/node-instance.proto)

  set(PROTO_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
  file(MAKE_DIRECTORY ${PROTO_BINARY_DIR})
  set(PROTO_IMPORT_DIRS "${CMAKE_CURRENT_LIST_DIR}/proto")

  protobuf_generate(TARGET iceflow IMPORT_DIRS ${PROTO_IMPORT_DIRS}
                    PROTOC_OUT_DIR "${PROTO_BINARY_DIR}")

  if(CMAKE_CROSSCOMPILING)
    set(_gRPC_CPP_PLUGIN "protoc-gen-grpc=${gRPC_CPP_PLUGIN_EXECUTABLE}")
  else()
    set(_gRPC_CPP_PLUGIN
        "protoc-gen-grpc=\$<TARGET_FILE:gRPC::grpc_cpp_plugin>")
  endif()

  protobuf_generate(
    TARGET
    iceflow
    LANGUAGE
    grpc
    GENERATE_EXTENSIONS
    .grpc.pb.h
    .grpc.pb.cc
    PLUGIN
    ${_gRPC_CPP_PLUGIN}
    IMPORT_DIRS
    ${PROTO_IMPORT_DIRS}
    PROTOC_OUT_DIR
    "${PROTO_BINARY_DIR}")

  target_link_libraries(iceflow PRIVATE gRPC::grpc++ protobuf::libprotobuf)
endif()

set_target_properties(iceflow PROPERTIES VERSION ${version} SOVERSION
                                                            ${so_version})

target_compile_options(iceflow PRIVATE -D_GNU_SOURCE -DBOOST_LOG_DYN_LINK)
target_link_libraries(
  iceflow
  PRIVATE ${Boost_LIBRARIES}
          -lboost_log
          -lboost_system
          -lpthread
          -lndn-cxx
          -lndn-svs
          OpenSSL::SSL)

option(BUILD_APPS "Build IceFlow examples" ON)
option(BUILD_TESTS "Build library tests" OFF)

configure_file(${IceFlow_SOURCE_DIR}/pkg/iceflow-config.cmake.in
               ${IceFlow_BINARY_DIR}/pkg/iceflow-config.cmake @ONLY)

configure_file(${IceFlow_SOURCE_DIR}/iceflow-config-version.cmake.in
               ${IceFlow_BINARY_DIR}/iceflow-config-version.cmake @ONLY)

install(FILES ${IceFlow_BINARY_DIR}/pkg/iceflow-config.cmake
              ${IceFlow_BINARY_DIR}/iceflow-config-version.cmake
        DESTINATION lib/iceflow)

target_include_directories(
  iceflow PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/iceflow>
                 "$<BUILD_INTERFACE:${PROTO_BINARY_DIR}>")
install(
  FILES include/iceflow/congestion-reporter.hpp
        include/iceflow/consumer.hpp
        include/iceflow/dag-parser.hpp
        include/iceflow/executor.hpp
        include/iceflow/iceflow.hpp
        include/iceflow/measurements.hpp
        include/iceflow/producer.hpp
        include/iceflow/ringbuffer.hpp
        include/iceflow/scaler.hpp
  DESTINATION include/iceflow)

if(USE_GRPC)
  install(
    FILES include/iceflow/node-executor-service.hpp
          include/iceflow/node-instance-service.hpp
          ${CMAKE_CURRENT_BINARY_DIR}/generated/node-executor.grpc.pb.h
          ${CMAKE_CURRENT_BINARY_DIR}/generated/node-executor.pb.h
          ${CMAKE_CURRENT_BINARY_DIR}/generated/node-instance.grpc.pb.h
          ${CMAKE_CURRENT_BINARY_DIR}/generated/node-instance.pb.h
    DESTINATION include/iceflow)
endif()

install(
  TARGETS iceflow
  DESTINATION lib
  EXPORT iceflow-targets)
install(EXPORT iceflow-targets DESTINATION lib/iceflow)

if(BUILD_APPS)
  message(STATUS "Building IceFlow examples...")
  add_subdirectory(examples)
endif()

if(BUILD_TESTS)
  message(STATUS "Building tests...")
  add_subdirectory(tests)
  enable_testing()
endif()
