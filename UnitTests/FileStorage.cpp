#include "gtest/gtest.h"

#include <ctype.h>
#include <glog/logging.h>

#include "../Core/FileStorage.h"
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
  FileInfo info = accessor.Write(data, FileType_Dicom);
  
  std::string r;
  accessor.Read(r, info.GetUuid());

  ASSERT_EQ(data, r);
  ASSERT_EQ(CompressionType_None, info.GetCompressionType());
  ASSERT_EQ(11u, info.GetUncompressedSize());
  ASSERT_EQ(11u, info.GetCompressedSize());
  ASSERT_EQ(FileType_Dicom, info.GetFileType());
}


TEST(FileStorageAccessor, NoCompression)
{
  FileStorage s("FileStorageUnitTests");
  CompressedFileStorageAccessor accessor(s);

  accessor.SetCompressionForNextOperations(CompressionType_None);
  std::string data = "Hello world";
  FileInfo info = accessor.Write(data, FileType_Dicom);
  
  std::string r;
  accessor.Read(r, info.GetUuid());

  ASSERT_EQ(data, r);
  ASSERT_EQ(CompressionType_None, info.GetCompressionType());
  ASSERT_EQ(11u, info.GetUncompressedSize());
  ASSERT_EQ(11u, info.GetCompressedSize());
  ASSERT_EQ(FileType_Dicom, info.GetFileType());
}


TEST(FileStorageAccessor, Compression)
{
  FileStorage s("FileStorageUnitTests");
  CompressedFileStorageAccessor accessor(s);

  accessor.SetCompressionForNextOperations(CompressionType_Zlib);
  std::string data = "Hello world";
  FileInfo info = accessor.Write(data, FileType_Dicom);
  
  std::string r;
  accessor.Read(r, info.GetUuid());

  ASSERT_EQ(data, r);
  ASSERT_EQ(CompressionType_Zlib, info.GetCompressionType());
  ASSERT_EQ(11u, info.GetUncompressedSize());
  ASSERT_EQ(FileType_Dicom, info.GetFileType());
}


TEST(FileStorageAccessor, Mix)
{
  FileStorage s("FileStorageUnitTests");
  CompressedFileStorageAccessor accessor(s);

  std::string r;
  std::string compressedData = "Hello";
  std::string uncompressedData = "HelloWorld";

  accessor.SetCompressionForNextOperations(CompressionType_Zlib);
  FileInfo compressedInfo = accessor.Write(compressedData, FileType_Dicom);
  
  accessor.SetCompressionForNextOperations(CompressionType_None);
  FileInfo uncompressedInfo = accessor.Write(uncompressedData, FileType_Dicom);
  
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



#if 0
// TODO REMOVE THIS STUFF
namespace Orthanc
{
  class ServerStorageAccessor : public StorageAccessor
  {
  private:
    CompressedFileStorageAccessor composite_;
    ServerIndex& index_;
    AttachedFileType contentType_;

  protected:
    virtual std::string WriteInternal(const void* data,
                                      size_t size)
    {
      switch (contentType_)
      {
      case AttachedFileType_Json:
        composite_.SetCompressionForNextOperations(CompressionType_None);
        break;

      case AttachedFileType_Dicom:
        // TODO GLOBAL PARAMETER
        composite_.SetCompressionForNextOperations(CompressionType_Zlib);
        break;
        
      default:
        throw OrthancException(ErrorCode_InternalError);
      }

      std::string fileUuid = composite_.Write(data, size);

      
    }

  public: 
    ServerStorageAccessor(FileStorage& storage,
                          ServerIndex& index) :
      composite_(storage),
      index_(index)
    {
      contentType_ = AttachedFileType_Dicom;
    }

    void SetAttachmentType(AttachedFileType type)
    {
      contentType_ = type;
    }

    AttachedFileType GetAttachmentType() const
    {
      return contentType_;
    }

    virtual void Read(std::string& content,
                      const std::string& uuid)
    {
      std::string fileUuid;
      CompressionType compression;

      if (index_.GetFile(fileUuid, compression, uuid, contentType_))
      {
        composite_.SetCompressionForNextOperations(compression);
        composite_.Read(content, fileUuid);
      }
      else
      {
        throw OrthancException(ErrorCode_InternalError);
      }
    }

    virtual HttpFileSender* ConstructHttpFileSender(const std::string& uuid)
    {
      std::string fileUuid;
      CompressionType compression;

      if (index_.GetFile(fileUuid, compression, uuid, contentType_))
      {
        composite_.SetCompressionForNextOperations(compression);
        return composite_.ConstructHttpFileSender(fileUuid);
      }
      else
      {
        throw OrthancException(ErrorCode_InternalError);
      }
    }
  };
}
#endif
