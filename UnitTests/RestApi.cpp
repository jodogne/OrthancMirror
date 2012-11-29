#include "gtest/gtest.h"

#include <ctype.h>
#include <glog/logging.h>

#include "../Core/RestApi/RestApi.h"
#include "../Core/Uuid.h"
#include "../Core/OrthancException.h"
#include "../Core/Compression/ZlibCompressor.h"

using namespace Orthanc;

TEST(RestApi, RestApiPath)
{
  RestApiPath::Components args;
  UriComponents trail;

  {
    RestApiPath uri("/coucou/{abc}/d/*");
    ASSERT_TRUE(uri.Match(args, trail, "/coucou/moi/d/e/f/g"));
    ASSERT_EQ(1u, args.size());
    ASSERT_EQ(3u, trail.size());
    ASSERT_EQ("moi", args["abc"]);
    ASSERT_EQ("e", trail[0]);
    ASSERT_EQ("f", trail[1]);
    ASSERT_EQ("g", trail[2]);

    ASSERT_FALSE(uri.Match(args, trail, "/coucou/moi/f"));
    ASSERT_TRUE(uri.Match(args, trail, "/coucou/moi/d/"));
    ASSERT_FALSE(uri.Match(args, trail, "/a/moi/d"));
    ASSERT_FALSE(uri.Match(args, trail, "/coucou/moi"));
  }

  {
    RestApiPath uri("/coucou/{abc}/d");
    ASSERT_FALSE(uri.Match(args, trail, "/coucou/moi/d/e/f/g"));
    ASSERT_TRUE(uri.Match(args, trail, "/coucou/moi/d"));
    ASSERT_EQ(1u, args.size());
    ASSERT_EQ(0u, trail.size());
    ASSERT_EQ("moi", args["abc"]);
  }

  {
    RestApiPath uri("/*");
    ASSERT_TRUE(uri.Match(args, trail, "/a/b/c"));
    ASSERT_EQ(0u, args.size());
    ASSERT_EQ(3u, trail.size());
    ASSERT_EQ("a", trail[0]);
    ASSERT_EQ("b", trail[1]);
    ASSERT_EQ("c", trail[2]);
  }
}



#if 0

#include "../Core/HttpServer/MongooseServer.h"

struct Tutu : public IDynamicObject
{
  static void Toto(RestApi::GetCall& call)
  {
    printf("DONE\n");
    Json::Value a = Json::objectValue;
    a["Tutu"] = "Toto";
    a["Youpie"] = call.GetArgument("coucou", "nope");
    a["Toto"] = call.GetUriComponent("test", "nope");
    call.GetOutput().AnswerJson(a);
  }
};



TEST(RestApi, Tutu)
{
  MongooseServer httpServer;
  httpServer.SetPortNumber(8042);
  httpServer.Start();

  RestApi* api = new RestApi;
  httpServer.RegisterHandler(api);
  api->Register("/coucou/{test}/a/*", Tutu::Toto);

  httpServer.Start();
  /*LOG(WARNING) << "REST has started";
    Toolbox::ServerBarrier();*/
}

#endif



#include "../Core/FileStorage.h"
#include "../Core/HttpServer/FilesystemHttpSender.h"
#include "../Core/HttpServer/BufferHttpSender.h"
#include "../Core/FileStorage/FileStorageAccessor.h"
#include "../Core/FileStorage/CompressedFileStorageAccessor.h"



TEST(FileStorageAccessor, Simple)
{
  FileStorage s("FileStorageUnitTests");
  FileStorageAccessor accessor(s);

  std::string data = "Hello world";
  std::string id = accessor.Write(data);
  
  std::string r;
  accessor.Read(r, id);

  ASSERT_EQ(data, r);
}



TEST(FileStorageAccessor, NoCompression)
{
  FileStorage s("FileStorageUnitTests");
  CompressedFileStorageAccessor accessor(s);

  accessor.SetCompressionForNextOperations(CompressionType_None);
  std::string data = "Hello world";
  std::string id = accessor.Write(data);
  
  std::string r;
  accessor.Read(r, id);

  ASSERT_EQ(data, r);
}


TEST(FileStorageAccessor, Compression)
{
  FileStorage s("FileStorageUnitTests");
  CompressedFileStorageAccessor accessor(s);

  accessor.SetCompressionForNextOperations(CompressionType_Zlib);
  std::string data = "Hello world";
  std::string id = accessor.Write(data);
  
  std::string r;
  accessor.Read(r, id);

  ASSERT_EQ(data, r);
}


TEST(FileStorageAccessor, Mix)
{
  FileStorage s("FileStorageUnitTests");
  CompressedFileStorageAccessor accessor(s);

  std::string r;
  std::string compressedData = "Hello";
  std::string uncompressedData = "HelloWorld";

  accessor.SetCompressionForNextOperations(CompressionType_Zlib);
  std::string compressedId = accessor.Write(compressedData);
  
  accessor.SetCompressionForNextOperations(CompressionType_None);
  std::string uncompressedId = accessor.Write(uncompressedData);
  
  accessor.SetCompressionForNextOperations(CompressionType_Zlib);
  accessor.Read(r, compressedId);
  ASSERT_EQ(compressedData, r);

  accessor.SetCompressionForNextOperations(CompressionType_None);
  accessor.Read(r, compressedId);
  ASSERT_NE(compressedData, r);
  
  accessor.SetCompressionForNextOperations(CompressionType_Zlib);
  ASSERT_THROW(accessor.Read(r, uncompressedId), OrthancException);
}
