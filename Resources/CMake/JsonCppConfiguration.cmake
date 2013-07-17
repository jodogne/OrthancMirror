if (USE_DYNAMIC_JSONCPP)
  CHECK_INCLUDE_FILE_CXX(jsoncpp/json/reader.h HAVE_JSONCPP_H)
  if (NOT HAVE_JSONCPP_H)
    message(FATAL_ERROR "Please install the libjsoncpp-dev package")
  endif()

  include_directories(/usr/include/jsoncpp)
  link_libraries(jsoncpp)

else()
  SET(JSONCPP_SOURCES_DIR ${CMAKE_BINARY_DIR}/jsoncpp-src-0.6.0-rc2)
  DownloadPackage(
    "363e2f4cbd3aeb63bf4e571f377400fb"
    "http://www.montefiore.ulg.ac.be/~jodogne/Orthanc/ThirdPartyDownloads/jsoncpp-src-0.6.0-rc2.tar.gz"
    "${JSONCPP_SOURCES_DIR}" "" "")

  list(APPEND THIRD_PARTY_SOURCES
    ${JSONCPP_SOURCES_DIR}/src/lib_json/json_reader.cpp
    ${JSONCPP_SOURCES_DIR}/src/lib_json/json_value.cpp
    ${JSONCPP_SOURCES_DIR}/src/lib_json/json_writer.cpp
    )

  include_directories(
    ${JSONCPP_SOURCES_DIR}/include
    )

  source_group(ThirdParty\\JsonCpp REGULAR_EXPRESSION ${JSONCPP_SOURCES_DIR}/.*)
endif()

