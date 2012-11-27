/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012 Medical Physics Department, CHU of Liege,
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


#include "DatabaseWrapper.h"

#include "../Core/DicomFormat/DicomArray.h"
#include "EmbeddedResources.h"

#include <glog/logging.h>
#include <stdio.h>

namespace Orthanc
{

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

    class SignalRemainingAncestor : public SQLite::IScalarFunction
    {
    private:
      bool hasRemainingAncestor_;
      std::string remainingPublicId_;
      ResourceType remainingType_;

    public:
      void Reset()
      {
        hasRemainingAncestor_ = false;
      }

      virtual const char* GetName() const
      {
        return "SignalRemainingAncestor";
      }

      virtual unsigned int GetCardinality() const
      {
        return 2;
      }

      virtual void Compute(SQLite::FunctionContext& context)
      {
        VLOG(1) << "There exists a remaining ancestor with public ID \""
                << context.GetStringValue(0)
                << "\" of type "
                << context.GetIntValue(1);

        if (!hasRemainingAncestor_ ||
            remainingType_ >= context.GetIntValue(1))
        {
          hasRemainingAncestor_ = true;
          remainingPublicId_ = context.GetStringValue(0);
          remainingType_ = static_cast<ResourceType>(context.GetIntValue(1));
        }
      }

      bool HasRemainingAncestor() const
      {
        return hasRemainingAncestor_;
      }

      const std::string& GetRemainingAncestorId() const
      {
        assert(hasRemainingAncestor_);
        return remainingPublicId_;
      }

      ResourceType GetRemainingAncestorType() const
      {
        assert(hasRemainingAncestor_);
        return remainingType_;
      }
    };
  }


  
  void DatabaseWrapper::SetGlobalProperty(const std::string& name,
                                          const std::string& value)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT OR REPLACE INTO GlobalProperties VALUES(?, ?)");
    s.BindString(0, name);
    s.BindString(1, value);
    s.Run();
  }

  bool DatabaseWrapper::LookupGlobalProperty(std::string& target,
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

  std::string DatabaseWrapper::GetGlobalProperty(const std::string& name,
                                                 const std::string& defaultValue)
  {
    std::string s;
    if (LookupGlobalProperty(s, name))
    {
      return s;
    }
    else
    {
      return defaultValue;
    }
  }

  int64_t DatabaseWrapper::CreateResource(const std::string& publicId,
                                          ResourceType type)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO Resources VALUES(NULL, ?, ?, NULL)");
    s.BindInt(0, type);
    s.BindString(1, publicId);
    s.Run();
    int64_t id = db_.GetLastInsertRowId();

    ChangeType changeType;
    switch (type)
    {
    case ResourceType_Patient: 
      changeType = ChangeType_NewPatient; 
      break;

    case ResourceType_Study: 
      changeType = ChangeType_NewStudy; 
      break;

    case ResourceType_Series: 
      changeType = ChangeType_NewSeries; 
      break;

    case ResourceType_Instance: 
      changeType = ChangeType_NewInstance; 
      break;

    default:
      throw OrthancException(ErrorCode_InternalError);
    }

    LogChange(changeType, id, type);
    return id;
  }

  bool DatabaseWrapper::LookupResource(const std::string& publicId,
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

  void DatabaseWrapper::AttachChild(int64_t parent,
                                    int64_t child)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "UPDATE Resources SET parentId = ? WHERE internalId = ?");
    s.BindInt(0, parent);
    s.BindInt(1, child);
    s.Run();
  }

  void DatabaseWrapper::GetChildren(Json::Value& childrenPublicIds,
                                    int64_t id)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT publicId FROM Resources WHERE parentId=?");
    s.BindInt(0, id);

    childrenPublicIds = Json::arrayValue;
    while (s.Step())
    {
      childrenPublicIds.append(s.ColumnString(0));
    }
  }


  void DatabaseWrapper::DeleteResource(int64_t id)
  {
    signalRemainingAncestor_->Reset();

    SQLite::Statement s(db_, SQLITE_FROM_HERE, "DELETE FROM Resources WHERE internalId=?");
    s.BindInt(0, id);
    s.Run();

    if (signalRemainingAncestor_->HasRemainingAncestor())
    {
      listener_.SignalRemainingAncestor(signalRemainingAncestor_->GetRemainingAncestorType(),
                                        signalRemainingAncestor_->GetRemainingAncestorId());
    }
  }

  void DatabaseWrapper::SetMetadata(int64_t id,
                                    MetadataType type,
                                    const std::string& value)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT OR REPLACE INTO Metadata VALUES(?, ?, ?)");
    s.BindInt(0, id);
    s.BindInt(1, type);
    s.BindString(2, value);
    s.Run();
  }

  bool DatabaseWrapper::LookupMetadata(std::string& target,
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

  std::string DatabaseWrapper::GetMetadata(int64_t id,
                                           MetadataType type,
                                           const std::string& defaultValue)
  {
    std::string s;
    if (LookupMetadata(s, id, type))
    {
      return s;
    }
    else
    {
      return defaultValue;
    }
  }

  void DatabaseWrapper::AttachFile(int64_t id,
                                   AttachedFileType contentType,
                                   const std::string& fileUuid,
                                   uint64_t compressedSize,
                                   uint64_t uncompressedSize,
                                   CompressionType compressionType)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO AttachedFiles VALUES(?, ?, ?, ?, ?, ?)");
    s.BindInt(0, id);
    s.BindInt(1, contentType);
    s.BindString(2, fileUuid);
    s.BindInt(3, compressedSize);
    s.BindInt(4, uncompressedSize);
    s.BindInt(5, compressionType);
    s.Run();
  }

  bool DatabaseWrapper::LookupFile(int64_t id,
                                   AttachedFileType contentType,
                                   std::string& fileUuid,
                                   uint64_t& compressedSize,
                                   uint64_t& uncompressedSize,
                                   CompressionType& compressionType)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                        "SELECT uuid, compressedSize, uncompressedSize, compressionType FROM AttachedFiles WHERE id=? AND fileType=?");
    s.BindInt(0, id);
    s.BindInt(1, contentType);

    if (!s.Step())
    {
      return false;
    }
    else
    {
      fileUuid = s.ColumnString(0);
      compressedSize = s.ColumnInt(1);
      uncompressedSize = s.ColumnInt(2);
      compressionType = static_cast<CompressionType>(s.ColumnInt(3));
      return true;
    }
  }

  void DatabaseWrapper::SetMainDicomTags(int64_t id,
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

  void DatabaseWrapper::GetMainDicomTags(DicomMap& map,
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


  bool DatabaseWrapper::GetParentPublicId(std::string& result,
                                          int64_t id)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT a.publicId FROM Resources AS a, Resources AS b "
                        "WHERE a.internalId = b.parentId AND b.internalId = ?");     
    s.BindInt(0, id);

    if (s.Step())
    {
      result = s.ColumnString(0);
      return true;
    }
    else
    {
      return false;
    }
  }


  void DatabaseWrapper::GetChildrenPublicId(std::list<std::string>& result,
                                            int64_t id)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT a.publicId FROM Resources AS a, Resources AS b  "
                        "WHERE a.parentId = b.internalId AND b.internalId = ?");     
    s.BindInt(0, id);

    result.clear();

    while (s.Step())
    {
      result.push_back(s.ColumnString(0));
    }
  }


  void DatabaseWrapper::LogChange(ChangeType changeType,
                                  int64_t internalId,
                                  ResourceType resourceType,
                                  const boost::posix_time::ptime& date)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO Changes VALUES(NULL, ?, ?, ?, ?)");
    s.BindInt(0, changeType);
    s.BindInt(1, internalId);
    s.BindInt(2, resourceType);
    s.BindString(3, boost::posix_time::to_iso_string(date));
    s.Run();      
  }


  void DatabaseWrapper::LogExportedInstance(const std::string& remoteModality,
                                            DicomInstanceHasher& hasher,
                                            const boost::posix_time::ptime& date)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO ExportedInstances VALUES(NULL, ?, ?, ?, ?, ?, ?)");
    s.BindString(0, remoteModality);
    s.BindString(1, hasher.HashInstance());
    s.BindString(2, hasher.GetPatientId());
    s.BindString(3, hasher.GetStudyUid());
    s.BindString(4, hasher.GetSeriesUid());
    s.BindString(5, hasher.GetInstanceUid());
    s.BindString(6, boost::posix_time::to_iso_string(date));
    s.Run();      
  }
    

  int64_t DatabaseWrapper::GetTableRecordCount(const std::string& table)
  {
    char buf[128];
    sprintf(buf, "SELECT COUNT(*) FROM %s", table.c_str());
    SQLite::Statement s(db_, buf);

    assert(s.Step());
    int64_t c = s.ColumnInt(0);
    assert(!s.Step());

    return c;
  }

    
  uint64_t DatabaseWrapper::GetTotalCompressedSize()
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT SUM(compressedSize) FROM AttachedFiles");
    s.Run();
    return static_cast<uint64_t>(s.ColumnInt64(0));
  }

    
  uint64_t DatabaseWrapper::GetTotalUncompressedSize()
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT SUM(uncompressedSize) FROM AttachedFiles");
    s.Run();
    return static_cast<uint64_t>(s.ColumnInt64(0));
  }

  void DatabaseWrapper::GetAllPublicIds(Json::Value& target,
                                        ResourceType resourceType)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT publicId FROM Resources WHERE resourceType=?");
    s.BindInt(0, resourceType);

    target = Json::arrayValue;
    while (s.Step())
    {
      target.append(s.ColumnString(0));
    }
  }


  DatabaseWrapper::DatabaseWrapper(const std::string& path,
                                   IServerIndexListener& listener) :
    listener_(listener)
  {
    db_.Open(path);
    Open();
  }

  DatabaseWrapper::DatabaseWrapper(IServerIndexListener& listener) :
    listener_(listener)
  {
    db_.OpenInMemory();
    Open();
  }

  void DatabaseWrapper::Open()
  {
    if (!db_.DoesTableExist("GlobalProperties"))
    {
      LOG(INFO) << "Creating the database";
      std::string query;
      EmbeddedResources::GetFileResource(query, EmbeddedResources::PREPARE_DATABASE_2);
      db_.Execute(query);
    }

    signalRemainingAncestor_ = new Internals::SignalRemainingAncestor;
    db_.Register(signalRemainingAncestor_);
    db_.Register(new Internals::SignalFileDeleted(listener_));
  }
}
