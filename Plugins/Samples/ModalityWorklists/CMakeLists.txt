cmake_minimum_required(VERSION 2.8)

project(SampleModalityWorklists)

SET(SAMPLE_MODALITY_WORKLISTS_VERSION "0.0" CACHE STRING "Version of the plugin")
SET(STATIC_BUILD OFF CACHE BOOL "Static build of the third-party libraries (necessary for Windows)")
SET(ALLOW_DOWNLOADS OFF CACHE BOOL "Allow CMake to download packages")

SET(USE_SYSTEM_JSONCPP ON CACHE BOOL "Use the system version of JsonCpp")
SET(USE_SYSTEM_BOOST ON CACHE BOOL "Use the system version of boost")

set(SAMPLES_ROOT ${CMAKE_SOURCE_DIR}/..)
include(${CMAKE_SOURCE_DIR}/../Common/OrthancPlugins.cmake)
include(${ORTHANC_ROOT}/Resources/CMake/JsonCppConfiguration.cmake)
include(${ORTHANC_ROOT}/Resources/CMake/BoostConfiguration.cmake)

add_library(SampleModalityWorklists SHARED 
  Plugin.cpp
  ${JSONCPP_SOURCES}
  ${BOOST_SOURCES}
  )

message("Setting the version of the plugin to ${SAMPLE_MODALITY_WORKLISTS_VERSION}")
add_definitions(
  -DSAMPLE_MODALITY_WORKLISTS_VERSION="${SAMPLE_MODALITY_WORKLISTS_VERSION}"
  -DDEFAULT_WORKLISTS_FOLDER="${CMAKE_SOURCE_DIR}/WorklistsDatabase"
  )

set_target_properties(SampleModalityWorklists PROPERTIES 
  VERSION ${SAMPLE_MODALITY_WORKLISTS_VERSION} 
  SOVERSION ${SAMPLE_MODALITY_WORKLISTS_VERSION})

install(
  TARGETS SampleModalityWorklists
  RUNTIME DESTINATION lib    # Destination for Windows
  LIBRARY DESTINATION share/orthanc/plugins    # Destination for Linux
  )
