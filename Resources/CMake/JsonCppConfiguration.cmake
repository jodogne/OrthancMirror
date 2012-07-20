SET(JSONCPP_SOURCES_DIR ${CMAKE_BINARY_DIR}/jsoncpp-src-0.5.0)
DownloadPackage("http://downloads.sourceforge.net/project/jsoncpp/jsoncpp/0.5.0/jsoncpp-src-0.5.0.tar.gz" "${JSONCPP_SOURCES_DIR}" "" "")

list(APPEND THIRD_PARTY_SOURCES
  ${JSONCPP_SOURCES_DIR}/src/lib_json/json_reader.cpp
  ${JSONCPP_SOURCES_DIR}/src/lib_json/json_value.cpp
  ${JSONCPP_SOURCES_DIR}/src/lib_json/json_writer.cpp
  )

include_directories(
  ${JSONCPP_SOURCES_DIR}/include
  )

source_group(ThirdParty\\JsonCpp REGULAR_EXPRESSION ${JSONCPP_SOURCES_DIR}/.*)
