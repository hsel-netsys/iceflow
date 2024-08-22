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

find_package(Boost)
find_package(PkgConfig)
find_package(OpenSSL REQUIRED)
find_package(gRPC CONFIG REQUIRED)
message(STATUS "Using gRPC ${gRPC_VERSION}")
find_program(gRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
pkg_check_modules(NDN_CXX REQUIRED libndn-cxx)
pkg_check_modules(SVS REQUIRED libndn-svs)

include(cmake/docs.cmake)
include(cmake/format.cmake)
include(cmake/lint.cmake)

include(GNUInstallDirs)

set(PROTO_BINARY_DIR ${CMAKE_BINARY_DIR}/build/generated)
file(MAKE_DIRECTORY ${PROTO_BINARY_DIR})
set(PROTO_IMPORT_DIRS proto/)

set(SOURCE_FILES
    src/iceflow.cpp
    src/consumer.cpp
    src/producer.cpp
    src/measurements.cpp
    src/services/node-instance-service.cpp
    src/services/node-executor-service.cpp
    src/scaler.cpp
    src/executor.cpp)

add_library(iceflow SHARED ${SOURCE_FILES} proto/node-executor.proto
                           proto/node-instance.proto)

target_compile_options(iceflow PRIVATE -D_GNU_SOURCE -DBOOST_LOG_DYN_LINK)
target_link_libraries(
  iceflow
  ${Boost_LIBRARIES}
  -lboost_log
  -lboost_system
  -lpthread
  -lndn-cxx
  -lndn-svs
  OpenSSL::SSL
  gRPC::grpc++)

option(BUILD_APPS "Build IceFlow examples" ON)
option(BUILD_TESTS "Build library tests" OFF)



protobuf_generate(TARGET iceflow IMPORT_DIRS ${PROTO_IMPORT_DIRS}
                  PROTOC_OUT_DIR ${PROTO_BINARY_DIR})

protobuf_generate(
  TARGET
  iceflow
  LANGUAGE
  grpc
  GENERATE_EXTENSIONS
  .grpc.pb.h
  .grpc.pb.cc
  PLUGIN
  protoc-gen-grpc=\$<TARGET_FILE:gRPC::grpc_cpp_plugin>
  IMPORT_DIRS
  ${PROTO_IMPORT_DIRS}
  PROTOC_OUT_DIR
  ${PROTO_BINARY_DIR})

target_include_directories(
  iceflow PUBLIC include/iceflow "$<BUILD_INTERFACE:${PROTO_BINARY_DIR}>")

install(TARGETS iceflow DESTINATION lib/iceflow-${version})
install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/build/generated/node-executor.grpc.pb.h
        ${CMAKE_CURRENT_BINARY_DIR}/build/generated/node-executor.pb.h
        ${CMAKE_CURRENT_BINARY_DIR}/build/generated/node-instance.grpc.pb.h
        ${CMAKE_CURRENT_BINARY_DIR}/build/generated/node-instance.pb.h
  DESTINATION include/iceflow-${version})
install(
  FILES include/iceflow/congestion-reporter.hpp
        include/iceflow/consumer.hpp
        include/iceflow/executor.hpp
        include/iceflow/iceflow.hpp
        include/iceflow/measurements.hpp
        include/iceflow/node-executor-service.hpp
        include/iceflow/node-instance-service.hpp
        include/iceflow/producer.hpp
        include/iceflow/ringbuffer.hpp
        include/iceflow/scaler.hpp
  DESTINATION include/iceflow-${version})

if(BUILD_APPS)
  message(STATUS "Building IceFlow examples...")
  add_subdirectory(examples)
endif()

if(BUILD_TESTS)
  message(STATUS "Building tests...")
  add_subdirectory(tests)
  enable_testing()
endif()
