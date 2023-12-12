include(CMakeFindDependencyMacro)

find_dependency(Boost REQUIRED)
find_dependency(PkgConfig)
pkg_check_modules(libndn-cxx REQUIRED)
pkg_check_modules(PSYNC REQUIRED PSync)
pkg_check_modules(SVS REQUIRED libndn-svs)

get_filename_component(SELF_DIR ${CMAKE_CURRENT_LIST_DIR} PATH)

include(${SELF_DIR}/cmake/IceFlow.cmake)
