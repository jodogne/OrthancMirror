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

#if !defined(ORTHANC_ENABLE_PUGIXML)
#  error ORTHANC_ENABLE_PUGIXML is not defined
#endif

#include "../Sources/EnumerationDictionary.h"

#include <gtest/gtest.h>

#include "../Sources/DicomFormat/DicomTag.h"
#include "../Sources/HttpServer/HttpToolbox.h"
#include "../Sources/Logging.h"
#include "../Sources/OrthancException.h"
#include "../Sources/Toolbox.h"

#if ORTHANC_SANDBOXED != 1
#  include "../Sources/FileBuffer.h"
#  include "../Sources/MetricsRegistry.h"
#  include "../Sources/SystemToolbox.h"
#  include "../Sources/TemporaryFile.h"
#endif

#include <ctype.h>


using namespace Orthanc;


TEST(Uuid, Generation)
{
  for (int i = 0; i < 10; i++)
  {
    std::string s = Toolbox::GenerateUuid();
    ASSERT_TRUE(Toolbox::IsUuid(s));
  }
}

TEST(Uuid, Test)
{
  ASSERT_FALSE(Toolbox::IsUuid(""));
  ASSERT_FALSE(Toolbox::IsUuid("012345678901234567890123456789012345"));
  ASSERT_TRUE(Toolbox::IsUuid("550e8400-e29b-41d4-a716-446655440000"));
  ASSERT_FALSE(Toolbox::IsUuid("550e8400-e29b-41d4-a716-44665544000_"));
  ASSERT_FALSE(Toolbox::IsUuid("01234567890123456789012345678901234_"));
  ASSERT_FALSE(Toolbox::StartsWithUuid("550e8400-e29b-41d4-a716-44665544000"));
  ASSERT_TRUE(Toolbox::StartsWithUuid("550e8400-e29b-41d4-a716-446655440000"));
  ASSERT_TRUE(Toolbox::StartsWithUuid("550e8400-e29b-41d4-a716-446655440000 ok"));
  ASSERT_FALSE(Toolbox::StartsWithUuid("550e8400-e29b-41d4-a716-446655440000ok"));
}

TEST(Toolbox, IsSHA1)
{
  ASSERT_FALSE(Toolbox::IsSHA1(""));
  ASSERT_FALSE(Toolbox::IsSHA1("01234567890123456789012345678901234567890123"));
  ASSERT_FALSE(Toolbox::IsSHA1("012345678901234567890123456789012345678901234"));
  ASSERT_TRUE(Toolbox::IsSHA1("b5ed549f-956400ce-69a8c063-bf5b78be-2732a4b9"));

  std::string sha = "         b5ed549f-956400ce-69a8c063-bf5b78be-2732a4b9          ";
  ASSERT_TRUE(Toolbox::IsSHA1(sha));
  sha[3] = '\0';
  sha[53] = '\0';
  ASSERT_TRUE(Toolbox::IsSHA1(sha));
  sha[40] = '\0';
  ASSERT_FALSE(Toolbox::IsSHA1(sha));
  ASSERT_FALSE(Toolbox::IsSHA1("       "));

  ASSERT_TRUE(Toolbox::IsSHA1("16738bc3-e47ed42a-43ce044c-a3414a45-cb069bd0"));

  std::string s;
  Toolbox::ComputeSHA1(s, "The quick brown fox jumps over the lazy dog");
  ASSERT_TRUE(Toolbox::IsSHA1(s));
  ASSERT_EQ("2fd4e1c6-7a2d28fc-ed849ee1-bb76e739-1b93eb12", s);

  ASSERT_FALSE(Toolbox::IsSHA1("b5ed549f-956400ce-69a8c063-bf5b78be-2732a4b_"));
}


TEST(ParseGetArguments, Basic)
{
  HttpToolbox::GetArguments b;
  HttpToolbox::ParseGetArguments(b, "aaa=baaa&bb=a&aa=c");

  HttpToolbox::Arguments a;
  HttpToolbox::CompileGetArguments(a, b);

  ASSERT_EQ(3u, a.size());
  ASSERT_EQ(a["aaa"], "baaa");
  ASSERT_EQ(a["bb"], "a");
  ASSERT_EQ(a["aa"], "c");
}


TEST(ParseGetArguments, BasicEmpty)
{
  HttpToolbox::GetArguments b;
  HttpToolbox::ParseGetArguments(b, "aaa&bb=aa&aa");

  HttpToolbox::Arguments a;
  HttpToolbox::CompileGetArguments(a, b);

  ASSERT_EQ(3u, a.size());
  ASSERT_EQ(a["aaa"], "");
  ASSERT_EQ(a["bb"], "aa");
  ASSERT_EQ(a["aa"], "");
}


TEST(ParseGetArguments, Single)
{
  HttpToolbox::GetArguments b;
  HttpToolbox::ParseGetArguments(b, "aaa=baaa");

  HttpToolbox::Arguments a;
  HttpToolbox::CompileGetArguments(a, b);

  ASSERT_EQ(1u, a.size());
  ASSERT_EQ(a["aaa"], "baaa");
}


TEST(ParseGetArguments, SingleEmpty)
{
  HttpToolbox::GetArguments b;
  HttpToolbox::ParseGetArguments(b, "aaa");

  HttpToolbox::Arguments a;
  HttpToolbox::CompileGetArguments(a, b);

  ASSERT_EQ(1u, a.size());
  ASSERT_EQ(a["aaa"], "");
}


TEST(ParseGetQuery, Test1)
{
  UriComponents uri;
  HttpToolbox::GetArguments b;
  HttpToolbox::ParseGetQuery(uri, b, "/instances/test/world?aaa=baaa&bb=a&aa=c");

  HttpToolbox::Arguments a;
  HttpToolbox::CompileGetArguments(a, b);

  ASSERT_EQ(3u, uri.size());
  ASSERT_EQ("instances", uri[0]);
  ASSERT_EQ("test", uri[1]);
  ASSERT_EQ("world", uri[2]);
  ASSERT_EQ(3u, a.size());
  ASSERT_EQ(a["aaa"], "baaa");
  ASSERT_EQ(a["bb"], "a");
  ASSERT_EQ(a["aa"], "c");
}


TEST(ParseGetQuery, Test2)
{
  UriComponents uri;
  HttpToolbox::GetArguments b;
  HttpToolbox::ParseGetQuery(uri, b, "/instances/test/world");

  HttpToolbox::Arguments a;
  HttpToolbox::CompileGetArguments(a, b);

  ASSERT_EQ(3u, uri.size());
  ASSERT_EQ("instances", uri[0]);
  ASSERT_EQ("test", uri[1]);
  ASSERT_EQ("world", uri[2]);
  ASSERT_EQ(0u, a.size());
}


TEST(Uri, SplitUriComponents)
{
  UriComponents c, d;
  Toolbox::SplitUriComponents(c, "/cou/hello/world");
  ASSERT_EQ(3u, c.size());
  ASSERT_EQ("cou", c[0]);
  ASSERT_EQ("hello", c[1]);
  ASSERT_EQ("world", c[2]);

  Toolbox::SplitUriComponents(c, "/cou/hello/world/");
  ASSERT_EQ(3u, c.size());
  ASSERT_EQ("cou", c[0]);
  ASSERT_EQ("hello", c[1]);
  ASSERT_EQ("world", c[2]);

  Toolbox::SplitUriComponents(c, "/cou/hello/world/a");
  ASSERT_EQ(4u, c.size());
  ASSERT_EQ("cou", c[0]);
  ASSERT_EQ("hello", c[1]);
  ASSERT_EQ("world", c[2]);
  ASSERT_EQ("a", c[3]);

  Toolbox::SplitUriComponents(c, "/");
  ASSERT_EQ(0u, c.size());

  Toolbox::SplitUriComponents(c, "/hello");
  ASSERT_EQ(1u, c.size());
  ASSERT_EQ("hello", c[0]);

  Toolbox::SplitUriComponents(c, "/hello/");
  ASSERT_EQ(1u, c.size());
  ASSERT_EQ("hello", c[0]);

  ASSERT_THROW(Toolbox::SplitUriComponents(c, ""), OrthancException);
  ASSERT_THROW(Toolbox::SplitUriComponents(c, "a"), OrthancException);
  ASSERT_THROW(Toolbox::SplitUriComponents(c, "/coucou//coucou"), OrthancException);

  c.clear();
  c.push_back("test");
  ASSERT_EQ("/", Toolbox::FlattenUri(c, 10));
}


TEST(Uri, Truncate)
{
  UriComponents c, d;
  Toolbox::SplitUriComponents(c, "/cou/hello/world");

  Toolbox::TruncateUri(d, c, 0);
  ASSERT_EQ(3u, d.size());
  ASSERT_EQ("cou", d[0]);
  ASSERT_EQ("hello", d[1]);
  ASSERT_EQ("world", d[2]);

  Toolbox::TruncateUri(d, c, 1);
  ASSERT_EQ(2u, d.size());
  ASSERT_EQ("hello", d[0]);
  ASSERT_EQ("world", d[1]);

  Toolbox::TruncateUri(d, c, 2);
  ASSERT_EQ(1u, d.size());
  ASSERT_EQ("world", d[0]);

  Toolbox::TruncateUri(d, c, 3);
  ASSERT_EQ(0u, d.size());

  Toolbox::TruncateUri(d, c, 4);
  ASSERT_EQ(0u, d.size());

  Toolbox::TruncateUri(d, c, 5);
  ASSERT_EQ(0u, d.size());
}


TEST(Uri, Child)
{
  UriComponents c1;  Toolbox::SplitUriComponents(c1, "/hello/world");  
  UriComponents c2;  Toolbox::SplitUriComponents(c2, "/hello/hello");  
  UriComponents c3;  Toolbox::SplitUriComponents(c3, "/hello");  
  UriComponents c4;  Toolbox::SplitUriComponents(c4, "/world");  
  UriComponents c5;  Toolbox::SplitUriComponents(c5, "/");  

  ASSERT_TRUE(Toolbox::IsChildUri(c1, c1));
  ASSERT_FALSE(Toolbox::IsChildUri(c1, c2));
  ASSERT_FALSE(Toolbox::IsChildUri(c1, c3));
  ASSERT_FALSE(Toolbox::IsChildUri(c1, c4));
  ASSERT_FALSE(Toolbox::IsChildUri(c1, c5));

  ASSERT_FALSE(Toolbox::IsChildUri(c2, c1));
  ASSERT_TRUE(Toolbox::IsChildUri(c2, c2));
  ASSERT_FALSE(Toolbox::IsChildUri(c2, c3));
  ASSERT_FALSE(Toolbox::IsChildUri(c2, c4));
  ASSERT_FALSE(Toolbox::IsChildUri(c2, c5));

  ASSERT_TRUE(Toolbox::IsChildUri(c3, c1));
  ASSERT_TRUE(Toolbox::IsChildUri(c3, c2));
  ASSERT_TRUE(Toolbox::IsChildUri(c3, c3));
  ASSERT_FALSE(Toolbox::IsChildUri(c3, c4));
  ASSERT_FALSE(Toolbox::IsChildUri(c3, c5));

  ASSERT_FALSE(Toolbox::IsChildUri(c4, c1));
  ASSERT_FALSE(Toolbox::IsChildUri(c4, c2));
  ASSERT_FALSE(Toolbox::IsChildUri(c4, c3));
  ASSERT_TRUE(Toolbox::IsChildUri(c4, c4));
  ASSERT_FALSE(Toolbox::IsChildUri(c4, c5));

  ASSERT_TRUE(Toolbox::IsChildUri(c5, c1));
  ASSERT_TRUE(Toolbox::IsChildUri(c5, c2));
  ASSERT_TRUE(Toolbox::IsChildUri(c5, c3));
  ASSERT_TRUE(Toolbox::IsChildUri(c5, c4));
  ASSERT_TRUE(Toolbox::IsChildUri(c5, c5));
}


#if ORTHANC_SANDBOXED != 1
TEST(Uri, AutodetectMimeType)
{
  ASSERT_EQ(MimeType_Binary, SystemToolbox::AutodetectMimeType("../NOTES"));
  ASSERT_EQ(MimeType_Binary, SystemToolbox::AutodetectMimeType(""));
  ASSERT_EQ(MimeType_Binary, SystemToolbox::AutodetectMimeType("/"));
  ASSERT_EQ(MimeType_Binary, SystemToolbox::AutodetectMimeType("a/a"));
  ASSERT_EQ(MimeType_Binary, SystemToolbox::AutodetectMimeType("..\\a\\"));
  ASSERT_EQ(MimeType_Binary, SystemToolbox::AutodetectMimeType("..\\a\\a"));

  ASSERT_EQ(MimeType_PlainText, SystemToolbox::AutodetectMimeType("../NOTES.txt"));
  ASSERT_EQ(MimeType_PlainText, SystemToolbox::AutodetectMimeType("../coucou.xml/NOTES.txt"));
  ASSERT_EQ(MimeType_Xml, SystemToolbox::AutodetectMimeType("..\\coucou.\\NOTES.xml"));
  ASSERT_EQ(MimeType_Xml, SystemToolbox::AutodetectMimeType("../.xml"));
  ASSERT_EQ(MimeType_Xml, SystemToolbox::AutodetectMimeType("../.XmL"));

  ASSERT_EQ(MimeType_JavaScript, SystemToolbox::AutodetectMimeType("NOTES.js"));
  ASSERT_EQ(MimeType_Json, SystemToolbox::AutodetectMimeType("NOTES.json"));
  ASSERT_EQ(MimeType_Pdf, SystemToolbox::AutodetectMimeType("NOTES.pdf"));
  ASSERT_EQ(MimeType_Css, SystemToolbox::AutodetectMimeType("NOTES.css"));
  ASSERT_EQ(MimeType_Html, SystemToolbox::AutodetectMimeType("NOTES.html"));
  ASSERT_EQ(MimeType_PlainText, SystemToolbox::AutodetectMimeType("NOTES.txt"));
  ASSERT_EQ(MimeType_Xml, SystemToolbox::AutodetectMimeType("NOTES.xml"));
  ASSERT_EQ(MimeType_Gif, SystemToolbox::AutodetectMimeType("NOTES.gif"));
  ASSERT_EQ(MimeType_Jpeg, SystemToolbox::AutodetectMimeType("NOTES.jpg"));
  ASSERT_EQ(MimeType_Jpeg, SystemToolbox::AutodetectMimeType("NOTES.jpeg"));
  ASSERT_EQ(MimeType_Png, SystemToolbox::AutodetectMimeType("NOTES.png"));
  ASSERT_EQ(MimeType_NaCl, SystemToolbox::AutodetectMimeType("NOTES.nexe"));
  ASSERT_EQ(MimeType_Json, SystemToolbox::AutodetectMimeType("NOTES.nmf"));
  ASSERT_EQ(MimeType_PNaCl, SystemToolbox::AutodetectMimeType("NOTES.pexe"));
  ASSERT_EQ(MimeType_Svg, SystemToolbox::AutodetectMimeType("NOTES.svg"));
  ASSERT_EQ(MimeType_Woff, SystemToolbox::AutodetectMimeType("NOTES.woff"));
  ASSERT_EQ(MimeType_Woff2, SystemToolbox::AutodetectMimeType("NOTES.woff2"));
  ASSERT_EQ(MimeType_Ico, SystemToolbox::AutodetectMimeType("NOTES.ico"));

  // Test primitives from the "RegisterDefaultExtensions()" that was
  // present in the sample "Serve Folders plugin" of Orthanc 1.4.2
  ASSERT_STREQ("application/javascript", EnumerationToString(SystemToolbox::AutodetectMimeType(".js")));
  ASSERT_STREQ("application/json", EnumerationToString(SystemToolbox::AutodetectMimeType(".json")));
  ASSERT_STREQ("application/json", EnumerationToString(SystemToolbox::AutodetectMimeType(".nmf")));
  ASSERT_STREQ("application/octet-stream", EnumerationToString(SystemToolbox::AutodetectMimeType("")));
  ASSERT_STREQ("application/wasm", EnumerationToString(SystemToolbox::AutodetectMimeType(".wasm")));
  ASSERT_STREQ("application/x-font-woff", EnumerationToString(SystemToolbox::AutodetectMimeType(".woff")));
  ASSERT_STREQ("application/x-nacl", EnumerationToString(SystemToolbox::AutodetectMimeType(".nexe")));
  ASSERT_STREQ("application/x-pnacl", EnumerationToString(SystemToolbox::AutodetectMimeType(".pexe")));
  ASSERT_STREQ("application/xml", EnumerationToString(SystemToolbox::AutodetectMimeType(".xml")));
  ASSERT_STREQ("font/woff2", EnumerationToString(SystemToolbox::AutodetectMimeType(".woff2")));
  ASSERT_STREQ("image/gif", EnumerationToString(SystemToolbox::AutodetectMimeType(".gif")));
  ASSERT_STREQ("image/jpeg", EnumerationToString(SystemToolbox::AutodetectMimeType(".jpeg")));
  ASSERT_STREQ("image/jpeg", EnumerationToString(SystemToolbox::AutodetectMimeType(".jpg")));
  ASSERT_STREQ("image/png", EnumerationToString(SystemToolbox::AutodetectMimeType(".png")));
  ASSERT_STREQ("image/svg+xml", EnumerationToString(SystemToolbox::AutodetectMimeType(".svg")));
  ASSERT_STREQ("text/css", EnumerationToString(SystemToolbox::AutodetectMimeType(".css")));
  ASSERT_STREQ("text/html", EnumerationToString(SystemToolbox::AutodetectMimeType(".html")));

  ASSERT_STREQ("model/obj", EnumerationToString(SystemToolbox::AutodetectMimeType(".obj")));
  ASSERT_STREQ("model/mtl", EnumerationToString(SystemToolbox::AutodetectMimeType(".mtl")));
  ASSERT_STREQ("model/stl", EnumerationToString(SystemToolbox::AutodetectMimeType(".stl")));
}
#endif


TEST(Toolbox, ComputeMD5)
{
  std::string s;

  // # echo -n "Hello" | md5sum

  Toolbox::ComputeMD5(s, "Hello");
  ASSERT_EQ("8b1a9953c4611296a827abf8c47804d7", s);
  Toolbox::ComputeMD5(s, "");
  ASSERT_EQ("d41d8cd98f00b204e9800998ecf8427e", s);

  Toolbox::ComputeMD5(s, "aaabbbccc");
  ASSERT_EQ("d1aaf4767a3c10a473407a4e47b02da6", s);

  std::set<std::string> set;

  Toolbox::ComputeMD5(s, set);
  ASSERT_EQ("d41d8cd98f00b204e9800998ecf8427e", s);  // empty set same as empty string

  set.insert("bbb");
  set.insert("ccc");
  set.insert("aaa");

  Toolbox::ComputeMD5(s, set);
  ASSERT_EQ("d1aaf4767a3c10a473407a4e47b02da6", s); // set md5 same as string with the values sorted
}

TEST(Toolbox, ComputeSHA1)
{
  std::string s;
  
  Toolbox::ComputeSHA1(s, "The quick brown fox jumps over the lazy dog");
  ASSERT_EQ("2fd4e1c6-7a2d28fc-ed849ee1-bb76e739-1b93eb12", s);
  Toolbox::ComputeSHA1(s, "");
  ASSERT_EQ("da39a3ee-5e6b4b0d-3255bfef-95601890-afd80709", s);
}

#if ORTHANC_SANDBOXED != 1
TEST(Toolbox, PathToExecutable)
{
  printf("[%s]\n", SystemToolbox::GetPathToExecutable().c_str());
  printf("[%s]\n", SystemToolbox::GetDirectoryOfExecutable().c_str());
}
#endif

TEST(Toolbox, StripSpaces)
{
  ASSERT_EQ("", Toolbox::StripSpaces("       \t  \r   \n  "));
  ASSERT_EQ("coucou", Toolbox::StripSpaces("    coucou   \t  \r   \n  "));
  ASSERT_EQ("cou   cou", Toolbox::StripSpaces("    cou   cou    \n  "));
  ASSERT_EQ("c", Toolbox::StripSpaces("    \n\t c\r    \n  "));

  std::string s = "\"  abd \"";
  Toolbox::RemoveSurroundingQuotes(s); ASSERT_EQ("  abd ", s);

  s = "  \"  abd \"  ";
  Toolbox::RemoveSurroundingQuotes(s); ASSERT_EQ("  \"  abd \"  ", s);

  s = Toolbox::StripSpaces(s);
  Toolbox::RemoveSurroundingQuotes(s); ASSERT_EQ("  abd ", s);

  s = "\"";
  Toolbox::RemoveSurroundingQuotes(s); ASSERT_EQ("", s);  

  s = "\"\"";
  Toolbox::RemoveSurroundingQuotes(s); ASSERT_EQ("", s);  

  s = "\"_\"";
  Toolbox::RemoveSurroundingQuotes(s); ASSERT_EQ("_", s);

  s = "\"\"\"";
  Toolbox::RemoveSurroundingQuotes(s); ASSERT_EQ("\"", s);
}

TEST(Toolbox, Case)
{
  std::string s = "CoU";
  std::string ss;

  Toolbox::ToUpperCase(ss, s);
  ASSERT_EQ("COU", ss);
  Toolbox::ToLowerCase(ss, s);
  ASSERT_EQ("cou", ss); 

  s = "CoU";
  Toolbox::ToUpperCase(s);
  ASSERT_EQ("COU", s);

  s = "CoU";
  Toolbox::ToLowerCase(s);
  ASSERT_EQ("cou", s);
}


TEST(Logger, Basic)
{
  LOG(INFO) << "I say hello";
}

TEST(Toolbox, ConvertFromLatin1)
{
  // This is a Latin-1 test string
  const unsigned char data[10] = { 0xe0, 0xe9, 0xea, 0xe7, 0x26, 0xc6, 0x61, 0x62, 0x63, 0x00 };
  
  std::string s((char*) &data[0], 10);
  ASSERT_EQ("&abc", Toolbox::ConvertToAscii(s));

  // Open in Emacs, then save with UTF-8 encoding, then "hexdump -C"
  std::string utf8 = Toolbox::ConvertToUtf8(s, Encoding_Latin1, false);
  ASSERT_EQ(15u, utf8.size());
  ASSERT_EQ(0xc3, static_cast<unsigned char>(utf8[0]));
  ASSERT_EQ(0xa0, static_cast<unsigned char>(utf8[1]));
  ASSERT_EQ(0xc3, static_cast<unsigned char>(utf8[2]));
  ASSERT_EQ(0xa9, static_cast<unsigned char>(utf8[3]));
  ASSERT_EQ(0xc3, static_cast<unsigned char>(utf8[4]));
  ASSERT_EQ(0xaa, static_cast<unsigned char>(utf8[5]));
  ASSERT_EQ(0xc3, static_cast<unsigned char>(utf8[6]));
  ASSERT_EQ(0xa7, static_cast<unsigned char>(utf8[7]));
  ASSERT_EQ(0x26, static_cast<unsigned char>(utf8[8]));
  ASSERT_EQ(0xc3, static_cast<unsigned char>(utf8[9]));
  ASSERT_EQ(0x86, static_cast<unsigned char>(utf8[10]));
  ASSERT_EQ(0x61, static_cast<unsigned char>(utf8[11]));
  ASSERT_EQ(0x62, static_cast<unsigned char>(utf8[12]));
  ASSERT_EQ(0x63, static_cast<unsigned char>(utf8[13]));
  ASSERT_EQ(0x00, static_cast<unsigned char>(utf8[14]));  // Null-terminated string
}


TEST(Toolbox, FixUtf8)
{
  // This is a Latin-1 test string: "crane" with a circumflex accent
  const unsigned char latin1[] = { 0x63, 0x72, 0xe2, 0x6e, 0x65 };

  std::string s((char*) &latin1[0], sizeof(latin1) / sizeof(char));

  ASSERT_EQ(s, Toolbox::ConvertFromUtf8(Toolbox::ConvertToUtf8(s, Encoding_Latin1, false), Encoding_Latin1));
  ASSERT_EQ("cre", Toolbox::ConvertToUtf8(s, Encoding_Utf8, false));
}


static int32_t GetUnicode(const uint8_t* data,
                          size_t size,
                          size_t expectedLength)
{
  std::string s((char*) &data[0], size);
  uint32_t unicode;
  size_t length;
  Toolbox::Utf8ToUnicodeCharacter(unicode, length, s, 0);
  if (length != expectedLength)
  {
    return -1;  // Error case
  }
  else
  {
    return unicode;
  }
}


TEST(Toolbox, Utf8ToUnicode)
{
  // https://en.wikipedia.org/wiki/UTF-8
  
  ASSERT_EQ(1u, sizeof(char));
  ASSERT_EQ(1u, sizeof(uint8_t));
  
  {
    const uint8_t data[] = { 0x24 };
    ASSERT_EQ(0x24, GetUnicode(data, 1, 1));
    ASSERT_THROW(GetUnicode(data, 0, 1), OrthancException);
  }
  
  {
    const uint8_t data[] = { 0xc2, 0xa2 };
    ASSERT_EQ(0xa2, GetUnicode(data, 2, 2));
    ASSERT_THROW(GetUnicode(data, 1, 2), OrthancException);
  }
  
  {
    const uint8_t data[] = { 0xe0, 0xa4, 0xb9 };
    ASSERT_EQ(0x0939, GetUnicode(data, 3, 3));
    ASSERT_THROW(GetUnicode(data, 2, 3), OrthancException);
  }
  
  {
    const uint8_t data[] = { 0xe2, 0x82, 0xac };
    ASSERT_EQ(0x20ac, GetUnicode(data, 3, 3));
    ASSERT_THROW(GetUnicode(data, 2, 3), OrthancException);
  }
  
  {
    const uint8_t data[] = { 0xf0, 0x90, 0x8d, 0x88 };
    ASSERT_EQ(0x010348, GetUnicode(data, 4, 4));
    ASSERT_THROW(GetUnicode(data, 3, 4), OrthancException);
  }
  
  {
    const uint8_t data[] = { 0xe0 };
    ASSERT_THROW(GetUnicode(data, 1, 1), OrthancException);
  }
}


TEST(Toolbox, UrlDecode)
{
  std::string s;

  s = "Hello%20World";
  Toolbox::UrlDecode(s);
  ASSERT_EQ("Hello World", s);

  s = "%21%23%24%26%27%28%29%2A%2B%2c%2f%3A%3b%3d%3f%40%5B%5D%90%ff";
  Toolbox::UrlDecode(s);
  std::string ss = "!#$&'()*+,/:;=?@[]"; 
  ss.push_back((char) 144); 
  ss.push_back((char) 255);
  ASSERT_EQ(ss, s);

  s = "(2000%2C00A4)+Other";
  Toolbox::UrlDecode(s);
  ASSERT_EQ("(2000,00A4) Other", s);
}


TEST(Toolbox, IsAsciiString)
{
  std::string s = "Hello 12 /";
  ASSERT_EQ(10u, s.size());
  ASSERT_TRUE(Toolbox::IsAsciiString(s));
  ASSERT_TRUE(Toolbox::IsAsciiString(s.c_str(), 10));
  ASSERT_FALSE(Toolbox::IsAsciiString(s.c_str(), 11));  // Taking the trailing hidden '\0'

  s[2] = '\0';
  ASSERT_EQ(10u, s.size());
  ASSERT_FALSE(Toolbox::IsAsciiString(s));

  ASSERT_TRUE(Toolbox::IsAsciiString("Hello\nworld"));
  ASSERT_FALSE(Toolbox::IsAsciiString("Hello\rworld"));

  ASSERT_EQ("Hello\nworld", Toolbox::ConvertToAscii("Hello\nworld"));
  ASSERT_EQ("Helloworld", Toolbox::ConvertToAscii("Hello\r\tworld"));
}


#if defined(__linux__)
TEST(Toolbox, AbsoluteDirectory)
{
  ASSERT_EQ("/tmp/hello", SystemToolbox::InterpretRelativePath("/tmp", "hello"));
  ASSERT_EQ("/tmp", SystemToolbox::InterpretRelativePath("/tmp", "/tmp"));
}
#endif


#if ORTHANC_SANDBOXED != 1
TEST(Toolbox, WriteFile)
{
  std::string path;

  {
    TemporaryFile tmp;
    path = tmp.GetPath();

    std::string s;
    s.append("Hello");
    s.push_back('\0');
    s.append("World");
    ASSERT_EQ(11u, s.size());

    SystemToolbox::WriteFile(s, path.c_str());

    std::string t;
    SystemToolbox::ReadFile(t, path.c_str());

    ASSERT_EQ(11u, t.size());
    ASSERT_EQ(0, t[5]);
    ASSERT_EQ(0, memcmp(s.c_str(), t.c_str(), s.size()));

    std::string h;
    ASSERT_EQ(true, SystemToolbox::ReadHeader(h, path.c_str(), 1));
    ASSERT_EQ(1u, h.size());
    ASSERT_EQ('H', h[0]);
    ASSERT_TRUE(SystemToolbox::ReadHeader(h, path.c_str(), 0));
    ASSERT_EQ(0u, h.size());
    ASSERT_FALSE(SystemToolbox::ReadHeader(h, path.c_str(), 32));
    ASSERT_EQ(11u, h.size());
    ASSERT_EQ(0, memcmp(s.c_str(), h.c_str(), s.size()));
  }

  std::string u;
  ASSERT_THROW(SystemToolbox::ReadFile(u, path.c_str()), OrthancException);

  {
    TemporaryFile tmp;
    std::string s = "Hello";
    SystemToolbox::WriteFile(s, tmp.GetPath(), true /* call fsync() */);
    std::string t;
    SystemToolbox::ReadFile(t, tmp.GetPath());
    ASSERT_EQ(s, t);
  }
}
#endif


#if ORTHANC_SANDBOXED != 1
TEST(Toolbox, FileBuffer)
{
  FileBuffer f;
  f.Append("a", 1);
  f.Append("", 0);
  f.Append("bc", 2);

  std::string s;
  f.Read(s);
  ASSERT_EQ("abc", s);

  ASSERT_THROW(f.Append("d", 1), OrthancException);  // File is closed
}
#endif


TEST(Toolbox, Wildcard)
{
  ASSERT_EQ("abcd", Toolbox::WildcardToRegularExpression("abcd"));
  ASSERT_EQ("ab.*cd", Toolbox::WildcardToRegularExpression("ab*cd"));
  ASSERT_EQ("ab..cd", Toolbox::WildcardToRegularExpression("ab??cd"));
  ASSERT_EQ("a.*b.c.*d", Toolbox::WildcardToRegularExpression("a*b?c*d"));
  ASSERT_EQ("a\\{b\\]", Toolbox::WildcardToRegularExpression("a{b]"));
}


TEST(Toolbox, Tokenize)
{
  std::vector<std::string> t;
  
  Toolbox::TokenizeString(t, "", ','); 
  ASSERT_EQ(1u, t.size());
  ASSERT_EQ("", t[0]);
  
  Toolbox::TokenizeString(t, "abc", ','); 
  ASSERT_EQ(1u, t.size());
  ASSERT_EQ("abc", t[0]);
  
  Toolbox::TokenizeString(t, "ab,cd,ef,", ','); 
  ASSERT_EQ(4u, t.size());
  ASSERT_EQ("ab", t[0]);
  ASSERT_EQ("cd", t[1]);
  ASSERT_EQ("ef", t[2]);
  ASSERT_EQ("", t[3]);
}

TEST(Toolbox, SplitString)
{
  {
    std::set<std::string> result;
    Toolbox::SplitString(result, "", ';');
    ASSERT_EQ(0u, result.size());
  }

  {
    std::set<std::string> result;
    Toolbox::SplitString(result, "a", ';');
    ASSERT_EQ(1u, result.size());
    ASSERT_TRUE(result.end() != result.find("a"));
  }

  {
    std::set<std::string> result;
    Toolbox::SplitString(result, "a;b", ';');
    ASSERT_EQ(2u, result.size());
    ASSERT_TRUE(result.end() != result.find("a"));
    ASSERT_TRUE(result.end() != result.find("b"));
  }

  {
    std::set<std::string> result;
    Toolbox::SplitString(result, "a;b;", ';');
    ASSERT_EQ(2u, result.size());
    ASSERT_TRUE(result.end() != result.find("a"));
    ASSERT_TRUE(result.end() != result.find("b"));
  }

  {
    std::set<std::string> result;
    Toolbox::SplitString(result, "a;a", ';');
    ASSERT_EQ(1u, result.size());
    ASSERT_TRUE(result.end() != result.find("a"));
  }

  {
    std::vector<std::string> result;
    Toolbox::SplitString(result, "", ';');
    ASSERT_EQ(0u, result.size());
  }

  {
    std::vector<std::string> result;
    Toolbox::SplitString(result, "a", ';');
    ASSERT_EQ(1u, result.size());
    ASSERT_EQ("a", result[0]);
  }

  {
    std::vector<std::string> result;
    Toolbox::SplitString(result, "a;b", ';');
    ASSERT_EQ(2u, result.size());
    ASSERT_EQ("a", result[0]);
    ASSERT_EQ("b", result[1]);
  }

  {
    std::vector<std::string> result;
    Toolbox::SplitString(result, "a;b;", ';');
    ASSERT_EQ(2u, result.size());
    ASSERT_EQ("a", result[0]);
    ASSERT_EQ("b", result[1]);
  }

  {
    std::vector<std::string> result;
    Toolbox::TokenizeString(result, "a;a", ';');
    ASSERT_EQ(2u, result.size());
    ASSERT_EQ("a", result[0]);
    ASSERT_EQ("a", result[1]);
  }
}

TEST(Toolbox, Enumerations)
{
  ASSERT_EQ(Encoding_Utf8, StringToEncoding(EnumerationToString(Encoding_Utf8)));
  ASSERT_EQ(Encoding_Ascii, StringToEncoding(EnumerationToString(Encoding_Ascii)));
  ASSERT_EQ(Encoding_Latin1, StringToEncoding(EnumerationToString(Encoding_Latin1)));
  ASSERT_EQ(Encoding_Latin2, StringToEncoding(EnumerationToString(Encoding_Latin2)));
  ASSERT_EQ(Encoding_Latin3, StringToEncoding(EnumerationToString(Encoding_Latin3)));
  ASSERT_EQ(Encoding_Latin4, StringToEncoding(EnumerationToString(Encoding_Latin4)));
  ASSERT_EQ(Encoding_Latin5, StringToEncoding(EnumerationToString(Encoding_Latin5)));
  ASSERT_EQ(Encoding_Cyrillic, StringToEncoding(EnumerationToString(Encoding_Cyrillic)));
  ASSERT_EQ(Encoding_Arabic, StringToEncoding(EnumerationToString(Encoding_Arabic)));
  ASSERT_EQ(Encoding_Greek, StringToEncoding(EnumerationToString(Encoding_Greek)));
  ASSERT_EQ(Encoding_Hebrew, StringToEncoding(EnumerationToString(Encoding_Hebrew)));
  ASSERT_EQ(Encoding_Japanese, StringToEncoding(EnumerationToString(Encoding_Japanese)));
  ASSERT_EQ(Encoding_Chinese, StringToEncoding(EnumerationToString(Encoding_Chinese)));
  ASSERT_EQ(Encoding_Thai, StringToEncoding(EnumerationToString(Encoding_Thai)));
  ASSERT_EQ(Encoding_Korean, StringToEncoding(EnumerationToString(Encoding_Korean)));
  ASSERT_EQ(Encoding_JapaneseKanji, StringToEncoding(EnumerationToString(Encoding_JapaneseKanji)));
  ASSERT_EQ(Encoding_SimplifiedChinese, StringToEncoding(EnumerationToString(Encoding_SimplifiedChinese)));

  ASSERT_EQ(ResourceType_Patient, StringToResourceType(EnumerationToString(ResourceType_Patient)));
  ASSERT_EQ(ResourceType_Study, StringToResourceType(EnumerationToString(ResourceType_Study)));
  ASSERT_EQ(ResourceType_Series, StringToResourceType(EnumerationToString(ResourceType_Series)));
  ASSERT_EQ(ResourceType_Instance, StringToResourceType(EnumerationToString(ResourceType_Instance)));

  ASSERT_EQ(ImageFormat_Png, StringToImageFormat(EnumerationToString(ImageFormat_Png)));

  ASSERT_EQ(PhotometricInterpretation_ARGB, StringToPhotometricInterpretation(EnumerationToString(PhotometricInterpretation_ARGB)));
  ASSERT_EQ(PhotometricInterpretation_CMYK, StringToPhotometricInterpretation(EnumerationToString(PhotometricInterpretation_CMYK)));
  ASSERT_EQ(PhotometricInterpretation_HSV, StringToPhotometricInterpretation(EnumerationToString(PhotometricInterpretation_HSV)));
  ASSERT_EQ(PhotometricInterpretation_Monochrome1, StringToPhotometricInterpretation(EnumerationToString(PhotometricInterpretation_Monochrome1)));
  ASSERT_EQ(PhotometricInterpretation_Monochrome2, StringToPhotometricInterpretation(EnumerationToString(PhotometricInterpretation_Monochrome2)));
  ASSERT_EQ(PhotometricInterpretation_Palette, StringToPhotometricInterpretation(EnumerationToString(PhotometricInterpretation_Palette)));
  ASSERT_EQ(PhotometricInterpretation_RGB, StringToPhotometricInterpretation(EnumerationToString(PhotometricInterpretation_RGB)));
  ASSERT_EQ(PhotometricInterpretation_YBRFull, StringToPhotometricInterpretation(EnumerationToString(PhotometricInterpretation_YBRFull)));
  ASSERT_EQ(PhotometricInterpretation_YBRFull422, StringToPhotometricInterpretation(EnumerationToString(PhotometricInterpretation_YBRFull422)));
  ASSERT_EQ(PhotometricInterpretation_YBRPartial420, StringToPhotometricInterpretation(EnumerationToString(PhotometricInterpretation_YBRPartial420)));
  ASSERT_EQ(PhotometricInterpretation_YBRPartial422, StringToPhotometricInterpretation(EnumerationToString(PhotometricInterpretation_YBRPartial422)));
  ASSERT_EQ(PhotometricInterpretation_YBR_ICT, StringToPhotometricInterpretation(EnumerationToString(PhotometricInterpretation_YBR_ICT)));
  ASSERT_EQ(PhotometricInterpretation_YBR_RCT, StringToPhotometricInterpretation(EnumerationToString(PhotometricInterpretation_YBR_RCT)));

  ASSERT_STREQ("Unknown", EnumerationToString(PhotometricInterpretation_Unknown));
  ASSERT_THROW(StringToPhotometricInterpretation("Unknown"), OrthancException);

  ASSERT_EQ(DicomVersion_2008, StringToDicomVersion(EnumerationToString(DicomVersion_2008)));
  ASSERT_EQ(DicomVersion_2017c, StringToDicomVersion(EnumerationToString(DicomVersion_2017c)));
  ASSERT_EQ(DicomVersion_2021b, StringToDicomVersion(EnumerationToString(DicomVersion_2021b)));
  ASSERT_EQ(DicomVersion_2023b, StringToDicomVersion(EnumerationToString(DicomVersion_2023b)));

  for (int i = static_cast<int>(ValueRepresentation_ApplicationEntity);
       i < static_cast<int>(ValueRepresentation_NotSupported); i += 1)
  {
    ValueRepresentation vr = static_cast<ValueRepresentation>(i);
    ASSERT_EQ(vr, StringToValueRepresentation(EnumerationToString(vr), true));
  }

  ASSERT_THROW(StringToValueRepresentation("nope", true), OrthancException);

  ASSERT_EQ(JobState_Pending, StringToJobState(EnumerationToString(JobState_Pending)));
  ASSERT_EQ(JobState_Running, StringToJobState(EnumerationToString(JobState_Running)));
  ASSERT_EQ(JobState_Success, StringToJobState(EnumerationToString(JobState_Success)));
  ASSERT_EQ(JobState_Failure, StringToJobState(EnumerationToString(JobState_Failure)));
  ASSERT_EQ(JobState_Paused, StringToJobState(EnumerationToString(JobState_Paused)));
  ASSERT_EQ(JobState_Retry, StringToJobState(EnumerationToString(JobState_Retry)));
  ASSERT_THROW(StringToJobState("nope"), OrthancException);

  ASSERT_EQ(MimeType_Binary, StringToMimeType(EnumerationToString(MimeType_Binary)));
  ASSERT_EQ(MimeType_Css, StringToMimeType(EnumerationToString(MimeType_Css)));
  ASSERT_EQ(MimeType_Dicom, StringToMimeType(EnumerationToString(MimeType_Dicom)));
  ASSERT_EQ(MimeType_Gif, StringToMimeType(EnumerationToString(MimeType_Gif)));
  ASSERT_EQ(MimeType_Gzip, StringToMimeType(EnumerationToString(MimeType_Gzip)));
  ASSERT_EQ(MimeType_Html, StringToMimeType(EnumerationToString(MimeType_Html)));
  ASSERT_EQ(MimeType_JavaScript, StringToMimeType(EnumerationToString(MimeType_JavaScript)));
  ASSERT_EQ(MimeType_Jpeg, StringToMimeType(EnumerationToString(MimeType_Jpeg)));
  ASSERT_EQ(MimeType_Jpeg2000, StringToMimeType(EnumerationToString(MimeType_Jpeg2000)));
  ASSERT_EQ(MimeType_Json, StringToMimeType(EnumerationToString(MimeType_Json)));
  ASSERT_EQ(MimeType_NaCl, StringToMimeType(EnumerationToString(MimeType_NaCl)));
  ASSERT_EQ(MimeType_PNaCl, StringToMimeType(EnumerationToString(MimeType_PNaCl)));
  ASSERT_EQ(MimeType_Pam, StringToMimeType(EnumerationToString(MimeType_Pam)));
  ASSERT_EQ(MimeType_Pdf, StringToMimeType(EnumerationToString(MimeType_Pdf)));
  ASSERT_EQ(MimeType_PlainText, StringToMimeType(EnumerationToString(MimeType_PlainText)));
  ASSERT_EQ(MimeType_Png, StringToMimeType(EnumerationToString(MimeType_Png)));
  ASSERT_EQ(MimeType_Svg, StringToMimeType(EnumerationToString(MimeType_Svg)));
  ASSERT_EQ(MimeType_WebAssembly, StringToMimeType(EnumerationToString(MimeType_WebAssembly)));
  ASSERT_EQ(MimeType_Xml, StringToMimeType("application/xml"));
  ASSERT_EQ(MimeType_Xml, StringToMimeType("text/xml"));
  ASSERT_EQ(MimeType_Xml, StringToMimeType(EnumerationToString(MimeType_Xml)));
  ASSERT_EQ(MimeType_DicomWebJson, StringToMimeType(EnumerationToString(MimeType_DicomWebJson)));
  ASSERT_EQ(MimeType_DicomWebXml, StringToMimeType(EnumerationToString(MimeType_DicomWebXml)));
  ASSERT_EQ(MimeType_Mtl, StringToMimeType(EnumerationToString(MimeType_Mtl)));
  ASSERT_EQ(MimeType_Obj, StringToMimeType(EnumerationToString(MimeType_Obj)));
  ASSERT_EQ(MimeType_Stl, StringToMimeType(EnumerationToString(MimeType_Stl)));
  ASSERT_THROW(StringToMimeType("nope"), OrthancException);

  ASSERT_TRUE(IsResourceLevelAboveOrEqual(ResourceType_Patient, ResourceType_Patient));
  ASSERT_TRUE(IsResourceLevelAboveOrEqual(ResourceType_Patient, ResourceType_Study));
  ASSERT_TRUE(IsResourceLevelAboveOrEqual(ResourceType_Patient, ResourceType_Series));
  ASSERT_TRUE(IsResourceLevelAboveOrEqual(ResourceType_Patient, ResourceType_Instance));

  ASSERT_FALSE(IsResourceLevelAboveOrEqual(ResourceType_Study, ResourceType_Patient));
  ASSERT_TRUE(IsResourceLevelAboveOrEqual(ResourceType_Study, ResourceType_Study));
  ASSERT_TRUE(IsResourceLevelAboveOrEqual(ResourceType_Study, ResourceType_Series));
  ASSERT_TRUE(IsResourceLevelAboveOrEqual(ResourceType_Study, ResourceType_Instance));

  ASSERT_FALSE(IsResourceLevelAboveOrEqual(ResourceType_Series, ResourceType_Patient));
  ASSERT_FALSE(IsResourceLevelAboveOrEqual(ResourceType_Series, ResourceType_Study));
  ASSERT_TRUE(IsResourceLevelAboveOrEqual(ResourceType_Series, ResourceType_Series));
  ASSERT_TRUE(IsResourceLevelAboveOrEqual(ResourceType_Series, ResourceType_Instance));

  ASSERT_FALSE(IsResourceLevelAboveOrEqual(ResourceType_Instance, ResourceType_Patient));
  ASSERT_FALSE(IsResourceLevelAboveOrEqual(ResourceType_Instance, ResourceType_Study));
  ASSERT_FALSE(IsResourceLevelAboveOrEqual(ResourceType_Instance, ResourceType_Series));
  ASSERT_TRUE(IsResourceLevelAboveOrEqual(ResourceType_Instance, ResourceType_Instance));

  ASSERT_STREQ("Patients", GetResourceTypeText(ResourceType_Patient, true /* plural */, true /* upper case */));
  ASSERT_STREQ("patients", GetResourceTypeText(ResourceType_Patient, true, false));
  ASSERT_STREQ("Patient", GetResourceTypeText(ResourceType_Patient, false, true));
  ASSERT_STREQ("patient", GetResourceTypeText(ResourceType_Patient, false, false));
  ASSERT_STREQ("Studies", GetResourceTypeText(ResourceType_Study, true, true));
  ASSERT_STREQ("studies", GetResourceTypeText(ResourceType_Study, true, false));
  ASSERT_STREQ("Study", GetResourceTypeText(ResourceType_Study, false, true));
  ASSERT_STREQ("study", GetResourceTypeText(ResourceType_Study, false, false));
  ASSERT_STREQ("Series", GetResourceTypeText(ResourceType_Series, true, true));
  ASSERT_STREQ("series", GetResourceTypeText(ResourceType_Series, true, false));
  ASSERT_STREQ("Series", GetResourceTypeText(ResourceType_Series, false, true));
  ASSERT_STREQ("series", GetResourceTypeText(ResourceType_Series, false, false));
  ASSERT_STREQ("Instances", GetResourceTypeText(ResourceType_Instance, true, true));
  ASSERT_STREQ("instances", GetResourceTypeText(ResourceType_Instance, true, false));
  ASSERT_STREQ("Instance", GetResourceTypeText(ResourceType_Instance, false, true));
  ASSERT_STREQ("instance", GetResourceTypeText(ResourceType_Instance, false, false));

  DicomTransferSyntax ts;
  ASSERT_FALSE(LookupTransferSyntax(ts, "nope"));
  ASSERT_TRUE(LookupTransferSyntax(ts, "1.2.840.10008.1.2")); ASSERT_EQ(DicomTransferSyntax_LittleEndianImplicit, ts);
  ASSERT_STREQ("1.2.840.10008.1.2", GetTransferSyntaxUid(ts));
}


#if defined(__linux__) || defined(__OpenBSD__)
#include <endian.h>
#elif defined(__FreeBSD__)
#include <machine/endian.h>
#endif


TEST(Toolbox, Endianness)
{
  // Parts of this test come from Adam Conrad
  // http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=728822#5


  /**
   * Windows and OS X are assumed to always little-endian.
   **/
  
#if defined(_WIN32) || defined(__APPLE__)
  ASSERT_EQ(Endianness_Little, Toolbox::DetectEndianness());

  
  /**
   * FreeBSD.
   **/
  
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
#  if _BYTE_ORDER == _BIG_ENDIAN
   ASSERT_EQ(Endianness_Big, Toolbox::DetectEndianness());
#  else // _LITTLE_ENDIAN
   ASSERT_EQ(Endianness_Little, Toolbox::DetectEndianness());
#  endif


  /**
   * Linux.
   **/
  
#elif defined(__linux__) || defined(__FreeBSD_kernel__)

#if !defined(__BYTE_ORDER)
#  error Support your platform here
#endif

#  if __BYTE_ORDER == __BIG_ENDIAN
  ASSERT_EQ(Endianness_Big, Toolbox::DetectEndianness());
#  else // __LITTLE_ENDIAN
  ASSERT_EQ(Endianness_Little, Toolbox::DetectEndianness());
#  endif

  
  /**
   * WebAssembly is always little-endian.
   **/
  
#elif defined(__EMSCRIPTEN__)
  ASSERT_EQ(Endianness_Little, Toolbox::DetectEndianness());
#else
#  error Support your platform here
#endif
}


#include "../Sources/Endianness.h"

static void ASSERT_EQ16(uint16_t a, uint16_t b)
{
#ifdef __MINGW32__
  // This cast solves a linking problem with MinGW
  ASSERT_EQ(static_cast<unsigned int>(a), static_cast<unsigned int>(b));
#else
  ASSERT_EQ(a, b);
#endif
}

static void ASSERT_NE16(uint16_t a, uint16_t b)
{
#ifdef __MINGW32__
  // This cast solves a linking problem with MinGW
  ASSERT_NE(static_cast<unsigned int>(a), static_cast<unsigned int>(b));
#else
  ASSERT_NE(a, b);
#endif
}

static void ASSERT_EQ32(uint32_t a, uint32_t b)
{
#ifdef __MINGW32__
  // This cast solves a linking problem with MinGW
  ASSERT_EQ(static_cast<unsigned int>(a), static_cast<unsigned int>(b));
#else
  ASSERT_EQ(a, b);
#endif
}

static void ASSERT_NE32(uint32_t a, uint32_t b)
{
#ifdef __MINGW32__
  // This cast solves a linking problem with MinGW
  ASSERT_NE(static_cast<unsigned int>(a), static_cast<unsigned int>(b));
#else
  ASSERT_NE(a, b);
#endif
}

static void ASSERT_EQ64(uint64_t a, uint64_t b)
{
#ifdef __MINGW32__
  // This cast solves a linking problem with MinGW
  ASSERT_EQ(static_cast<unsigned int>(a), static_cast<unsigned int>(b));
#else
  ASSERT_EQ(a, b);
#endif
}

static void ASSERT_NE64(uint64_t a, uint64_t b)
{
#ifdef __MINGW32__
  // This cast solves a linking problem with MinGW
  ASSERT_NE(static_cast<unsigned long long>(a), static_cast<unsigned long long>(b));
#else
  ASSERT_NE(a, b);
#endif
}



TEST(Toolbox, EndiannessConversions16)
{
  Endianness e = Toolbox::DetectEndianness();

  for (unsigned int i = 0; i < 65536; i += 17)
  {
    uint16_t v = static_cast<uint16_t>(i);
    ASSERT_EQ16(v, be16toh(htobe16(v)));
    ASSERT_EQ16(v, le16toh(htole16(v)));

    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&v);
    if (bytes[0] != bytes[1])
    {
      ASSERT_NE16(v, le16toh(htobe16(v)));
      ASSERT_NE16(v, be16toh(htole16(v)));
    }
    else
    {
      ASSERT_EQ16(v, le16toh(htobe16(v)));
      ASSERT_EQ16(v, be16toh(htole16(v)));
    }

    switch (e)
    {
      case Endianness_Little:
        ASSERT_EQ16(v, htole16(v));
        if (bytes[0] != bytes[1])
        {
          ASSERT_NE16(v, htobe16(v));
        }
        else
        {
          ASSERT_EQ16(v, htobe16(v));
        }
        break;

      case Endianness_Big:
        ASSERT_EQ16(v, htobe16(v));
        if (bytes[0] != bytes[1])
        {
          ASSERT_NE16(v, htole16(v));
        }
        else
        {
          ASSERT_EQ16(v, htole16(v));
        }
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }
}


TEST(Toolbox, EndiannessConversions32)
{
  const uint32_t v = 0xff010203u;
  const uint32_t r = 0x030201ffu;
  ASSERT_EQ32(v, be32toh(htobe32(v)));
  ASSERT_EQ32(v, le32toh(htole32(v)));
  ASSERT_NE32(v, be32toh(htole32(v)));
  ASSERT_NE32(v, le32toh(htobe32(v)));

  switch (Toolbox::DetectEndianness())
  {
    case Endianness_Little:
      ASSERT_EQ32(r, htobe32(v));
      ASSERT_EQ32(v, htole32(v));
      ASSERT_EQ32(r, be32toh(v));
      ASSERT_EQ32(v, le32toh(v));
      break;

    case Endianness_Big:
      ASSERT_EQ32(v, htobe32(v));
      ASSERT_EQ32(r, htole32(v));
      ASSERT_EQ32(v, be32toh(v));
      ASSERT_EQ32(r, le32toh(v));
      break;

    default:
      throw OrthancException(ErrorCode_ParameterOutOfRange);
  }
}


TEST(Toolbox, EndiannessConversions64)
{
  const uint64_t v = 0xff01020304050607LL;
  const uint64_t r = 0x07060504030201ffLL;
  ASSERT_EQ64(v, be64toh(htobe64(v)));
  ASSERT_EQ64(v, le64toh(htole64(v)));
  ASSERT_NE64(v, be64toh(htole64(v)));
  ASSERT_NE64(v, le64toh(htobe64(v)));

  switch (Toolbox::DetectEndianness())
  {
    case Endianness_Little:
      ASSERT_EQ64(r, htobe64(v));
      ASSERT_EQ64(v, htole64(v));
      ASSERT_EQ64(r, be64toh(v));
      ASSERT_EQ64(v, le64toh(v));
      break;

    case Endianness_Big:
      ASSERT_EQ64(v, htobe64(v));
      ASSERT_EQ64(r, htole64(v));
      ASSERT_EQ64(v, be64toh(v));
      ASSERT_EQ64(r, le64toh(v));
      break;

    default:
      throw OrthancException(ErrorCode_ParameterOutOfRange);
  }
}


#if ORTHANC_SANDBOXED != 1
TEST(Toolbox, Now)
{
  LOG(WARNING) << "Local time: " << SystemToolbox::GetNowIsoString(false);
  LOG(WARNING) << "Universal time: " << SystemToolbox::GetNowIsoString(true);

  std::string date, time;
  SystemToolbox::GetNowDicom(date, time, false);
  LOG(WARNING) << "Local DICOM time: [" << date << "] [" << time << "]";

  SystemToolbox::GetNowDicom(date, time, true);
  LOG(WARNING) << "Universal DICOM time: [" << date << "] [" << time << "]";
}
#endif


#if ORTHANC_ENABLE_PUGIXML == 1
TEST(Toolbox, Xml)
{
  Json::Value a;
  a["hello"] = "world";
  a["42"] = 43;
  a["b"] = Json::arrayValue;
  a["b"].append("test");
  a["b"].append("test2");

  std::string s;
  Toolbox::JsonToXml(s, a);

  std::cout << s;
}
#endif


#if !defined(_WIN32) && (ORTHANC_SANDBOXED != 1)
TEST(Toolbox, ExecuteSystemCommand)
{
  std::vector<std::string> args(2);
  args[0] = "Hello";
  args[1] = "World";

  SystemToolbox::ExecuteSystemCommand("echo", args);
}
#endif


TEST(Toolbox, IsInteger)
{
  ASSERT_TRUE(Toolbox::IsInteger("00236"));
  ASSERT_TRUE(Toolbox::IsInteger("-0042"));
  ASSERT_TRUE(Toolbox::IsInteger("0"));
  ASSERT_TRUE(Toolbox::IsInteger("-0"));

  ASSERT_FALSE(Toolbox::IsInteger(""));
  ASSERT_FALSE(Toolbox::IsInteger("42a"));
  ASSERT_FALSE(Toolbox::IsInteger("42-"));
}


TEST(Toolbox, StartsWith)
{
  ASSERT_TRUE(Toolbox::StartsWith("hello world", ""));
  ASSERT_TRUE(Toolbox::StartsWith("hello world", "hello"));
  ASSERT_TRUE(Toolbox::StartsWith("hello world", "h"));
  ASSERT_FALSE(Toolbox::StartsWith("hello world", "H"));
  ASSERT_FALSE(Toolbox::StartsWith("h", "hello"));
  ASSERT_TRUE(Toolbox::StartsWith("h", "h"));
  ASSERT_FALSE(Toolbox::StartsWith("", "h"));
}


TEST(Toolbox, UriEncode)
{
  std::string s;

  // Unreserved characters must not be modified
  std::string t = "aAzZ09.-~_";
  Toolbox::UriEncode(s, t); 
  ASSERT_EQ(t, s);

  Toolbox::UriEncode(s, "!#$&'()*+,/:;=?@[]"); ASSERT_EQ("%21%23%24%26%27%28%29%2A%2B%2C/%3A%3B%3D%3F%40%5B%5D", s);  
  Toolbox::UriEncode(s, "%"); ASSERT_EQ("%25", s);

  // Encode characters from UTF-8. This is the test string from the
  // file "../Resources/EncodingTests.py"
  Toolbox::UriEncode(s, "\x54\x65\x73\x74\xc3\xa9\xc3\xa4\xc3\xb6\xc3\xb2\xd0\x94\xce\x98\xc4\x9d\xd7\x93\xd8\xb5\xc4\xb7\xd1\x9b\xe0\xb9\x9b\xef\xbe\x88\xc4\xb0"); 
  ASSERT_EQ("Test%C3%A9%C3%A4%C3%B6%C3%B2%D0%94%CE%98%C4%9D%D7%93%D8%B5%C4%B7%D1%9B%E0%B9%9B%EF%BE%88%C4%B0", s);
}


TEST(Toolbox, AccessJson)
{
  Json::Value v = Json::arrayValue;
  ASSERT_EQ("nope", Toolbox::GetJsonStringField(v, "hello", "nope"));

  v = Json::objectValue;
  ASSERT_EQ("nope", Toolbox::GetJsonStringField(v, "hello", "nope"));
  ASSERT_EQ(-10, Toolbox::GetJsonIntegerField(v, "hello", -10));
  ASSERT_EQ(10u, Toolbox::GetJsonUnsignedIntegerField(v, "hello", 10));
  ASSERT_TRUE(Toolbox::GetJsonBooleanField(v, "hello", true));

  v["hello"] = "world";
  ASSERT_EQ("world", Toolbox::GetJsonStringField(v, "hello", "nope"));
  ASSERT_THROW(Toolbox::GetJsonIntegerField(v, "hello", -10), OrthancException);
  ASSERT_THROW(Toolbox::GetJsonUnsignedIntegerField(v, "hello", 10), OrthancException);
  ASSERT_THROW(Toolbox::GetJsonBooleanField(v, "hello", true), OrthancException);

  v["hello"] = -42;
  ASSERT_THROW(Toolbox::GetJsonStringField(v, "hello", "nope"), OrthancException);
  ASSERT_EQ(-42, Toolbox::GetJsonIntegerField(v, "hello", -10));
  ASSERT_THROW(Toolbox::GetJsonUnsignedIntegerField(v, "hello", 10), OrthancException);
  ASSERT_THROW(Toolbox::GetJsonBooleanField(v, "hello", true), OrthancException);

  v["hello"] = 42;
  ASSERT_THROW(Toolbox::GetJsonStringField(v, "hello", "nope"), OrthancException);
  ASSERT_EQ(42, Toolbox::GetJsonIntegerField(v, "hello", -10));
  ASSERT_EQ(42u, Toolbox::GetJsonUnsignedIntegerField(v, "hello", 10));
  ASSERT_THROW(Toolbox::GetJsonBooleanField(v, "hello", true), OrthancException);

  v["hello"] = false;
  ASSERT_THROW(Toolbox::GetJsonStringField(v, "hello", "nope"), OrthancException);
  ASSERT_THROW(Toolbox::GetJsonIntegerField(v, "hello", -10), OrthancException);
  ASSERT_THROW(Toolbox::GetJsonUnsignedIntegerField(v, "hello", 10), OrthancException);
  ASSERT_FALSE(Toolbox::GetJsonBooleanField(v, "hello", true));
}


TEST(Toolbox, LinesIterator)
{
  std::string s;

  {
    std::string content;
    Toolbox::LinesIterator it(content);
    ASSERT_FALSE(it.GetLine(s));
  }

  {
    std::string content = "\n\r";
    Toolbox::LinesIterator it(content);
    ASSERT_TRUE(it.GetLine(s)); it.Next(); ASSERT_EQ("", s);
    ASSERT_FALSE(it.GetLine(s));
  }
  
  {
    std::string content = "\n Hello \n\nWorld\n\n";
    Toolbox::LinesIterator it(content);
    ASSERT_TRUE(it.GetLine(s)); it.Next(); ASSERT_EQ("", s);
    ASSERT_TRUE(it.GetLine(s)); it.Next(); ASSERT_EQ(" Hello ", s);
    ASSERT_TRUE(it.GetLine(s)); it.Next(); ASSERT_EQ("", s);
    ASSERT_TRUE(it.GetLine(s)); it.Next(); ASSERT_EQ("World", s);
    ASSERT_TRUE(it.GetLine(s)); it.Next(); ASSERT_EQ("", s);
    ASSERT_FALSE(it.GetLine(s)); it.Next();
    ASSERT_FALSE(it.GetLine(s));
  }

  {
    std::string content = "\r Hello \r\rWorld\r\r";
    Toolbox::LinesIterator it(content);
    ASSERT_TRUE(it.GetLine(s)); it.Next(); ASSERT_EQ("", s);
    ASSERT_TRUE(it.GetLine(s)); it.Next(); ASSERT_EQ(" Hello ", s);
    ASSERT_TRUE(it.GetLine(s)); it.Next(); ASSERT_EQ("", s);
    ASSERT_TRUE(it.GetLine(s)); it.Next(); ASSERT_EQ("World", s);
    ASSERT_TRUE(it.GetLine(s)); it.Next(); ASSERT_EQ("", s);
    ASSERT_FALSE(it.GetLine(s)); it.Next();
    ASSERT_FALSE(it.GetLine(s));
  }

  {
    std::string content = "\n\r Hello \n\r\n\rWorld\n\r\n\r";
    Toolbox::LinesIterator it(content);
    ASSERT_TRUE(it.GetLine(s)); it.Next(); ASSERT_EQ("", s);
    ASSERT_TRUE(it.GetLine(s)); it.Next(); ASSERT_EQ(" Hello ", s);
    ASSERT_TRUE(it.GetLine(s)); it.Next(); ASSERT_EQ("", s);
    ASSERT_TRUE(it.GetLine(s)); it.Next(); ASSERT_EQ("World", s);
    ASSERT_TRUE(it.GetLine(s)); it.Next(); ASSERT_EQ("", s);
    ASSERT_FALSE(it.GetLine(s)); it.Next();
    ASSERT_FALSE(it.GetLine(s));
  }

  {
    std::string content = "\r\n Hello \r\n\r\nWorld\r\n\r\n";
    Toolbox::LinesIterator it(content);
    ASSERT_TRUE(it.GetLine(s)); it.Next(); ASSERT_EQ("", s);
    ASSERT_TRUE(it.GetLine(s)); it.Next(); ASSERT_EQ(" Hello ", s);
    ASSERT_TRUE(it.GetLine(s)); it.Next(); ASSERT_EQ("", s);
    ASSERT_TRUE(it.GetLine(s)); it.Next(); ASSERT_EQ("World", s);
    ASSERT_TRUE(it.GetLine(s)); it.Next(); ASSERT_EQ("", s);
    ASSERT_FALSE(it.GetLine(s)); it.Next();
    ASSERT_FALSE(it.GetLine(s));
  }
}


#if ORTHANC_SANDBOXED != 1
TEST(Toolbox, SubstituteVariables)
{
  std::map<std::string, std::string> env;
  env["NOPE"] = "nope";
  env["WORLD"] = "world";

  ASSERT_EQ("Hello world\r\nWorld \r\nDone world\r\n",
            Toolbox::SubstituteVariables(
              "Hello ${WORLD}\r\nWorld ${HELLO}\r\nDone ${WORLD}\r\n",
              env));

  ASSERT_EQ("world A a B world C 'c' D {\"a\":\"b\"} E ",
            Toolbox::SubstituteVariables(
              "${WORLD} A ${WORLD2:-a} B ${WORLD:-b} C ${WORLD2:-\"'c'\"} D ${WORLD2:-'{\"a\":\"b\"}'} E ${WORLD2:-}",
              env));
  
  SystemToolbox::GetEnvironmentVariables(env);
  ASSERT_TRUE(env.find("NOPE") == env.end());

  // The "PATH" environment variable should always be available on
  // machines running the unit tests
  ASSERT_TRUE(env.find("PATH") != env.end() /* Case used by UNIX */ ||
              env.find("Path") != env.end() /* Case used by Windows */);

  env["PATH"] = "hello";
  ASSERT_EQ("AhelloB",
            Toolbox::SubstituteVariables("A${PATH}B", env));
}
#endif


#if ORTHANC_SANDBOXED != 1
TEST(MetricsRegistry, Basic)
{
  {
    MetricsRegistry m;
    m.SetEnabled(false);
    m.SetIntegerValue("hello.world", 42);
    
    std::string s;
    m.ExportPrometheusText(s);
    ASSERT_TRUE(s.empty());
  }

  {
    MetricsRegistry m;
    m.Register("hello.world", MetricsUpdatePolicy_Directly, MetricsDataType_Integer);
    
    std::string s;
    m.ExportPrometheusText(s);
    ASSERT_TRUE(s.empty());
  }

  {
    MetricsRegistry m;
    m.SetIntegerValue("hello.world", -42);
    ASSERT_EQ(MetricsUpdatePolicy_Directly, m.GetUpdatePolicy("hello.world"));
    ASSERT_THROW(m.GetUpdatePolicy("nope"), OrthancException);
    
    std::string s;
    m.ExportPrometheusText(s);

    std::vector<std::string> t;
    Toolbox::TokenizeString(t, s, '\n');
    ASSERT_EQ(2u, t.size());
    ASSERT_EQ("hello.world -42 ", t[0].substr(0, 16));
    ASSERT_TRUE(t[1].empty());
  }

  {
    MetricsRegistry m;
    m.Register("hello.max", MetricsUpdatePolicy_MaxOver10Seconds, MetricsDataType_Integer);
    m.SetIntegerValue("hello.max", 10);
    m.SetIntegerValue("hello.max", 20);
    m.SetIntegerValue("hello.max", -10);
    m.SetIntegerValue("hello.max", 5);

    m.Register("hello.min", MetricsUpdatePolicy_MinOver10Seconds, MetricsDataType_Integer);
    m.SetIntegerValue("hello.min", 10);
    m.SetIntegerValue("hello.min", 20);
    m.SetIntegerValue("hello.min", -10);
    m.SetIntegerValue("hello.min", 5);
    
    m.Register("hello.directly", MetricsUpdatePolicy_Directly, MetricsDataType_Integer);
    m.SetIntegerValue("hello.directly", 10);
    m.SetIntegerValue("hello.directly", 20);
    m.SetIntegerValue("hello.directly", -10);
    m.SetIntegerValue("hello.directly", 5);
    
    ASSERT_EQ(MetricsUpdatePolicy_MaxOver10Seconds, m.GetUpdatePolicy("hello.max"));
    ASSERT_EQ(MetricsUpdatePolicy_MinOver10Seconds, m.GetUpdatePolicy("hello.min"));
    ASSERT_EQ(MetricsUpdatePolicy_Directly, m.GetUpdatePolicy("hello.directly"));

    std::string s;
    m.ExportPrometheusText(s);

    std::vector<std::string> t;
    Toolbox::TokenizeString(t, s, '\n');
    ASSERT_EQ(4u, t.size());
    ASSERT_TRUE(t[3].empty());

    std::map<std::string, std::string> u;
    for (size_t i = 0; i < t.size() - 1; i++)
    {
      std::vector<std::string> v;
      Toolbox::TokenizeString(v, t[i], ' ');
      u[v[0]] = v[1];
    }

    ASSERT_EQ("20", u["hello.max"]);
    ASSERT_EQ("-10", u["hello.min"]);
    ASSERT_EQ("5", u["hello.directly"]);
  }

  {
    MetricsRegistry m;

    m.SetIntegerValue("a", 10);
    m.SetIntegerValue("b", 10, MetricsUpdatePolicy_MinOver10Seconds);

    m.Register("c", MetricsUpdatePolicy_MaxOver10Seconds, MetricsDataType_Integer);
    m.SetIntegerValue("c", 10, MetricsUpdatePolicy_MinOver10Seconds);

    m.Register("d", MetricsUpdatePolicy_MaxOver10Seconds, MetricsDataType_Integer);
    ASSERT_THROW(m.Register("d", MetricsUpdatePolicy_Directly, MetricsDataType_Integer), OrthancException);

    ASSERT_EQ(MetricsUpdatePolicy_Directly, m.GetUpdatePolicy("a"));
    ASSERT_EQ(MetricsUpdatePolicy_MinOver10Seconds, m.GetUpdatePolicy("b"));
    ASSERT_EQ(MetricsUpdatePolicy_MaxOver10Seconds, m.GetUpdatePolicy("c"));
    ASSERT_EQ(MetricsUpdatePolicy_MaxOver10Seconds, m.GetUpdatePolicy("d"));
  }

  {
    MetricsRegistry m;

    {
      MetricsRegistry::Timer t1(m, "a");
      MetricsRegistry::Timer t2(m, "b", MetricsUpdatePolicy_MinOver10Seconds);
    }

    ASSERT_EQ(MetricsUpdatePolicy_MaxOver10Seconds, m.GetUpdatePolicy("a"));
    ASSERT_EQ(MetricsUpdatePolicy_MinOver10Seconds, m.GetUpdatePolicy("b"));
  }

  {
    MetricsRegistry m;
    m.Register("c", MetricsUpdatePolicy_MaxOver10Seconds, MetricsDataType_Integer);
    m.SetFloatValue("c", 100, MetricsUpdatePolicy_MinOver10Seconds);

    ASSERT_EQ(MetricsUpdatePolicy_MaxOver10Seconds, m.GetUpdatePolicy("c"));
    ASSERT_EQ(MetricsDataType_Integer, m.GetDataType("c"));
  }

  {
    MetricsRegistry m;
    m.Register("c", MetricsUpdatePolicy_MaxOver10Seconds, MetricsDataType_Float);
    m.SetIntegerValue("c", 100, MetricsUpdatePolicy_MinOver10Seconds);

    ASSERT_EQ(MetricsUpdatePolicy_MaxOver10Seconds, m.GetUpdatePolicy("c"));
    ASSERT_EQ(MetricsDataType_Float, m.GetDataType("c"));
  }

  {
    MetricsRegistry m;
    m.SetIntegerValue("c", 100, MetricsUpdatePolicy_MinOver10Seconds);
    m.SetFloatValue("c", 101, MetricsUpdatePolicy_MaxOver10Seconds);

    ASSERT_EQ(MetricsUpdatePolicy_MinOver10Seconds, m.GetUpdatePolicy("c"));
    ASSERT_EQ(MetricsDataType_Integer, m.GetDataType("c"));
  }

  {
    MetricsRegistry m;
    m.SetIntegerValue("c", 100);
    m.SetFloatValue("c", 101, MetricsUpdatePolicy_MaxOver10Seconds);

    ASSERT_EQ(MetricsUpdatePolicy_Directly, m.GetUpdatePolicy("c"));
    ASSERT_EQ(MetricsDataType_Integer, m.GetDataType("c"));
  }
}
#endif


#if ORTHANC_SANDBOXED != 1
TEST(Toolbox, ReadFileRange)
{
  TemporaryFile tmp;
  std::string s;

  tmp.Write("");
  tmp.Read(s);                     ASSERT_TRUE(s.empty());
  tmp.ReadRange(s, 0, 0, true);    ASSERT_TRUE(s.empty());
  tmp.ReadRange(s, 0, 10, false);  ASSERT_TRUE(s.empty());
  
  ASSERT_THROW(tmp.ReadRange(s, 0, 1, true), OrthancException);
  
  tmp.Write("Hello");
  tmp.Read(s);                     ASSERT_EQ("Hello", s);
  tmp.ReadRange(s, 0, 5, true);    ASSERT_EQ("Hello", s);
  tmp.ReadRange(s, 0, 1, true);    ASSERT_EQ("H", s);
  tmp.ReadRange(s, 1, 2, true);    ASSERT_EQ("e", s);
  tmp.ReadRange(s, 2, 3, true);    ASSERT_EQ("l", s);
  tmp.ReadRange(s, 3, 4, true);    ASSERT_EQ("l", s);
  tmp.ReadRange(s, 4, 5, true);    ASSERT_EQ("o", s);
  tmp.ReadRange(s, 2, 5, true);    ASSERT_EQ("llo", s);
  tmp.ReadRange(s, 2, 50, false);  ASSERT_EQ("llo", s);
  tmp.ReadRange(s, 2, 2, false);   ASSERT_TRUE(s.empty());
  tmp.ReadRange(s, 10, 50, false); ASSERT_TRUE(s.empty());
  
  ASSERT_THROW(tmp.ReadRange(s, 5, 10, true), OrthancException);
  ASSERT_THROW(tmp.ReadRange(s, 10, 50, true), OrthancException);
  ASSERT_THROW(tmp.ReadRange(s, 50, 10, true), OrthancException);
  ASSERT_THROW(tmp.ReadRange(s, 2, 1, true), OrthancException);
}
#endif


#if ORTHANC_SANDBOXED != 1
TEST(Toolbox, GetMacAddressess)
{
  std::set<std::string> mac;
  SystemToolbox::GetMacAddresses(mac);

  for (std::set<std::string>::const_iterator it = mac.begin(); it != mac.end(); ++it)
  {
    printf("MAC address: [%s]\n", it->c_str());
  }
}
#endif
