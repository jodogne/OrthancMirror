#include "gtest/gtest.h"

#include <stdint.h>
#include <math.h>
#include <png.h>
#include <ctype.h>
#include <zlib.h>
#include <curl/curl.h>
#include <boost/version.hpp>


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


#if PALANTIR_STATIC == 1
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
#endif
