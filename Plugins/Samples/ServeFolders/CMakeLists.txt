cmake_minimum_required(VERSION 2.8)

project(ServeFolders)

SET(ALLOW_DOWNLOADS OFF CACHE BOOL "Allow CMake to download packages")
SET(USE_SYSTEM_JSONCPP ON CACHE BOOL "Use the system version of JsonCpp")

set(ORTHANC_ROOT ${CMAKE_SOURCE_DIR}/../../../)

include(CheckIncludeFiles)
include(CheckIncludeFileCXX)
include(${CMAKE_SOURCE_DIR}/../Common/OrthancPlugins.cmake)
include(${ORTHANC_ROOT}/Resources/CMake/DownloadPackage.cmake)
include(${ORTHANC_ROOT}/Resources/CMake/JsonCppConfiguration.cmake)

add_library(ServeFolders SHARED 
  Plugin.cpp
  ${THIRD_PARTY_SOURCES}
  )
