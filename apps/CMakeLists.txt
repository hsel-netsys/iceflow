project(${CMAKE_PROJECT_NAME})

find_package(yaml-cpp REQUIRED)
find_package(OpenCV REQUIRED)
find_package(Boost)
find_package(PkgConfig)
pkg_check_modules(NDN_CXX REQUIRED libndn-cxx)
pkg_check_modules(PSYNC REQUIRED PSync)
pkg_check_modules(SVS REQUIRED libndn-svs)

set(SOURCES
    ImageSource/imagesource.cpp PeopleCounter/peoplecounter.cpp
    FaceDetection/facedetection.cpp AgeDetection/agedetection.cpp
    GenderDetection/genderdetection.cpp Aggregate/aggregate.cpp JsonSampleApp/functionA.cpp JsonSampleApp/functionB.cpp
    wordCount/text2lines.cpp wordCount/lines2words1.cpp wordCount/lines2words2.cpp)

foreach(SOURCE_FILE ${SOURCES})
  get_filename_component(EXECUTABLE_NAME ${SOURCE_FILE} NAME_WE)
  get_filename_component(EXECUTABLE_PATH ${SOURCE_FILE} DIRECTORY)
  add_executable(${EXECUTABLE_NAME} ${SOURCE_FILE} util/util.cpp )
  set_target_properties(${EXECUTABLE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                                      ../build)
  target_include_directories(
    ${EXECUTABLE_NAME} PRIVATE ../include include ${NDN_CXX_INCLUDE_DIRS}
                               ${PSYNC_INCLUDE_DIRS} ${YAML_INCLUDE_DIRS})
  target_compile_options(${EXECUTABLE_NAME} PRIVATE -D_GNU_SOURCE
                                                    -DBOOST_LOG_DYN_LINK)

  if(APPLE)
    target_link_directories(${EXECUTABLE_NAME} PRIVATE /usr/local/lib)
    target_link_libraries(
      ${EXECUTABLE_NAME}
      -lpthread
      -lPSync
      -lndn-cxx
      -lndn-svs
      ${OpenCV_LIBS}
      -lboost_log-mt
      -lboost_chrono-mt
      -lyaml-cpp)
  else()
    target_link_libraries(
      ${EXECUTABLE_NAME}
      ${Boost_LIBRARIES}
      -lboost_log
      -lboost_system
      -lpthread
      -lndn-cxx
      -lndn-svs
      -lPSync
      ${OpenCV_LIBS}
      ${YAML_CPP_LIBRARIES})
  endif()
endforeach(SOURCE_FILE ${SOURCES})
