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
  # This is the last version of icu that compiles with C++11
  # support. It can be used for Linux Standard Base and Visual Studio 2008.
  set(LIBICU_URL "${LIBICU_BASE_URL}/icu4c-58_2-src.tgz")
  set(LIBICU_MD5 "fac212b32b7ec7ab007a12dff1f3aea1")
  set(LIBICU_DATA_VERSION "icudt58")
  set(LIBICU_DATA_MD5 "ce2c7791ab637898553c121633155fb6")
  set(LIBICU_MASM_MD5 "8593ff014574adce0e5119762b428dc5")
else()
  set(LIBICU_URL "${LIBICU_BASE_URL}/icu4c-63_1-src.tgz")
  set(LIBICU_MD5 "9e40f6055294284df958200e308bce50")
  set(LIBICU_DATA_VERSION "icudt63")
  set(LIBICU_DATA_MD5 "92b5c73a1accd8ecf8c20c89bc6925a9")
  set(LIBICU_MASM_MD5 "016c28245796502194e4d81d5d1255e4")
endif()

set(LIBICU_SOURCES_DIR ${CMAKE_BINARY_DIR}/icu)
set(LIBICU_DATA "${LIBICU_DATA_VERSION}${LIBICU_SUFFIX}_dat.c")
set(LIBICU_DATA_URL "${LIBICU_BASE_URL}/${LIBICU_DATA}.gz")

# For Microsoft assembler (masm). This is necessarily little-endian.
set(LIBICU_MASM "${LIBICU_DATA_VERSION}_dat.asm")
set(LIBICU_MASM_URL "${LIBICU_BASE_URL}/${LIBICU_MASM}.gz")
