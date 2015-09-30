cmake_minimum_required(VERSION 2.8)

project(GdcmDecoding)

SET(ALLOW_DOWNLOADS OFF CACHE BOOL "Allow CMake to download packages")
SET(USE_SYSTEM_BOOST ON CACHE BOOL "Use the system version of Boost")
SET(USE_SYSTEM_GOOGLE_LOG ON CACHE BOOL "Use the system version of Google Log")

set(ORTHANC_ROOT ${CMAKE_SOURCE_DIR}/../../..)
set(SAMPLES_ROOT ${CMAKE_SOURCE_DIR}/..)

include(CheckIncludeFiles)
include(CheckIncludeFileCXX)
include(${CMAKE_SOURCE_DIR}/../Common/OrthancPlugins.cmake)
include(${ORTHANC_ROOT}/Resources/CMake/DownloadPackage.cmake)
include(${ORTHANC_ROOT}/Resources/CMake/BoostConfiguration.cmake)
include(${ORTHANC_ROOT}/Resources/CMake/GoogleLogConfiguration.cmake)
include(${ORTHANC_ROOT}/Resources/CMake/JsonCppConfiguration.cmake)

find_package(GDCM REQUIRED)
if (GDCM_FOUND)
  include(${GDCM_USE_FILE})
  set(GDCM_LIBRARIES gdcmCommon gdcmMSFF)
else(GDCM_FOUND)
  message(FATAL_ERROR "Cannot find GDCM, did you set GDCM_DIR?")
endif(GDCM_FOUND)

add_library(GdcmDecoding SHARED
  Plugin.cpp
  OrthancContext.cpp

  # Sources from Orthanc
  ${GOOGLE_LOG_SOURCES}
  ${ORTHANC_ROOT}/Core/ChunkedBuffer.cpp
  ${ORTHANC_ROOT}/Core/Enumerations.cpp
  ${ORTHANC_ROOT}/Core/Images/ImageAccessor.cpp
  ${ORTHANC_ROOT}/Core/Images/ImageBuffer.cpp
  ${ORTHANC_ROOT}/Core/Images/ImageProcessing.cpp
  ${ORTHANC_ROOT}/Core/Toolbox.cpp
  ${ORTHANC_ROOT}/Resources/ThirdParty/base64/base64.cpp
  ${ORTHANC_ROOT}/Resources/ThirdParty/md5/md5.c
  ${JSONCPP_SOURCES}
  ${THIRD_PARTY_SOURCES}
  )

target_link_libraries(GdcmDecoding ${GDCM_LIBRARIES})
