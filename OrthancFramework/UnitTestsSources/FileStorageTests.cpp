/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include "../Sources/FileStorage/FilesystemStorage.h"
#include "../Sources/FileStorage/StorageAccessor.h"
#include "../Sources/FileStorage/StorageCache.h"
#include "../Sources/HttpServer/BufferHttpSender.h"
#include "../Sources/HttpServer/FilesystemHttpSender.h"
#include "../Sources/Logging.h"
#include "../Sources/OrthancException.h"
#include "../Sources/Toolbox.h"

#include <ctype.h>


using namespace Orthanc;


static void StringToVector(std::vector<uint8_t>& v,
                           const std::string& s)
{
  v.resize(s.size());
  for (size_t i = 0; i < s.size(); i++)
    v[i] = s[i];
}


TEST(FilesystemStorage, Basic)
{
  FilesystemStorage s("UnitTestsStorage");

  std::string data = Toolbox::GenerateUuid();
  std::string uid = Toolbox::GenerateUuid();
  s.Create(uid.c_str(), &data[0], data.size(), FileContentType_Unknown);
  std::string d;
  {
    std::unique_ptr<IMemoryBuffer> buffer(s.Read(uid, FileContentType_Unknown));
    buffer->MoveToString(d);    
  }
  ASSERT_EQ(d.size(), data.size());
  ASSERT_FALSE(memcmp(&d[0], &data[0], data.size()));
  ASSERT_EQ(s.GetSize(uid), data.size());
}

TEST(FilesystemStorage, Basic2)
{
  FilesystemStorage s("UnitTestsStorage");

  std::vector<uint8_t> data;
  StringToVector(data, Toolbox::GenerateUuid());
  std::string uid = Toolbox::GenerateUuid();
  s.Create(uid.c_str(), &data[0], data.size(), FileContentType_Unknown);
  std::string d;
  {
    std::unique_ptr<IMemoryBuffer> buffer(s.Read(uid, FileContentType_Unknown));
    buffer->MoveToString(d);    
  }
  ASSERT_EQ(d.size(), data.size());
  ASSERT_FALSE(memcmp(&d[0], &data[0], data.size()));
  ASSERT_EQ(s.GetSize(uid), data.size());
}

TEST(FilesystemStorage, EndToEnd)
{
  FilesystemStorage s("UnitTestsStorage");
  s.Clear();

  std::list<std::string> u;
  for (unsigned int i = 0; i < 10; i++)
  {
    std::string t = Toolbox::GenerateUuid();
    std::string uid = Toolbox::GenerateUuid();
    s.Create(uid.c_str(), &t[0], t.size(), FileContentType_Unknown);
    u.push_back(uid);
  }

  std::set<std::string> ss;
  s.ListAllFiles(ss);
  ASSERT_EQ(10u, ss.size());
  
  unsigned int c = 0;
  for (std::list<std::string>::const_iterator
         i = u.begin(); i != u.end(); ++i, c++)
  {
    ASSERT_TRUE(ss.find(*i) != ss.end());
    if (c < 5)
      s.Remove(*i, FileContentType_Unknown);
  }

  s.ListAllFiles(ss);
  ASSERT_EQ(5u, ss.size());

  s.Clear();
  s.ListAllFiles(ss);
  ASSERT_EQ(0u, ss.size());
}


TEST(StorageAccessor, NoCompression)
{
  FilesystemStorage s("UnitTestsStorage");
  StorageCache cache;
  StorageAccessor accessor(s, &cache);

  std::string data = "Hello world";
  FileInfo info = accessor.Write(data, FileContentType_Dicom, CompressionType_None, true);
  
  std::string r;
  accessor.Read(r, info);

  ASSERT_EQ(data, r);
  ASSERT_EQ(CompressionType_None, info.GetCompressionType());
  ASSERT_EQ(11u, info.GetUncompressedSize());
  ASSERT_EQ(11u, info.GetCompressedSize());
  ASSERT_EQ(FileContentType_Dicom, info.GetContentType());
  ASSERT_EQ("3e25960a79dbc69b674cd4ec67a72c62", info.GetUncompressedMD5());
  ASSERT_EQ(info.GetUncompressedMD5(), info.GetCompressedMD5());
}


TEST(StorageAccessor, Compression)
{
  FilesystemStorage s("UnitTestsStorage");
  StorageCache cache;
  StorageAccessor accessor(s, &cache);

  std::string data = "Hello world";
  FileInfo info = accessor.Write(data, FileContentType_Dicom, CompressionType_ZlibWithSize, true);
  
  std::string r;
  accessor.Read(r, info);

  ASSERT_EQ(data, r);
  ASSERT_EQ(CompressionType_ZlibWithSize, info.GetCompressionType());
  ASSERT_EQ(11u, info.GetUncompressedSize());
  ASSERT_EQ(FileContentType_Dicom, info.GetContentType());
  ASSERT_EQ("3e25960a79dbc69b674cd4ec67a72c62", info.GetUncompressedMD5());
  ASSERT_NE(info.GetUncompressedMD5(), info.GetCompressedMD5());
}


TEST(StorageAccessor, Mix)
{
  FilesystemStorage s("UnitTestsStorage");
  StorageCache cache;
  StorageAccessor accessor(s, &cache);

  std::string r;
  std::string compressedData = "Hello";
  std::string uncompressedData = "HelloWorld";

  FileInfo compressedInfo = accessor.Write(compressedData, FileContentType_Dicom, CompressionType_ZlibWithSize, false);  
  FileInfo uncompressedInfo = accessor.Write(uncompressedData, FileContentType_Dicom, CompressionType_None, false);
  
  accessor.Read(r, compressedInfo);
  ASSERT_EQ(compressedData, r);

  accessor.Read(r, uncompressedInfo);
  ASSERT_EQ(uncompressedData, r);
  ASSERT_NE(compressedData, r);

  /*
  // This test is too slow on Windows
  accessor.SetCompressionForNextOperations(CompressionType_ZlibWithSize);
  ASSERT_THROW(accessor.Read(r, uncompressedInfo.GetUuid(), FileContentType_Unknown), OrthancException);
  */
}
