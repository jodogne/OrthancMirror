/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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
#include "../Core/Compatibility.h"
#include "../Core/IDynamicObject.h"
#include "../Core/OrthancException.h"
#include "../Core/Toolbox.h"

using namespace Orthanc;

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
