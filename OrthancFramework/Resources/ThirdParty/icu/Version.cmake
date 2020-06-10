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

set(LIBICU_BASE_URL "http://orthanc.osimis.io/ThirdPartyDownloads")

if (USE_LEGACY_LIBICU)
  # This is the latest version of icu that compiles without C++11
  # support. It is used for Linux Standard Base and Visual Studio 2008.
  set(LIBICU_URL "${LIBICU_BASE_URL}/icu4c-58_2-src.tgz")
  set(LIBICU_MD5 "fac212b32b7ec7ab007a12dff1f3aea1")
  set(LIBICU_DATA_VERSION "icudt58")
  set(LIBICU_DATA_COMPRESSED_MD5 "a39b07b38195158c6c3070332cef2173")
  set(LIBICU_DATA_UNCOMPRESSED_MD5 "54d2593cec5c6a4469373231658153ce")
else()
  set(LIBICU_URL "${LIBICU_BASE_URL}/icu4c-63_1-src.tgz")
  set(LIBICU_MD5 "9e40f6055294284df958200e308bce50")
  set(LIBICU_DATA_VERSION "icudt63")
  set(LIBICU_DATA_COMPRESSED_MD5 "be495c0830de5f377fdfa8301a5faf3d")
  set(LIBICU_DATA_UNCOMPRESSED_MD5 "99613c3f2ca9426c45dc554ad28cfb79")
endif()

set(LIBICU_SOURCES_DIR ${CMAKE_BINARY_DIR}/icu)
set(LIBICU_DATA "${LIBICU_DATA_VERSION}${LIBICU_SUFFIX}.dat.gz")
set(LIBICU_DATA_URL "${LIBICU_BASE_URL}/${LIBICU_DATA}")
