# We always statically link against DCMTK 3.6.0, as there are many
# differences wrt. DCMTK 3.5.x.

if (${STATIC_BUILD})
  SET(DCMTK_VERSION_NUMBER 360)
  SET(DCMTK_SOURCES_DIR ${CMAKE_BINARY_DIR}/dcmtk-3.6.0)
  DownloadPackage("ftp://dicom.offis.de/pub/dicom/offis/software/dcmtk/dcmtk360/dcmtk-3.6.0.zip" "${DCMTK_SOURCES_DIR}" "" "")

  IF(CMAKE_CROSSCOMPILING)
    SET(C_CHAR_UNSIGNED 1 CACHE INTERNAL "Whether char is unsigned.")
  ENDIF()
  SET(DCMTK_SOURCE_DIR ${CMAKE_BINARY_DIR}/dcmtk-3.6.0)
  include(${DCMTK_SOURCES_DIR}/CMake/CheckFunctionWithHeaderExists.cmake)
  include(${DCMTK_SOURCES_DIR}/CMake/GenerateDCMTKConfigure.cmake)
  CONFIGURE_FILE(${DCMTK_SOURCES_DIR}/CMake/osconfig.h.in
    ${DCMTK_SOURCES_DIR}/config/include/dcmtk/config/osconfig.h)

  AUX_SOURCE_DIRECTORY(${DCMTK_SOURCES_DIR}/dcmnet/libsrc THIRD_PARTY_SOURCES)
  AUX_SOURCE_DIRECTORY(${DCMTK_SOURCES_DIR}/dcmdata/libsrc THIRD_PARTY_SOURCES)
  AUX_SOURCE_DIRECTORY(${DCMTK_SOURCES_DIR}/ofstd/libsrc THIRD_PARTY_SOURCES)

  # Source for the logging facility of DCMTK
  AUX_SOURCE_DIRECTORY(${DCMTK_SOURCES_DIR}/oflog/libsrc THIRD_PARTY_SOURCES)
  if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    list(REMOVE_ITEM THIRD_PARTY_SOURCES 
      ${DCMTK_SOURCES_DIR}/oflog/libsrc/windebap.cc
      ${DCMTK_SOURCES_DIR}/oflog/libsrc/winsock.cc
      )
  elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    list(REMOVE_ITEM THIRD_PARTY_SOURCES 
      ${DCMTK_SOURCES_DIR}/oflog/libsrc/unixsock.cc
      )
  endif()

  list(REMOVE_ITEM THIRD_PARTY_SOURCES 
    ${DCMTK_SOURCES_DIR}/dcmdata/libsrc/mkdictbi.cc
    ${DCMTK_SOURCES_DIR}/dcmdata/libsrc/mkdeftag.cc
    ${DCMTK_SOURCES_DIR}/dcmdata/libsrc/dcdictbi.cc
    )

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

else()
  #include(FindDCMTK)
  set(DCMTK_DIR /usr/include/dcmtk)
  set(DCMTK_INCLUDE_DIR ${DCMTK_DIR})

  #message(${DCMTK_LIBRARIES})

  include_directories(${DCMTK_INCLUDE_DIR})
  link_libraries(dcmdata dcmnet wrap)

  add_definitions(
    -DHAVE_CONFIG_H=1
    )

  if (NOT EXISTS "${DCMTK_DIR}/config/cfunix.h")
    message(FATAL_ERROR "Please install libdcmtk1-dev")
  endif()

  # Autodetection of the version of DCMTK
  file(STRINGS "${DCMTK_DIR}/config/cfunix.h" DCMTK_VERSION_NUMBER1
    REGEX ".*PACKAGE_VERSION .*")
  string(REGEX REPLACE ".*PACKAGE_VERSION.*\"([0-9]*)\\.([0-9]*)\\.([0-9]*)\"$" "\\1\\2\\3" DCMTK_VERSION_NUMBER ${DCMTK_VERSION_NUMBER1})


  IF (EXISTS "${DCMTK_DIR}/oflog")
    set(DCMTK_BUNDLES_LOG4CPLUS 1)
    link_libraries(ofstd oflog)
  else()
    set(DCMTK_BUNDLES_LOG4CPLUS 0)
  endif()
endif()

add_definitions(-DDCMTK_VERSION_NUMBER=${DCMTK_VERSION_NUMBER})
message("DCMTK version: ${DCMTK_VERSION_NUMBER}")
message("Does DCMTK includes its own copy of Log4Cplus: ${DCMTK_BUNDLES_LOG4CPLUS}")
