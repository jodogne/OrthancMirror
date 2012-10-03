#include "gtest/gtest.h"

#include <ctype.h>

#include "../Core/Compression/ZlibCompressor.h"
#include "../Core/DicomFormat/DicomTag.h"
#include "../Core/FileStorage.h"
#include "../OrthancCppClient/HttpClient.h"
#include "../Core/HttpServer/HttpHandler.h"
#include "../Core/OrthancException.h"
#include "../Core/Toolbox.h"
#include "../Core/Uuid.h"
#include "../OrthancServer/FromDcmtkBridge.h"
#include "../OrthancServer/OrthancInitialization.h"

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
}

TEST(Zlib, Basic)
{
  std::string s = Toolbox::GenerateUuid();
  s = s + s + s + s;
 
  std::string compressed;
  ZlibCompressor c;
  c.Compress(compressed, s);

  std::string uncompressed;
  c.Uncompress(uncompressed, compressed);

  ASSERT_EQ(s.size(), uncompressed.size());
  ASSERT_EQ(0, memcmp(&s[0], &uncompressed[0], s.size()));
}

TEST(Zlib, Empty)
{
  std::string s = "";
 
  std::string compressed;
  ZlibCompressor c;
  c.Compress(compressed, s);

  std::string uncompressed;
  c.Uncompress(uncompressed, compressed);

  ASSERT_EQ(0u, uncompressed.size());
}

TEST(ParseGetQuery, Basic)
{
  HttpHandler::Arguments a;
  HttpHandler::ParseGetQuery(a, "aaa=baaa&bb=a&aa=c");
  ASSERT_EQ(3u, a.size());
  ASSERT_EQ(a["aaa"], "baaa");
  ASSERT_EQ(a["bb"], "a");
  ASSERT_EQ(a["aa"], "c");
}

TEST(ParseGetQuery, BasicEmpty)
{
  HttpHandler::Arguments a;
  HttpHandler::ParseGetQuery(a, "aaa&bb=aa&aa");
  ASSERT_EQ(3u, a.size());
  ASSERT_EQ(a["aaa"], "");
  ASSERT_EQ(a["bb"], "aa");
  ASSERT_EQ(a["aa"], "");
}

TEST(ParseGetQuery, Single)
{
  HttpHandler::Arguments a;
  HttpHandler::ParseGetQuery(a, "aaa=baaa");
  ASSERT_EQ(1u, a.size());
  ASSERT_EQ(a["aaa"], "baaa");
}

TEST(ParseGetQuery, SingleEmpty)
{
  HttpHandler::Arguments a;
  HttpHandler::ParseGetQuery(a, "aaa");
  ASSERT_EQ(1u, a.size());
  ASSERT_EQ(a["aaa"], "");
}

TEST(FileStorage, Basic)
{
  FileStorage s("FileStorageUnitTests");

  std::string data = Toolbox::GenerateUuid();
  std::string uid = s.Create(data);
  std::string d;
  s.ReadFile(d, uid);
  ASSERT_EQ(d.size(), data.size());
  ASSERT_FALSE(memcmp(&d[0], &data[0], data.size()));
}

TEST(FileStorage, EndToEnd)
{
  FileStorage s("FileStorageUnitTests");
  s.Clear();

  std::list<std::string> u;
  for (unsigned int i = 0; i < 10; i++)
  {
    u.push_back(s.Create(Toolbox::GenerateUuid()));
  }

  std::set<std::string> ss;
  s.ListAllFiles(ss);
  ASSERT_EQ(10u, ss.size());
  
  unsigned int c = 0;
  for (std::list<std::string>::iterator
         i = u.begin(); i != u.end(); i++, c++)
  {
    ASSERT_TRUE(ss.find(*i) != ss.end());
    if (c < 5)
      s.Remove(*i);
  }

  s.ListAllFiles(ss);
  ASSERT_EQ(5u, ss.size());

  s.Clear();
  s.ListAllFiles(ss);
  ASSERT_EQ(0u, ss.size());
}


TEST(DicomFormat, Tag)
{
  ASSERT_EQ("PatientName", FromDcmtkBridge::GetName(DicomTag(0x0010, 0x0010)));

  DicomTag t = FromDcmtkBridge::FindTag("SeriesDescription");
  ASSERT_EQ(0x0008, t.GetGroup());
  ASSERT_EQ(0x103E, t.GetElement());
}


TEST(Uri, SplitUriComponents)
{
  UriComponents c;
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

TEST(Toolbox, Base64)
{
  ASSERT_EQ("", Toolbox::EncodeBase64(""));
  ASSERT_EQ("YQ==", Toolbox::EncodeBase64("a"));
  ASSERT_EQ("SGVsbG8gd29ybGQ=", Toolbox::EncodeBase64("Hello world"));
}

TEST(Toolbox, PathToExecutable)
{
  printf("[%s]\n", Toolbox::GetPathToExecutable().c_str());
  printf("[%s]\n", Toolbox::GetDirectoryOfExecutable().c_str());
}



#if DCMTK_BUNDLES_LOG4CPLUS == 0
#include <log4cplus/logger.h>
#include <log4cplus/consoleappender.h>
#include <log4cplus/fileappender.h>
#include <log4cplus/configurator.h>
#else
#include <dcmtk/oflog/logger.h>
#include <dcmtk/oflog/consap.h>
#include <dcmtk/oflog/fileap.h>
//#include <dcmtk/oflog/configurator.h>
#endif


static log4cplus::Logger logger(log4cplus::Logger::getInstance("UnitTests"));

TEST(Logger, Basic)
{
  LOG4CPLUS_INFO(logger, "I say hello");
}


int main(int argc, char **argv)
{
  using namespace log4cplus;
  SharedAppenderPtr myAppender(new ConsoleAppender());
  //SharedAppenderPtr myAppender(new FileAppender("UnitTests.log"));
#if DCMTK_BUNDLES_LOG4CPLUS == 0
  std::auto_ptr<Layout> myLayout(new TTCCLayout());
#else
  OFauto_ptr<Layout> myLayout(new TTCCLayout());
#endif
  myAppender->setLayout(myLayout);
  Logger::getRoot().addAppender(myAppender);
  Logger::getRoot().setLogLevel(INFO_LOG_LEVEL);

  OrthancInitialize();
  ::testing::InitGoogleTest(&argc, argv);
  int result = RUN_ALL_TESTS();
  OrthancFinalize();
  return result;
}
