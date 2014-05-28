set(PLUSTACHE_SOURCES_DIR ${CMAKE_BINARY_DIR}/plustache-0.3.0)
DownloadPackage(
  "6162946bdb3dccf3b2185fcf149671ee"
  "http://www.montefiore.ulg.ac.be/~jodogne/Orthanc/ThirdPartyDownloads/plustache-0.3.0.tar.gz"
  "${PLUSTACHE_SOURCES_DIR}")

list(APPEND THIRD_PARTY_SOURCES
  ${PLUSTACHE_SOURCES_DIR}/src/context.cpp
  ${PLUSTACHE_SOURCES_DIR}/src/template.cpp
  )

include_directories(
  ${PLUSTACHE_SOURCES_DIR}
  )

source_group(ThirdParty\\Plustache REGULAR_EXPRESSION ${PLUSTACHE_SOURCES_DIR}/.*)
