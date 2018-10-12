if (STATIC_BUILD OR NOT USE_SYSTEM_CIVETWEB)
  set(CIVETWEB_SOURCES_DIR ${CMAKE_BINARY_DIR}/civetweb-1.9.1)
  set(CIVETWEB_URL "http://www.orthanc-server.com/downloads/third-party/civetweb-1.9.1.tar.gz")
  set(CIVETWEB_MD5 "c713f7336582d1a78897971260c67c2a")

  DownloadPackage(${CIVETWEB_MD5} ${CIVETWEB_URL} "${CIVETWEB_SOURCES_DIR}")

  include_directories(
    ${CIVETWEB_SOURCES_DIR}/include
    )

  set(CIVETWEB_SOURCES
    ${CIVETWEB_SOURCES_DIR}/src/civetweb.c
    )


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


  if (CMAKE_SYSTEM_NAME STREQUAL "Windows" AND
      CMAKE_COMPILER_IS_GNUCXX)
    # This is a patch for MinGW64
    add_definitions(-D_TIMESPEC_DEFINED=1)
  endif()

  source_group(ThirdParty\\Civetweb REGULAR_EXPRESSION ${CIVETWEB_SOURCES_DIR}/.*)

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
endif()
