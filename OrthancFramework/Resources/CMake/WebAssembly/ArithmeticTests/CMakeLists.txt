# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2021 Osimis S.A., Belgium
#
# This program is free software: you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation, either version 3 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this program. If not, see
# <http://www.gnu.org/licenses/>.


# source ~/Downloads/emsdk/emsdk_env.sh
# cmake .. -DCMAKE_TOOLCHAIN_FILE=${EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/tmp/wasm-install/
# make install
# cd /tmp/wasm-install
# python -m SimpleHTTPServer 8000
# firefox http://localhost:8000/
# -> Copy the result as "../arith.h"


cmake_minimum_required(VERSION 2.8.3)


#####################################################################
## Configuration of the Emscripten compiler for WebAssembly target
#####################################################################

set(WASM_FLAGS "-s WASM=1 -s DISABLE_EXCEPTION_CATCHING=0")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WASM_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WASM_FLAGS}")

# Turn on support for debug exceptions
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s DISABLE_EXCEPTION_CATCHING=0")


#####################################################################
## Prepare DCMTK 3.6.2
#####################################################################

include(${CMAKE_SOURCE_DIR}/../../Compiler.cmake)
include(${CMAKE_SOURCE_DIR}/../../DownloadPackage.cmake)

set(DCMTK_SOURCES_DIR ${CMAKE_BINARY_DIR}/dcmtk-3.6.2)
set(DCMTK_URL "http://orthanc.osimis.io/ThirdPartyDownloads/dcmtk-3.6.2.tar.gz")
set(DCMTK_MD5 "d219a4152772985191c9b89d75302d12")

if (IS_DIRECTORY "${DCMTK_SOURCES_DIR}")
  set(FirstRun OFF)
else()
  set(FirstRun ON)
endif()

DownloadPackage(${DCMTK_MD5} ${DCMTK_URL} "${DCMTK_SOURCES_DIR}")

if (FirstRun)
  message("Patching file")
  execute_process(
    COMMAND ${PATCH_EXECUTABLE} -p0 -N -i
    ${CMAKE_SOURCE_DIR}/arith.patch
    WORKING_DIRECTORY ${DCMTK_SOURCES_DIR}/config/tests
    RESULT_VARIABLE Failure
    )

  if (Failure)
    message(FATAL_ERROR "Error while patching a file")
  endif()
endif()


#####################################################################
## Build the DCMTK tests for arithmetics
#####################################################################

# https://github.com/kripken/emscripten/wiki/WebAssembly#web-server-setup
file(WRITE ${CMAKE_BINARY_DIR}/.htaccess "
AddType application/wasm .wasm
AddOutputFilterByType DEFLATE application/wasm
")

file(WRITE ${CMAKE_BINARY_DIR}/dcmtk/config/osconfig.h "
#pragma once
#define HAVE_CMATH 1
#define HAVE_MATH_H 1
#define HAVE_PROTOTYPE_FINITE 1
#define HAVE_PROTOTYPE_STD__ISINF 1
#define HAVE_PROTOTYPE_STD__ISNAN 1
#define HAVE_STD_NAMESPACE 1
#define HAVE_STRSTREAM 1
#define SIZEOF_VOID_P 4
#define USE_STD_CXX_INCLUDES
")

include_directories(
  ${DCMTK_SOURCES_DIR}/ofstd/include
  ${CMAKE_BINARY_DIR}
  )

add_executable(dcmtk
  ${DCMTK_SOURCES_DIR}/config/tests/arith.cc
  ${CMAKE_SOURCE_DIR}/Run2.cpp
  )

install(TARGETS dcmtk DESTINATION .)

install(FILES
  ${CMAKE_BINARY_DIR}/.htaccess
  ${CMAKE_BINARY_DIR}/dcmtk.wasm
  ${CMAKE_SOURCE_DIR}/app.js
  ${CMAKE_SOURCE_DIR}/index.html
  DESTINATION .
  )
