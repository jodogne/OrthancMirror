cmake_minimum_required(VERSION 2.8)

project(StorageCommitmentScp)

SET(PLUGIN_VERSION "0.0" CACHE STRING "Version of the plugin")
SET(STATIC_BUILD OFF CACHE BOOL "Static build of the third-party libraries (necessary for Windows)")
SET(ALLOW_DOWNLOADS OFF CACHE BOOL "Allow CMake to download packages")

SET(USE_SYSTEM_JSONCPP ON CACHE BOOL "Use the system version of JsonCpp")
SET(USE_SYSTEM_BOOST ON CACHE BOOL "Use the system version of boost")

set(SAMPLES_ROOT ${CMAKE_SOURCE_DIR}/..)
include(${SAMPLES_ROOT}/Common/OrthancPlugins.cmake)
include(${ORTHANC_ROOT}/Resources/CMake/JsonCppConfiguration.cmake)
include(${ORTHANC_ROOT}/Resources/CMake/BoostConfiguration.cmake)

add_library(StorageCommitmentScp SHARED 
  Plugin.cpp
  ../Common/OrthancPluginCppWrapper.cpp
  ${JSONCPP_SOURCES}
  ${BOOST_SOURCES}
  )

message("Setting the version of the plugin to ${PLUGIN_VERSION}")
add_definitions(
  -DPLUGIN_VERSION="${PLUGIN_VERSION}"
  )

set_target_properties(StorageCommitmentScp PROPERTIES 
  VERSION ${PLUGIN_VERSION} 
  SOVERSION ${PLUGIN_VERSION})

install(
  TARGETS StorageCommitmentScp
  RUNTIME DESTINATION lib    # Destination for Windows
  LIBRARY DESTINATION share/orthanc/plugins    # Destination for Linux
  )
