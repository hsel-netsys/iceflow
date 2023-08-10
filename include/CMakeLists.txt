project(${CMAKE_PROJECT_NAME})

add_library(iceflow INTERFACE ${ICEFLOW_INTERFACE_FILES})

set(ICEFLOW_INTERFACE_FILES
    iceflow/block.hpp
    iceflow/consumer-tlv.hpp
    iceflow/content-type-value.hpp
    iceflow/data.hpp
    iceflow/logger.hpp
    iceflow/measurements.hpp
    iceflow/producer-tlv.hpp
    iceflow/ringbuffer.hpp
    iceflow/typed-data.hpp)

target_include_directories(
  iceflow INTERFACE $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
                    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>)

include(GNUInstallDirs)

install(
  TARGETS iceflow
  EXPORT ${CMAKE_PROJECT_NAME}
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/iceflow/cmake)

install(FILES ${ICEFLOW_INTERFACE_FILES}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/iceflow)
