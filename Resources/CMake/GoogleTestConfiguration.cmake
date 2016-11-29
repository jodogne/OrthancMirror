if (USE_GTEST_DEBIAN_SOURCE_PACKAGE)
  find_path(GTEST_DEBIAN_SOURCES_DIR
    NAMES src/gtest-all.cc
    PATHS
    /usr/src/gtest
    /usr/src/googletest/googletest
    PATH_SUFFIXES src
    )

  find_path(GTEST_DEBIAN_INCLUDE_DIR
    NAMES gtest.h
    PATHS
    /usr/include/gtest
    )

  message("Path to the Debian Google Test sources: ${GTEST_DEBIAN_SOURCES_DIR}")
  message("Path to the Debian Google Test includes: ${GTEST_DEBIAN_INCLUDE_DIR}")

  set(GTEST_SOURCES ${GTEST_DEBIAN_SOURCES_DIR}/src/gtest-all.cc)
  include_directories(${GTEST_DEBIAN_SOURCES_DIR})

  if (NOT EXISTS ${GTEST_SOURCES} OR
      NOT EXISTS ${GTEST_DEBIAN_INCLUDE_DIR}/gtest.h)
    message(FATAL_ERROR "Please install the libgtest-dev package")
  endif()

elseif (STATIC_BUILD OR NOT USE_SYSTEM_GOOGLE_TEST)
  set(GTEST_SOURCES_DIR ${CMAKE_BINARY_DIR}/gtest-1.7.0)
  set(GTEST_URL "http://www.orthanc-server.com/downloads/third-party/gtest-1.7.0.zip")
  set(GTEST_MD5 "2d6ec8ccdf5c46b05ba54a9fd1d130d7")

  DownloadPackage(${GTEST_MD5} ${GTEST_URL} "${GTEST_SOURCES_DIR}")

  include_directories(
    ${GTEST_SOURCES_DIR}/include
    ${GTEST_SOURCES_DIR}
    )

  set(GTEST_SOURCES
    ${GTEST_SOURCES_DIR}/src/gtest-all.cc
    )

  # https://code.google.com/p/googletest/issues/detail?id=412
  if (MSVC) # VS2012 does not support tuples correctly yet
    add_definitions(/D _VARIADIC_MAX=10)
  endif()

  source_group(ThirdParty\\GoogleTest REGULAR_EXPRESSION ${GTEST_SOURCES_DIR}/.*)

else()
  include(FindGTest)
  if (NOT GTEST_FOUND)
    message(FATAL_ERROR "Unable to find GoogleTest")
  endif()

  include_directories(${GTEST_INCLUDE_DIRS})
  link_libraries(${GTEST_LIBRARIES})
endif()
