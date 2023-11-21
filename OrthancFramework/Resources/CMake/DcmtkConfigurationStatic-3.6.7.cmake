# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2023 Osimis S.A., Belgium
# Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


SET(DCMTK_VERSION_NUMBER 367)
SET(DCMTK_PACKAGE_VERSION "3.6.7")
SET(DCMTK_SOURCES_DIR ${CMAKE_BINARY_DIR}/dcmtk-3.6.7)
SET(DCMTK_URL "https://orthanc.uclouvain.be/downloads/third-party-downloads/dcmtk-3.6.7.tar.gz")
SET(DCMTK_MD5 "e4d519bb315ec3944f3f1d61df465cbd")

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
    ${CMAKE_CURRENT_LIST_DIR}/../Patches/dcmtk-3.6.7.patch
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    RESULT_VARIABLE Failure
    )

  if (Failure)
    message(FATAL_ERROR "Error while patching a file")
  endif()

  if (MSVC)
    # Older versions of Microsoft Visual Studio (notably MSVC2008)
    # don't like void usage of function arguments in C source files,
    # in order to avoid a warning about unused arguments. This patch
    # removes such usages that were not present in DCMTK <= 3.6.6.
    execute_process(
      COMMAND ${PATCH_EXECUTABLE} -p0 -N -i
      ${CMAKE_CURRENT_LIST_DIR}/../Patches/dcmtk-3.6.7-visual-studio.patch
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      RESULT_VARIABLE Failure
      )
    
    if (Failure)
      message(FATAL_ERROR "Error while patching a file")
    endif()    
  endif()  

  configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/../Patches/dcmtk-dcdict_orthanc.cc
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


# For the dcmtls module, necessary since DCMTK 3.6.7 (cf. file
# "dcmtls/libsrc/tlslayer.cc"). This must be done before the
# invokation of "configure_file()"!
if (STATIC_BUILD OR NOT USE_SYSTEM_OPENSSL)
  # The "CHECK_FUNCTIONWITHHEADER_EXISTS()" provided by DCMTK only
  # works with the system-wide version of OpenSSL. If statically
  # linking against OpenSSL, we manually provide information about
  # OpenSSL 3.0.x
  set(HAVE_OPENSSL_PROTOTYPE_DH_BITS 1)
  set(HAVE_OPENSSL_PROTOTYPE_EVP_PKEY_BASE_ID 1)
  set(HAVE_OPENSSL_PROTOTYPE_SSL_CTX_GET0_PARAM 1)
  set(HAVE_OPENSSL_PROTOTYPE_SSL_CTX_GET_CERT_STORE 1)
  set(HAVE_OPENSSL_PROTOTYPE_SSL_CTX_GET_CIPHERS 1)
  set(HAVE_OPENSSL_PROTOTYPE_X509_GET_SIGNATURE_NID 1)
  set(HAVE_OPENSSL_PROTOTYPE_X509_STORE_GET0_PARAM 1)
else()
  CHECK_FUNCTIONWITHHEADER_EXISTS("DH_bits" "openssl/dh.h" HAVE_OPENSSL_PROTOTYPE_DH_BITS)
  CHECK_FUNCTIONWITHHEADER_EXISTS("EVP_PKEY_base_id" "openssl/evp.h" HAVE_OPENSSL_PROTOTYPE_EVP_PKEY_BASE_ID)
  CHECK_FUNCTIONWITHHEADER_EXISTS("SSL_CTX_get0_param" "openssl/ssl.h" HAVE_OPENSSL_PROTOTYPE_SSL_CTX_GET0_PARAM)
  CHECK_FUNCTIONWITHHEADER_EXISTS("SSL_CTX_get_cert_store" "openssl/ssl.h" HAVE_OPENSSL_PROTOTYPE_SSL_CTX_GET_CERT_STORE)
  CHECK_FUNCTIONWITHHEADER_EXISTS("SSL_CTX_get_ciphers" "openssl/ssl.h" HAVE_OPENSSL_PROTOTYPE_SSL_CTX_GET_CIPHERS)
  CHECK_FUNCTIONWITHHEADER_EXISTS("X509_STORE_get0_param" "openssl/x509.h" HAVE_OPENSSL_PROTOTYPE_X509_STORE_GET0_PARAM)
  CHECK_FUNCTIONWITHHEADER_EXISTS("X509_get_signature_nid" "openssl/x509.h" HAVE_OPENSSL_PROTOTYPE_X509_GET_SIGNATURE_NID)
endif()


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


if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
  # For compatibility with Windows XP, avoid using fiber-local-storage
  # in log4cplus, but use thread-local-storage instead. Otherwise,
  # Windows XP complains about missing "FlsGetValue()" in KERNEL32.dll
  add_definitions(
    -DDCMTK_LOG4CPLUS_AVOID_WIN32_FLS
    )

  if (CMAKE_COMPILER_IS_GNUCXX OR             # MinGW
      "${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")  # MSVC for 32bit (*)

    # (*) With multithreaded logging enabled, Visual Studio 2008 fails
    # with error: ".\dcmtk-3.6.7\oflog\libsrc\globinit.cc(422) : error
    # C2664: 'dcmtk::log4cplus::thread::impl::tls_init' : cannot
    # convert parameter 1 from 'void (__stdcall *)(void *)' to
    # 'dcmtk::log4cplus::thread::impl::tls_init_cleanup_func_type'"
    #   None of the functions with this name in scope match the target type

    add_definitions(
      -DDCMTK_LOG4CPLUS_SINGLE_THREADED
      )
  endif()
endif()
