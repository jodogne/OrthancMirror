if (DEBIAN_USE_GTEST_SOURCE_PACKAGE)
  set(GTEST_SOURCES /usr/src/gtest/src/gtest-all.cc)
  include_directories(/usr/src/gtest)

  if (NOT EXISTS /usr/include/gtest/gtest.h OR
      NOT EXISTS ${GTEST_SOURCES})
    message(FATAL_ERROR "Please install the libgtest-dev package")
  endif()

elseif (STATIC_BUILD OR NOT USE_DYNAMIC_GOOGLE_TEST)
  SET(GTEST_SOURCES_DIR ${CMAKE_BINARY_DIR}/gtest-1.6.0)
  DownloadPackage("http://www.orthanc-server.com/downloads/third-party/gtest-1.6.0.zip" "${GTEST_SOURCES_DIR}" "" "")

  include_directories(
    ${GTEST_SOURCES_DIR}/include
    ${GTEST_SOURCES_DIR}
    )

  set(GTEST_SOURCES
    ${GTEST_SOURCES_DIR}/src/gtest-all.cc
    )

else()
  include(FindGTest)
  if (NOT GTEST_FOUND)
    message(FATAL_ERROR "Unable to find GoogleTest")
  endif()

  include_directories(${GTEST_INCLUDE_DIRS})
  link_libraries(${GTEST_LIBRARIES})
endif()
