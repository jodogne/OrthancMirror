/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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


#include "../PrecompiledHeadersServer.h"
#include "SQLiteDatabaseWrapper.h"

#include "../../Core/DicomFormat/DicomArray.h"
#include "../../Core/Logging.h"
#include "../../Core/SQLite/Transaction.h"
#include "../Search/ISqlLookupFormatter.h"
#include "../ServerToolbox.h"

#include <EmbeddedResources.h>

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


  void SQLiteDatabaseWrapper::GetChangesInternal(std::list<ServerIndexChange>& target,
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


  void SQLiteDatabaseWrapper::GetExportedResourcesInternal(std::list<ExportedResource>& target,
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


  void SQLiteDatabaseWrapper::GetChildren(std::list<std::string>& childrenPublicIds,
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


  void SQLiteDatabaseWrapper::DeleteResource(int64_t id)
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


  bool SQLiteDatabaseWrapper::GetParentPublicId(std::string& target,
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


  int64_t SQLiteDatabaseWrapper::GetTableRecordCount(const std::string& table)
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

    
  SQLiteDatabaseWrapper::SQLiteDatabaseWrapper(const std::string& path) : 
    listener_(NULL), 
    signalRemainingAncestor_(NULL),
    version_(0)
  {
    db_.Open(path);
  }


  SQLiteDatabaseWrapper::SQLiteDatabaseWrapper() : 
    listener_(NULL), 
    signalRemainingAncestor_(NULL),
    version_(0)
  {
    db_.OpenInMemory();
  }


  int SQLiteDatabaseWrapper::GetGlobalIntegerProperty(GlobalProperty property,
                                                      int defaultValue)
  {
    std::string tmp;

    if (!LookupGlobalProperty(tmp, GlobalProperty_DatabasePatchLevel))
    {
      return defaultValue;
    }
    else
    {
      try
      {
        return boost::lexical_cast<int>(tmp);
      }
      catch (boost::bad_lexical_cast&)
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange,
                               "Global property " + boost::lexical_cast<std::string>(property) +
                               " should be an integer, but found: " + tmp);
      }
    }
  }


  void SQLiteDatabaseWrapper::Open()
  {
    db_.Execute("PRAGMA ENCODING=\"UTF-8\";");

    // Performance tuning of SQLite with PRAGMAs
    // http://www.sqlite.org/pragma.html
    db_.Execute("PRAGMA SYNCHRONOUS=NORMAL;");
    db_.Execute("PRAGMA JOURNAL_MODE=WAL;");
    db_.Execute("PRAGMA LOCKING_MODE=EXCLUSIVE;");
    db_.Execute("PRAGMA WAL_AUTOCHECKPOINT=1000;");
    //db_.Execute("PRAGMA TEMP_STORE=memory");

    // Make "LIKE" case-sensitive in SQLite 
    db_.Execute("PRAGMA case_sensitive_like = true;");
    
    {
      SQLite::Transaction t(db_);
      t.Begin();

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
        throw OrthancException(ErrorCode_IncompatibleDatabaseVersion,
                               "Incompatible version of the Orthanc database: " + tmp);
      }

      // New in Orthanc 1.5.1
      if (version_ == 6)
      {
        if (!LookupGlobalProperty(tmp, GlobalProperty_GetTotalSizeIsFast) ||
            tmp != "1")
        {
          LOG(INFO) << "Installing the SQLite triggers to track the size of the attachments";
          std::string query;
          EmbeddedResources::GetFileResource(query, EmbeddedResources::INSTALL_TRACK_ATTACHMENTS_SIZE);
          db_.Execute(query);
        }
      }

      t.Commit();
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


  void SQLiteDatabaseWrapper::Upgrade(unsigned int targetVersion,
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


  void SQLiteDatabaseWrapper::SetListener(IDatabaseListener& listener)
  {
    listener_ = &listener;
    db_.Register(new Internals::SignalFileDeleted(listener));
    db_.Register(new Internals::SignalResourceDeleted(listener));
  }


  void SQLiteDatabaseWrapper::ClearTable(const std::string& tableName)
  {
    db_.Execute("DELETE FROM " + tableName);    
  }


  bool SQLiteDatabaseWrapper::LookupParent(int64_t& parentId,
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


  ResourceType SQLiteDatabaseWrapper::GetResourceType(int64_t resourceId)
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


  std::string SQLiteDatabaseWrapper::GetPublicId(int64_t resourceId)
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


  void SQLiteDatabaseWrapper::GetChanges(std::list<ServerIndexChange>& target /*out*/,
                                         bool& done /*out*/,
                                         int64_t since,
                                         uint32_t maxResults)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT * FROM Changes WHERE seq>? ORDER BY seq LIMIT ?");
    s.BindInt64(0, since);
    s.BindInt(1, maxResults + 1);
    GetChangesInternal(target, done, s, maxResults);
  }


  void SQLiteDatabaseWrapper::GetLastChange(std::list<ServerIndexChange>& target /*out*/)
  {
    bool done;  // Ignored
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT * FROM Changes ORDER BY seq DESC LIMIT 1");
    GetChangesInternal(target, done, s, 1);
  }


  class SQLiteDatabaseWrapper::Transaction : public IDatabaseWrapper::ITransaction
  {
  private:
    SQLiteDatabaseWrapper&                that_;
    std::unique_ptr<SQLite::Transaction>  transaction_;
    int64_t                               initialDiskSize_;

  public:
    Transaction(SQLiteDatabaseWrapper& that) :
      that_(that),
      transaction_(new SQLite::Transaction(that_.db_))
    {
#if defined(NDEBUG)
      // Release mode
      initialDiskSize_ = 0;
#else
      // Debug mode
      initialDiskSize_ = static_cast<int64_t>(that_.GetTotalCompressedSize());
#endif
    }

    virtual void Begin()
    {
      transaction_->Begin();
    }

    virtual void Rollback() 
    {
      transaction_->Rollback();
    }

    virtual void Commit(int64_t fileSizeDelta /* only used in debug */)
    {
      transaction_->Commit();

      assert(initialDiskSize_ + fileSizeDelta >= 0 &&
             initialDiskSize_ + fileSizeDelta == static_cast<int64_t>(that_.GetTotalCompressedSize()));
    }
  };


  IDatabaseWrapper::ITransaction* SQLiteDatabaseWrapper::StartTransaction()
  {
    return new Transaction(*this);
  }


  void SQLiteDatabaseWrapper::GetAllMetadata(std::map<MetadataType, std::string>& target,
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


  void SQLiteDatabaseWrapper::SetGlobalProperty(GlobalProperty property,
                                                const std::string& value)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT OR REPLACE INTO GlobalProperties VALUES(?, ?)");
    s.BindInt(0, property);
    s.BindString(1, value);
    s.Run();
  }


  bool SQLiteDatabaseWrapper::LookupGlobalProperty(std::string& target,
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


  int64_t SQLiteDatabaseWrapper::CreateResource(const std::string& publicId,
                                                ResourceType type)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO Resources VALUES(NULL, ?, ?, NULL)");
    s.BindInt(0, type);
    s.BindString(1, publicId);
    s.Run();
    return db_.GetLastInsertRowId();
  }


  bool SQLiteDatabaseWrapper::LookupResource(int64_t& id,
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


  void SQLiteDatabaseWrapper::AttachChild(int64_t parent,
                                          int64_t child)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "UPDATE Resources SET parentId = ? WHERE internalId = ?");
    s.BindInt64(0, parent);
    s.BindInt64(1, child);
    s.Run();
  }


  void SQLiteDatabaseWrapper::SetMetadata(int64_t id,
                                          MetadataType type,
                                          const std::string& value)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT OR REPLACE INTO Metadata VALUES(?, ?, ?)");
    s.BindInt64(0, id);
    s.BindInt(1, type);
    s.BindString(2, value);
    s.Run();
  }


  void SQLiteDatabaseWrapper::DeleteMetadata(int64_t id,
                                             MetadataType type)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "DELETE FROM Metadata WHERE id=? and type=?");
    s.BindInt64(0, id);
    s.BindInt(1, type);
    s.Run();
  }


  bool SQLiteDatabaseWrapper::LookupMetadata(std::string& target,
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


  void SQLiteDatabaseWrapper::AddAttachment(int64_t id,
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


  void SQLiteDatabaseWrapper::DeleteAttachment(int64_t id,
                                               FileContentType attachment)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "DELETE FROM AttachedFiles WHERE id=? AND fileType=?");
    s.BindInt64(0, id);
    s.BindInt(1, attachment);
    s.Run();
  }


  void SQLiteDatabaseWrapper::ListAvailableAttachments(std::list<FileContentType>& target,
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

  bool SQLiteDatabaseWrapper::LookupAttachment(FileInfo& attachment,
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


  void SQLiteDatabaseWrapper::ClearMainDicomTags(int64_t id)
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


  void SQLiteDatabaseWrapper::SetMainDicomTag(int64_t id,
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


  void SQLiteDatabaseWrapper::SetIdentifierTag(int64_t id,
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


  void SQLiteDatabaseWrapper::GetMainDicomTags(DicomMap& map,
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


  void SQLiteDatabaseWrapper::GetChildrenPublicId(std::list<std::string>& target,
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


  void SQLiteDatabaseWrapper::GetChildrenInternalId(std::list<int64_t>& target,
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


  void SQLiteDatabaseWrapper::LogChange(int64_t internalId,
                                        const ServerIndexChange& change)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO Changes VALUES(NULL, ?, ?, ?, ?)");
    s.BindInt(0, change.GetChangeType());
    s.BindInt64(1, internalId);
    s.BindInt(2, change.GetResourceType());
    s.BindString(3, change.GetDate());
    s.Run();
  }


  void SQLiteDatabaseWrapper::LogExportedResource(const ExportedResource& resource)
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


  void SQLiteDatabaseWrapper::GetExportedResources(std::list<ExportedResource>& target,
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

    
  void SQLiteDatabaseWrapper::GetLastExportedResource(std::list<ExportedResource>& target)
  {
    bool done;  // Ignored
    SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                        "SELECT * FROM ExportedResources ORDER BY seq DESC LIMIT 1");
    GetExportedResourcesInternal(target, done, s, 1);
  }

    
  uint64_t SQLiteDatabaseWrapper::GetTotalCompressedSize()
  {
    // Old SQL query that was used in Orthanc <= 1.5.0:
    // SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT SUM(compressedSize) FROM AttachedFiles");

    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT value FROM GlobalIntegers WHERE key=0");
    s.Run();
    return static_cast<uint64_t>(s.ColumnInt64(0));
  }

    
  uint64_t SQLiteDatabaseWrapper::GetTotalUncompressedSize()
  {
    // Old SQL query that was used in Orthanc <= 1.5.0:
    // SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT SUM(uncompressedSize) FROM AttachedFiles");

    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT value FROM GlobalIntegers WHERE key=1");
    s.Run();
    return static_cast<uint64_t>(s.ColumnInt64(0));
  }


  uint64_t SQLiteDatabaseWrapper::GetResourceCount(ResourceType resourceType)
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


  void SQLiteDatabaseWrapper::GetAllPublicIds(std::list<std::string>& target,
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


  void SQLiteDatabaseWrapper::GetAllPublicIds(std::list<std::string>& target,
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


  bool SQLiteDatabaseWrapper::SelectPatientToRecycle(int64_t& internalId)
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


  bool SQLiteDatabaseWrapper::SelectPatientToRecycle(int64_t& internalId,
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


  bool SQLiteDatabaseWrapper::IsProtectedPatient(int64_t internalId)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE,
                        "SELECT * FROM PatientRecyclingOrder WHERE patientId = ?");
    s.BindInt64(0, internalId);
    return !s.Step();
  }


  void SQLiteDatabaseWrapper::SetProtectedPatient(int64_t internalId, 
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


  bool SQLiteDatabaseWrapper::IsExistingResource(int64_t internalId)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                        "SELECT * FROM Resources WHERE internalId=?");
    s.BindInt64(0, internalId);
    return s.Step();
  }


  bool SQLiteDatabaseWrapper::IsDiskSizeAbove(uint64_t threshold)
  {
    return GetTotalCompressedSize() > threshold;
  }



  class SQLiteDatabaseWrapper::LookupFormatter : public ISqlLookupFormatter
  {
  private:
    std::list<std::string>  values_;

  public:
    virtual std::string GenerateParameter(const std::string& value)
    {
      values_.push_back(value);
      return "?";
    }
    
    virtual std::string FormatResourceType(ResourceType level)
    {
      return boost::lexical_cast<std::string>(level);
    }

    virtual std::string FormatWildcardEscape()
    {
      return "ESCAPE '\\'";
    }

    void Bind(SQLite::Statement& statement) const
    {
      size_t pos = 0;
      
      for (std::list<std::string>::const_iterator
             it = values_.begin(); it != values_.end(); ++it, pos++)
      {
        statement.BindString(pos, *it);
      }
    }
  };

  
  static void AnswerLookup(std::list<std::string>& resourcesId,
                           std::list<std::string>& instancesId,
                           SQLite::Connection& db,
                           ResourceType level)
  {
    resourcesId.clear();
    instancesId.clear();
    
    std::unique_ptr<SQLite::Statement> statement;
    
    switch (level)
    {
      case ResourceType_Patient:
      {
        statement.reset(
          new SQLite::Statement(
            db, SQLITE_FROM_HERE,
            "SELECT patients.publicId, instances.publicID FROM Lookup AS patients "
            "INNER JOIN Resources studies ON patients.internalId=studies.parentId "
            "INNER JOIN Resources series ON studies.internalId=series.parentId "
            "INNER JOIN Resources instances ON series.internalId=instances.parentId "
            "GROUP BY patients.publicId"));
      
        break;
      }

      case ResourceType_Study:
      {
        statement.reset(
          new SQLite::Statement(
            db, SQLITE_FROM_HERE,
            "SELECT studies.publicId, instances.publicID FROM Lookup AS studies "
            "INNER JOIN Resources series ON studies.internalId=series.parentId "
            "INNER JOIN Resources instances ON series.internalId=instances.parentId "
            "GROUP BY studies.publicId"));
      
        break;
      }

      case ResourceType_Series:
      {
        statement.reset(
          new SQLite::Statement(
            db, SQLITE_FROM_HERE,
            "SELECT series.publicId, instances.publicID FROM Lookup AS series "
            "INNER JOIN Resources instances ON series.internalId=instances.parentId "
            "GROUP BY series.publicId"));
      
        break;
      }

      case ResourceType_Instance:
      {
        statement.reset(
          new SQLite::Statement(
            db, SQLITE_FROM_HERE, "SELECT publicId, publicId FROM Lookup"));
        
        break;
      }
      
      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    assert(statement.get() != NULL);
      
    while (statement->Step())
    {
      resourcesId.push_back(statement->ColumnString(0));
      instancesId.push_back(statement->ColumnString(1));
    }
  }


  void SQLiteDatabaseWrapper::ApplyLookupResources(std::list<std::string>& resourcesId,
                                                   std::list<std::string>* instancesId,
                                                   const std::vector<DatabaseConstraint>& lookup,
                                                   ResourceType queryLevel,
                                                   size_t limit)
  {
    LookupFormatter formatter;

    std::string sql;
    LookupFormatter::Apply(sql, formatter, lookup, queryLevel, limit);

    sql = "CREATE TEMPORARY TABLE Lookup AS " + sql;
    
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, "DROP TABLE IF EXISTS Lookup");
      s.Run();
    }

    {
      SQLite::Statement statement(db_, sql);
      formatter.Bind(statement);
      statement.Run();
    }

    if (instancesId != NULL)
    {
      AnswerLookup(resourcesId, *instancesId, db_, queryLevel);
    }
    else
    {
      resourcesId.clear();
    
      SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT publicId FROM Lookup");
        
      while (s.Step())
      {
        resourcesId.push_back(s.ColumnString(0));
      }
    }
  }


  int64_t SQLiteDatabaseWrapper::GetLastChangeIndex()
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                        "SELECT seq FROM sqlite_sequence WHERE name='Changes'");

    if (s.Step())
    {
      int64_t c = s.ColumnInt(0);
      assert(!s.Step());
      return c;
    }
    else
    {
      // No change has been recorded so far in the database
      return 0;
    }
  }


  void SQLiteDatabaseWrapper::TagMostRecentPatient(int64_t patient)
  {
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE,
                          "DELETE FROM PatientRecyclingOrder WHERE patientId=?");
      s.BindInt64(0, patient);
      s.Run();

      assert(db_.GetLastChangeCount() == 0 ||
             db_.GetLastChangeCount() == 1);
      
      if (db_.GetLastChangeCount() == 0)
      {
        // The patient was protected, there was nothing to delete from the recycling order
        return;
      }
    }

    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE,
                          "INSERT INTO PatientRecyclingOrder VALUES(NULL, ?)");
      s.BindInt64(0, patient);
      s.Run();
    }
  }
}
