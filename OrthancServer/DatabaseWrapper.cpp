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
    base_(db_),
    signalRemainingAncestor_(NULL),
    version_(0)
  {
    db_.Open(path);
  }

  DatabaseWrapper::DatabaseWrapper() : 
    listener_(NULL), 
    base_(db_),
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
    bool found;
    ErrorCode error = base_.LookupParent(found, parentId, resourceId);

    if (error != ErrorCode_Success)
    {
      throw OrthancException(error);
    }
    else
    {
      return found;
    }
  }


  ResourceType DatabaseWrapper::GetResourceType(int64_t resourceId)
  {
    ResourceType result;
    ErrorCode code = base_.GetResourceType(result, resourceId);

    if (code == ErrorCode_Success)
    {
      return result;
    }
    else
    {
      throw OrthancException(code);
    }
  }


  std::string DatabaseWrapper::GetPublicId(int64_t resourceId)
  {
    std::string id;

    if (base_.GetPublicId(id, resourceId))
    {
      return id;
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
    ErrorCode code = base_.GetChanges(target, done, since, maxResults);

    if (code != ErrorCode_Success)
    {
      throw OrthancException(code);
    }
  }


  void DatabaseWrapper::GetLastChange(std::list<ServerIndexChange>& target /*out*/)
  {
    ErrorCode code = base_.GetLastChange(target);

    if (code != ErrorCode_Success)
    {
      throw OrthancException(code);
    }
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

}
