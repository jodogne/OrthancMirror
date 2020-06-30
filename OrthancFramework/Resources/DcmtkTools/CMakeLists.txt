#   $ LSB_CC=gcc-4.8 LSB_CXX=g++-4.8 cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=../../Resources/Toolchains/LinuxStandardBaseToolchain.cmake -G Ninja

cmake_minimum_required(VERSION 2.8)

project(DcmtkTools)

include(${CMAKE_SOURCE_DIR}/../../Resources/CMake/Compiler.cmake)
include(${CMAKE_SOURCE_DIR}/../../Resources/CMake/DownloadPackage.cmake)

set(STATIC_BUILD ON CACHE BOOL "")
set(ALLOW_DOWNLOADS ON CACHE BOOL "")

set(DCMTK_STATIC_VERSION "3.6.5" CACHE STRING "")
set(ENABLE_DCMTK_JPEG ON CACHE BOOL "")
set(ENABLE_DCMTK_JPEG_LOSSLESS ON CACHE BOOL "")
set(ENABLE_DCMTK_LOG ON CACHE BOOL "")
set(ENABLE_DCMTK_NETWORKING ON CACHE BOOL "")
set(ENABLE_DCMTK_TRANSCODING ON CACHE BOOL "")

include(${CMAKE_SOURCE_DIR}/../../Resources/CMake/DcmtkConfiguration.cmake)

add_library(dcmtk STATIC
  ${CMAKE_SOURCE_DIR}/dummy.cpp
  ${DCMTK_SOURCES}
  )

add_executable(getscu
  ${DCMTK_SOURCES_DIR}/dcmnet/apps/getscu.cc
  )
target_link_libraries(getscu dcmtk)
