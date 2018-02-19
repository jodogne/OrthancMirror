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
#include "../Core/EnumerationDictionary.h"

#include "gtest/gtest.h"

#include <ctype.h>

#include "../Core/DicomFormat/DicomTag.h"
#include "../Core/HttpServer/HttpToolbox.h"
#include "../Core/Logging.h"
#include "../Core/OrthancException.h"
#include "../Core/TemporaryFile.h"
#include "../Core/Toolbox.h"
#include "../OrthancServer/OrthancInitialization.h"


using namespace Orthanc;


TEST(Uuid, Generation)
{
  for (int i = 0; i < 10; i++)
  {
    std::string s = SystemToolbox::GenerateUuid();
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
  IHttpHandler::GetArguments b;
  HttpToolbox::ParseGetArguments(b, "aaa=baaa&bb=a&aa=c");

  IHttpHandler::Arguments a;
  HttpToolbox::CompileGetArguments(a, b);

  ASSERT_EQ(3u, a.size());
  ASSERT_EQ(a["aaa"], "baaa");
  ASSERT_EQ(a["bb"], "a");
  ASSERT_EQ(a["aa"], "c");
}

TEST(ParseGetArguments, BasicEmpty)
{
  IHttpHandler::GetArguments b;
  HttpToolbox::ParseGetArguments(b, "aaa&bb=aa&aa");

  IHttpHandler::Arguments a;
  HttpToolbox::CompileGetArguments(a, b);

  ASSERT_EQ(3u, a.size());
  ASSERT_EQ(a["aaa"], "");
  ASSERT_EQ(a["bb"], "aa");
  ASSERT_EQ(a["aa"], "");
}

TEST(ParseGetArguments, Single)
{
  IHttpHandler::GetArguments b;
  HttpToolbox::ParseGetArguments(b, "aaa=baaa");

  IHttpHandler::Arguments a;
  HttpToolbox::CompileGetArguments(a, b);

  ASSERT_EQ(1u, a.size());
  ASSERT_EQ(a["aaa"], "baaa");
}

TEST(ParseGetArguments, SingleEmpty)
{
  IHttpHandler::GetArguments b;
  HttpToolbox::ParseGetArguments(b, "aaa");

  IHttpHandler::Arguments a;
  HttpToolbox::CompileGetArguments(a, b);

  ASSERT_EQ(1u, a.size());
  ASSERT_EQ(a["aaa"], "");
}

TEST(ParseGetQuery, Test1)
{
  UriComponents uri;
  IHttpHandler::GetArguments b;
  HttpToolbox::ParseGetQuery(uri, b, "/instances/test/world?aaa=baaa&bb=a&aa=c");

  IHttpHandler::Arguments a;
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
  IHttpHandler::GetArguments b;
  HttpToolbox::ParseGetQuery(uri, b, "/instances/test/world");

  IHttpHandler::Arguments a;
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

TEST(Uri, AutodetectMimeType)
{
  ASSERT_EQ("", Toolbox::AutodetectMimeType("../NOTES"));
  ASSERT_EQ("", Toolbox::AutodetectMimeType(""));
  ASSERT_EQ("", Toolbox::AutodetectMimeType("/"));
  ASSERT_EQ("", Toolbox::AutodetectMimeType("a/a"));

  ASSERT_EQ("text/plain", Toolbox::AutodetectMimeType("../NOTES.txt"));
  ASSERT_EQ("text/plain", Toolbox::AutodetectMimeType("../coucou.xml/NOTES.txt"));
  ASSERT_EQ("text/xml", Toolbox::AutodetectMimeType("../.xml"));

  ASSERT_EQ("application/javascript", Toolbox::AutodetectMimeType("NOTES.js"));
  ASSERT_EQ("application/json", Toolbox::AutodetectMimeType("NOTES.json"));
  ASSERT_EQ("application/pdf", Toolbox::AutodetectMimeType("NOTES.pdf"));
  ASSERT_EQ("text/css", Toolbox::AutodetectMimeType("NOTES.css"));
  ASSERT_EQ("text/html", Toolbox::AutodetectMimeType("NOTES.html"));
  ASSERT_EQ("text/plain", Toolbox::AutodetectMimeType("NOTES.txt"));
  ASSERT_EQ("text/xml", Toolbox::AutodetectMimeType("NOTES.xml"));
  ASSERT_EQ("image/gif", Toolbox::AutodetectMimeType("NOTES.gif"));
  ASSERT_EQ("image/jpeg", Toolbox::AutodetectMimeType("NOTES.jpg"));
  ASSERT_EQ("image/jpeg", Toolbox::AutodetectMimeType("NOTES.jpeg"));
  ASSERT_EQ("image/png", Toolbox::AutodetectMimeType("NOTES.png"));
}

TEST(Toolbox, ComputeMD5)
{
  std::string s;

  // # echo -n "Hello" | md5sum

  Toolbox::ComputeMD5(s, "Hello");
  ASSERT_EQ("8b1a9953c4611296a827abf8c47804d7", s);
  Toolbox::ComputeMD5(s, "");
  ASSERT_EQ("d41d8cd98f00b204e9800998ecf8427e", s);
}

TEST(Toolbox, ComputeSHA1)
{
  std::string s;
  
  Toolbox::ComputeSHA1(s, "The quick brown fox jumps over the lazy dog");
  ASSERT_EQ("2fd4e1c6-7a2d28fc-ed849ee1-bb76e739-1b93eb12", s);
  Toolbox::ComputeSHA1(s, "");
  ASSERT_EQ("da39a3ee-5e6b4b0d-3255bfef-95601890-afd80709", s);
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

TEST(Toolbox, PathToExecutable)
{
  printf("[%s]\n", SystemToolbox::GetPathToExecutable().c_str());
  printf("[%s]\n", SystemToolbox::GetDirectoryOfExecutable().c_str());
}

TEST(Toolbox, StripSpaces)
{
  ASSERT_EQ("", Toolbox::StripSpaces("       \t  \r   \n  "));
  ASSERT_EQ("coucou", Toolbox::StripSpaces("    coucou   \t  \r   \n  "));
  ASSERT_EQ("cou   cou", Toolbox::StripSpaces("    cou   cou    \n  "));
  ASSERT_EQ("c", Toolbox::StripSpaces("    \n\t c\r    \n  "));
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
  std::string utf8 = Toolbox::ConvertToUtf8(s, Encoding_Latin1);
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
}


#if defined(__linux__)
TEST(OrthancInitialization, AbsoluteDirectory)
{
  ASSERT_EQ("/tmp/hello", Configuration::InterpretRelativePath("/tmp", "hello"));
  ASSERT_EQ("/tmp", Configuration::InterpretRelativePath("/tmp", "/tmp"));
}
#endif



#include "../OrthancServer/ServerEnumerations.h"

TEST(EnumerationDictionary, Simple)
{
  Toolbox::EnumerationDictionary<MetadataType>  d;

  ASSERT_THROW(d.Translate("ReceptionDate"), OrthancException);
  ASSERT_EQ(MetadataType_ModifiedFrom, d.Translate("5"));
  ASSERT_EQ(256, d.Translate("256"));

  d.Add(MetadataType_Instance_ReceptionDate, "ReceptionDate");

  ASSERT_EQ(MetadataType_Instance_ReceptionDate, d.Translate("ReceptionDate"));
  ASSERT_EQ(MetadataType_Instance_ReceptionDate, d.Translate("2"));
  ASSERT_EQ("ReceptionDate", d.Translate(MetadataType_Instance_ReceptionDate));

  ASSERT_THROW(d.Add(MetadataType_Instance_ReceptionDate, "Hello"), OrthancException);
  ASSERT_THROW(d.Add(MetadataType_ModifiedFrom, "ReceptionDate"), OrthancException); // already used
  ASSERT_THROW(d.Add(MetadataType_ModifiedFrom, "1024"), OrthancException); // cannot register numbers
  d.Add(MetadataType_ModifiedFrom, "ModifiedFrom");  // ok
}


TEST(EnumerationDictionary, ServerEnumerations)
{
  ASSERT_STREQ("Patient", EnumerationToString(ResourceType_Patient));
  ASSERT_STREQ("Study", EnumerationToString(ResourceType_Study));
  ASSERT_STREQ("Series", EnumerationToString(ResourceType_Series));
  ASSERT_STREQ("Instance", EnumerationToString(ResourceType_Instance));

  ASSERT_STREQ("ModifiedSeries", EnumerationToString(ChangeType_ModifiedSeries));

  ASSERT_STREQ("Failure", EnumerationToString(StoreStatus_Failure));
  ASSERT_STREQ("Success", EnumerationToString(StoreStatus_Success));

  ASSERT_STREQ("CompletedSeries", EnumerationToString(ChangeType_CompletedSeries));

  ASSERT_EQ("IndexInSeries", EnumerationToString(MetadataType_Instance_IndexInSeries));
  ASSERT_EQ("LastUpdate", EnumerationToString(MetadataType_LastUpdate));

  ASSERT_EQ(ResourceType_Patient, StringToResourceType("PATienT"));
  ASSERT_EQ(ResourceType_Study, StringToResourceType("STudy"));
  ASSERT_EQ(ResourceType_Series, StringToResourceType("SeRiEs"));
  ASSERT_EQ(ResourceType_Instance, StringToResourceType("INStance"));
  ASSERT_EQ(ResourceType_Instance, StringToResourceType("IMagE"));
  ASSERT_THROW(StringToResourceType("heLLo"), OrthancException);

  ASSERT_EQ(2047, StringToMetadata("2047"));
  ASSERT_THROW(StringToMetadata("Ceci est un test"), OrthancException);
  ASSERT_THROW(RegisterUserMetadata(128, ""), OrthancException); // too low (< 1024)
  ASSERT_THROW(RegisterUserMetadata(128000, ""), OrthancException); // too high (> 65535)
  RegisterUserMetadata(2047, "Ceci est un test");
  ASSERT_EQ(2047, StringToMetadata("2047"));
  ASSERT_EQ(2047, StringToMetadata("Ceci est un test"));

  ASSERT_STREQ("Generic", EnumerationToString(StringToModalityManufacturer("Generic")));
  ASSERT_STREQ("GenericNoWildcardInDates", EnumerationToString(StringToModalityManufacturer("GenericNoWildcardInDates")));
  ASSERT_STREQ("GenericNoUniversalWildcard", EnumerationToString(StringToModalityManufacturer("GenericNoUniversalWildcard")));
  ASSERT_STREQ("StoreScp", EnumerationToString(StringToModalityManufacturer("StoreScp")));
  ASSERT_STREQ("ClearCanvas", EnumerationToString(StringToModalityManufacturer("ClearCanvas")));
  ASSERT_STREQ("Dcm4Chee", EnumerationToString(StringToModalityManufacturer("Dcm4Chee")));
  ASSERT_STREQ("Vitrea", EnumerationToString(StringToModalityManufacturer("Vitrea")));
  // backward compatibility tests (to remove once we make these manufacturer really obsolete)
  ASSERT_STREQ("Generic", EnumerationToString(StringToModalityManufacturer("MedInria")));
  ASSERT_STREQ("Generic", EnumerationToString(StringToModalityManufacturer("EFilm2")));
  ASSERT_STREQ("GenericNoWildcardInDates", EnumerationToString(StringToModalityManufacturer("SyngoVia")));
  ASSERT_STREQ("GenericNoWildcardInDates", EnumerationToString(StringToModalityManufacturer("AgfaImpax")));
}



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
}


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
   * FreeBSD.
   **/
  
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
#  if _BYTE_ORDER == _BIG_ENDIAN
   ASSERT_EQ(Endianness_Big, Toolbox::DetectEndianness());
#  else // _LITTLE_ENDIAN
   ASSERT_EQ(Endianness_Little, Toolbox::DetectEndianness());
#  endif

#else
#error Support your platform here
#endif
}


#include "../Core/Endianness.h"

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


#if !defined(_WIN32)
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

  Toolbox::UriEncode(s, "!#$&'()*+,/:;=?@[]"); ASSERT_EQ("%21%23%24%26%27%28%29%2A%2B%2C%2F%3A%3B%3D%3F%40%5B%5D", s);  
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


int main(int argc, char **argv)
{
  Logging::Initialize();
  Logging::EnableInfoLevel(true);
  Toolbox::DetectEndianness();
  SystemToolbox::MakeDirectory("UnitTestsResults");
  OrthancInitialize();

  ::testing::InitGoogleTest(&argc, argv);
  int result = RUN_ALL_TESTS();

  OrthancFinalize();
  Logging::Finalize();

  return result;
}
