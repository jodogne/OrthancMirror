cmake_minimum_required(VERSION 2.8)

project(GdcmDecoder)

SET(GDCM_DECODER_VERSION "0.0" CACHE STRING "Version of the plugin")
SET(STATIC_BUILD OFF CACHE BOOL "Static build of the third-party libraries (necessary for Windows)")
SET(ALLOW_DOWNLOADS OFF CACHE BOOL "Allow CMake to download packages")
SET(USE_SYSTEM_BOOST ON CACHE BOOL "Use the system version of boost")

set(SAMPLES_ROOT ${CMAKE_SOURCE_DIR}/..)
include(${SAMPLES_ROOT}/Common/OrthancPlugins.cmake)
include(${ORTHANC_ROOT}/Resources/CMake/BoostConfiguration.cmake)

find_package(GDCM REQUIRED)
if (GDCM_FOUND)
  include(${GDCM_USE_FILE})
  set(GDCM_LIBRARIES gdcmCommon gdcmMSFF)
else(GDCM_FOUND)
  message(FATAL_ERROR "Cannot find GDCM, did you set GDCM_DIR?")
endif(GDCM_FOUND)

add_definitions(-DGDCM_DECODER_VERSION="${GDCM_DECODER_VERSION}")

add_library(GdcmDecoder SHARED
  ${BOOST_SOURCES}
  GdcmDecoderCache.cpp
  GdcmImageDecoder.cpp
  OrthancImageWrapper.cpp
  Plugin.cpp
  )

target_link_libraries(GdcmDecoder ${GDCM_LIBRARIES})
