cmake_minimum_required(VERSION 2.8)

project(GdcmDecoding)

SET(ALLOW_DOWNLOADS ON CACHE BOOL "Allow CMake to download packages")
SET(USE_SYSTEM_BOOST ON CACHE BOOL "Use the system version of Boost")
SET(USE_SYSTEM_GOOGLE_LOG OFF CACHE BOOL "Use the system version of Google Log")

set(ORTHANC_ROOT ${CMAKE_SOURCE_DIR}/../../..)

if (${CMAKE_COMPILER_IS_GNUCXX})
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")
endif()

include(${ORTHANC_ROOT}/Resources/CMake/DownloadPackage.cmake)
include(${ORTHANC_ROOT}/Resources/CMake/BoostConfiguration.cmake)
include(${ORTHANC_ROOT}/Resources/CMake/GoogleLogConfiguration.cmake)

find_package(GDCM REQUIRED)
if (GDCM_FOUND)
  include(${GDCM_USE_FILE})
  set(GDCM_LIBRARIES 
    gdcmCommon
    gdcmDICT
    gdcmDSED
    gdcmIOD
    gdcmjpeg12
    gdcmjpeg16
    gdcmjpeg8
    gdcmMEXD
    gdcmMSFF
    )
else(GDCM_FOUND)
  message(FATAL_ERROR "Cannot find GDCM, did you set GDCM_DIR?")
endif(GDCM_FOUND)


include_directories(
  ${ORTHANC_ROOT}/Plugins/OrthancCPlugin/
  ${ORTHANC_ROOT}/OrthancCppClient/SharedLibrary/Laaw
  )
add_library(GdcmDecoding SHARED
  Plugin.cpp
  OrthancContext.cpp

  # Sources from Orthanc
  ${GOOGLE_LOG_SOURCES}
  ${ORTHANC_ROOT}/Core/ChunkedBuffer.cpp
  ${ORTHANC_ROOT}/Core/Enumerations.cpp
  ${ORTHANC_ROOT}/Core/ImageFormats/ImageAccessor.cpp
  ${ORTHANC_ROOT}/Core/ImageFormats/ImageBuffer.cpp
  ${ORTHANC_ROOT}/Core/ImageFormats/ImageProcessing.cpp
  ${ORTHANC_ROOT}/Core/OrthancException.cpp
  ${ORTHANC_ROOT}/Core/Toolbox.cpp
  ${ORTHANC_ROOT}/Resources/ThirdParty/base64/base64.cpp
  ${ORTHANC_ROOT}/Resources/ThirdParty/md5/md5.c
  ${THIRD_PARTY_SOURCES}
  )
target_link_libraries(GdcmDecoding ${GDCM_LIBRARIES})

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  target_link_libraries(GdcmDecoding pthread dl rt)
endif()
