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


if (NOT DEFINED ENABLE_DCMTK_NETWORKING)
  set(ENABLE_DCMTK_NETWORKING ON)
endif()

if (STATIC_BUILD OR NOT USE_SYSTEM_DCMTK)
  if (DCMTK_STATIC_VERSION STREQUAL "3.6.0")
    include(${CMAKE_CURRENT_LIST_DIR}/DcmtkConfigurationStatic-3.6.0.cmake)   
  elseif (DCMTK_STATIC_VERSION STREQUAL "3.6.2")
    include(${CMAKE_CURRENT_LIST_DIR}/DcmtkConfigurationStatic-3.6.2.cmake)
  elseif (DCMTK_STATIC_VERSION STREQUAL "3.6.4")
    include(${CMAKE_CURRENT_LIST_DIR}/DcmtkConfigurationStatic-3.6.4.cmake)
  elseif (DCMTK_STATIC_VERSION STREQUAL "3.6.5")
    include(${CMAKE_CURRENT_LIST_DIR}/DcmtkConfigurationStatic-3.6.5.cmake)
  elseif (DCMTK_STATIC_VERSION STREQUAL "3.6.6")
    include(${CMAKE_CURRENT_LIST_DIR}/DcmtkConfigurationStatic-3.6.6.cmake)
  else()
    message(FATAL_ERROR "Unsupported version of DCMTK: ${DCMTK_STATIC_VERSION}")
  endif()


  ##
  ## Commands shared by all versions of DCMTK
  ##

  AUX_SOURCE_DIRECTORY(${DCMTK_SOURCES_DIR}/dcmdata/libsrc DCMTK_SOURCES)
  AUX_SOURCE_DIRECTORY(${DCMTK_SOURCES_DIR}/ofstd/libsrc DCMTK_SOURCES)

  LIST(REMOVE_ITEM DCMTK_SOURCES 
    ${DCMTK_SOURCES_DIR}/dcmdata/libsrc/mkdictbi.cc
    ${DCMTK_SOURCES_DIR}/dcmdata/libsrc/mkdeftag.cc
    ${DCMTK_SOURCES_DIR}/dcmdata/libsrc/dcdict_orthanc.cc
    )

  if (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    message(${DCMTK_SOURCES_DIR})
    list(REMOVE_ITEM DCMTK_SOURCES 
      ${DCMTK_SOURCES_DIR}/ofstd/libsrc/offilsys.cc
      )
  endif()

  if (ENABLE_DCMTK_NETWORKING)
    AUX_SOURCE_DIRECTORY(${DCMTK_SOURCES_DIR}/dcmnet/libsrc DCMTK_SOURCES)
    include_directories(
      ${DCMTK_SOURCES_DIR}/dcmnet/include
      )
  endif()

  if (ENABLE_DCMTK_TRANSCODING)
    AUX_SOURCE_DIRECTORY(${DCMTK_SOURCES_DIR}/dcmimgle/libsrc DCMTK_SOURCES)
    AUX_SOURCE_DIRECTORY(${DCMTK_SOURCES_DIR}/dcmimage/libsrc DCMTK_SOURCES)
    include_directories(
      ${DCMTK_SOURCES_DIR}/dcmimage/include
      )
  endif()
  
  if (ENABLE_DCMTK_JPEG)
    AUX_SOURCE_DIRECTORY(${DCMTK_SOURCES_DIR}/dcmjpeg/libsrc DCMTK_SOURCES)
    AUX_SOURCE_DIRECTORY(${DCMTK_SOURCES_DIR}/dcmjpeg/libijg8 DCMTK_SOURCES)
    AUX_SOURCE_DIRECTORY(${DCMTK_SOURCES_DIR}/dcmjpeg/libijg12 DCMTK_SOURCES)
    AUX_SOURCE_DIRECTORY(${DCMTK_SOURCES_DIR}/dcmjpeg/libijg16 DCMTK_SOURCES)
    include_directories(
      ${DCMTK_SOURCES_DIR}/dcmjpeg/include
      ${DCMTK_SOURCES_DIR}/dcmjpeg/libijg8
      ${DCMTK_SOURCES_DIR}/dcmjpeg/libijg12
      ${DCMTK_SOURCES_DIR}/dcmjpeg/libijg16
      ${DCMTK_SOURCES_DIR}/dcmimgle/include
      )
    list(REMOVE_ITEM DCMTK_SOURCES 
      ${DCMTK_SOURCES_DIR}/dcmjpeg/libsrc/ddpiimpl.cc

      # Solves linking problem in WebAssembly: "wasm-ld: error:
      # duplicate symbol: jaritab" (modification in Orthanc 1.5.9)
      ${DCMTK_SOURCES_DIR}/dcmjpeg/libijg8/jaricom.c
      ${DCMTK_SOURCES_DIR}/dcmjpeg/libijg12/jaricom.c
      ${DCMTK_SOURCES_DIR}/dcmjpeg/libijg24/jaricom.c
      )

    if (NOT ENABLE_DCMTK_TRANSCODING)
      list(REMOVE_ITEM DCMTK_SOURCES 
        # Disable support for encoding JPEG (modification in Orthanc 1.0.1)
        ${DCMTK_SOURCES_DIR}/dcmjpeg/libsrc/djcodece.cc
        ${DCMTK_SOURCES_DIR}/dcmjpeg/libsrc/djencsv1.cc
        ${DCMTK_SOURCES_DIR}/dcmjpeg/libsrc/djencbas.cc
        ${DCMTK_SOURCES_DIR}/dcmjpeg/libsrc/djencpro.cc
        ${DCMTK_SOURCES_DIR}/dcmjpeg/libsrc/djenclol.cc
        ${DCMTK_SOURCES_DIR}/dcmjpeg/libsrc/djencode.cc
        ${DCMTK_SOURCES_DIR}/dcmjpeg/libsrc/djencext.cc
        ${DCMTK_SOURCES_DIR}/dcmjpeg/libsrc/djencsps.cc
        )
    endif()
  endif()


  if (ENABLE_DCMTK_JPEG_LOSSLESS)
    AUX_SOURCE_DIRECTORY(${DCMTK_SOURCES_DIR}/dcmjpls/libsrc DCMTK_SOURCES)
    AUX_SOURCE_DIRECTORY(${DCMTK_SOURCES_DIR}/dcmjpls/libcharls DCMTK_SOURCES)
    include_directories(
      ${DCMTK_SOURCES_DIR}/dcmjpeg/include
      ${DCMTK_SOURCES_DIR}/dcmjpls/include
      ${DCMTK_SOURCES_DIR}/dcmjpls/libcharls
      )
    list(APPEND DCMTK_SOURCES 
      ${DCMTK_SOURCES_DIR}/dcmjpeg/libsrc/djrplol.cc
      )

    if (NOT ENABLE_DCMTK_TRANSCODING)
      list(REMOVE_ITEM DCMTK_SOURCES 
        ${DCMTK_SOURCES_DIR}/dcmjpls/libsrc/djcodece.cc

        # Disable support for encoding JPEG-LS (modification in Orthanc 1.0.1)
        ${DCMTK_SOURCES_DIR}/dcmjpls/libsrc/djencode.cc
        )
    endif()
  endif()


  # New in Orthanc 1.9.0 for DICOM TLS
  if (ENABLE_DCMTK_NETWORKING AND ENABLE_SSL)
    AUX_SOURCE_DIRECTORY(${DCMTK_SOURCES_DIR}/dcmtls/libsrc DCMTK_SOURCES)
    include_directories(
      ${DCMTK_SOURCES_DIR}/dcmtls/include
      )

    if (STATIC_BUILD OR NOT USE_SYSTEM_OPENSSL)
      # The function "SSL_CTX_get0_param()" is available on both
      # OpenSSL 1.0.x and 1.1.x that are used for static builds
      set(HAVE_SSL_CTX_GET0_PARAM ON)
    else()
      # The call below requires "OpenSslConfiguration.cmake" to have
      # been included beforehand (which is automatically done if using
      # "OrthancFrameworkConfiguration.cmake")
      CHECK_LIBRARY_EXISTS(ssl "SSL_CTX_get0_param" "" HAVE_SSL_CTX_GET0_PARAM)
    endif()

    if (HAVE_SSL_CTX_GET0_PARAM)
      message("Have SSL_CTX_get0_param(): yes")
      add_definitions(-DHAVE_SSL_CTX_GET0_PARAM=1)
    else()
      message("Have SSL_CTX_get0_param(): no")
    endif()

    add_definitions(-DWITH_OPENSSL=1)
  endif()
  
  
  # This fixes crashes related to the destruction of the DCMTK OFLogger
  # http://support.dcmtk.org/docs-snapshot/file_macros.html
  add_definitions(
    -DLOG4CPLUS_DISABLE_FATAL=1
    -DDCMTK_VERSION_NUMBER=${DCMTK_VERSION_NUMBER}
    )


  if (NOT ENABLE_DCMTK_LOG)
    # Disable logging internal to DCMTK
    # https://groups.google.com/d/msg/orthanc-users/v2SzzAmY948/VxT1QVGiBAAJ
    add_definitions(
      -DDCMTK_LOG4CPLUS_DISABLE_FATAL=1
      -DDCMTK_LOG4CPLUS_DISABLE_ERROR=1
      -DDCMTK_LOG4CPLUS_DISABLE_WARN=1
      -DDCMTK_LOG4CPLUS_DISABLE_INFO=1
      -DDCMTK_LOG4CPLUS_DISABLE_DEBUG=1
      )
  endif()

  include_directories(
    #${DCMTK_SOURCES_DIR}
    ${DCMTK_SOURCES_DIR}/config/include
    ${DCMTK_SOURCES_DIR}/ofstd/include
    ${DCMTK_SOURCES_DIR}/oflog/include
    ${DCMTK_SOURCES_DIR}/dcmdata/include
    )

  source_group(ThirdParty\\Dcmtk REGULAR_EXPRESSION ${DCMTK_SOURCES_DIR}/.*)

  if (STANDALONE_BUILD)
    set(DCMTK_USE_EMBEDDED_DICTIONARIES 1)
    set(DCMTK_DICTIONARIES
      DICTIONARY_DICOM   ${DCMTK_SOURCES_DIR}/dcmdata/data/dicom.dic
      DICTIONARY_PRIVATE ${DCMTK_SOURCES_DIR}/dcmdata/data/private.dic
      DICTIONARY_DICONDE ${DCMTK_SOURCES_DIR}/dcmdata/data/diconde.dic
      )
  else()
    set(DCMTK_USE_EMBEDDED_DICTIONARIES 0)
  endif()


else()
  if (CMAKE_CROSSCOMPILING AND
      "${CMAKE_SYSTEM_VERSION}" STREQUAL "CrossToolNg")

    CHECK_INCLUDE_FILE_CXX(dcmtk/dcmdata/dcfilefo.h HAVE_DCMTK_H)
    if (NOT HAVE_DCMTK_H)
      message(FATAL_ERROR "Please install the libdcmtk-dev package")
    endif()

    CHECK_LIBRARY_EXISTS(dcmdata "dcmDataDict" "" HAVE_DCMTK_LIB)
    if (NOT HAVE_DCMTK_LIB)
      message(FATAL_ERROR "Please install the libdcmtk package")
    endif()  

    find_path(DCMTK_INCLUDE_DIRS dcmtk/config/osconfig.h
      /usr/include
      )

    link_libraries(dcmdata dcmnet dcmjpeg oflog ofstd)

  else()
    # The following line allows one to manually add libraries at the
    # command-line, which is necessary for Ubuntu/Debian packages
    set(tmp "${DCMTK_LIBRARIES}")
    include(FindDCMTK)
    list(APPEND DCMTK_LIBRARIES "${tmp}")

    include_directories(${DCMTK_INCLUDE_DIRS})
  endif()

  add_definitions(
    -DHAVE_CONFIG_H=1
    )

  if (EXISTS "${DCMTK_config_INCLUDE_DIR}/cfunix.h")
    set(DCMTK_CONFIGURATION_FILE "${DCMTK_config_INCLUDE_DIR}/cfunix.h")
  elseif (EXISTS "${DCMTK_config_INCLUDE_DIR}/osconfig.h")  # This is for Arch Linux
    set(DCMTK_CONFIGURATION_FILE "${DCMTK_config_INCLUDE_DIR}/osconfig.h")
  elseif (EXISTS "${DCMTK_INCLUDE_DIRS}/dcmtk/config/osconfig.h")  # This is for Debian Buster
    set(DCMTK_CONFIGURATION_FILE "${DCMTK_INCLUDE_DIRS}/dcmtk/config/osconfig.h")
  else()
    message(FATAL_ERROR "Please install libdcmtk*-dev")
  endif()

  message("DCMTK configuration file: ${DCMTK_CONFIGURATION_FILE}")
  
  # Autodetection of the version of DCMTK
  file(STRINGS
    "${DCMTK_CONFIGURATION_FILE}" 
    DCMTK_VERSION_NUMBER1 REGEX
    ".*PACKAGE_VERSION .*")    

  string(REGEX REPLACE
    ".*PACKAGE_VERSION.*\"([0-9]*)\\.([0-9]*)\\.([0-9]*)\"$"
    "\\1\\2\\3" 
    DCMTK_VERSION_NUMBER 
    ${DCMTK_VERSION_NUMBER1})

  set(DCMTK_USE_EMBEDDED_DICTIONARIES 0)
endif()


add_definitions(-DDCMTK_VERSION_NUMBER=${DCMTK_VERSION_NUMBER})
message("DCMTK version: ${DCMTK_VERSION_NUMBER}")


add_definitions(-DDCMTK_USE_EMBEDDED_DICTIONARIES=${DCMTK_USE_EMBEDDED_DICTIONARIES})
if (NOT DCMTK_USE_EMBEDDED_DICTIONARIES)
  # Lookup for DICOM dictionaries, if none is specified by the user
  if (DCMTK_DICTIONARY_DIR STREQUAL "")
    find_path(DCMTK_DICTIONARY_DIR_AUTO dicom.dic
      /usr/share/dcmtk
      /usr/share/libdcmtk1
      /usr/share/libdcmtk2
      /usr/share/libdcmtk3
      /usr/share/libdcmtk4
      /usr/share/libdcmtk5
      /usr/share/libdcmtk6
      /usr/share/libdcmtk7
      /usr/share/libdcmtk8
      /usr/share/libdcmtk9
      /usr/share/libdcmtk10
      /usr/share/libdcmtk11
      /usr/share/libdcmtk12
      /usr/share/libdcmtk13
      /usr/share/libdcmtk14
      /usr/share/libdcmtk15
      /usr/share/libdcmtk16
      /usr/share/libdcmtk17
      /usr/share/libdcmtk18
      /usr/share/libdcmtk19
      /usr/share/libdcmtk20
      /usr/local/share/dcmtk
      )

    if (${DCMTK_DICTIONARY_DIR_AUTO} MATCHES "DCMTK_DICTIONARY_DIR_AUTO-NOTFOUND")
      message(FATAL_ERROR "Cannot locate the DICOM dictionary on this system")
    endif()

    if (CMAKE_CROSSCOMPILING AND
        "${CMAKE_SYSTEM_VERSION}" STREQUAL "CrossToolNg")
      # Remove the sysroot prefix
      file(RELATIVE_PATH tmp ${CMAKE_FIND_ROOT_PATH} ${DCMTK_DICTIONARY_DIR_AUTO})
      set(DCMTK_DICTIONARY_DIR_AUTO /${tmp} CACHE INTERNAL "")
    endif()

    message("Autodetected path to the DICOM dictionaries: ${DCMTK_DICTIONARY_DIR_AUTO}")
    add_definitions(-DDCMTK_DICTIONARY_DIR="${DCMTK_DICTIONARY_DIR_AUTO}")
  else()
    add_definitions(-DDCMTK_DICTIONARY_DIR="${DCMTK_DICTIONARY_DIR}")
  endif()
endif()
