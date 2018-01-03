if (USE_GOOGLE_TEST_DEBIAN_PACKAGE)
  find_path(GOOGLE_TEST_DEBIAN_SOURCES_DIR
    NAMES src/gtest-all.cc
    PATHS
    /usr/src/gtest
    /usr/src/googletest/googletest
    PATH_SUFFIXES src
    )

  find_path(GOOGLE_TEST_DEBIAN_INCLUDE_DIR
    NAMES gtest.h
    PATHS
    /usr/include/gtest
    )

  message("Path to the Debian Google Test sources: ${GOOGLE_TEST_DEBIAN_SOURCES_DIR}")
  message("Path to the Debian Google Test includes: ${GOOGLE_TEST_DEBIAN_INCLUDE_DIR}")

  set(GOOGLE_TEST_SOURCES
    ${GOOGLE_TEST_DEBIAN_SOURCES_DIR}/src/gtest-all.cc
    )

  include_directories(${GOOGLE_TEST_DEBIAN_SOURCES_DIR})

  if (NOT EXISTS ${GOOGLE_TEST_SOURCES} OR
      NOT EXISTS ${GOOGLE_TEST_DEBIAN_INCLUDE_DIR}/gtest.h)
    message(FATAL_ERROR "Please install the libgtest-dev package")
  endif()

elseif (STATIC_BUILD OR NOT USE_SYSTEM_GOOGLE_TEST)
  set(GOOGLE_TEST_SOURCES_DIR ${CMAKE_BINARY_DIR}/gtest-1.7.0)
  set(GOOGLE_TEST_URL "http://www.orthanc-server.com/downloads/third-party/gtest-1.7.0.zip")
  set(GOOGLE_TEST_MD5 "2d6ec8ccdf5c46b05ba54a9fd1d130d7")

  DownloadPackage(${GOOGLE_TEST_MD5} ${GOOGLE_TEST_URL} "${GOOGLE_TEST_SOURCES_DIR}")

  include_directories(
    ${GOOGLE_TEST_SOURCES_DIR}/include
    ${GOOGLE_TEST_SOURCES_DIR}
    )

  set(GOOGLE_TEST_SOURCES
    ${GOOGLE_TEST_SOURCES_DIR}/src/gtest-all.cc
    )

  # https://code.google.com/p/googletest/issues/detail?id=412
  if (MSVC) # VS2012 does not support tuples correctly yet
    add_definitions(/D _VARIADIC_MAX=10)
  endif()
  
  if ("${CMAKE_SYSTEM_VERSION}" STREQUAL "LinuxStandardBase")
    add_definitions(-DGTEST_HAS_CLONE=0)
  endif()
  
  source_group(ThirdParty\\GoogleTest REGULAR_EXPRESSION ${GOOGLE_TEST_SOURCES_DIR}/.*)

else()
  include(FindGTest)
  if (NOT GOOGLE_TEST_FOUND)
    message(FATAL_ERROR "Unable to find GoogleTest")
  endif()

  include_directories(${GOOGLE_TEST_INCLUDE_DIRS})

  # The variable GOOGLE_TEST_LIBRARIES contains the shared library of
  # Google Test
endif()
