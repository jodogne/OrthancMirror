cmake_minimum_required(VERSION 2.8)

project(Sample)

include(${CMAKE_SOURCE_DIR}/../../../CMake/OrthancFrameworkParameters.cmake)

set(ENABLE_WEB_SERVER ON)

include(${CMAKE_SOURCE_DIR}/../../../CMake/OrthancFrameworkConfiguration.cmake)

add_executable(Sample
  ${ORTHANC_CORE_SOURCES}
  Sample.cpp
  )

include_directories(${ORTHANC_ROOT}/Core)
