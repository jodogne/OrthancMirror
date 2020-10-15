set(LIBICONV_SOURCES_DIR ${CMAKE_BINARY_DIR}/libiconv-1.14)
DownloadPackage(
  "e34509b1623cec449dfeb73d7ce9c6c6"
  "http://www.orthanc-server.com/downloads/third-party/libiconv-1.14.tar.gz"
  "${LIBICONV_SOURCES_DIR}")

# https://groups.google.com/d/msg/android-ndk/AS1nkxnk6m4/EQm09hD1tigJ
add_definitions(
  -DBOOST_LOCALE_WITH_ICONV=1
  -DBUILDING_LIBICONV=1
  -DIN_LIBRARY=1
  -DLIBDIR=""
  -DICONV_CONST=
  )

configure_file(
  ${LIBICONV_SOURCES_DIR}/srclib/localcharset.h
  ${LIBICONV_SOURCES_DIR}/include
  COPYONLY)

set(HAVE_VISIBILITY 0)
set(ICONV_CONST ${ICONV_CONST})
set(USE_MBSTATE_T 1)
set(BROKEN_WCHAR_H 0)
set(EILSEQ)
set(HAVE_WCHAR_T 1)   
configure_file(
  ${LIBICONV_SOURCES_DIR}/include/iconv.h.build.in
  ${LIBICONV_SOURCES_DIR}/include/iconv.h
  )
unset(HAVE_VISIBILITY)
unset(ICONV_CONST)
unset(USE_MBSTATE_T)
unset(BROKEN_WCHAR_H)
unset(EILSEQ)
unset(HAVE_WCHAR_T)   

include_directories(
  ${LIBICONV_SOURCES_DIR}/include
  )

list(APPEND BOOST_SOURCES
  ${LIBICONV_SOURCES_DIR}/lib/iconv.c  
  ${LIBICONV_SOURCES_DIR}/lib/relocatable.c
  ${LIBICONV_SOURCES_DIR}/libcharset/lib/localcharset.c  
  ${LIBICONV_SOURCES_DIR}/libcharset/lib/relocatable.c
  )
