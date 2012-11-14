#include "gtest/gtest.h"

#include <stdint.h>
#include <math.h>
#include <png.h>
#include <ctype.h>
#include <zlib.h>
#include <curl/curl.h>
#include <boost/version.hpp>
#include <sqlite3.h>


TEST(Versions, Zlib)
{
  ASSERT_STREQ(zlibVersion(), ZLIB_VERSION);
}

TEST(Versions, Curl)
{
  curl_version_info_data* v = curl_version_info(CURLVERSION_NOW);
  ASSERT_STREQ(LIBCURL_VERSION, v->version);
}

TEST(Versions, Png)
{
  ASSERT_EQ(PNG_LIBPNG_VER_MAJOR * 10000 + PNG_LIBPNG_VER_MINOR * 100 + PNG_LIBPNG_VER_RELEASE,
            png_access_version_number());
}

TEST(Versions, SQLite)
{
  // http://www.sqlite.org/capi3ref.html#sqlite3_libversion
  assert(sqlite3_libversion_number() == SQLITE_VERSION_NUMBER );
  assert(strcmp(sqlite3_sourceid(), SQLITE_SOURCE_ID) == 0);
  assert(strcmp(sqlite3_libversion(), SQLITE_VERSION) == 0);

  // Ensure that the SQLite version is above 3.7.0.
  // "sqlite3_create_function_v2" is not defined in previous versions.
  ASSERT_GE(SQLITE_VERSION_NUMBER, 3007000);
}


#if ORTHANC_STATIC == 1
TEST(Versions, ZlibStatic)
{
  ASSERT_STREQ("1.2.7", zlibVersion());
}

TEST(Versions, BoostStatic)
{
  ASSERT_STREQ("1_49", BOOST_LIB_VERSION);
}

TEST(Versions, CurlStatic)
{
  curl_version_info_data* v = curl_version_info(CURLVERSION_NOW);
  ASSERT_STREQ("7.26.0", v->version);
}

TEST(Versions, PngStatic)
{
  ASSERT_EQ(10512, png_access_version_number());
  ASSERT_STREQ("1.5.12", PNG_LIBPNG_VER_STRING);
}

TEST(Versions, CurlSsl)
{
  curl_version_info_data * vinfo = curl_version_info(CURLVERSION_NOW);

  // Check that SSL support is enabled when required
  bool curlSupportsSsl = vinfo->features & CURL_VERSION_SSL;

#if ORTHANC_SSL_ENABLED == 0
  ASSERT_FALSE(curlSupportsSsl);
#else
  ASSERT_TRUE(curlSupportsSsl);
#endif
}
#endif
