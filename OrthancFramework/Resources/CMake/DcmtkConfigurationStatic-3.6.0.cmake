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


SET(DCMTK_VERSION_NUMBER 360)
SET(DCMTK_PACKAGE_VERSION "3.6.0")
SET(DCMTK_SOURCES_DIR ${CMAKE_BINARY_DIR}/dcmtk-3.6.0)
SET(DCMTK_URL "http://orthanc.osimis.io/ThirdPartyDownloads/dcmtk-3.6.0.zip")
SET(DCMTK_MD5 "219ad631b82031806147e4abbfba4fa4")

if (IS_DIRECTORY "${DCMTK_SOURCES_DIR}")
  set(FirstRun OFF)
else()
  set(FirstRun ON)
endif()

DownloadPackage(${DCMTK_MD5} ${DCMTK_URL} "${DCMTK_SOURCES_DIR}")


if (FirstRun)
  # If using DCMTK 3.6.0, backport the "private.dic" file from DCMTK
  # 3.6.2. This adds support for more private tags, and fixes some
  # import problems with Philips MRI Achieva.
  if (USE_DCMTK_362_PRIVATE_DIC)
    message("Using the dictionary of private tags from DCMTK 3.6.2")
    configure_file(
      ${CMAKE_CURRENT_LIST_DIR}/../Patches/dcmtk-3.6.2-private.dic
      ${DCMTK_SOURCES_DIR}/dcmdata/data/private.dic
      COPYONLY)
  else()
    message("Using the dictionary of private tags from DCMTK 3.6.0")
  endif()
  
  # Patches specific to DCMTK 3.6.0
  message("Applying patch to solve vulnerability in DCMTK 3.6.0")
  execute_process(
    COMMAND ${PATCH_EXECUTABLE} -p0 -N -i
    ${CMAKE_CURRENT_LIST_DIR}/../Patches/dcmtk-3.6.0-dulparse-vulnerability.patch
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    RESULT_VARIABLE Failure
    )

  if (Failure)
    message(FATAL_ERROR "Error while patching a file")
  endif()

  # This patch is not needed anymore thanks to the following commit
  # (information sent by Jorg Riesmeier on Twitter on 2017-07-19):
  # http://git.dcmtk.org/?p=dcmtk.git;a=commit;h=8df1f5e517b8629ae09088d0935c2a8dd333c76f
  message("Applying patch for speed in DCMTK 3.6.0")
  execute_process(
    COMMAND ${PATCH_EXECUTABLE} -p0 -N -i
    ${CMAKE_CURRENT_LIST_DIR}/../Patches/dcmtk-3.6.0-speed.patch
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    RESULT_VARIABLE Failure
    )

  if (Failure)
    message(FATAL_ERROR "Error while patching a file")
  endif()
else()
  message("The patches for DCMTK have already been applied")
endif()


# C_CHAR_UNSIGNED *must* be set before calling "GenerateDCMTKConfigure.cmake"
IF (CMAKE_CROSSCOMPILING)
  if (CMAKE_COMPILER_IS_GNUCXX AND
      CMAKE_SYSTEM_NAME STREQUAL "Windows")  # MinGW
    SET(C_CHAR_UNSIGNED 1 CACHE INTERNAL "Whether char is unsigned.")

  elseif(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")  # WebAssembly or asm.js

    # Check out "../WebAssembly/ArithmeticTests/" to regenerate the
    # "arith.h" file
    configure_file(
      ${CMAKE_CURRENT_LIST_DIR}/WebAssembly/arith.h
      ${DCMTK_SOURCES_DIR}/config/include/dcmtk/config/arith.h
      COPYONLY)

    UNSET(C_CHAR_UNSIGNED CACHE)
    SET(C_CHAR_UNSIGNED 0 CACHE INTERNAL "")

  else()
    message(FATAL_ERROR "Support your platform here")
  endif()
ENDIF()


if ("${CMAKE_SYSTEM_VERSION}" STREQUAL "LinuxStandardBase")
  SET(DCMTK_ENABLE_CHARSET_CONVERSION "iconv" CACHE STRING "")
  SET(HAVE_SYS_GETTID 0 CACHE INTERNAL "")

  execute_process(
    COMMAND ${PATCH_EXECUTABLE} -p0 -N -i
    ${CMAKE_CURRENT_LIST_DIR}/../Patches/dcmtk-3.6.2-linux-standard-base.patch
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    RESULT_VARIABLE Failure
    )

  if (FirstRun AND Failure)
    message(FATAL_ERROR "Error while patching a file")
  endif()
endif()

SET(DCMTK_SOURCE_DIR ${DCMTK_SOURCES_DIR})
include(${DCMTK_SOURCES_DIR}/CMake/CheckFunctionWithHeaderExists.cmake)
include(${DCMTK_SOURCES_DIR}/CMake/GenerateDCMTKConfigure.cmake)


if (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")  # WebAssembly or
  # asm.js The macros below are not properly discovered by DCMTK
  # when using WebAssembly. Check out "../WebAssembly/arith.h" for
  # how we produced these values. This step MUST be after
  # "GenerateDCMTKConfigure" and before the generation of
  # "osconfig.h".
  UNSET(SIZEOF_VOID_P   CACHE)
  UNSET(SIZEOF_CHAR     CACHE)
  UNSET(SIZEOF_DOUBLE   CACHE)
  UNSET(SIZEOF_FLOAT    CACHE)
  UNSET(SIZEOF_INT      CACHE)
  UNSET(SIZEOF_LONG     CACHE)
  UNSET(SIZEOF_SHORT    CACHE)
  UNSET(SIZEOF_VOID_P   CACHE)

  SET(SIZEOF_VOID_P 4   CACHE INTERNAL "")
  SET(SIZEOF_CHAR 1     CACHE INTERNAL "")
  SET(SIZEOF_DOUBLE 8   CACHE INTERNAL "")
  SET(SIZEOF_FLOAT 4    CACHE INTERNAL "")
  SET(SIZEOF_INT 4      CACHE INTERNAL "")
  SET(SIZEOF_LONG 4     CACHE INTERNAL "")
  SET(SIZEOF_SHORT 2    CACHE INTERNAL "")
  SET(SIZEOF_VOID_P 4   CACHE INTERNAL "")
endif()


set(DCMTK_PACKAGE_VERSION_SUFFIX "")
set(DCMTK_PACKAGE_VERSION_NUMBER ${DCMTK_VERSION_NUMBER})

CONFIGURE_FILE(
  ${DCMTK_SOURCES_DIR}/CMake/osconfig.h.in
  ${DCMTK_SOURCES_DIR}/config/include/dcmtk/config/osconfig.h)



# Source for the logging facility of DCMTK
AUX_SOURCE_DIRECTORY(${DCMTK_SOURCES_DIR}/oflog/libsrc DCMTK_SOURCES)
if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux" OR
    ${CMAKE_SYSTEM_NAME} STREQUAL "Darwin" OR
    ${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD" OR
    ${CMAKE_SYSTEM_NAME} STREQUAL "kFreeBSD" OR
    ${CMAKE_SYSTEM_NAME} STREQUAL "OpenBSD" OR
    ${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")
  list(REMOVE_ITEM DCMTK_SOURCES 
    ${DCMTK_SOURCES_DIR}/oflog/libsrc/clfsap.cc
    ${DCMTK_SOURCES_DIR}/oflog/libsrc/windebap.cc
    ${DCMTK_SOURCES_DIR}/oflog/libsrc/winsock.cc
    )

elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
  list(REMOVE_ITEM DCMTK_SOURCES 
    ${DCMTK_SOURCES_DIR}/oflog/libsrc/unixsock.cc
    ${DCMTK_SOURCES_DIR}/oflog/libsrc/clfsap.cc
    )

  if (CMAKE_COMPILER_IS_GNUCXX)
    # This is a patch for DCMTK 3.6.0 and MinGW64
    execute_process(
      COMMAND ${PATCH_EXECUTABLE} -p0 -N -i
      ${CMAKE_CURRENT_LIST_DIR}/../Patches/dcmtk-3.6.0-mingw64.patch
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      RESULT_VARIABLE Failure
      )

    if (Failure AND FirstRun)
      message(FATAL_ERROR "Error while patching a file")
    endif()
  endif()
endif()
