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
project(ICEFLOW_APPLICATIONS)

find_package(OpenCV REQUIRED)
find_package(yaml-cpp REQUIRED)

set(CMAKE_CXX_STANDARD 20)
FIND_PACKAGE( Boost 1.74 COMPONENTS program_options REQUIRED )
INCLUDE_DIRECTORIES( src ${Boost_INCLUDE_DIRS} ${OpenCV_INCLUDE_DIR} ${YAML_INCLUDE_DIRS} /usr/local/include )
add_compile_options(-D_GNU_SOURCE -DBOOST_LOG_DYN_LINK)

if(APPLE)
    link_directories( /usr/local/lib )
endif()

set(SOURCES
    src/ImageSource/imagesource.cpp
    src/PeopleCounter/peoplecounter.cpp
    src/FaceDetection/facedetection.cpp
    src/AgeDetection/agedetection.cpp
    src/GenderDetection/genderdetection.cpp
    src/Aggregate/aggregate.cpp
)

foreach( SOURCE_FILE ${SOURCES} )
    get_filename_component(EXECUTABLE_NAME ${SOURCE_FILE} NAME_WE)
    get_filename_component(EXECUTABLE_PATH ${SOURCE_FILE} DIRECTORY)
    add_executable( ${EXECUTABLE_NAME} ${SOURCE_FILE} )
    set_target_properties( ${EXECUTABLE_NAME}
            PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${EXECUTABLE_PATH}"
    )
    if(APPLE)
        target_link_libraries( ${EXECUTABLE_NAME} -lpthread -lPSync -lndn-cxx  ${OpenCV_LIBS} -lboost_log-mt -lboost_chrono-mt -lyaml-cpp)
    else()
        target_link_libraries( ${EXECUTABLE_NAME} ${Boost_LIBRARIES} -lboost_log -lboost_system -lpthread -lndn-cxx  -lPSync ${OpenCV_LIBS} ${YAML_CPP_LIBRARIES} )
    endif()
endforeach( SOURCE_FILE ${SOURCES} )
