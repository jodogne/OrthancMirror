cmake_minimum_required(VERSION 2.8)

project(Orthanc)

set(ORTHANC_ROOT ${CMAKE_SOURCE_DIR}/../../../../Resources/CMake)

include(${ORTHANC_ROOT}/OrthancFrameworkParameters.cmake)
set(ENABLE_WEB_CLIENT ON)
include(${ORTHANC_ROOT}/OrthancFrameworkConfiguration.cmake)

add_definitions(
  -DORTHANC_ENABLE_LOGGING_PLUGIN=OFF
  )

include_directories(${ORTHANC_ROOT})

add_executable(Sample
  main.cpp
  ${ORTHANC_CORE_SOURCES}
  )