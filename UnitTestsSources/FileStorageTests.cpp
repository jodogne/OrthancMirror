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

#include <ctype.h>

#include "../Core/FileStorage/FilesystemStorage.h"
#include "../Core/FileStorage/StorageAccessor.h"
#include "../Core/HttpServer/BufferHttpSender.h"
#include "../Core/HttpServer/FilesystemHttpSender.h"
#include "../Core/Logging.h"
#include "../Core/OrthancException.h"
#include "../Core/Toolbox.h"
#include "../OrthancServer/ServerIndex.h"

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

  std::string data = SystemToolbox::GenerateUuid();
  std::string uid = SystemToolbox::GenerateUuid();
  s.Create(uid.c_str(), &data[0], data.size(), FileContentType_Unknown);
  std::string d;
  s.Read(d, uid, FileContentType_Unknown);
  ASSERT_EQ(d.size(), data.size());
  ASSERT_FALSE(memcmp(&d[0], &data[0], data.size()));
  ASSERT_EQ(s.GetSize(uid), data.size());
}

TEST(FilesystemStorage, Basic2)
{
  FilesystemStorage s("UnitTestsStorage");

  std::vector<uint8_t> data;
  StringToVector(data, SystemToolbox::GenerateUuid());
  std::string uid = SystemToolbox::GenerateUuid();
  s.Create(uid.c_str(), &data[0], data.size(), FileContentType_Unknown);
  std::string d;
  s.Read(d, uid, FileContentType_Unknown);
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
    std::string t = SystemToolbox::GenerateUuid();
    std::string uid = SystemToolbox::GenerateUuid();
    s.Create(uid.c_str(), &t[0], t.size(), FileContentType_Unknown);
    u.push_back(uid);
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
  StorageAccessor accessor(s);

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
  StorageAccessor accessor(s);

  std::string data = "Hello world";
  FileInfo info = accessor.Write(data, FileContentType_DicomAsJson, CompressionType_ZlibWithSize, true);
  
  std::string r;
  accessor.Read(r, info);

  ASSERT_EQ(data, r);
  ASSERT_EQ(CompressionType_ZlibWithSize, info.GetCompressionType());
  ASSERT_EQ(11u, info.GetUncompressedSize());
  ASSERT_EQ(FileContentType_DicomAsJson, info.GetContentType());
  ASSERT_EQ("3e25960a79dbc69b674cd4ec67a72c62", info.GetUncompressedMD5());
  ASSERT_NE(info.GetUncompressedMD5(), info.GetCompressedMD5());
}


TEST(StorageAccessor, Mix)
{
  FilesystemStorage s("UnitTestsStorage");
  StorageAccessor accessor(s);

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
