if (STATIC_BUILD OR NOT USE_SYSTEM_CIVETWEB)
  set(CIVETWEB_SOURCES_DIR ${CMAKE_BINARY_DIR}/civetweb-1.11)
  set(CIVETWEB_URL "http://orthanc.osimis.io/ThirdPartyDownloads/civetweb-1.11.tar.gz")
  set(CIVETWEB_MD5 "b6d2175650a27924bccb747cbe084cd4")

  if (IS_DIRECTORY "${CIVETWEB_SOURCES_DIR}")
    set(FirstRun OFF)
  else()
    set(FirstRun ON)
  endif()

  DownloadPackage(${CIVETWEB_MD5} ${CIVETWEB_URL} "${CIVETWEB_SOURCES_DIR}")

  execute_process(
    COMMAND ${PATCH_EXECUTABLE} -p0 -N -i
    ${ORTHANC_ROOT}/Resources/Patches/civetweb-1.11.patch
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    RESULT_VARIABLE Failure
    )

  if (FirstRun AND Failure)
    message(FATAL_ERROR "Error while patching a file")
  endif()
  
  include_directories(
    ${CIVETWEB_SOURCES_DIR}/include
    )

  set(CIVETWEB_SOURCES
    ${CIVETWEB_SOURCES_DIR}/src/civetweb.c
    )

  # New in Orthanc 1.6.0: Enable support of compression in civetweb
  set_source_files_properties(
    ${CIVETWEB_SOURCES}
    PROPERTIES COMPILE_DEFINITIONS
    "USE_ZLIB=1")
  
  if (ENABLE_SSL)
    add_definitions(
      -DNO_SSL_DL=1
      )
    if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux" OR
        ${CMAKE_SYSTEM_NAME} STREQUAL "kFreeBSD")
      link_libraries(dl)
    endif()

  else()
    add_definitions(
      -DNO_SSL=1   # Remove SSL support from civetweb
      )
  endif()

  source_group(ThirdParty\\Civetweb REGULAR_EXPRESSION ${CIVETWEB_SOURCES_DIR}/.*)

  add_definitions(
    -DCIVETWEB_HAS_DISABLE_KEEP_ALIVE=1
    )

else()
  CHECK_INCLUDE_FILE_CXX(civetweb.h HAVE_CIVETWEB_H)
  if (NOT HAVE_CIVETWEB_H)
    message(FATAL_ERROR "Please install the libcivetweb-devel package")
  endif()

  cmake_reset_check_state()
  set(CMAKE_REQUIRED_LIBRARIES dl pthread)
  CHECK_LIBRARY_EXISTS(civetweb mg_start "" HAVE_CIVETWEB_LIB)
  if (NOT HAVE_CIVETWEB_LIB)
    message(FATAL_ERROR "Please install the libcivetweb-devel package")
  endif()

  link_libraries(civetweb)

  add_definitions(
    -DCIVETWEB_HAS_DISABLE_KEEP_ALIVE=0
    )
endif()
