if (STATIC_BUILD OR NOT USE_SYSTEM_JSONCPP)
  set(JSONCPP_SOURCES_DIR ${CMAKE_BINARY_DIR}/jsoncpp-src-0.6.0-rc2)
  DownloadPackage(
    "363e2f4cbd3aeb63bf4e571f377400fb"
    "http://www.montefiore.ulg.ac.be/~jodogne/Orthanc/ThirdPartyDownloads/jsoncpp-src-0.6.0-rc2.tar.gz"
    "${JSONCPP_SOURCES_DIR}")

  set(JSONCPP_SOURCES
    ${JSONCPP_SOURCES_DIR}/src/lib_json/json_reader.cpp
    ${JSONCPP_SOURCES_DIR}/src/lib_json/json_value.cpp
    ${JSONCPP_SOURCES_DIR}/src/lib_json/json_writer.cpp
    )

  include_directories(
    ${JSONCPP_SOURCES_DIR}/include
    )

  source_group(ThirdParty\\JsonCpp REGULAR_EXPRESSION ${JSONCPP_SOURCES_DIR}/.*)

else()
  find_path(JSONCPP_INCLUDE_DIR json/reader.h
    /usr/include/jsoncpp
    /usr/local/include/jsoncpp
    )

  message("JsonCpp include dir: ${JSONCPP_INCLUDE_DIR}")
  include_directories(${JSONCPP_INCLUDE_DIR})
  link_libraries(jsoncpp)

  CHECK_INCLUDE_FILE_CXX(${JSONCPP_INCLUDE_DIR}/json/reader.h HAVE_JSONCPP_H)
  if (NOT HAVE_JSONCPP_H)
    message(FATAL_ERROR "Please install the libjsoncpp-dev package")
  endif()

endif()
