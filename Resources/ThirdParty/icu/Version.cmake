# NB: Orthanc assume that the platform is of ASCII-family, not of
# EBCDIC-family. A "e" suffix would be needed on EBCDIC. Look for
# macro "U_ICUDATA_TYPE_LETTER" in the source code of icu for more
# information.

include(TestBigEndian)
TEST_BIG_ENDIAN(IS_BIG_ENDIAN)
if(IS_BIG_ENDIAN)
  set(LIBICU_SUFFIX "b")
else()
  set(LIBICU_SUFFIX "l")
endif()

set(LIBICU_SOURCES_DIR ${CMAKE_BINARY_DIR}/icu)
set(LIBICU_URL "http://orthanc.osimis.io/ThirdPartyDownloads/icu4c-63_1-src.tgz")
set(LIBICU_MD5 "9e40f6055294284df958200e308bce50")
set(LIBICU_DATA "icudt63${LIBICU_SUFFIX}_dat.c")
set(LIBICU_SOURCE_DATA "${LIBICU_SOURCES_DIR}/source/data/in/icudt63l.dat")
