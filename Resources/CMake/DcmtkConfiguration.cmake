if (NOT DEFINED ENABLE_DCMTK_NETWORKING)
    set(ENABLE_DCMTK_NETWORKING ON)
endif()

if (STATIC_BUILD OR NOT USE_SYSTEM_DCMTK)
  if (USE_DCMTK_360)
    SET(DCMTK_VERSION_NUMBER 360)
    SET(DCMTK_PACKAGE_VERSION "3.6.0")
    SET(DCMTK_SOURCES_DIR ${CMAKE_BINARY_DIR}/dcmtk-3.6.0)
    SET(DCMTK_URL "http://www.orthanc-server.com/downloads/third-party/dcmtk-3.6.0.zip")
    SET(DCMTK_MD5 "219ad631b82031806147e4abbfba4fa4")
    SET(DCMTK_PATCH_SPEED "${ORTHANC_ROOT}/Resources/Patches/dcmtk-3.6.0-speed.patch")
    SET(DCMTK_PATCH_MINGW64 "${ORTHANC_ROOT}/Resources/Patches/dcmtk-3.6.0-mingw64.patch")
  else()
    SET(DCMTK_VERSION_NUMBER 362)
    SET(DCMTK_PACKAGE_VERSION "3.6.2")
    SET(DCMTK_SOURCES_DIR ${CMAKE_BINARY_DIR}/dcmtk-3.6.2)
    SET(DCMTK_URL "http://www.orthanc-server.com/downloads/third-party/dcmtk-3.6.2.tar.gz")
    SET(DCMTK_MD5 "d219a4152772985191c9b89d75302d12")
    SET(DCMTK_PATCH_SPEED "${ORTHANC_ROOT}/Resources/Patches/dcmtk-3.6.2-speed.patch")

    macro(DCMTK_UNSET)
    endmacro()

    macro(DCMTK_UNSET_CACHE)
    endmacro()

    set(DCMTK_BINARY_DIR ${DCMTK_SOURCES_DIR}/)
    set(DCMTK_CMAKE_INCLUDE ${DCMTK_SOURCES_DIR}/)
    add_definitions(-DDCMTK_INSIDE_LOG4CPLUS=1)
  endif()
  
  if (IS_DIRECTORY "${DCMTK_SOURCES_DIR}")
    set(FirstRun OFF)
  else()
    set(FirstRun ON)
  endif()

  DownloadPackage(${DCMTK_MD5} ${DCMTK_URL} "${DCMTK_SOURCES_DIR}")

  
  if (FirstRun AND
      USE_DCMTK_360)
    # If using DCMTK 3.6.0, backport the "private.dic" file from DCMTK
    # 3.6.2. This adds support for more private tags, and fixes some
    # import problems with Philips MRI Achieva.
    if (USE_DCMTK_362_PRIVATE_DIC)
      message("Using the dictionary of private tags from DCMTK 3.6.2")
      configure_file(
        ${ORTHANC_ROOT}/Resources/Patches/dcmtk-3.6.2-private.dic
        ${DCMTK_SOURCES_DIR}/dcmdata/data/private.dic
        COPYONLY)
    else()
      message("Using the dictionary of private tags from DCMTK 3.6.0")
    endif()
    
    # Patches specific to DCMTK 3.6.0
    execute_process(
      COMMAND ${PATCH_EXECUTABLE} -p0 -N -i ${ORTHANC_ROOT}/Resources/Patches/dcmtk-3.6.0-dulparse-vulnerability.patch
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      RESULT_VARIABLE Failure
      )

    if (Failure)
      message(FATAL_ERROR "Error while patching a file")
    endif()
  endif()


  IF (CMAKE_CROSSCOMPILING)
    SET(C_CHAR_UNSIGNED 1 CACHE INTERNAL "Whether char is unsigned.")
  ENDIF()
  SET(DCMTK_SOURCE_DIR ${DCMTK_SOURCES_DIR})
  include(${DCMTK_SOURCES_DIR}/CMake/CheckFunctionWithHeaderExists.cmake)
  include(${DCMTK_SOURCES_DIR}/CMake/GenerateDCMTKConfigure.cmake)

  if ("${CMAKE_SYSTEM_VERSION}" STREQUAL "LinuxStandardBase")
    set(HAVE_SSTREAM 1)
    set(HAVE_PROTOTYPE_BZERO 1)
    set(HAVE_PROTOTYPE_GETHOSTNAME 1)
    set(HAVE_PROTOTYPE_GETSOCKOPT 1)
    set(HAVE_PROTOTYPE_SETSOCKOPT 1)
    set(HAVE_PROTOTYPE_CONNECT 1)
    set(HAVE_PROTOTYPE_BIND 1)
    set(HAVE_PROTOTYPE_ACCEPT 1)
    set(HAVE_PROTOTYPE_SETSOCKNAME 1)
    set(HAVE_PROTOTYPE_GETSOCKNAME 1)
  endif()

  set(DCMTK_PACKAGE_VERSION_SUFFIX "")
  set(DCMTK_PACKAGE_VERSION_NUMBER ${DCMTK_VERSION_NUMBER})

  CONFIGURE_FILE(
    ${DCMTK_SOURCES_DIR}/CMake/osconfig.h.in
    ${DCMTK_SOURCES_DIR}/config/include/dcmtk/config/osconfig.h)

  if (NOT USE_DCMTK_360)
    # Configure Wine if cross-compiling for Windows
    if (CMAKE_CROSSCOMPILING AND WIN32)
      include(${DCMTK_SOURCES_DIR}/CMake/dcmtkUseWine.cmake)
      FIND_PROGRAM(WINE_WINE_PROGRAM wine)
      FIND_PROGRAM(WINE_WINEPATH_PROGRAM winepath)
      list(APPEND DCMTK_TRY_COMPILE_REQUIRED_CMAKE_FLAGS "-DCMAKE_EXE_LINKER_FLAGS=-static")
    endif()

    # This step must be after the generation of "osconfig.h"
    INSPECT_FUNDAMENTAL_ARITHMETIC_TYPES()

    link_libraries(-lnetapi32)  # For NetWkstaUserGetInfo@12
    link_libraries(-liphlpapi)  # For GetAdaptersInfo@8
  endif()

  AUX_SOURCE_DIRECTORY(${DCMTK_SOURCES_DIR}/dcmdata/libsrc DCMTK_SOURCES)
  AUX_SOURCE_DIRECTORY(${DCMTK_SOURCES_DIR}/ofstd/libsrc DCMTK_SOURCES)

  if (ENABLE_DCMTK_NETWORKING)
    AUX_SOURCE_DIRECTORY(${DCMTK_SOURCES_DIR}/dcmnet/libsrc DCMTK_SOURCES)
    include_directories(
      ${DCMTK_SOURCES_DIR}/dcmnet/include
      )
  endif()

  if (ENABLE_JPEG)
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


  if (ENABLE_JPEG_LOSSLESS)
    AUX_SOURCE_DIRECTORY(${DCMTK_SOURCES_DIR}/dcmjpls/libsrc DCMTK_SOURCES)
    AUX_SOURCE_DIRECTORY(${DCMTK_SOURCES_DIR}/dcmjpls/libcharls DCMTK_SOURCES)
    include_directories(
      ${DCMTK_SOURCES_DIR}/dcmjpeg/include
      ${DCMTK_SOURCES_DIR}/dcmjpls/include
      ${DCMTK_SOURCES_DIR}/dcmjpls/libcharls
      )
    list(REMOVE_ITEM DCMTK_SOURCES 
      ${DCMTK_SOURCES_DIR}/dcmjpls/libsrc/djcodece.cc

      # Disable support for encoding JPEG-LS (modification in Orthanc 1.0.1)
      ${DCMTK_SOURCES_DIR}/dcmjpls/libsrc/djencode.cc
      )
    list(APPEND DCMTK_SOURCES 
      ${DCMTK_SOURCES_DIR}/dcmjpeg/libsrc/djrplol.cc
      )
  endif()


  # Source for the logging facility of DCMTK
  AUX_SOURCE_DIRECTORY(${DCMTK_SOURCES_DIR}/oflog/libsrc DCMTK_SOURCES)
  if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux" OR
      ${CMAKE_SYSTEM_NAME} STREQUAL "Darwin" OR
      ${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD" OR
      ${CMAKE_SYSTEM_NAME} STREQUAL "kFreeBSD")
    list(REMOVE_ITEM DCMTK_SOURCES 
      ${DCMTK_SOURCES_DIR}/oflog/libsrc/clfsap.cc
      ${DCMTK_SOURCES_DIR}/oflog/libsrc/windebap.cc
      ${DCMTK_SOURCES_DIR}/oflog/libsrc/winsock.cc
      )
    
    execute_process(
      COMMAND ${PATCH_EXECUTABLE} -p0 -N -i ${DCMTK_PATCH_SPEED}
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      RESULT_VARIABLE Failure
      )

    if (Failure AND FirstRun)
      message(FATAL_ERROR "Error while patching a file")
    endif()

  elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    list(REMOVE_ITEM DCMTK_SOURCES 
      ${DCMTK_SOURCES_DIR}/oflog/libsrc/unixsock.cc
      ${DCMTK_SOURCES_DIR}/oflog/libsrc/clfsap.cc
      )

    if (CMAKE_COMPILER_IS_GNUCXX AND DCMTK_PATCH_MINGW64)
      # This is a patch for MinGW64
      execute_process(
        COMMAND ${PATCH_EXECUTABLE} -p0 -N -i ${DCMTK_PATCH_MINGW64}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        RESULT_VARIABLE Failure
        )

      if (Failure AND FirstRun)
        message(FATAL_ERROR "Error while patching a file")
      endif()
    endif()

    # This patch improves speed, even for Windows
    execute_process(
      COMMAND ${PATCH_EXECUTABLE} -p0 -N 
      INPUT_FILE ${DCMTK_PATCH_SPEED}
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      RESULT_VARIABLE Failure
      )

    if (Failure AND FirstRun)
      message(FATAL_ERROR "Error while patching a file")
    endif()
  endif()

  list(REMOVE_ITEM DCMTK_SOURCES 
    ${DCMTK_SOURCES_DIR}/dcmdata/libsrc/mkdictbi.cc
    ${DCMTK_SOURCES_DIR}/dcmdata/libsrc/mkdeftag.cc
    )

  if (USE_DCMTK_360)
    # Removing this file is required with DCMTK 3.6.0
    list(REMOVE_ITEM DCMTK_SOURCES 
      ${DCMTK_SOURCES_DIR}/dcmdata/libsrc/dcdictbi.cc
      )
  endif()

  #set_source_files_properties(${DCMTK_SOURCES}
  #  PROPERTIES COMPILE_DEFINITIONS
  #  "PACKAGE_VERSION=\"${DCMTK_PACKAGE_VERSION}\";PACKAGE_VERSION_NUMBER=\"${DCMTK_VERSION_NUMBER}\"")

  # This fixes crashes related to the destruction of the DCMTK OFLogger
  # http://support.dcmtk.org/docs-snapshot/file_macros.html
  add_definitions(
    -DLOG4CPLUS_DISABLE_FATAL=1
    -DDCMTK_VERSION_NUMBER=${DCMTK_VERSION_NUMBER}
    )

  include_directories(
    #${DCMTK_SOURCES_DIR}
    ${DCMTK_SOURCES_DIR}/config/include
    ${DCMTK_SOURCES_DIR}/ofstd/include
    ${DCMTK_SOURCES_DIR}/oflog/include
    ${DCMTK_SOURCES_DIR}/dcmdata/include
    )

  source_group(ThirdParty\\Dcmtk REGULAR_EXPRESSION ${DCMTK_SOURCES_DIR}/.*)

  set(DCMTK_BUNDLES_LOG4CPLUS 1)

  if (STANDALONE_BUILD)
    set(DCMTK_USE_EMBEDDED_DICTIONARIES 1)
    set(DCMTK_DICTIONARIES
      DICTIONARY_DICOM ${DCMTK_SOURCES_DIR}/dcmdata/data/dicom.dic
      DICTIONARY_PRIVATE ${DCMTK_SOURCES_DIR}/dcmdata/data/private.dic
      DICTIONARY_DICONDE ${DCMTK_SOURCES_DIR}/dcmdata/data/diconde.dic
      )
  else()
    set(DCMTK_USE_EMBEDDED_DICTIONARIES 0)
  endif()

else()
  # The following line allows to manually add libraries at the
  # command-line, which is necessary for Ubuntu/Debian packages
  set(tmp "${DCMTK_LIBRARIES}")
  include(FindDCMTK)
  list(APPEND DCMTK_LIBRARIES "${tmp}")

  include_directories(${DCMTK_INCLUDE_DIRS})

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
      /usr/local/share/dcmtk
      )

    if (${DCMTK_DICTIONARY_DIR_AUTO} MATCHES "DCMTK_DICTIONARY_DIR_AUTO-NOTFOUND")
      message(FATAL_ERROR "Cannot locate the DICOM dictionary on this system")
    endif()

    message("Autodetected path to the DICOM dictionaries: ${DCMTK_DICTIONARY_DIR_AUTO}")
    add_definitions(-DDCMTK_DICTIONARY_DIR="${DCMTK_DICTIONARY_DIR_AUTO}")
  else()
    add_definitions(-DDCMTK_DICTIONARY_DIR="${DCMTK_DICTIONARY_DIR}")
  endif()
endif()
