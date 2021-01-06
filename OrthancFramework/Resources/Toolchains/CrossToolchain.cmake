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


#
#  $ CROSSTOOL_NG_ARCH=mips CROSSTOOL_NG_BOARD=malta CROSSTOOL_NG_IMAGE=/tmp/mips cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=../Resources/CrossToolchain.cmake -DBUILD_CONNECTIVITY_CHECKS=OFF -DUSE_SYSTEM_CIVETWEB=OFF -DUSE_GOOGLE_TEST_DEBIAN_PACKAGE=ON -DUSE_SYSTEM_JSONCPP=OFF -DUSE_SYSTEM_UUID=OFF -DENABLE_DCMTK_JPEG_LOSSLESS=OFF -G Ninja && ninja
#

INCLUDE(CMakeForceCompiler)

SET(CROSSTOOL_NG_ROOT $ENV{CROSSTOOL_NG_ROOT} CACHE STRING "")
SET(CROSSTOOL_NG_ARCH $ENV{CROSSTOOL_NG_ARCH} CACHE STRING "")
SET(CROSSTOOL_NG_BOARD $ENV{CROSSTOOL_NG_BOARD} CACHE STRING "")
SET(CROSSTOOL_NG_SUFFIX $ENV{CROSSTOOL_NG_SUFFIX} CACHE STRING "")
SET(CROSSTOOL_NG_IMAGE $ENV{CROSSTOOL_NG_IMAGE} CACHE STRING "")

IF ("${CROSSTOOL_NG_ROOT}" STREQUAL "")
  SET(CROSSTOOL_NG_ROOT "/home/$ENV{USER}/x-tools")
ENDIF()

IF ("${CROSSTOOL_NG_SUFFIX}" STREQUAL "")
  SET(CROSSTOOL_NG_SUFFIX "linux-gnu")
ENDIF()

SET(CROSSTOOL_NG_NAME ${CROSSTOOL_NG_ARCH}-${CROSSTOOL_NG_BOARD}-${CROSSTOOL_NG_SUFFIX})
SET(CROSSTOOL_NG_BASE ${CROSSTOOL_NG_ROOT}/${CROSSTOOL_NG_NAME})

# the name of the target operating system
SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_VERSION CrossToolNg)
SET(CMAKE_SYSTEM_PROCESSOR ${CROSSTOOL_NG_ARCH})

# which compilers to use for C and C++
SET(CMAKE_C_COMPILER ${CROSSTOOL_NG_BASE}/bin/${CROSSTOOL_NG_NAME}-gcc)

if (${CMAKE_VERSION} VERSION_LESS "3.6.0") 
  CMAKE_FORCE_CXX_COMPILER(${CROSSTOOL_NG_BASE}/bin/${CROSSTOOL_NG_NAME}-g++ GNU)
else()
  SET(CMAKE_CXX_COMPILER ${CROSSTOOL_NG_BASE}/bin/${CROSSTOOL_NG_NAME}-g++)
endif()

# here is the target environment located
SET(CMAKE_FIND_ROOT_PATH ${CROSSTOOL_NG_IMAGE})
#SET(CMAKE_FIND_ROOT_PATH ${CROSSTOOL_NG_BASE}/${CROSSTOOL_NG_NAME}/sysroot)

# adjust the default behaviour of the FIND_XXX() commands:
# search headers and libraries in the target environment, search 
# programs in the host environment
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

SET(CMAKE_CROSSCOMPILING ON)
#SET(CROSS_COMPILER_PREFIX ${CROSSTOOL_NG_ARCH}-${CROSSTOOL_NG_SUFFIX})

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I${CROSSTOOL_NG_IMAGE}/usr/include -I${CROSSTOOL_NG_IMAGE}/usr/include/${CROSSTOOL_NG_ARCH}-${CROSSTOOL_NG_SUFFIX}" CACHE INTERNAL "" FORCE)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I${CROSSTOOL_NG_IMAGE}/usr/include -I${CROSSTOOL_NG_IMAGE}/usr/include/${CROSSTOOL_NG_ARCH}-${CROSSTOOL_NG_SUFFIX}" CACHE INTERNAL "" FORCE)
SET(CMAKE_EXE_LINKER_FLAGS "-Wl,--unresolved-symbols=ignore-in-shared-libs -L${CROSSTOOL_NG_BASE}/${CROSSTOOL_NG_NAME}/sysroot/usr/lib -L${CROSSTOOL_NG_IMAGE}/usr/lib -L${CROSSTOOL_NG_IMAGE}/usr/lib/${CROSSTOOL_NG_ARCH}-${CROSSTOOL_NG_SUFFIX} -L${CROSSTOOL_NG_IMAGE}/lib -L${CROSSTOOL_NG_IMAGE}/lib/${CROSSTOOL_NG_ARCH}-${CROSSTOOL_NG_SUFFIX}" CACHE INTERNAL "" FORCE)
SET(CMAKE_SHARED_LINKER_FLAGS "-Wl,--unresolved-symbols=ignore-in-shared-libs -L${CROSSTOOL_NG_BASE}/${CROSSTOOL_NG_NAME}/sysroot/usr/lib -L${CROSSTOOL_NG_IMAGE}/usr/lib -L${CROSSTOOL_NG_IMAGE}/usr/lib/${CROSSTOOL_NG_ARCH}-${CROSSTOOL_NG_SUFFIX} -L${CROSSTOOL_NG_IMAGE}/lib -L${CROSSTOOL_NG_IMAGE}/lib/${CROSSTOOL_NG_ARCH}-${CROSSTOOL_NG_SUFFIX}" CACHE INTERNAL "" FORCE)
