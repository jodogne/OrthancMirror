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


TEST(FileStorageAccessor, Simple)
{
  FileStorage s("FileStorageUnitTests");
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
  FileStorage s("FileStorageUnitTests");
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


TEST(FileStorageAccessor, Compression)
{
  FileStorage s("FileStorageUnitTests");
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
  FileStorage s("FileStorageUnitTests");
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
