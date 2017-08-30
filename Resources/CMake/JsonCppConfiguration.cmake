if (STATIC_BUILD OR NOT USE_SYSTEM_JSONCPP)
  set(JSONCPP_SOURCES_DIR ${CMAKE_BINARY_DIR}/jsoncpp-0.10.5)
  set(JSONCPP_URL "http://www.orthanc-server.com/downloads/third-party/jsoncpp-0.10.5.tar.gz")
  set(JSONCPP_MD5 "db146bac5a126ded9bd728ab7b61ed6b")

  DownloadPackage(${JSONCPP_MD5} ${JSONCPP_URL} "${JSONCPP_SOURCES_DIR}")

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

  # Switch to the C++11 standard if the version of JsonCpp is 1.y.z
  if (EXISTS ${JSONCPP_INCLUDE_DIR}/json/version.h)
    file(STRINGS
      "${JSONCPP_INCLUDE_DIR}/json/version.h" 
      JSONCPP_VERSION_MAJOR1 REGEX
      ".*define JSONCPP_VERSION_MAJOR.*")

    if (NOT JSONCPP_VERSION_MAJOR1)
      message(FATAL_ERROR "Unable to extract the major version of JsonCpp")
    endif()
    
    string(REGEX REPLACE
      ".*JSONCPP_VERSION_MAJOR.*([0-9]+)$" "\\1" 
      JSONCPP_VERSION_MAJOR ${JSONCPP_VERSION_MAJOR1})
    message("JsonCpp major version: ${JSONCPP_VERSION_MAJOR}")

    if (CMAKE_COMPILER_IS_GNUCXX AND 
        JSONCPP_VERSION_MAJOR GREATER 0)
      message("Switching to C++11 standard, as version of JsonCpp is >= 1.0.0")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wno-deprecated-declarations")
    endif()
  else()
    message("Unable to detect the major version of JsonCpp, assuming < 1.0.0")
  endif()

endif()
