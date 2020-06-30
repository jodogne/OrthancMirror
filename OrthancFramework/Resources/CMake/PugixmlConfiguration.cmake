if (STATIC_BUILD OR NOT USE_SYSTEM_PUGIXML)
  set(PUGIXML_SOURCES_DIR ${CMAKE_BINARY_DIR}/pugixml-1.9)
  set(PUGIXML_MD5 "7286ee2ed11376b6b780ced19fae0b64")
  set(PUGIXML_URL "http://orthanc.osimis.io/ThirdPartyDownloads/pugixml-1.9.tar.gz")

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
