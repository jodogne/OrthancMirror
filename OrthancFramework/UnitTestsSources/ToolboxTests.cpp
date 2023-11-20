/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 **/


#if ORTHANC_UNIT_TESTS_LINK_FRAMEWORK == 1
// Must be the first to be sure to use the Orthanc framework shared library
#  include <OrthancFramework.h>
#endif

#include <gtest/gtest.h>

#include "../Sources/Compatibility.h"
#include "../Sources/IDynamicObject.h"
#include "../Sources/OrthancException.h"
#include "../Sources/Toolbox.h"

using namespace Orthanc;

TEST(Toolbox, Json)
{
  Json::Value a = Json::objectValue;
  a["hello"] = "world";

  std::string b = "{\"hello\"    :   \"world\"}";

  Json::Value c;
  ASSERT_TRUE(Toolbox::ReadJson(c, b));

  std::string d, e;
  Toolbox::WriteFastJson(d, a);
  Toolbox::WriteFastJson(e, c);
  ASSERT_EQ(d, e);

  std::string f, g;
  Toolbox::WriteStyledJson(f, a);
  Toolbox::WriteStyledJson(g, c);
  ASSERT_EQ(f, g);

  /**
   * Check compatibility with the serialized string generated by
   * JsonCpp 1.7.4 (Ubuntu 18.04). "StripSpaces()" removes the
   * trailing end-of-line character that was not present in the
   * deprecated serialization classes of JsonCpp.
   **/
  ASSERT_EQ(Toolbox::StripSpaces(d), "{\"hello\":\"world\"}");
  ASSERT_EQ(Toolbox::StripSpaces(f), "{\n   \"hello\" : \"world\"\n}");
}

TEST(Toolbox, JsonComments)
{
  std::string a = "/* a */ { /* b */ \"hello\" : /* c */ \"world\" /* d */ } // e";

  Json::Value b;
  ASSERT_TRUE(Toolbox::ReadJsonWithoutComments(b, a));

  std::string c;
  Toolbox::WriteFastJson(c, b);
  ASSERT_EQ(Toolbox::StripSpaces(c), "{\"hello\":\"world\"}");
  
  Toolbox::WriteStyledJson(c, b);
  ASSERT_EQ(Toolbox::StripSpaces(c), "{\n   \"hello\" : \"world\"\n}");
}

TEST(Toolbox, Base64_allByteValues)
{
  std::string toEncode;
  std::string base64Result;
  std::string decodedResult;

  size_t size = 2*256;
  toEncode.reserve(size);
  for (size_t i = 0; i < size; i++)
    toEncode.push_back(i % 256);

  Toolbox::EncodeBase64(base64Result, toEncode);
  Toolbox::DecodeBase64(decodedResult, base64Result);

  ASSERT_EQ(toEncode, decodedResult);
}

TEST(Toolbox, Base64_multipleSizes)
{
  std::string toEncode;
  std::string base64Result;
  std::string decodedResult;

  for (size_t size = 0; size <= 5; size++)
  {
    printf("base64, testing size %zu\n", size);
    toEncode.clear();
    toEncode.reserve(size);
    for (size_t i = 0; i < size; i++)
      toEncode.push_back(i % 256);

    Toolbox::EncodeBase64(base64Result, toEncode);
    Toolbox::DecodeBase64(decodedResult, base64Result);

    ASSERT_EQ(toEncode, decodedResult);
  }
}

static std::string EncodeBase64Bis(const std::string& s)
{
  std::string result;
  Toolbox::EncodeBase64(result, s);
  return result;
}


TEST(Toolbox, Base64)
{
  ASSERT_EQ("", EncodeBase64Bis(""));
  ASSERT_EQ("YQ==", EncodeBase64Bis("a"));

  const std::string hello = "SGVsbG8gd29ybGQ=";
  ASSERT_EQ(hello, EncodeBase64Bis("Hello world"));

  std::string decoded;
  Toolbox::DecodeBase64(decoded, hello);
  ASSERT_EQ("Hello world", decoded);

  // Invalid character
  ASSERT_THROW(Toolbox::DecodeBase64(decoded, "?"), OrthancException);

  // All the allowed characters
  Toolbox::DecodeBase64(decoded, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=");
}


#if 0 // enable only when compiling in Release with a C++ 11 compiler
#include <chrono> // I had troubles to link with boost::chrono ...

TEST(Toolbox, Base64_largeString)
{
  std::string toEncode;
  std::string base64Result;
  std::string decodedResult;

  size_t size = 10 * 1024 * 1024;
  toEncode.reserve(size);
  for (size_t i = 0; i < size; i++)
    toEncode.push_back(i % 256);

  std::chrono::high_resolution_clock::time_point start;
  std::chrono::high_resolution_clock::time_point afterEncoding;
  std::chrono::high_resolution_clock::time_point afterDecoding;

  start = std::chrono::high_resolution_clock::now();
  Orthanc::Toolbox::EncodeBase64(base64Result, toEncode);
  afterEncoding = std::chrono::high_resolution_clock::now();
  Orthanc::Toolbox::DecodeBase64(decodedResult, base64Result);
  afterDecoding = std::chrono::high_resolution_clock::now();

  ASSERT_EQ(toEncode, decodedResult);

  printf("encoding took %zu ms\n", (std::chrono::duration_cast<std::chrono::milliseconds>(afterEncoding - start)));
  printf("decoding took %zu ms\n", (std::chrono::duration_cast<std::chrono::milliseconds>(afterDecoding - afterEncoding)));
}
#endif


TEST(Toolbox, LargeHexadecimalToDecimal)
{
  // https://stackoverflow.com/a/16967286/881731
  ASSERT_EQ(
    "166089946137986168535368849184301740204613753693156360462575217560130904921953976324839782808018277000296027060873747803291797869684516494894741699267674246881622658654267131250470956587908385447044319923040838072975636163137212887824248575510341104029461758594855159174329892125993844566497176102668262139513",
    Toolbox::LargeHexadecimalToDecimal("EC851A69B8ACD843164E10CFF70CF9E86DC2FEE3CF6F374B43C854E3342A2F1AC3E30C741CC41E679DF6D07CE6FA3A66083EC9B8C8BF3AF05D8BDBB0AA6Cb3ef8c5baa2a5e531ba9e28592f99e0fe4f95169a6c63f635d0197e325c5ec76219b907e4ebdcd401fb1986e4e3ca661ff73e7e2b8fd9988e753b7042b2bbca76679"));

  ASSERT_EQ("0", Toolbox::LargeHexadecimalToDecimal(""));
  ASSERT_EQ("0", Toolbox::LargeHexadecimalToDecimal("0"));
  ASSERT_EQ("0", Toolbox::LargeHexadecimalToDecimal("0000"));
  ASSERT_EQ("255", Toolbox::LargeHexadecimalToDecimal("00000ff"));

  ASSERT_THROW(Toolbox::LargeHexadecimalToDecimal("g"), Orthanc::OrthancException);
}


TEST(Toolbox, GenerateDicomPrivateUniqueIdentifier)
{
  std::string s = Toolbox::GenerateDicomPrivateUniqueIdentifier();
  ASSERT_EQ("2.25.", s.substr(0, 5));
}


TEST(Toolbox, UniquePtr)
{
  std::unique_ptr<int> i(new int(42));
  ASSERT_EQ(42, *i);

  std::unique_ptr<SingleValueObject<int> > j(new SingleValueObject<int>(42));
  ASSERT_EQ(42, j->GetValue());
}

TEST(Toolbox, IsSetInSet)
{
  {
    std::set<int> needles;
    std::set<int> haystack;
    std::set<int> missings;

    ASSERT_TRUE(Toolbox::IsSetInSet<int>(needles, haystack));
    ASSERT_EQ(0u, Toolbox::GetMissingsFromSet<int>(missings, needles, haystack));
  }

  {
    std::set<int> needles;
    std::set<int> haystack;
    std::set<int> missings;

    haystack.insert(5);
    ASSERT_TRUE(Toolbox::IsSetInSet<int>(needles, haystack));
    ASSERT_EQ(0u, Toolbox::GetMissingsFromSet<int>(missings, needles, haystack));
  }

  {
    std::set<int> needles;
    std::set<int> haystack;
    std::set<int> missings;

    needles.insert(5);
    haystack.insert(5);
    ASSERT_TRUE(Toolbox::IsSetInSet<int>(needles, haystack));
    ASSERT_EQ(0u, Toolbox::GetMissingsFromSet<int>(missings, needles, haystack));
  }

  {
    std::set<int> needles;
    std::set<int> haystack;
    std::set<int> missings;

    needles.insert(5);
    
    ASSERT_FALSE(Toolbox::IsSetInSet<int>(needles, haystack));
    ASSERT_EQ(1u, Toolbox::GetMissingsFromSet<int>(missings, needles, haystack));
    ASSERT_TRUE(missings.count(5) == 1);
  }

  {
    std::set<int> needles;
    std::set<int> haystack;
    std::set<int> missings;

    needles.insert(6);
    haystack.insert(5);
    ASSERT_FALSE(Toolbox::IsSetInSet<int>(needles, haystack));
    ASSERT_EQ(1u, Toolbox::GetMissingsFromSet<int>(missings, needles, haystack));
    ASSERT_TRUE(missings.count(6) == 1);
  }

  {
    std::set<int> needles;
    std::set<int> haystack;
    std::set<int> missings;

    needles.insert(5);
    needles.insert(6);
    haystack.insert(5);
    haystack.insert(6);
    ASSERT_TRUE(Toolbox::IsSetInSet<int>(needles, haystack));
    ASSERT_EQ(0u, Toolbox::GetMissingsFromSet<int>(missings, needles, haystack));
  }
}

TEST(Toolbox, GetSetIntersection)
{
  {
    std::set<int> target;
    std::set<int> a;
    std::set<int> b;

    Toolbox::GetIntersection(target, a, b);
    ASSERT_EQ(0u, target.size());
  }

  {
    std::set<int> target;
    std::set<int> a;
    std::set<int> b;

    a.insert(1);
    b.insert(1);

    Toolbox::GetIntersection(target, a, b);
    ASSERT_EQ(1u, target.size());
    ASSERT_EQ(1u, target.count(1));
  }

  {
    std::set<int> target;
    std::set<int> a;
    std::set<int> b;

    a.insert(1);
    a.insert(2);
    b.insert(2);

    Toolbox::GetIntersection(target, a, b);
    ASSERT_EQ(1u, target.size());
    ASSERT_EQ(0u, target.count(1));
    ASSERT_EQ(1u, target.count(2));
  }

}


TEST(Toolbox, JoinStrings)
{
  {
    std::set<std::string> source;
    std::string result;

    Toolbox::JoinStrings(result, source, ";");
    ASSERT_EQ("", result);
  }

  {
    std::set<std::string> source;
    source.insert("1");

    std::string result;

    Toolbox::JoinStrings(result, source, ";");
    ASSERT_EQ("1", result);
  }

  {
    std::set<std::string> source;
    source.insert("2");
    source.insert("1");

    std::string result;

    Toolbox::JoinStrings(result, source, ";");
    ASSERT_EQ("1;2", result);
  }

  {
    std::set<std::string> source;
    source.insert("2");
    source.insert("1");

    std::string result;

    Toolbox::JoinStrings(result, source, "\\");
    ASSERT_EQ("1\\2", result);
  }
}

TEST(Toolbox, JoinUri)
{
  ASSERT_EQ("https://test.org/path", Toolbox::JoinUri("https://test.org", "path"));
  ASSERT_EQ("https://test.org/path", Toolbox::JoinUri("https://test.org/", "path"));
  ASSERT_EQ("https://test.org/path", Toolbox::JoinUri("https://test.org", "/path"));
  ASSERT_EQ("https://test.org/path", Toolbox::JoinUri("https://test.org/", "/path"));

  ASSERT_EQ("http://test.org:8042", Toolbox::JoinUri("http://test.org:8042", ""));
  ASSERT_EQ("http://test.org:8042/", Toolbox::JoinUri("http://test.org:8042/", ""));
}

TEST(Toolbox, GetHumanFileSize)
{
  ASSERT_EQ("234bytes", Toolbox::GetHumanFileSize(234));
  ASSERT_EQ("2.29KB", Toolbox::GetHumanFileSize(2345));
  ASSERT_EQ("22.91KB", Toolbox::GetHumanFileSize(23456));
  ASSERT_EQ("229.07KB", Toolbox::GetHumanFileSize(234567));
  ASSERT_EQ("2.24MB", Toolbox::GetHumanFileSize(2345678));
  ASSERT_EQ("22.37MB", Toolbox::GetHumanFileSize(23456789));
  ASSERT_EQ("223.70MB", Toolbox::GetHumanFileSize(234567890));
  ASSERT_EQ("2.18GB", Toolbox::GetHumanFileSize(2345678901));
  ASSERT_EQ("21.33TB", Toolbox::GetHumanFileSize(23456789012345));
}

TEST(Toolbox, GetHumanDuration)
{
  ASSERT_EQ("234ns", Toolbox::GetHumanDuration(234));
  ASSERT_EQ("2.35us", Toolbox::GetHumanDuration(2345));
  ASSERT_EQ("23.46us", Toolbox::GetHumanDuration(23456));
  ASSERT_EQ("234.57us", Toolbox::GetHumanDuration(234567));
  ASSERT_EQ("2.35ms", Toolbox::GetHumanDuration(2345678));
  ASSERT_EQ("2.35s", Toolbox::GetHumanDuration(2345678901));
  ASSERT_EQ("23456.79s", Toolbox::GetHumanDuration(23456789012345));
}

TEST(Toolbox, GetHumanTransferSpeed)
{
  ASSERT_EQ("8.00Mbps", Toolbox::GetHumanTransferSpeed(false, 1000, 1000000));
  ASSERT_EQ("8.59Gbps", Toolbox::GetHumanTransferSpeed(false, 1024*1024*1024, 1000000000));
  ASSERT_EQ("1.00GB in 1.00s = 8.59Gbps", Toolbox::GetHumanTransferSpeed(true, 1024*1024*1024, 1000000000));
  ASSERT_EQ("976.56KB in 1.00s = 8.00Mbps", Toolbox::GetHumanTransferSpeed(true, 1000*1000, 1000000000));
}