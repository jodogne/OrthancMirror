#include "gtest/gtest.h"

#include <ctype.h>

#include "../Core/SQLite/Connection.h"
#include "../Core/Compression/ZlibCompressor.h"
#include "../Core/DicomFormat/DicomTag.h"
#include "../Core/DicomFormat/DicomArray.h"
#include "../Core/FileStorage.h"
#include "../OrthancCppClient/HttpClient.h"
#include "../Core/HttpServer/HttpHandler.h"
#include "../Core/OrthancException.h"
#include "../Core/Toolbox.h"
#include "../Core/Uuid.h"
#include "../OrthancServer/FromDcmtkBridge.h"
#include "../OrthancServer/OrthancInitialization.h"
#include "../OrthancServer/ServerIndex.h"
#include "EmbeddedResources.h"

#include <glog/logging.h>
#include <boost/thread.hpp>


namespace Orthanc
{
  enum CompressionType
  {
    CompressionType_None = 1,
    CompressionType_Zlib = 2
  };

  enum MetadataType
  {
    MetadataType_Instance_RemoteAet = 1,
    MetadataType_Instance_IndexInSeries = 2,
    MetadataType_Series_ExpectedNumberOfInstances = 3
  };

  class IServerIndexListener
  {
  public:
    virtual ~IServerIndexListener()
    {
    }

    virtual void SignalResourceDeleted(ResourceType type,
                                       const std::string& parentPublicId) = 0;

    virtual void SignalFileDeleted(const std::string& fileUuid) = 0;                     
                                 
  };

  namespace Internals
  {
    class SignalFileDeleted : public SQLite::IScalarFunction
    {
    private:
      IServerIndexListener& listener_;

    public:
      SignalFileDeleted(IServerIndexListener& listener) :
        listener_(listener)
      {
      }

      virtual const char* GetName() const
      {
        return "SignalFileDeleted";
      }

      virtual unsigned int GetCardinality() const
      {
        return 1;
      }

      virtual void Compute(SQLite::FunctionContext& context)
      {
        listener_.SignalFileDeleted(context.GetStringValue(0));
      }
    };

    class SignalResourceDeleted : public SQLite::IScalarFunction
    {
    public:
      virtual const char* GetName() const
      {
        return "SignalResourceDeleted";
      }

      virtual unsigned int GetCardinality() const
      {
        return 2;
      }

      virtual void Compute(SQLite::FunctionContext& context)
      {
        LOG(INFO) << "A resource has been removed, of type "
                  << context.GetIntValue(0)
                  << ", with parent "
                  << context.GetIntValue(1);
      }
    };
  }


  class ServerIndexHelper
  {
  private:
    IServerIndexListener& listener_;
    SQLite::Connection db_;
    boost::mutex mutex_;

    void Open(const std::string& path);

  public:
    void SetGlobalProperty(const std::string& name,
                           const std::string& value)
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT OR REPLACE INTO GlobalProperties VALUES(?, ?)");
      s.BindString(0, name);
      s.BindString(1, value);
      s.Run();
    }

    bool FindGlobalProperty(std::string& target,
                            const std::string& name)
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                          "SELECT value FROM GlobalProperties WHERE name=?");
      s.BindString(0, name);

      if (!s.Step())
      {
        return false;
      }
      else
      {
        target = s.ColumnString(0);
        return true;
      }
    }

    std::string GetGlobalProperty(const std::string& name,
                                  const std::string& defaultValue = "")
    {
      std::string s;
      if (FindGlobalProperty(s, name))
      {
        return s;
      }
      else
      {
        return defaultValue;
      }
    }

    int64_t CreateResource(const std::string& publicId,
                           ResourceType type)
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO Resources VALUES(NULL, ?, ?, NULL)");
      s.BindInt(0, type);
      s.BindString(1, publicId);
      s.Run();
      return db_.GetLastInsertRowId();
    }

    bool FindResource(const std::string& publicId,
                      int64_t& id,
                      ResourceType& type)
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                          "SELECT internalId, resourceType FROM Resources WHERE publicId=?");
      s.BindString(0, publicId);

      if (!s.Step())
      {
        return false;
      }
      else
      {
        id = s.ColumnInt(0);
        type = static_cast<ResourceType>(s.ColumnInt(1));

        // Check whether there is a single resource with this public id
        assert(!s.Step());

        return true;
      }
    }

    void AttachChild(int64_t parent,
                     int64_t child)
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, "UPDATE Resources SET parentId = ? WHERE internalId = ?");
      s.BindInt(0, parent);
      s.BindInt(1, child);
      s.Run();
    }

    void DeleteResource(int64_t id)
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, "DELETE FROM Resources WHERE internalId=?");
      s.BindInt(0, id);
      s.Run();      
    }

    void SetMetadata(int64_t id,
                     MetadataType type,
                     const std::string& value)
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT OR REPLACE INTO Metadata VALUES(?, ?, ?)");
      s.BindInt(0, id);
      s.BindInt(1, type);
      s.BindString(2, value);
      s.Run();
    }

    bool FindMetadata(std::string& target,
                      int64_t id,
                      MetadataType type)
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                          "SELECT value FROM Metadata WHERE id=? AND type=?");
      s.BindInt(0, id);
      s.BindInt(1, type);

      if (!s.Step())
      {
        return false;
      }
      else
      {
        target = s.ColumnString(0);
        return true;
      }
    }

    std::string GetMetadata(int64_t id,
                            MetadataType type,
                            const std::string& defaultValue = "")
    {
      std::string s;
      if (FindMetadata(s, id, type))
      {
        return s;
      }
      else
      {
        return defaultValue;
      }
    }

    void AttachFile(int64_t id,
                    const std::string& name,
                    const std::string& fileUuid,
                    size_t uncompressedSize,
                    CompressionType compressionType)
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO AttachedFiles VALUES(?, ?, ?, ?, ?)");
      s.BindInt(0, id);
      s.BindString(1, name);
      s.BindString(2, fileUuid);
      s.BindInt(3, uncompressedSize);
      s.BindInt(4, compressionType);
      s.Run();
    }

    bool FindFile(int64_t id,
                  const std::string& name,
                  std::string& fileUuid,
                  size_t& uncompressedSize,
                  CompressionType& compressionType)
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                          "SELECT uuid, uncompressedSize, compressionType FROM AttachedFiles WHERE id=? AND name=?");
      s.BindInt(0, id);
      s.BindString(1, name);

      if (!s.Step())
      {
        return false;
      }
      else
      {
        fileUuid = s.ColumnString(0);
        uncompressedSize = s.ColumnInt(1);
        compressionType = static_cast<CompressionType>(s.ColumnInt(2));
        return true;
      }
    }

    void SetMainDicomTags(int64_t id,
                          const DicomMap& tags)
    {
      DicomArray flattened(tags);
      for (size_t i = 0; i < flattened.GetSize(); i++)
      {
        SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO MainDicomTags VALUES(?, ?, ?, ?)");
        s.BindInt(0, id);
        s.BindInt(1, flattened.GetElement(i).GetTag().GetGroup());
        s.BindInt(2, flattened.GetElement(i).GetTag().GetElement());
        s.BindString(3, flattened.GetElement(i).GetValue().AsString());
        s.Run();
      }
    }

    void GetMainDicomTags(DicomMap& map,
                          int64_t id)
    {
      map.Clear();

      SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT * FROM MainDicomTags WHERE id=?");
      s.BindInt(0, id);
      while (s.Step())
      {
        map.SetValue(s.ColumnInt(1),
                     s.ColumnInt(2),
                     s.ColumnString(3));
      }
    }

    int64_t GetTableRecordCount(const std::string& table)
    {
      char buf[128];
      sprintf(buf, "SELECT COUNT(*) FROM %s", table.c_str());
      SQLite::Statement s(db_, buf);

      assert(s.Step());
      int64_t c = s.ColumnInt(0);
      assert(!s.Step());

      return c;
    }

    ServerIndexHelper(const std::string& path,
                      IServerIndexListener& listener) :
      listener_(listener)
    {
      Open(path);
    }

    ServerIndexHelper(IServerIndexListener& listener) :
      listener_(listener)
    {
      Open("");
    }
  };



  void ServerIndexHelper::Open(const std::string& path)
  {
    if (path == "")
    {
      db_.OpenInMemory();
    }
    else
    {
      db_.Open(path);
    }

    if (!db_.DoesTableExist("GlobalProperties"))
    {
      LOG(INFO) << "Creating the database";
      std::string query;
      EmbeddedResources::GetFileResource(query, EmbeddedResources::PREPARE_DATABASE_2);
      db_.Execute(query);
    }

    db_.Register(new Internals::SignalFileDeleted(listener_));
    db_.Register(new Internals::SignalResourceDeleted);
  }


  class ServerIndexListener : public IServerIndexListener
  {
  public:
    virtual void SignalResourceDeleted(ResourceType type,
                                       const std::string& parentPublicId) 
    {
    }

    virtual void SignalFileDeleted(const std::string& fileUuid)
    {
      LOG(INFO) << "A file must be removed: " << fileUuid;
    }                                
  };

  /*
  class ServerIndex2
  {
  private:
    ServerIndexListener listener_;
    ServerIndexHelper helper_;

    void Open(const std::string& storagePath)
    {
      boost::filesystem::path p = storagePath;

      try
      {
        boost::filesystem::create_directories(storagePath);
      }
      catch (boost::filesystem::filesystem_error)
      {
      }

      p /= "index";
    }

  public:
    ServerIndexHelper(const std::string& storagePath) :
      helper_(storagePath)
    {
      Open(storagePath);
    }
  };
  */
}



using namespace Orthanc;

TEST(ServerIndexHelper, Simple)
{
  ServerIndexListener listener;
  /*Toolbox::RemoveFile("toto");
    ServerIndexHelper index("toto", listener);*/
  ServerIndexHelper index(listener);

  LOG(WARNING) << "ok";

  int64_t a[] = {
    index.CreateResource("a", ResourceType_Patient),
    index.CreateResource("b", ResourceType_Study),
    index.CreateResource("c", ResourceType_Series),
    index.CreateResource("d", ResourceType_Instance),
    index.CreateResource("e", ResourceType_Instance),
    index.CreateResource("f", ResourceType_Instance),
    index.CreateResource("g", ResourceType_Study)
  };

  index.SetGlobalProperty("Hello", "World");

  index.AttachChild(a[0], a[1]);
  index.AttachChild(a[1], a[2]);
  index.AttachChild(a[2], a[3]);
  index.AttachChild(a[2], a[4]);
  index.AttachChild(a[6], a[5]);
  index.AttachFile(a[4], "_json", "my json file", 42, CompressionType_Zlib);
  index.AttachFile(a[4], "_dicom", "my dicom file", 42, CompressionType_None);
  index.SetMetadata(a[4], MetadataType_Instance_RemoteAet, "PINNACLE");

  DicomMap m;
  m.SetValue(0x0010, 0x0010, "PatientName");
  index.SetMainDicomTags(a[3], m);

  int64_t b;
  ResourceType t;
  ASSERT_TRUE(index.FindResource("g", b, t));
  ASSERT_EQ(7, b);
  ASSERT_EQ(ResourceType_Study, t);

  std::string s;

  ASSERT_TRUE(index.FindMetadata(s, a[4], MetadataType_Instance_RemoteAet));
  ASSERT_FALSE(index.FindMetadata(s, a[4], MetadataType_Instance_IndexInSeries));
  ASSERT_EQ("PINNACLE", s);
  ASSERT_EQ("PINNACLE", index.GetMetadata(a[4], MetadataType_Instance_RemoteAet));
  ASSERT_EQ("None", index.GetMetadata(a[4], MetadataType_Instance_IndexInSeries, "None"));

  ASSERT_TRUE(index.FindGlobalProperty(s, "Hello"));
  ASSERT_FALSE(index.FindGlobalProperty(s, "Hello2"));
  ASSERT_EQ("World", s);
  ASSERT_EQ("World", index.GetGlobalProperty("Hello"));
  ASSERT_EQ("None", index.GetGlobalProperty("Hello2", "None"));

  size_t us;
  CompressionType ct;
  ASSERT_TRUE(index.FindFile(a[4], "_json", s, us, ct));
  ASSERT_EQ("my json file", s);
  ASSERT_EQ(42, us);
  ASSERT_EQ(CompressionType_Zlib, ct);

  ASSERT_EQ(7, index.GetTableRecordCount("Resources"));
  ASSERT_EQ(2, index.GetTableRecordCount("AttachedFiles"));
  ASSERT_EQ(1, index.GetTableRecordCount("Metadata"));
  ASSERT_EQ(1, index.GetTableRecordCount("MainDicomTags"));
  index.DeleteResource(a[0]);
  ASSERT_EQ(2, index.GetTableRecordCount("Resources"));
  ASSERT_EQ(0, index.GetTableRecordCount("Metadata"));
  ASSERT_EQ(0, index.GetTableRecordCount("AttachedFiles"));
  ASSERT_EQ(0, index.GetTableRecordCount("MainDicomTags"));
  index.DeleteResource(a[6]);
  ASSERT_EQ(0, index.GetTableRecordCount("Resources"));
  ASSERT_EQ(1, index.GetTableRecordCount("GlobalProperties"));
}
