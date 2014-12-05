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


#include "PrecompiledHeadersServer.h"
#include "DatabaseWrapper.h"

#include "../Core/DicomFormat/DicomArray.h"
#include "../Core/Uuid.h"
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
        return 7;
      }

      virtual void Compute(SQLite::FunctionContext& context)
      {
        std::string uncompressedMD5, compressedMD5;

        if (!context.IsNullValue(5))
        {
          uncompressedMD5 = context.GetStringValue(5);
        }

        if (!context.IsNullValue(6))
        {
          compressedMD5 = context.GetStringValue(6);
        }

        FileInfo info(context.GetStringValue(0),
                      static_cast<FileContentType>(context.GetIntValue(1)),
                      static_cast<uint64_t>(context.GetInt64Value(2)),
                      uncompressedMD5,
                      static_cast<CompressionType>(context.GetIntValue(3)),
                      static_cast<uint64_t>(context.GetInt64Value(4)),
                      compressedMD5);
        
        listener_.SignalFileDeleted(info);
      }
    };

    class SignalResourceDeleted : public SQLite::IScalarFunction
    {
    private:
      IServerIndexListener& listener_;

    public:
      SignalResourceDeleted(IServerIndexListener& listener) :
        listener_(listener)
      {
      }

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
        ResourceType type = static_cast<ResourceType>(context.GetIntValue(1));
        ServerIndexChange change(ChangeType_Deleted, type, context.GetStringValue(0));
        listener_.SignalChange(change);
      }
    };

    class SignalRemainingAncestor : public SQLite::IScalarFunction
    {
    private:
      bool hasRemainingAncestor_;
      std::string remainingPublicId_;
      ResourceType remainingType_;

    public:
      SignalRemainingAncestor() : 
        hasRemainingAncestor_(false)
      {
      }

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


  
  void DatabaseWrapper::SetGlobalProperty(GlobalProperty property,
                                          const std::string& value)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT OR REPLACE INTO GlobalProperties VALUES(?, ?)");
    s.BindInt(0, property);
    s.BindString(1, value);
    s.Run();
  }

  bool DatabaseWrapper::LookupGlobalProperty(std::string& target,
                                             GlobalProperty property)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                        "SELECT value FROM GlobalProperties WHERE property=?");
    s.BindInt(0, property);

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

  std::string DatabaseWrapper::GetGlobalProperty(GlobalProperty property,
                                                 const std::string& defaultValue)
  {
    std::string s;
    if (LookupGlobalProperty(s, property))
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

    ServerIndexChange change(changeType, type, publicId);
    LogChange(id, change);
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

  bool DatabaseWrapper::LookupParent(int64_t& parentId,
                                     int64_t resourceId)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                        "SELECT parentId FROM Resources WHERE internalId=?");
    s.BindInt64(0, resourceId);

    if (!s.Step())
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }

    if (s.ColumnIsNull(0))
    {
      return false;
    }
    else
    {
      parentId = s.ColumnInt(0);
      return true;
    }
  }

  std::string DatabaseWrapper::GetPublicId(int64_t resourceId)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                        "SELECT publicId FROM Resources WHERE internalId=?");
    s.BindInt64(0, resourceId);
    
    if (!s.Step())
    { 
      throw OrthancException(ErrorCode_UnknownResource);
    }

    return s.ColumnString(0);
  }


  ResourceType DatabaseWrapper::GetResourceType(int64_t resourceId)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                        "SELECT resourceType FROM Resources WHERE internalId=?");
    s.BindInt64(0, resourceId);
    
    if (!s.Step())
    { 
      throw OrthancException(ErrorCode_UnknownResource);
    }

    return static_cast<ResourceType>(s.ColumnInt(0));
  }


  void DatabaseWrapper::AttachChild(int64_t parent,
                                    int64_t child)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "UPDATE Resources SET parentId = ? WHERE internalId = ?");
    s.BindInt64(0, parent);
    s.BindInt64(1, child);
    s.Run();
  }

  void DatabaseWrapper::GetChildren(Json::Value& childrenPublicIds,
                                    int64_t id)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT publicId FROM Resources WHERE parentId=?");
    s.BindInt64(0, id);

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
    s.BindInt64(0, id);
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
    s.BindInt64(0, id);
    s.BindInt(1, type);
    s.BindString(2, value);
    s.Run();
  }

  void DatabaseWrapper::DeleteMetadata(int64_t id,
                                       MetadataType type)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "DELETE FROM Metadata WHERE id=? and type=?");
    s.BindInt64(0, id);
    s.BindInt(1, type);
    s.Run();
  }

  bool DatabaseWrapper::LookupMetadata(std::string& target,
                                       int64_t id,
                                       MetadataType type)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                        "SELECT value FROM Metadata WHERE id=? AND type=?");
    s.BindInt64(0, id);
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

  void DatabaseWrapper::ListAvailableMetadata(std::list<MetadataType>& target,
                                              int64_t id)
  {
    target.clear();

    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT type FROM Metadata WHERE id=?");
    s.BindInt64(0, id);

    while (s.Step())
    {
      target.push_back(static_cast<MetadataType>(s.ColumnInt(0)));
    }
  }


  void DatabaseWrapper::AddAttachment(int64_t id,
                                      const FileInfo& attachment)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO AttachedFiles VALUES(?, ?, ?, ?, ?, ?, ?, ?)");
    s.BindInt64(0, id);
    s.BindInt(1, attachment.GetContentType());
    s.BindString(2, attachment.GetUuid());
    s.BindInt64(3, attachment.GetCompressedSize());
    s.BindInt64(4, attachment.GetUncompressedSize());
    s.BindInt(5, attachment.GetCompressionType());
    s.BindString(6, attachment.GetUncompressedMD5());
    s.BindString(7, attachment.GetCompressedMD5());
    s.Run();
  }


  void DatabaseWrapper::DeleteAttachment(int64_t id,
                                         FileContentType attachment)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "DELETE FROM AttachedFiles WHERE id=? AND fileType=?");
    s.BindInt64(0, id);
    s.BindInt(1, attachment);
    s.Run();
  }



  void DatabaseWrapper::ListAvailableAttachments(std::list<FileContentType>& result,
                                                 int64_t id)
  {
    result.clear();

    SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                        "SELECT fileType FROM AttachedFiles WHERE id=?");
    s.BindInt64(0, id);

    while (s.Step())
    {
      result.push_back(static_cast<FileContentType>(s.ColumnInt(0)));
    }
  }

  bool DatabaseWrapper::LookupAttachment(FileInfo& attachment,
                                         int64_t id,
                                         FileContentType contentType)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                        "SELECT uuid, uncompressedSize, compressionType, compressedSize, uncompressedMD5, compressedMD5 FROM AttachedFiles WHERE id=? AND fileType=?");
    s.BindInt64(0, id);
    s.BindInt(1, contentType);

    if (!s.Step())
    {
      return false;
    }
    else
    {
      attachment = FileInfo(s.ColumnString(0),
                            contentType,
                            s.ColumnInt64(1),
                            s.ColumnString(4),
                            static_cast<CompressionType>(s.ColumnInt(2)),
                            s.ColumnInt64(3),
                            s.ColumnString(5));
      return true;
    }
  }


  static void SetMainDicomTagsInternal(SQLite::Statement& s,
                                       int64_t id,
                                       const DicomElement& element)
  {
    s.BindInt64(0, id);
    s.BindInt(1, element.GetTag().GetGroup());
    s.BindInt(2, element.GetTag().GetElement());
    s.BindString(3, element.GetValue().AsString());
    s.Run();
  }


  void DatabaseWrapper::SetMainDicomTags(int64_t id,
                                         const DicomMap& tags)
  {
    DicomArray flattened(tags);
    for (size_t i = 0; i < flattened.GetSize(); i++)
    {
      if (flattened.GetElement(i).GetTag().IsIdentifier())
      {
        SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO DicomIdentifiers VALUES(?, ?, ?, ?)");
        SetMainDicomTagsInternal(s, id, flattened.GetElement(i));
      }
      else
      {
        SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO MainDicomTags VALUES(?, ?, ?, ?)");
        SetMainDicomTagsInternal(s, id, flattened.GetElement(i));
      }
    }
  }

  void DatabaseWrapper::GetMainDicomTags(DicomMap& map,
                                         int64_t id)
  {
    map.Clear();

    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT * FROM MainDicomTags WHERE id=?");
    s.BindInt64(0, id);
    while (s.Step())
    {
      map.SetValue(s.ColumnInt(1),
                   s.ColumnInt(2),
                   s.ColumnString(3));
    }

    SQLite::Statement s2(db_, SQLITE_FROM_HERE, "SELECT * FROM DicomIdentifiers WHERE id=?");
    s2.BindInt64(0, id);
    while (s2.Step())
    {
      map.SetValue(s2.ColumnInt(1),
                   s2.ColumnInt(2),
                   s2.ColumnString(3));
    }
  }


  bool DatabaseWrapper::GetParentPublicId(std::string& result,
                                          int64_t id)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT a.publicId FROM Resources AS a, Resources AS b "
                        "WHERE a.internalId = b.parentId AND b.internalId = ?");     
    s.BindInt64(0, id);

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
    s.BindInt64(0, id);

    result.clear();

    while (s.Step())
    {
      result.push_back(s.ColumnString(0));
    }
  }


  void DatabaseWrapper::GetChildrenInternalId(std::list<int64_t>& result,
                                              int64_t id)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT a.internalId FROM Resources AS a, Resources AS b  "
                        "WHERE a.parentId = b.internalId AND b.internalId = ?");     
    s.BindInt64(0, id);

    result.clear();

    while (s.Step())
    {
      result.push_back(s.ColumnInt64(0));
    }
  }


  void DatabaseWrapper::LogChange(int64_t internalId,
                                  const ServerIndexChange& change)
  {
    if (change.GetChangeType() <= ChangeType_INTERNAL_LastLogged)
    {
      const boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();

      SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO Changes VALUES(NULL, ?, ?, ?, ?)");
      s.BindInt(0, change.GetChangeType());
      s.BindInt64(1, internalId);
      s.BindInt(2, change.GetResourceType());
      s.BindString(3, boost::posix_time::to_iso_string(now));
      s.Run();
    }

    listener_.SignalChange(change);
  }


  void DatabaseWrapper::GetChangesInternal(Json::Value& target,
                                           SQLite::Statement& s,
                                           int64_t since,
                                           unsigned int maxResults)
  {
    Json::Value changes = Json::arrayValue;
    int64_t last = since;

    while (changes.size() < maxResults && s.Step())
    {
      int64_t seq = s.ColumnInt64(0);
      ChangeType changeType = static_cast<ChangeType>(s.ColumnInt(1));
      int64_t internalId = s.ColumnInt64(2);
      ResourceType resourceType = static_cast<ResourceType>(s.ColumnInt(3));
      const std::string& date = s.ColumnString(4);
      std::string publicId = GetPublicId(internalId);

      Json::Value item = Json::objectValue;
      item["Seq"] = static_cast<int>(seq);
      item["ChangeType"] = EnumerationToString(changeType);
      item["ResourceType"] = EnumerationToString(resourceType);
      item["ID"] = publicId;
      item["Path"] = GetBasePath(resourceType, publicId);
      item["Date"] = date;
      last = seq;

      changes.append(item);
    }

    target = Json::objectValue;
    target["Changes"] = changes;
    target["Done"] = !(changes.size() == maxResults && s.Step());
    target["Last"] = static_cast<int>(last);
  }


  void DatabaseWrapper::GetChanges(Json::Value& target,
                                   int64_t since,
                                   unsigned int maxResults)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT * FROM Changes WHERE seq>? ORDER BY seq LIMIT ?");
    s.BindInt64(0, since);
    s.BindInt(1, maxResults + 1);
    GetChangesInternal(target, s, since, maxResults);
  }

  void DatabaseWrapper::GetLastChange(Json::Value& target)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT * FROM Changes ORDER BY seq DESC LIMIT 1");
    GetChangesInternal(target, s, 0, 1);
  }


  void DatabaseWrapper::LogExportedResource(ResourceType resourceType,
                                            const std::string& publicId,
                                            const std::string& remoteModality,
                                            const std::string& patientId,
                                            const std::string& studyInstanceUid,
                                            const std::string& seriesInstanceUid,
                                            const std::string& sopInstanceUid,
                                            const boost::posix_time::ptime& date)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                        "INSERT INTO ExportedResources VALUES(NULL, ?, ?, ?, ?, ?, ?, ?, ?)");

    s.BindInt(0, resourceType);
    s.BindString(1, publicId);
    s.BindString(2, remoteModality);
    s.BindString(3, patientId);
    s.BindString(4, studyInstanceUid);
    s.BindString(5, seriesInstanceUid);
    s.BindString(6, sopInstanceUid);
    s.BindString(7, boost::posix_time::to_iso_string(date));

    s.Run();      
  }


  void DatabaseWrapper::GetExportedResourcesInternal(Json::Value& target,
                                                     SQLite::Statement& s,
                                                     int64_t since,
                                                     unsigned int maxResults)
  {
    Json::Value changes = Json::arrayValue;
    int64_t last = since;

    while (changes.size() < maxResults && s.Step())
    {
      int64_t seq = s.ColumnInt64(0);
      ResourceType resourceType = static_cast<ResourceType>(s.ColumnInt(1));
      std::string publicId = s.ColumnString(2);

      Json::Value item = Json::objectValue;
      item["Seq"] = static_cast<int>(seq);
      item["ResourceType"] = EnumerationToString(resourceType);
      item["ID"] = publicId;
      item["Path"] = GetBasePath(resourceType, publicId);
      item["RemoteModality"] = s.ColumnString(3);
      item["Date"] = s.ColumnString(8);

      // WARNING: Do not add "break" below and do not reorder the case items!
      switch (resourceType)
      {
      case ResourceType_Instance:
        item["SopInstanceUid"] = s.ColumnString(7);

      case ResourceType_Series:
        item["SeriesInstanceUid"] = s.ColumnString(6);

      case ResourceType_Study:
        item["StudyInstanceUid"] = s.ColumnString(5);

      case ResourceType_Patient:
        item["PatientId"] = s.ColumnString(4);
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
      }

      last = seq;

      changes.append(item);
    }

    target = Json::objectValue;
    target["Exports"] = changes;
    target["Done"] = !(changes.size() == maxResults && s.Step());
    target["Last"] = static_cast<int>(last);
  }


  void DatabaseWrapper::GetExportedResources(Json::Value& target,
                                             int64_t since,
                                             unsigned int maxResults)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                        "SELECT * FROM ExportedResources WHERE seq>? ORDER BY seq LIMIT ?");
    s.BindInt64(0, since);
    s.BindInt(1, maxResults + 1);
    GetExportedResourcesInternal(target, s, since, maxResults);
  }

    
  void DatabaseWrapper::GetLastExportedResource(Json::Value& target)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                        "SELECT * FROM ExportedResources ORDER BY seq DESC LIMIT 1");
    GetExportedResourcesInternal(target, s, 0, 1);
  }


    

  int64_t DatabaseWrapper::GetTableRecordCount(const std::string& table)
  {
    char buf[128];
    sprintf(buf, "SELECT COUNT(*) FROM %s", table.c_str());
    SQLite::Statement s(db_, buf);

    if (!s.Step())
    {
      throw OrthancException(ErrorCode_InternalError);
    }

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

  static void UpgradeDatabase(SQLite::Connection& db,
                              EmbeddedResources::FileResourceId script)
  {
    std::string upgrade;
    EmbeddedResources::GetFileResource(upgrade, script);
    db.BeginTransaction();
    db.Execute(upgrade);
    db.CommitTransaction();    
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
    // Performance tuning of SQLite with PRAGMAs
    // http://www.sqlite.org/pragma.html
    db_.Execute("PRAGMA SYNCHRONOUS=NORMAL;");
    db_.Execute("PRAGMA JOURNAL_MODE=WAL;");
    db_.Execute("PRAGMA LOCKING_MODE=EXCLUSIVE;");
    db_.Execute("PRAGMA WAL_AUTOCHECKPOINT=1000;");
    //db_.Execute("PRAGMA TEMP_STORE=memory");

    if (!db_.DoesTableExist("GlobalProperties"))
    {
      LOG(INFO) << "Creating the database";
      std::string query;
      EmbeddedResources::GetFileResource(query, EmbeddedResources::PREPARE_DATABASE);
      db_.Execute(query);
    }

    // Check the version of the database
    std::string version = GetGlobalProperty(GlobalProperty_DatabaseSchemaVersion, "Unknown");
    bool ok = false;
    try
    {
      LOG(INFO) << "Version of the Orthanc database: " << version;
      unsigned int v = boost::lexical_cast<unsigned int>(version);

      /**
       * History of the database versions:
       *  - Orthanc before Orthanc 0.3.0 (inclusive) had no version
       *  - Version 2: only Orthanc 0.3.1
       *  - Version 3: from Orthanc 0.3.2 to Orthanc 0.7.2 (inclusive)
       *  - Version 4: from Orthanc 0.7.3 to Orthanc 0.8.4 (inclusive)
       *  - Version 5: from Orthanc 0.8.5 (inclusive)
       **/

      // This version of Orthanc is only compatible with versions 3, 4 and 5 of the DB schema
      ok = (v == 3 || v == 4 || v == 5);

      if (v == 3)
      {
        LOG(WARNING) << "Upgrading database version from 3 to 4";
        UpgradeDatabase(db_, EmbeddedResources::UPGRADE_DATABASE_3_TO_4);
        v = 4;
      }

      if (v == 4)
      {
        LOG(WARNING) << "Upgrading database version from 4 to 5";
        UpgradeDatabase(db_, EmbeddedResources::UPGRADE_DATABASE_4_TO_5);
        v = 5;
      }
    }
    catch (boost::bad_lexical_cast&)
    {
      ok = false;
    }

    if (!ok)
    {
      LOG(ERROR) << "Incompatible version of the Orthanc database: " << version;
      throw OrthancException(ErrorCode_IncompatibleDatabaseVersion);
    }

    signalRemainingAncestor_ = new Internals::SignalRemainingAncestor;
    db_.Register(signalRemainingAncestor_);
    db_.Register(new Internals::SignalFileDeleted(listener_));
    db_.Register(new Internals::SignalResourceDeleted(listener_));
  }

  uint64_t DatabaseWrapper::GetResourceCount(ResourceType resourceType)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                        "SELECT COUNT(*) FROM Resources WHERE resourceType=?");
    s.BindInt(0, resourceType);
    
    if (!s.Step())
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    int64_t c = s.ColumnInt(0);
    assert(!s.Step());

    return c;
  }

  bool DatabaseWrapper::SelectPatientToRecycle(int64_t& internalId)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE,
                        "SELECT patientId FROM PatientRecyclingOrder ORDER BY seq ASC LIMIT 1");
   
    if (!s.Step())
    {
      // No patient remaining or all the patients are protected
      return false;
    }
    else
    {
      internalId = s.ColumnInt(0);
      return true;
    }    
  }

  bool DatabaseWrapper::SelectPatientToRecycle(int64_t& internalId,
                                               int64_t patientIdToAvoid)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE,
                        "SELECT patientId FROM PatientRecyclingOrder "
                        "WHERE patientId != ? ORDER BY seq ASC LIMIT 1");
    s.BindInt64(0, patientIdToAvoid);

    if (!s.Step())
    {
      // No patient remaining or all the patients are protected
      return false;
    }
    else
    {
      internalId = s.ColumnInt(0);
      return true;
    }   
  }

  bool DatabaseWrapper::IsProtectedPatient(int64_t internalId)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE,
                        "SELECT * FROM PatientRecyclingOrder WHERE patientId = ?");
    s.BindInt64(0, internalId);
    return !s.Step();
  }

  void DatabaseWrapper::SetProtectedPatient(int64_t internalId, 
                                            bool isProtected)
  {
    if (isProtected)
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, "DELETE FROM PatientRecyclingOrder WHERE patientId=?");
      s.BindInt64(0, internalId);
      s.Run();
    }
    else if (IsProtectedPatient(internalId))
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO PatientRecyclingOrder VALUES(NULL, ?)");
      s.BindInt64(0, internalId);
      s.Run();
    }
    else
    {
      // Nothing to do: The patient is already unprotected
    }
  }



  void DatabaseWrapper::ClearTable(const std::string& tableName)
  {
    db_.Execute("DELETE FROM " + tableName);    
  }


  bool DatabaseWrapper::IsExistingResource(int64_t internalId)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                        "SELECT * FROM Resources WHERE internalId=?");
    s.BindInt64(0, internalId);
    return s.Step();
  }


  void  DatabaseWrapper::LookupIdentifier(std::list<int64_t>& result,
                                          const DicomTag& tag,
                                          const std::string& value)
  {
    if (!tag.IsIdentifier())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                        "SELECT id FROM DicomIdentifiers WHERE tagGroup=? AND tagElement=? and value=?");

    s.BindInt(0, tag.GetGroup());
    s.BindInt(1, tag.GetElement());
    s.BindString(2, value);

    result.clear();

    while (s.Step())
    {
      result.push_back(s.ColumnInt64(0));
    }
  }


  void  DatabaseWrapper::LookupIdentifier(std::list<int64_t>& result,
                                          const std::string& value)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                        "SELECT id FROM DicomIdentifiers WHERE value=?");

    s.BindString(0, value);

    result.clear();

    while (s.Step())
    {
      result.push_back(s.ColumnInt64(0));
    }
  }


  void DatabaseWrapper::GetAllMetadata(std::map<MetadataType, std::string>& result,
                                       int64_t id)
  {
    result.clear();

    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT type, value FROM Metadata WHERE id=?");
    s.BindInt64(0, id);

    while (s.Step())
    {
      MetadataType key = static_cast<MetadataType>(s.ColumnInt(0));
      result[key] = s.ColumnString(1);
    }
  }

}
