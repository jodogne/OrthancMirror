/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2014 Medical Physics Department, CHU of Liege,
 * Belgium
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

#include <ctype.h>
#include <glog/logging.h>

#include "../Core/FileStorage/FileStorage.h"
#include "../OrthancServer/ServerIndex.h"
#include "../Core/Toolbox.h"
#include "../Core/OrthancException.h"
#include "../Core/Uuid.h"
#include "../Core/HttpServer/FilesystemHttpSender.h"
#include "../Core/HttpServer/BufferHttpSender.h"
#include "../Core/FileStorage/FileStorageAccessor.h"
#include "../Core/FileStorage/CompressedFileStorageAccessor.h"

using namespace Orthanc;


static void StringToVector(std::vector<uint8_t>& v,
                           const std::string& s)
{
  v.resize(s.size());
  for (size_t i = 0; i < s.size(); i++)
    v[i] = s[i];
}


TEST(FileStorage, Basic)
{
  FileStorage s("UnitTestsStorage");

  std::string data = Toolbox::GenerateUuid();
  std::string uid = s.Create(data);
  std::string d;
  s.Read(d, uid);
  ASSERT_EQ(d.size(), data.size());
  ASSERT_FALSE(memcmp(&d[0], &data[0], data.size()));
  ASSERT_EQ(s.GetSize(uid), data.size());
}

TEST(FileStorage, Basic2)
{
  FileStorage s("UnitTestsStorage");

  std::vector<uint8_t> data;
  StringToVector(data, Toolbox::GenerateUuid());
  std::string uid = s.Create(data);
  std::string d;
  s.Read(d, uid);
  ASSERT_EQ(d.size(), data.size());
  ASSERT_FALSE(memcmp(&d[0], &data[0], data.size()));
  ASSERT_EQ(s.GetSize(uid), data.size());
}

TEST(FileStorage, EndToEnd)
{
  FileStorage s("UnitTestsStorage");
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


TEST(FileStorageAccessor, Simple)
{
  FileStorage s("UnitTestsStorage");
  FileStorageAccessor accessor(s);

  std::string data = "Hello world";
  FileInfo info = accessor.Write(data, FileContentType_Dicom);
  
  std::string r;
  accessor.Read(r, info.GetUuid());

  ASSERT_EQ(data, r);
  ASSERT_EQ(CompressionType_None, info.GetCompressionType());
  ASSERT_EQ(11u, info.GetUncompressedSize());
  ASSERT_EQ(11u, info.GetCompressedSize());
  ASSERT_EQ(FileContentType_Dicom, info.GetContentType());
}


TEST(FileStorageAccessor, NoCompression)
{
  FileStorage s("UnitTestsStorage");
  CompressedFileStorageAccessor accessor(s);

  accessor.SetCompressionForNextOperations(CompressionType_None);
  std::string data = "Hello world";
  FileInfo info = accessor.Write(data, FileContentType_Dicom);
  
  std::string r;
  accessor.Read(r, info.GetUuid());

  ASSERT_EQ(data, r);
  ASSERT_EQ(CompressionType_None, info.GetCompressionType());
  ASSERT_EQ(11u, info.GetUncompressedSize());
  ASSERT_EQ(11u, info.GetCompressedSize());
  ASSERT_EQ(FileContentType_Dicom, info.GetContentType());
}


TEST(FileStorageAccessor, NoCompression2)
{
  FileStorage s("UnitTestsStorage");
  CompressedFileStorageAccessor accessor(s);

  accessor.SetCompressionForNextOperations(CompressionType_None);
  std::vector<uint8_t> data;
  StringToVector(data, "Hello world");
  FileInfo info = accessor.Write(data, FileContentType_Dicom);
  
  std::string r;
  accessor.Read(r, info.GetUuid());

  ASSERT_EQ(0, memcmp(&r[0], &data[0], data.size()));
  ASSERT_EQ(CompressionType_None, info.GetCompressionType());
  ASSERT_EQ(11u, info.GetUncompressedSize());
  ASSERT_EQ(11u, info.GetCompressedSize());
  ASSERT_EQ(FileContentType_Dicom, info.GetContentType());
}


TEST(FileStorageAccessor, Compression)
{
  FileStorage s("UnitTestsStorage");
  CompressedFileStorageAccessor accessor(s);

  accessor.SetCompressionForNextOperations(CompressionType_Zlib);
  std::string data = "Hello world";
  FileInfo info = accessor.Write(data, FileContentType_Dicom);
  
  std::string r;
  accessor.Read(r, info.GetUuid());

  ASSERT_EQ(data, r);
  ASSERT_EQ(CompressionType_Zlib, info.GetCompressionType());
  ASSERT_EQ(11u, info.GetUncompressedSize());
  ASSERT_EQ(FileContentType_Dicom, info.GetContentType());
}


TEST(FileStorageAccessor, Mix)
{
  FileStorage s("UnitTestsStorage");
  CompressedFileStorageAccessor accessor(s);

  std::string r;
  std::string compressedData = "Hello";
  std::string uncompressedData = "HelloWorld";

  accessor.SetCompressionForNextOperations(CompressionType_Zlib);
  FileInfo compressedInfo = accessor.Write(compressedData, FileContentType_Dicom);
  
  accessor.SetCompressionForNextOperations(CompressionType_None);
  FileInfo uncompressedInfo = accessor.Write(uncompressedData, FileContentType_Dicom);
  
  accessor.SetCompressionForNextOperations(CompressionType_Zlib);
  accessor.Read(r, compressedInfo.GetUuid());
  ASSERT_EQ(compressedData, r);

  accessor.SetCompressionForNextOperations(CompressionType_None);
  accessor.Read(r, compressedInfo.GetUuid());
  ASSERT_NE(compressedData, r);

  /*
  // This test is too slow on Windows
  accessor.SetCompressionForNextOperations(CompressionType_Zlib);
  ASSERT_THROW(accessor.Read(r, uncompressedInfo.GetUuid()), OrthancException);
  */
}
