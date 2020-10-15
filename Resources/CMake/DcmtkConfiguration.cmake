# Lookup for DICOM dictionaries, if none is specified by the user
if (DCMTK_DICTIONARY_DIR STREQUAL "")
  find_path(DCMTK_DICTIONARY_DIR_AUTO dicom.dic
    /usr/share/dcmtk
    /usr/share/libdcmtk2
    )

  message("Autodetected path to the DICOM dictionaries: ${DCMTK_DICTIONARY_DIR_AUTO}")
  add_definitions(-DDCMTK_DICTIONARY_DIR="${DCMTK_DICTIONARY_DIR_AUTO}")
else()
  add_definitions(-DDCMTK_DICTIONARY_DIR="${DCMTK_DICTIONARY_DIR}")
endif()


if (STATIC_BUILD OR NOT USE_SYSTEM_DCMTK)
  SET(DCMTK_VERSION_NUMBER 360)
  SET(DCMTK_SOURCES_DIR ${CMAKE_BINARY_DIR}/dcmtk-3.6.0)
  DownloadPackage(
    "219ad631b82031806147e4abbfba4fa4"
    "http://www.orthanc-server.com/downloads/third-party/dcmtk-3.6.0.zip" 
    "${DCMTK_SOURCES_DIR}")

  IF(CMAKE_CROSSCOMPILING)
    SET(C_CHAR_UNSIGNED 1 CACHE INTERNAL "Whether char is unsigned.")
  ENDIF()
  SET(DCMTK_SOURCE_DIR ${CMAKE_BINARY_DIR}/dcmtk-3.6.0)
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

  set(DCMTK_PACKAGE_VERSION "3.6.0")
  set(DCMTK_PACKAGE_VERSION_SUFFIX "")
  set(DCMTK_PACKAGE_VERSION_NUMBER 360)

  CONFIGURE_FILE(
    ${DCMTK_SOURCES_DIR}/CMake/osconfig.h.in
    ${DCMTK_SOURCES_DIR}/config/include/dcmtk/config/osconfig.h)

  AUX_SOURCE_DIRECTORY(${DCMTK_SOURCES_DIR}/dcmnet/libsrc DCMTK_SOURCES)
  AUX_SOURCE_DIRECTORY(${DCMTK_SOURCES_DIR}/dcmdata/libsrc DCMTK_SOURCES)
  AUX_SOURCE_DIRECTORY(${DCMTK_SOURCES_DIR}/ofstd/libsrc DCMTK_SOURCES)


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
      )
    list(APPEND DCMTK_SOURCES 
      ${DCMTK_SOURCES_DIR}/dcmjpeg/libsrc/djrplol.cc
      )
  endif()


  # Source for the logging facility of DCMTK
  AUX_SOURCE_DIRECTORY(${DCMTK_SOURCES_DIR}/oflog/libsrc DCMTK_SOURCES)
  if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux" OR
      ${CMAKE_SYSTEM_NAME} STREQUAL "Darwin" OR
      ${CMAKE_SYSTEM_NAME} STREQUAL "kFreeBSD")
    list(REMOVE_ITEM DCMTK_SOURCES 
      ${DCMTK_SOURCES_DIR}/oflog/libsrc/windebap.cc
      ${DCMTK_SOURCES_DIR}/oflog/libsrc/winsock.cc
      )
  elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    list(REMOVE_ITEM DCMTK_SOURCES 
      ${DCMTK_SOURCES_DIR}/oflog/libsrc/unixsock.cc
      )

    if (${CMAKE_COMPILER_IS_GNUCXX})
      # This is a patch for MinGW64
      execute_process(
        COMMAND patch -p0 -i ${ORTHANC_ROOT}/Resources/Patches/dcmtk-mingw64.patch
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
    endif()

  endif()

  list(REMOVE_ITEM DCMTK_SOURCES 
    ${DCMTK_SOURCES_DIR}/dcmdata/libsrc/mkdictbi.cc
    ${DCMTK_SOURCES_DIR}/dcmdata/libsrc/mkdeftag.cc
    ${DCMTK_SOURCES_DIR}/dcmdata/libsrc/dcdictbi.cc
    )

  #set_source_files_properties(${DCMTK_SOURCES}
  #  PROPERTIES COMPILE_DEFINITIONS
  #  "PACKAGE_VERSION=\"3.6.0\";PACKAGE_VERSION_NUMBER=\"360\"")

  # This fixes crashes related to the destruction of the DCMTK OFLogger
  # http://support.dcmtk.org/docs-snapshot/file_macros.html
  add_definitions(
    -DLOG4CPLUS_DISABLE_FATAL=1
    -DDCMTK_VERSION_NUMBER=360
    )

  include_directories(
    #${DCMTK_SOURCES_DIR}
    ${DCMTK_SOURCES_DIR}/config/include
    ${DCMTK_SOURCES_DIR}/dcmnet/include
    ${DCMTK_SOURCES_DIR}/ofstd/include
    ${DCMTK_SOURCES_DIR}/oflog/include
    ${DCMTK_SOURCES_DIR}/dcmdata/include
    )

  source_group(ThirdParty\\Dcmtk REGULAR_EXPRESSION ${DCMTK_SOURCES_DIR}/.*)

  set(DCMTK_BUNDLES_LOG4CPLUS 1)

  if (STANDALONE_BUILD)
    add_definitions(-DDCMTK_USE_EMBEDDED_DICTIONARIES=1)
  else()
    add_definitions(-DDCMTK_USE_EMBEDDED_DICTIONARIES=0)
  endif()

  set(DCMTK_DICTIONARIES
    DICTIONARY_DICOM ${DCMTK_SOURCES_DIR}/dcmdata/data/dicom.dic
    DICTIONARY_PRIVATE ${DCMTK_SOURCES_DIR}/dcmdata/data/private.dic
    DICTIONARY_DICONDE ${DCMTK_SOURCES_DIR}/dcmdata/data/diconde.dic
    )

else()
  # The following line allows to manually add libraries at the
  # command-line, which is necessary for Ubuntu/Debian packages
  set(tmp "${DCMTK_LIBRARIES}")
  include(FindDCMTK)
  list(APPEND DCMTK_LIBRARIES "${tmp}")

  include_directories(${DCMTK_INCLUDE_DIR})
  link_libraries(${DCMTK_LIBRARIES})

  add_definitions(
    -DHAVE_CONFIG_H=1
    )

  if (EXISTS "${DCMTK_DIR}/config/cfunix.h")
    set(DCMTK_CONFIGURATION_FILE "${DCMTK_DIR}/config/cfunix.h")
  elseif (EXISTS "${DCMTK_DIR}/config/osconfig.h")  # This is for Arch Linux
    set(DCMTK_CONFIGURATION_FILE "${DCMTK_DIR}/config/osconfig.h")
  else()
    message(FATAL_ERROR "Please install libdcmtk1-dev")
  endif()

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

  add_definitions(-DDCMTK_USE_EMBEDDED_DICTIONARIES=0)

endif()

add_definitions(-DDCMTK_VERSION_NUMBER=${DCMTK_VERSION_NUMBER})
message("DCMTK version: ${DCMTK_VERSION_NUMBER}")
