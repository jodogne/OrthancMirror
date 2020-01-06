SET(DCMTK_VERSION_NUMBER 364)
SET(DCMTK_PACKAGE_VERSION "3.6.4")
SET(DCMTK_SOURCES_DIR ${CMAKE_BINARY_DIR}/dcmtk-3.6.4)
SET(DCMTK_URL "http://orthanc.osimis.io/ThirdPartyDownloads/dcmtk-3.6.4.tar.gz")
SET(DCMTK_MD5 "97597439a2ae7a39086066318db5f3bc")

macro(DCMTK_UNSET)
endmacro()

macro(DCMTK_UNSET_CACHE)
endmacro()

set(DCMTK_BINARY_DIR ${DCMTK_SOURCES_DIR}/)
set(DCMTK_CMAKE_INCLUDE ${DCMTK_SOURCES_DIR}/)

if (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  set(DCMTK_WITH_THREADS OFF)  # Disable thread support in wasm/asm.js
else()
  set(DCMTK_WITH_THREADS ON)
endif()

add_definitions(-DDCMTK_INSIDE_LOG4CPLUS=1)

if (IS_DIRECTORY "${DCMTK_SOURCES_DIR}")
  set(FirstRun OFF)
else()
  set(FirstRun ON)
endif()

DownloadPackage(${DCMTK_MD5} ${DCMTK_URL} "${DCMTK_SOURCES_DIR}")


if (FirstRun)
  # Apply the patches
  execute_process(
    COMMAND ${PATCH_EXECUTABLE} -p0 -N -i
    ${ORTHANC_ROOT}/Resources/Patches/dcmtk-3.6.4.patch
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    RESULT_VARIABLE Failure
    )

  if (Failure)
    message(FATAL_ERROR "Error while patching a file")
  endif()

  configure_file(
    ${ORTHANC_ROOT}/Resources/Patches/dcmtk-dcdict_orthanc.cc
    ${DCMTK_SOURCES_DIR}/dcmdata/libsrc/dcdict_orthanc.cc
    COPYONLY)
else()
  message("The patches for DCMTK have already been applied")
endif()


include_directories(
  ${DCMTK_SOURCES_DIR}/dcmiod/include
  )


# C_CHAR_UNSIGNED *must* be set before calling "GenerateDCMTKConfigure.cmake"
IF (CMAKE_CROSSCOMPILING)
  if (CMAKE_COMPILER_IS_GNUCXX AND
      CMAKE_SYSTEM_NAME STREQUAL "Windows")  # MinGW
    SET(C_CHAR_UNSIGNED 1 CACHE INTERNAL "Whether char is unsigned.")

  elseif(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")  # WebAssembly or asm.js

    # Check out "../WebAssembly/ArithmeticTests/" to regenerate the
    # "arith.h" file
    configure_file(
      ${ORTHANC_ROOT}/Resources/WebAssembly/arith.h
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

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
  link_libraries(netapi32)  # For NetWkstaUserGetInfo@12
  link_libraries(iphlpapi)  # For GetAdaptersInfo@8

  # Configure Wine if cross-compiling for Windows
  if (CMAKE_COMPILER_IS_GNUCXX)
    include(${DCMTK_SOURCES_DIR}/CMake/dcmtkUseWine.cmake)
    FIND_PROGRAM(WINE_WINE_PROGRAM wine)
    FIND_PROGRAM(WINE_WINEPATH_PROGRAM winepath)
    list(APPEND DCMTK_TRY_COMPILE_REQUIRED_CMAKE_FLAGS "-DCMAKE_EXE_LINKER_FLAGS=-static")
  endif()
endif()

# This step must be after the generation of "osconfig.h"
if (NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  INSPECT_FUNDAMENTAL_ARITHMETIC_TYPES()
endif()


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
endif()


list(REMOVE_ITEM DCMTK_SOURCES 
  ${DCMTK_SOURCES_DIR}/dcmdata/libsrc/mkdictbi.cc
  ${DCMTK_SOURCES_DIR}/dcmdata/libsrc/mkdeftag.cc
  )


# Starting with DCMTK 3.6.2, the Nagle algorithm is not disabled by
# default since this does not seem to be appropriate (anymore) for
# most modern operating systems. In order to change this default, the
# environment variable NO_TCPDELAY can be set to "1" (see envvars.txt
# for details). Alternatively, the macro DISABLE_NAGLE_ALGORITHM can
# be defined to change this setting at compilation time (see
# macros.txt for details).
# https://forum.dcmtk.org/viewtopic.php?t=4632
add_definitions(
  -DDISABLE_NAGLE_ALGORITHM=1
  )
