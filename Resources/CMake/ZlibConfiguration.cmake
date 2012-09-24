if (ON) #(${STATIC_BUILD})
  SET(ZLIB_SOURCES_DIR ${CMAKE_BINARY_DIR}/zlib-1.2.7)
  DownloadPackage("http://zlib.net/zlib-1.2.7.tar.gz" "${ZLIB_SOURCES_DIR}" "${ZLIB_PRELOADED}" "")

  include_directories(
    ${ZLIB_SOURCES_DIR}
    )

  list(APPEND THIRD_PARTY_SOURCES 
    ${ZLIB_SOURCES_DIR}/adler32.c
    ${ZLIB_SOURCES_DIR}/compress.c
    ${ZLIB_SOURCES_DIR}/crc32.c 
    ${ZLIB_SOURCES_DIR}/deflate.c 
    ${ZLIB_SOURCES_DIR}/gzclose.c 
    ${ZLIB_SOURCES_DIR}/gzlib.c 
    ${ZLIB_SOURCES_DIR}/gzread.c 
    ${ZLIB_SOURCES_DIR}/gzwrite.c 
    ${ZLIB_SOURCES_DIR}/infback.c 
    ${ZLIB_SOURCES_DIR}/inffast.c 
    ${ZLIB_SOURCES_DIR}/inflate.c 
    ${ZLIB_SOURCES_DIR}/inftrees.c 
    ${ZLIB_SOURCES_DIR}/trees.c 
    ${ZLIB_SOURCES_DIR}/uncompr.c 
    ${ZLIB_SOURCES_DIR}/zutil.c
    ${ZLIB_SOURCES_DIR}/contrib/minizip/ioapi.c
    ${ZLIB_SOURCES_DIR}/contrib/minizip/zip.c
    )

  source_group(ThirdParty\\ZLib REGULAR_EXPRESSION ${ZLIB_SOURCES_DIR}/.*)

else()
  include(FindZLIB)
  include_directories(${ZLIB_INCLUDE_DIRS})
  link_libraries(${ZLIB_LIBRARIES})
endif()
