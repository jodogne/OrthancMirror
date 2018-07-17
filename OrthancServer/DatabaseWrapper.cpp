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


#include "PrecompiledHeadersServer.h"
#include "DatabaseWrapper.h"

#include "../Core/DicomFormat/DicomArray.h"
#include "../Core/Logging.h"
#include "EmbeddedResources.h"
#include "ServerToolbox.h"

#include <stdio.h>
#include <boost/lexical_cast.hpp>

namespace Orthanc
{
  namespace Internals
  {
    class SignalFileDeleted : public SQLite::IScalarFunction
    {
    private:
      IDatabaseListener& listener_;

    public:
      SignalFileDeleted(IDatabaseListener& listener) :
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
      IDatabaseListener& listener_;

    public:
      SignalResourceDeleted(IDatabaseListener& listener) :
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


  void DatabaseWrapper::GetChangesInternal(std::list<ServerIndexChange>& target,
                                           bool& done,
                                           SQLite::Statement& s,
                                           uint32_t maxResults)
  {
    target.clear();

    while (target.size() < maxResults && s.Step())
    {
      int64_t seq = s.ColumnInt64(0);
      ChangeType changeType = static_cast<ChangeType>(s.ColumnInt(1));
      ResourceType resourceType = static_cast<ResourceType>(s.ColumnInt(3));
      const std::string& date = s.ColumnString(4);

      int64_t internalId = s.ColumnInt64(2);
      std::string publicId = GetPublicId(internalId);

      target.push_back(ServerIndexChange(seq, changeType, resourceType, publicId, date));
    }

    done = !(target.size() == maxResults && s.Step());
  }


  void DatabaseWrapper::GetExportedResourcesInternal(std::list<ExportedResource>& target,
                                                     bool& done,
                                                     SQLite::Statement& s,
                                                     uint32_t maxResults)
  {
    target.clear();

    while (target.size() < maxResults && s.Step())
    {
      int64_t seq = s.ColumnInt64(0);
      ResourceType resourceType = static_cast<ResourceType>(s.ColumnInt(1));
      std::string publicId = s.ColumnString(2);

      ExportedResource resource(seq, 
                                resourceType,
                                publicId,
                                s.ColumnString(3),  // modality
                                s.ColumnString(8),  // date
                                s.ColumnString(4),  // patient ID
                                s.ColumnString(5),  // study instance UID
                                s.ColumnString(6),  // series instance UID
                                s.ColumnString(7)); // sop instance UID

      target.push_back(resource);
    }

    done = !(target.size() == maxResults && s.Step());
  }


  void DatabaseWrapper::GetChildren(std::list<std::string>& childrenPublicIds,
                                    int64_t id)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT publicId FROM Resources WHERE parentId=?");
    s.BindInt64(0, id);

    childrenPublicIds.clear();
    while (s.Step())
    {
      childrenPublicIds.push_back(s.ColumnString(0));
    }
  }


  void DatabaseWrapper::DeleteResource(int64_t id)
  {
    signalRemainingAncestor_->Reset();

    SQLite::Statement s(db_, SQLITE_FROM_HERE, "DELETE FROM Resources WHERE internalId=?");
    s.BindInt64(0, id);
    s.Run();

    if (signalRemainingAncestor_->HasRemainingAncestor() &&
        listener_ != NULL)
    {
      listener_->SignalRemainingAncestor(signalRemainingAncestor_->GetRemainingAncestorType(),
                                         signalRemainingAncestor_->GetRemainingAncestorId());
    }
  }


  bool DatabaseWrapper::GetParentPublicId(std::string& target,
                                          int64_t id)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT a.publicId FROM Resources AS a, Resources AS b "
                        "WHERE a.internalId = b.parentId AND b.internalId = ?");     
    s.BindInt64(0, id);

    if (s.Step())
    {
      target = s.ColumnString(0);
      return true;
    }
    else
    {
      return false;
    }
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

    
  DatabaseWrapper::DatabaseWrapper(const std::string& path) : 
    listener_(NULL), 
    signalRemainingAncestor_(NULL),
    version_(0)
  {
    db_.Open(path);
  }


  DatabaseWrapper::DatabaseWrapper() : 
    listener_(NULL), 
    signalRemainingAncestor_(NULL),
    version_(0)
  {
    db_.OpenInMemory();
  }


  void DatabaseWrapper::Open()
  {
    db_.Execute("PRAGMA ENCODING=\"UTF-8\";");

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
    std::string tmp;
    if (!LookupGlobalProperty(tmp, GlobalProperty_DatabaseSchemaVersion))
    {
      tmp = "Unknown";
    }

    bool ok = false;
    try
    {
      LOG(INFO) << "Version of the Orthanc database: " << tmp;
      version_ = boost::lexical_cast<unsigned int>(tmp);
      ok = true;
    }
    catch (boost::bad_lexical_cast&)
    {
    }

    if (!ok)
    {
      LOG(ERROR) << "Incompatible version of the Orthanc database: " << tmp;
      throw OrthancException(ErrorCode_IncompatibleDatabaseVersion);
    }

    signalRemainingAncestor_ = new Internals::SignalRemainingAncestor;
    db_.Register(signalRemainingAncestor_);
  }


  static void ExecuteUpgradeScript(SQLite::Connection& db,
                                   EmbeddedResources::FileResourceId script)
  {
    std::string upgrade;
    EmbeddedResources::GetFileResource(upgrade, script);
    db.BeginTransaction();
    db.Execute(upgrade);
    db.CommitTransaction();    
  }


  void DatabaseWrapper::Upgrade(unsigned int targetVersion,
                                IStorageArea& storageArea)
  {
    if (targetVersion != 6)
    {
      throw OrthancException(ErrorCode_IncompatibleDatabaseVersion);
    }

    // This version of Orthanc is only compatible with versions 3, 4,
    // 5 and 6 of the DB schema
    if (version_ != 3 &&
        version_ != 4 &&
        version_ != 5 &&
        version_ != 6)
    {
      throw OrthancException(ErrorCode_IncompatibleDatabaseVersion);
    }

    if (version_ == 3)
    {
      LOG(WARNING) << "Upgrading database version from 3 to 4";
      ExecuteUpgradeScript(db_, EmbeddedResources::UPGRADE_DATABASE_3_TO_4);
      version_ = 4;
    }

    if (version_ == 4)
    {
      LOG(WARNING) << "Upgrading database version from 4 to 5";
      ExecuteUpgradeScript(db_, EmbeddedResources::UPGRADE_DATABASE_4_TO_5);
      version_ = 5;
    }

    if (version_ == 5)
    {
      LOG(WARNING) << "Upgrading database version from 5 to 6";
      // No change in the DB schema, the step from version 5 to 6 only
      // consists in reconstructing the main DICOM tags information
      // (as more tags got included).
      db_.BeginTransaction();
      ServerToolbox::ReconstructMainDicomTags(*this, storageArea, ResourceType_Patient);
      ServerToolbox::ReconstructMainDicomTags(*this, storageArea, ResourceType_Study);
      ServerToolbox::ReconstructMainDicomTags(*this, storageArea, ResourceType_Series);
      ServerToolbox::ReconstructMainDicomTags(*this, storageArea, ResourceType_Instance);
      db_.Execute("UPDATE GlobalProperties SET value=\"6\" WHERE property=" +
                  boost::lexical_cast<std::string>(GlobalProperty_DatabaseSchemaVersion) + ";");
      db_.CommitTransaction();
      version_ = 6;
    }
  }


  void DatabaseWrapper::SetListener(IDatabaseListener& listener)
  {
    listener_ = &listener;
    db_.Register(new Internals::SignalFileDeleted(listener));
    db_.Register(new Internals::SignalResourceDeleted(listener));
  }


  void DatabaseWrapper::ClearTable(const std::string& tableName)
  {
    db_.Execute("DELETE FROM " + tableName);    
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


  ResourceType DatabaseWrapper::GetResourceType(int64_t resourceId)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                        "SELECT resourceType FROM Resources WHERE internalId=?");
    s.BindInt64(0, resourceId);
    
    if (s.Step())
    {
      return static_cast<ResourceType>(s.ColumnInt(0));
    }
    else
    { 
      throw OrthancException(ErrorCode_UnknownResource);
    }
  }


  std::string DatabaseWrapper::GetPublicId(int64_t resourceId)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                        "SELECT publicId FROM Resources WHERE internalId=?");
    s.BindInt64(0, resourceId);
    
    if (s.Step())
    { 
      return s.ColumnString(0);
    }
    else
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }
  }


  void DatabaseWrapper::GetChanges(std::list<ServerIndexChange>& target /*out*/,
                                   bool& done /*out*/,
                                   int64_t since,
                                   uint32_t maxResults)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT * FROM Changes WHERE seq>? ORDER BY seq LIMIT ?");
    s.BindInt64(0, since);
    s.BindInt(1, maxResults + 1);
    GetChangesInternal(target, done, s, maxResults);
  }


  void DatabaseWrapper::GetLastChange(std::list<ServerIndexChange>& target /*out*/)
  {
    bool done;  // Ignored
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT * FROM Changes ORDER BY seq DESC LIMIT 1");
    GetChangesInternal(target, done, s, 1);
  }


  void DatabaseWrapper::GetAllMetadata(std::map<MetadataType, std::string>& target,
                                       int64_t id)
  {
    target.clear();

    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT type, value FROM Metadata WHERE id=?");
    s.BindInt64(0, id);

    while (s.Step())
    {
      MetadataType key = static_cast<MetadataType>(s.ColumnInt(0));
      target[key] = s.ColumnString(1);
    }
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


  int64_t DatabaseWrapper::CreateResource(const std::string& publicId,
                                          ResourceType type)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO Resources VALUES(NULL, ?, ?, NULL)");
    s.BindInt(0, type);
    s.BindString(1, publicId);
    s.Run();
    return db_.GetLastInsertRowId();
  }


  bool DatabaseWrapper::LookupResource(int64_t& id,
                                       ResourceType& type,
                                       const std::string& publicId)
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
    s.BindInt64(0, parent);
    s.BindInt64(1, child);
    s.Run();
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


  void DatabaseWrapper::ListAvailableAttachments(std::list<FileContentType>& target,
                                                 int64_t id)
  {
    target.clear();

    SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                        "SELECT fileType FROM AttachedFiles WHERE id=?");
    s.BindInt64(0, id);

    while (s.Step())
    {
      target.push_back(static_cast<FileContentType>(s.ColumnInt(0)));
    }
  }

  bool DatabaseWrapper::LookupAttachment(FileInfo& attachment,
                                         int64_t id,
                                         FileContentType contentType)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                        "SELECT uuid, uncompressedSize, compressionType, compressedSize, "
                        "uncompressedMD5, compressedMD5 FROM AttachedFiles WHERE id=? AND fileType=?");
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


  void DatabaseWrapper::ClearMainDicomTags(int64_t id)
  {
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, "DELETE FROM DicomIdentifiers WHERE id=?");
      s.BindInt64(0, id);
      s.Run();
    }

    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, "DELETE FROM MainDicomTags WHERE id=?");
      s.BindInt64(0, id);
      s.Run();
    }
  }


  void DatabaseWrapper::SetMainDicomTag(int64_t id,
                                        const DicomTag& tag,
                                        const std::string& value)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO MainDicomTags VALUES(?, ?, ?, ?)");
    s.BindInt64(0, id);
    s.BindInt(1, tag.GetGroup());
    s.BindInt(2, tag.GetElement());
    s.BindString(3, value);
    s.Run();
  }


  void DatabaseWrapper::SetIdentifierTag(int64_t id,
                                         const DicomTag& tag,
                                         const std::string& value)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO DicomIdentifiers VALUES(?, ?, ?, ?)");
    s.BindInt64(0, id);
    s.BindInt(1, tag.GetGroup());
    s.BindInt(2, tag.GetElement());
    s.BindString(3, value);
    s.Run();
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
                   s.ColumnString(3), false);
    }
  }


  void DatabaseWrapper::GetChildrenPublicId(std::list<std::string>& target,
                                            int64_t id)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT a.publicId FROM Resources AS a, Resources AS b  "
                        "WHERE a.parentId = b.internalId AND b.internalId = ?");     
    s.BindInt64(0, id);

    target.clear();

    while (s.Step())
    {
      target.push_back(s.ColumnString(0));
    }
  }


  void DatabaseWrapper::GetChildrenInternalId(std::list<int64_t>& target,
                                              int64_t id)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT a.internalId FROM Resources AS a, Resources AS b  "
                        "WHERE a.parentId = b.internalId AND b.internalId = ?");     
    s.BindInt64(0, id);

    target.clear();

    while (s.Step())
    {
      target.push_back(s.ColumnInt64(0));
    }
  }


  void DatabaseWrapper::LogChange(int64_t internalId,
                                  const ServerIndexChange& change)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO Changes VALUES(NULL, ?, ?, ?, ?)");
    s.BindInt(0, change.GetChangeType());
    s.BindInt64(1, internalId);
    s.BindInt(2, change.GetResourceType());
    s.BindString(3, change.GetDate());
    s.Run();
  }


  void DatabaseWrapper::LogExportedResource(const ExportedResource& resource)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                        "INSERT INTO ExportedResources VALUES(NULL, ?, ?, ?, ?, ?, ?, ?, ?)");

    s.BindInt(0, resource.GetResourceType());
    s.BindString(1, resource.GetPublicId());
    s.BindString(2, resource.GetModality());
    s.BindString(3, resource.GetPatientId());
    s.BindString(4, resource.GetStudyInstanceUid());
    s.BindString(5, resource.GetSeriesInstanceUid());
    s.BindString(6, resource.GetSopInstanceUid());
    s.BindString(7, resource.GetDate());
    s.Run();      
  }


  void DatabaseWrapper::GetExportedResources(std::list<ExportedResource>& target,
                                             bool& done,
                                             int64_t since,
                                             uint32_t maxResults)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                        "SELECT * FROM ExportedResources WHERE seq>? ORDER BY seq LIMIT ?");
    s.BindInt64(0, since);
    s.BindInt(1, maxResults + 1);
    GetExportedResourcesInternal(target, done, s, maxResults);
  }

    
  void DatabaseWrapper::GetLastExportedResource(std::list<ExportedResource>& target)
  {
    bool done;  // Ignored
    SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                        "SELECT * FROM ExportedResources ORDER BY seq DESC LIMIT 1");
    GetExportedResourcesInternal(target, done, s, 1);
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


  uint64_t DatabaseWrapper::GetResourceCount(ResourceType resourceType)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                        "SELECT COUNT(*) FROM Resources WHERE resourceType=?");
    s.BindInt(0, resourceType);
    
    if (!s.Step())
    {
      return 0;
    }
    else
    {
      int64_t c = s.ColumnInt(0);
      assert(!s.Step());
      return c;
    }
  }


  void DatabaseWrapper::GetAllInternalIds(std::list<int64_t>& target,
                                          ResourceType resourceType)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT internalId FROM Resources WHERE resourceType=?");
    s.BindInt(0, resourceType);

    target.clear();
    while (s.Step())
    {
      target.push_back(s.ColumnInt64(0));
    }
  }


  void DatabaseWrapper::GetAllPublicIds(std::list<std::string>& target,
                                        ResourceType resourceType)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT publicId FROM Resources WHERE resourceType=?");
    s.BindInt(0, resourceType);

    target.clear();
    while (s.Step())
    {
      target.push_back(s.ColumnString(0));
    }
  }


  void DatabaseWrapper::GetAllPublicIds(std::list<std::string>& target,
                                        ResourceType resourceType,
                                        size_t since,
                                        size_t limit)
  {
    if (limit == 0)
    {
      target.clear();
      return;
    }

    SQLite::Statement s(db_, SQLITE_FROM_HERE,
                        "SELECT publicId FROM Resources WHERE "
                        "resourceType=? LIMIT ? OFFSET ?");
    s.BindInt(0, resourceType);
    s.BindInt64(1, limit);
    s.BindInt64(2, since);

    target.clear();
    while (s.Step())
    {
      target.push_back(s.ColumnString(0));
    }
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


  bool DatabaseWrapper::IsExistingResource(int64_t internalId)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                        "SELECT * FROM Resources WHERE internalId=?");
    s.BindInt64(0, internalId);
    return s.Step();
  }


  void DatabaseWrapper::LookupIdentifier(std::list<int64_t>& target,
                                         ResourceType level,
                                         const DicomTag& tag,
                                         IdentifierConstraintType type,
                                         const std::string& value)
  {
    static const char* COMMON = ("SELECT d.id FROM DicomIdentifiers AS d, Resources AS r WHERE "
                                 "d.id = r.internalId AND r.resourceType=? AND "
                                 "d.tagGroup=? AND d.tagElement=? AND ");

    std::auto_ptr<SQLite::Statement> s;

    switch (type)
    {
      case IdentifierConstraintType_GreaterOrEqual:
        s.reset(new SQLite::Statement(db_, std::string(COMMON) + "d.value>=?"));
        break;

      case IdentifierConstraintType_SmallerOrEqual:
        s.reset(new SQLite::Statement(db_, std::string(COMMON) + "d.value<=?"));
        break;

      case IdentifierConstraintType_Wildcard:
        s.reset(new SQLite::Statement(db_, std::string(COMMON) + "d.value GLOB ?"));
        break;

      case IdentifierConstraintType_Equal:
      default:
        s.reset(new SQLite::Statement(db_, std::string(COMMON) + "d.value=?"));
        break;
    }

    assert(s.get() != NULL);

    s->BindInt(0, level);
    s->BindInt(1, tag.GetGroup());
    s->BindInt(2, tag.GetElement());
    s->BindString(3, value);

    target.clear();

    while (s->Step())
    {
      target.push_back(s->ColumnInt64(0));
    }    
  }


  void DatabaseWrapper::LookupIdentifierRange(std::list<int64_t>& target,
                                              ResourceType level,
                                              const DicomTag& tag,
                                              const std::string& start,
                                              const std::string& end)
  {
    SQLite::Statement statement(db_, SQLITE_FROM_HERE,
                                "SELECT d.id FROM DicomIdentifiers AS d, Resources AS r WHERE "
                                "d.id = r.internalId AND r.resourceType=? AND "
                                "d.tagGroup=? AND d.tagElement=? AND d.value>=? AND d.value<=?");

    statement.BindInt(0, level);
    statement.BindInt(1, tag.GetGroup());
    statement.BindInt(2, tag.GetElement());
    statement.BindString(3, start);
    statement.BindString(4, end);

    target.clear();

    while (statement.Step())
    {
      target.push_back(statement.ColumnInt64(0));
    }    
  }
}
