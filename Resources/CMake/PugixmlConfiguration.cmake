if (STATIC_BUILD OR NOT USE_SYSTEM_PUGIXML)
  set(PUGIXML_SOURCES_DIR ${CMAKE_BINARY_DIR}/pugixml-1.4)
  set(PUGIXML_MD5 "7c56c91cfe3ecdee248a8e4892ef5781")
  set(PUGIXML_URL "http://www.orthanc-server.com/downloads/third-party/pugixml-1.4.tar.gz")

  DownloadPackage(${PUGIXML_MD5} ${PUGIXML_URL} "${PUGIXML_SOURCES_DIR}")

  include_directories(
    ${PUGIXML_SOURCES_DIR}/src
    )

  set(PUGIXML_SOURCES
    #${PUGIXML_SOURCES_DIR}/src/vlog_is_on.cc
    ${PUGIXML_SOURCES_DIR}/src/pugixml.cpp
    )

  source_group(ThirdParty\\pugixml REGULAR_EXPRESSION ${PUGIXML_SOURCES_DIR}/.*)

else()
  CHECK_INCLUDE_FILE_CXX(pugixml.hpp HAVE_PUGIXML_H)
  if (NOT HAVE_PUGIXML_H)
    message(FATAL_ERROR "Please install the libpugixml-dev package")
  endif()

  link_libraries(pugixml)
endif()
