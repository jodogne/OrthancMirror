/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * In addition, as a special exception, the copyright holders of this
 * program give permission to link the code of its release with the
 * OpenSSL project's "OpenSSL" library (or with modified versions of it
 * that use the same license as the "OpenSSL" library), and distribute
 * the linked executables. You must obey the GNU General Public License
 * in all respects for all of the code used other than "OpenSSL". If you
 * modify file(s) with this exception, you may extend this exception to
 * your version of the file(s), but you are not obligated to do so. If
 * you do not wish to do so, delete this exception statement from your
 * version. If you delete this exception statement from all source files
 * in the program, then also delete it here.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include "PrecompiledHeadersUnitTests.h"
#include "gtest/gtest.h"

#include <stdint.h>
#include <math.h>
#include <png.h>
#include <ctype.h>
#include <zlib.h>
#include <curl/curl.h>
#include <boost/version.hpp>
#include <sqlite3.h>
#include <lua.h>
#include <jpeglib.h>
#include <iconv.h>

#if ORTHANC_ENABLE_SSL == 1
#include <openssl/opensslv.h>
#endif


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
            static_cast<int>(png_access_version_number()));
}

TEST(Versions, SQLite)
{
#if defined(__APPLE__)
  // Under OS X, there might exist minor differences between the
  // version of the headers and the version of the library, for the
  // system-wide SQLite. Ignore these differences.
#else
  // http://www.sqlite.org/capi3ref.html#sqlite3_libversion
  EXPECT_EQ(sqlite3_libversion_number(), SQLITE_VERSION_NUMBER);
  EXPECT_STREQ(sqlite3_sourceid(), SQLITE_SOURCE_ID);
  EXPECT_STREQ(sqlite3_libversion(), SQLITE_VERSION);
#endif

  // Ensure that the SQLite version is above 3.7.0.
  // "sqlite3_create_function_v2" is not defined in previous versions.
  ASSERT_GE(SQLITE_VERSION_NUMBER, 3007000);
}


TEST(Versions, Lua)
{
  // Ensure that the Lua version is above 5.1.0. This version has
  // introduced some API changes.
  ASSERT_GE(LUA_VERSION_NUM, 501);
}


#if ORTHANC_STATIC == 1

TEST(Versions, ZlibStatic)
{
  ASSERT_STREQ("1.2.7", zlibVersion());
}

TEST(Versions, BoostStatic)
{
  ASSERT_STREQ("1_65_1", BOOST_LIB_VERSION);
}

TEST(Versions, CurlStatic)
{
  curl_version_info_data* v = curl_version_info(CURLVERSION_NOW);
  ASSERT_STREQ("7.57.0", v->version);
}

TEST(Versions, PngStatic)
{
  ASSERT_EQ(10512u, png_access_version_number());
  ASSERT_STREQ("1.5.12", PNG_LIBPNG_VER_STRING);
}

TEST(Versions, JpegStatic)
{
  ASSERT_EQ(9, JPEG_LIB_VERSION_MAJOR);
  ASSERT_EQ(1, JPEG_LIB_VERSION_MINOR);
}

TEST(Versions, CurlSslStatic)
{
  curl_version_info_data * vinfo = curl_version_info(CURLVERSION_NOW);

  // Check that SSL support is enabled when required
  bool curlSupportsSsl = (vinfo->features & CURL_VERSION_SSL) != 0;

#if ORTHANC_ENABLE_SSL == 0
  ASSERT_FALSE(curlSupportsSsl);
#else
  ASSERT_TRUE(curlSupportsSsl);
#endif
}

TEST(Version, LuaStatic)
{
  ASSERT_STREQ("Lua 5.1.5", LUA_RELEASE);
}

TEST(Version, LibIconvStatic)
{
  static const int major = 1;
  static const int minor = 15;  
  ASSERT_EQ((major << 8) + minor, _LIBICONV_VERSION);
}


#if ORTHANC_ENABLE_SSL == 1
TEST(Version, OpenSslStatic)
{
  ASSERT_EQ(0x1000204fL /* openssl-1.0.2d */, OPENSSL_VERSION_NUMBER);
}
#endif


#include <json/version.h>

TEST(Version, JsonCpp)
{
  ASSERT_STREQ("0.10.5", JSONCPP_VERSION_STRING);
}

#endif
