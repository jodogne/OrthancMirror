# Mini-project to check whether "OrthancCppClient" can compile in a
# standalone fashion

cmake_minimum_required(VERSION 2.8)

project(OrthancCppClientTest)

SET(STATIC_BUILD OFF)

set(ORTHANC_ROOT ${CMAKE_SOURCE_DIR}/../../../..)

include(${ORTHANC_ROOT}/Resources/CMake/DownloadPackage.cmake)
include(${ORTHANC_ROOT}/Resources/CMake/JsonCppConfiguration.cmake)
include(${ORTHANC_ROOT}/Resources/CMake/LibCurlConfiguration.cmake)
include(${ORTHANC_ROOT}/Resources/CMake/LibPngConfiguration.cmake)
include(${ORTHANC_ROOT}/Resources/CMake/BoostConfiguration.cmake)

if (${CMAKE_COMPILER_IS_GNUCXX})
  set(CMAKE_C_FLAGS "-Wall -pedantic -Wno-implicit-function-declaration")  # --std=c99 makes libcurl not to compile
  set(CMAKE_CXX_FLAGS "-Wall -pedantic -Wno-long-long -Wno-variadic-macros")
  set(CMAKE_EXE_LINKER_FLAGS "-Wl,--as-needed")
  set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-undefined")
elseif (${MSVC})
  add_definitions(-D_CRT_SECURE_NO_WARNINGS=1)  
endif()

add_library(OrthancCppClient
  SHARED
  ${THIRD_PARTY_SOURCES}
  ${ORTHANC_ROOT}/Core/OrthancException.cpp
  ${ORTHANC_ROOT}/Core/Enumerations.cpp
  ${ORTHANC_ROOT}/Core/Toolbox.cpp
  ${ORTHANC_ROOT}/Core/HttpClient.cpp
  ${ORTHANC_ROOT}/Core/MultiThreading/ArrayFilledByThreads.cpp
  ${ORTHANC_ROOT}/Core/MultiThreading/ThreadedCommandProcessor.cpp
  ${ORTHANC_ROOT}/Core/MultiThreading/SharedMessageQueue.cpp
  ${ORTHANC_ROOT}/Core/FileFormats/PngReader.cpp
  ${ORTHANC_ROOT}/OrthancCppClient/OrthancConnection.cpp
  ${ORTHANC_ROOT}/OrthancCppClient/Series.cpp
  ${ORTHANC_ROOT}/OrthancCppClient/Study.cpp
  ${ORTHANC_ROOT}/OrthancCppClient/Instance.cpp
  ${ORTHANC_ROOT}/OrthancCppClient/Patient.cpp
  ${ORTHANC_ROOT}/Resources/sha1/sha1.cpp
  ${ORTHANC_ROOT}/Resources/md5/md5.c
  ${ORTHANC_ROOT}/Resources/base64/base64.cpp
  )

add_executable(Test
  main.cpp
  )

target_link_libraries(Test OrthancCppClient)
