if (DEBIAN_USE_STATIC_GOOGLE_TEST)
  message(FATAL_ERROR "todo")

elseif (STATIC_BUILD OR NOT USE_DYNAMIC_GOOGLE_TEST)
  SET(GTEST_SOURCES_DIR ${CMAKE_BINARY_DIR}/gtest-1.6.0)
  DownloadPackage("http://googletest.googlecode.com/files/gtest-1.6.0.zip" "${GTEST_SOURCES_DIR}" "" "")

  include_directories(
    ${GTEST_SOURCES_DIR}/include
    ${GTEST_SOURCES_DIR}
    )

  set(GTEST_SOURCES
    ${GTEST_SOURCES_DIR}/src/gtest-all.cc
    )

else()
  include(FindGTest)
  include_directories(${GTEST_INCLUDE_DIRS})
  link_libraries(${GTEST_LIBRARIES})
endif()
