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


#include "Database.h"

#include "../../../Core/DicomFormat/DicomArray.h"

#include <EmbeddedResources.h>
#include <boost/lexical_cast.hpp>


namespace Internals
{
  class SignalFileDeleted : public Orthanc::SQLite::IScalarFunction
  {
  private:
    OrthancPlugins::DatabaseBackendOutput&  output_;

  public:
    SignalFileDeleted(OrthancPlugins::DatabaseBackendOutput&  output) : output_(output)
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

    virtual void Compute(Orthanc::SQLite::FunctionContext& context)
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
      
      output_.SignalDeletedAttachment(context.GetStringValue(0),
                                      context.GetIntValue(1),
                                      context.GetInt64Value(2),
                                      uncompressedMD5,
                                      context.GetIntValue(3),
                                      context.GetInt64Value(4),
                                      compressedMD5);
    }
  };


  class SignalResourceDeleted : public Orthanc::SQLite::IScalarFunction
  {
  private:
    OrthancPlugins::DatabaseBackendOutput&  output_;

  public:
    SignalResourceDeleted(OrthancPlugins::DatabaseBackendOutput&  output) : output_(output)
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

    virtual void Compute(Orthanc::SQLite::FunctionContext& context)
    {
      output_.SignalDeletedResource(context.GetStringValue(0),
                                    Orthanc::Plugins::Convert(static_cast<Orthanc::ResourceType>(context.GetIntValue(1))));
    }
  };
}


class Database::SignalRemainingAncestor : public Orthanc::SQLite::IScalarFunction
{
private:
  bool hasRemainingAncestor_;
  std::string remainingPublicId_;
  OrthancPluginResourceType remainingType_;

public:
  SignalRemainingAncestor() : 
    hasRemainingAncestor_(false),
    remainingType_(OrthancPluginResourceType_Instance)  // Some dummy value
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

  virtual void Compute(Orthanc::SQLite::FunctionContext& context)
  {
    if (!hasRemainingAncestor_ ||
        remainingType_ >= context.GetIntValue(1))
    {
      hasRemainingAncestor_ = true;
      remainingPublicId_ = context.GetStringValue(0);
      remainingType_ = Orthanc::Plugins::Convert(static_cast<Orthanc::ResourceType>(context.GetIntValue(1)));
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

  OrthancPluginResourceType GetRemainingAncestorType() const
  {
    assert(hasRemainingAncestor_);
    return remainingType_;
  }
};



Database::Database(const std::string& path) : 
  path_(path),
  base_(db_),
  signalRemainingAncestor_(NULL)
{
}


void Database::Open()
{
  db_.Open(path_);

  db_.Execute("PRAGMA ENCODING=\"UTF-8\";");

  // http://www.sqlite.org/pragma.html
  db_.Execute("PRAGMA SYNCHRONOUS=NORMAL;");
  db_.Execute("PRAGMA JOURNAL_MODE=WAL;");
  db_.Execute("PRAGMA LOCKING_MODE=EXCLUSIVE;");
  db_.Execute("PRAGMA WAL_AUTOCHECKPOINT=1000;");
  //db_.Execute("PRAGMA TEMP_STORE=memory");

  if (!db_.DoesTableExist("GlobalProperties"))
  {
    std::string query;
    Orthanc::EmbeddedResources::GetFileResource(query, Orthanc::EmbeddedResources::PREPARE_DATABASE);
    db_.Execute(query);
  }

  signalRemainingAncestor_ = new SignalRemainingAncestor;
  db_.Register(signalRemainingAncestor_);
  db_.Register(new Internals::SignalFileDeleted(GetOutput()));
  db_.Register(new Internals::SignalResourceDeleted(GetOutput()));
}


void Database::Close()
{
  db_.Close();
}


void Database::AddAttachment(int64_t id,
                             const OrthancPluginAttachment& attachment)
{
  Orthanc::FileInfo info(attachment.uuid,
                         static_cast<Orthanc::FileContentType>(attachment.contentType),
                         attachment.uncompressedSize,
                         attachment.uncompressedHash,
                         static_cast<Orthanc::CompressionType>(attachment.compressionType),
                         attachment.compressedSize,
                         attachment.compressedHash);
  base_.AddAttachment(id, info);
}


void Database::DeleteResource(int64_t id)
{
  signalRemainingAncestor_->Reset();

  Orthanc::SQLite::Statement s(db_, SQLITE_FROM_HERE, "DELETE FROM Resources WHERE internalId=?");
  s.BindInt64(0, id);
  s.Run();

  if (signalRemainingAncestor_->HasRemainingAncestor())
  {
    GetOutput().SignalRemainingAncestor(signalRemainingAncestor_->GetRemainingAncestorId(),
                                        signalRemainingAncestor_->GetRemainingAncestorType());
  }
}


static void Answer(OrthancPlugins::DatabaseBackendOutput& output,
                   const Orthanc::ServerIndexChange& change)
{
  output.AnswerChange(change.GetSeq(), 
                      change.GetChangeType(),
                      Orthanc::Plugins::Convert(change.GetResourceType()),
                      change.GetPublicId(),
                      change.GetDate());
}


static void Answer(OrthancPlugins::DatabaseBackendOutput& output,
                   const Orthanc::ExportedResource& resource)
{
  output.AnswerExportedResource(resource.GetSeq(),
                                Orthanc::Plugins::Convert(resource.GetResourceType()),
                                resource.GetPublicId(),
                                resource.GetModality(),
                                resource.GetDate(),
                                resource.GetPatientId(),
                                resource.GetStudyInstanceUid(),
                                resource.GetSeriesInstanceUid(),
                                resource.GetSopInstanceUid());
}


void Database::GetChanges(bool& done /*out*/,
                          int64_t since,
                          uint32_t maxResults)
{
  typedef std::list<Orthanc::ServerIndexChange> Changes;

  Changes changes;
  base_.GetChanges(changes, done, since, maxResults);

  for (Changes::const_iterator it = changes.begin(); it != changes.end(); ++it)
  {
    Answer(GetOutput(), *it);
  }
}


void Database::GetExportedResources(bool& done /*out*/,
                                    int64_t since,
                                    uint32_t maxResults)
{
  typedef std::list<Orthanc::ExportedResource> Resources;

  Resources resources;
  base_.GetExportedResources(resources, done, since, maxResults);

  for (Resources::const_iterator it = resources.begin(); it != resources.end(); ++it)
  {
    Answer(GetOutput(), *it);
  }
}


void Database::GetLastChange()
{
  std::list<Orthanc::ServerIndexChange> change;
  Orthanc::ErrorCode code = base_.GetLastChange(change);
  
  if (code != Orthanc::ErrorCode_Success)
  {
    throw OrthancPlugins::DatabaseException(static_cast<OrthancPluginErrorCode>(code));
  }

  if (!change.empty())
  {
    Answer(GetOutput(), change.front());
  }
}


void Database::GetLastExportedResource()
{
  std::list<Orthanc::ExportedResource> resource;
  base_.GetLastExportedResource(resource);
  
  if (!resource.empty())
  {
    Answer(GetOutput(), resource.front());
  }
}


void Database::GetMainDicomTags(int64_t id)
{
  Orthanc::DicomMap tags;
  base_.GetMainDicomTags(tags, id);

  Orthanc::DicomArray arr(tags);
  for (size_t i = 0; i < arr.GetSize(); i++)
  {
    GetOutput().AnswerDicomTag(arr.GetElement(i).GetTag().GetGroup(),
                               arr.GetElement(i).GetTag().GetElement(),
                               arr.GetElement(i).GetValue().GetContent());
  }
}


std::string Database::GetPublicId(int64_t resourceId)
{
  std::string id;
  if (base_.GetPublicId(id, resourceId))
  {
    return id;
  }
  else
  {
    throw OrthancPlugins::DatabaseException(OrthancPluginErrorCode_UnknownResource);
  }
}


OrthancPluginResourceType Database::GetResourceType(int64_t resourceId)
{
  Orthanc::ResourceType  result;
  Orthanc::ErrorCode  code = base_.GetResourceType(result, resourceId);

  if (code == Orthanc::ErrorCode_Success)
  {
    return Orthanc::Plugins::Convert(result);
  }
  else
  {
    throw OrthancPlugins::DatabaseException(static_cast<OrthancPluginErrorCode>(code));
  }
}



template <typename I>
static void ConvertList(std::list<int32_t>& target,
                        const std::list<I>& source)
{
  for (typename std::list<I>::const_iterator 
         it = source.begin(); it != source.end(); ++it)
  {
    target.push_back(*it);
  }
}


void Database::ListAvailableMetadata(std::list<int32_t>& target /*out*/,
                                     int64_t id)
{
  std::list<Orthanc::MetadataType> tmp;
  base_.ListAvailableMetadata(tmp, id);
  ConvertList(target, tmp);
}


void Database::ListAvailableAttachments(std::list<int32_t>& target /*out*/,
                                        int64_t id)
{
  std::list<Orthanc::FileContentType> tmp;
  base_.ListAvailableAttachments(tmp, id);
  ConvertList(target, tmp);
}


void Database::LogChange(const OrthancPluginChange& change)
{
  int64_t id;
  OrthancPluginResourceType type;
  if (!LookupResource(id, type, change.publicId) ||
      type != change.resourceType)
  {
    throw OrthancPlugins::DatabaseException(OrthancPluginErrorCode_DatabasePlugin);
  }

  Orthanc::ServerIndexChange tmp(change.seq,
                                 static_cast<Orthanc::ChangeType>(change.changeType),
                                 Orthanc::Plugins::Convert(change.resourceType),
                                 change.publicId,
                                 change.date);

  base_.LogChange(id, tmp);
}


void Database::LogExportedResource(const OrthancPluginExportedResource& resource) 
{
  Orthanc::ExportedResource tmp(resource.seq,
                                Orthanc::Plugins::Convert(resource.resourceType),
                                resource.publicId,
                                resource.modality,
                                resource.date,
                                resource.patientId,
                                resource.studyInstanceUid,
                                resource.seriesInstanceUid,
                                resource.sopInstanceUid);

  base_.LogExportedResource(tmp);
}

    
bool Database::LookupAttachment(int64_t id,
                                int32_t contentType)
{
  Orthanc::FileInfo attachment;
  if (base_.LookupAttachment(attachment, id, static_cast<Orthanc::FileContentType>(contentType)))
  {
    GetOutput().AnswerAttachment(attachment.GetUuid(),
                                 attachment.GetContentType(),
                                 attachment.GetUncompressedSize(),
                                 attachment.GetUncompressedMD5(),
                                 attachment.GetCompressionType(),
                                 attachment.GetCompressedSize(),
                                 attachment.GetCompressedMD5());
    return true;
  }
  else
  {
    return false;
  }
}


bool Database::LookupParent(int64_t& parentId /*out*/,
                            int64_t resourceId)
{
  bool found;
  Orthanc::ErrorCode code = base_.LookupParent(found, parentId, resourceId);

  if (code == Orthanc::ErrorCode_Success)
  {
    return found;
  }
  else
  {
    throw OrthancPlugins::DatabaseException(static_cast<OrthancPluginErrorCode>(code));
  }
}


bool Database::LookupResource(int64_t& id /*out*/,
                              OrthancPluginResourceType& type /*out*/,
                              const char* publicId)
{
  Orthanc::ResourceType tmp;
  if (base_.LookupResource(id, tmp, publicId))
  {
    type = Orthanc::Plugins::Convert(tmp);
    return true;
  }
  else
  {
    return false;
  }
}


void Database::StartTransaction()
{
  transaction_.reset(new Orthanc::SQLite::Transaction(db_));
  transaction_->Begin();
}


void Database::RollbackTransaction()
{
  transaction_->Rollback();
  transaction_.reset(NULL);
}


void Database::CommitTransaction()
{
  transaction_->Commit();
  transaction_.reset(NULL);
}


uint32_t Database::GetDatabaseVersion()
{
  std::string version;

  if (!LookupGlobalProperty(version, Orthanc::GlobalProperty_DatabaseSchemaVersion))
  {
    throw OrthancPlugins::DatabaseException(OrthancPluginErrorCode_InternalError);
  }

  try
  {
    return boost::lexical_cast<uint32_t>(version);
  }
  catch (boost::bad_lexical_cast&)
  {
    throw OrthancPlugins::DatabaseException(OrthancPluginErrorCode_InternalError);
  }
}


void Database::UpgradeDatabase(uint32_t  targetVersion,
                               OrthancPluginStorageArea* storageArea)
{
  if (targetVersion == 6)
  {
    OrthancPluginErrorCode code = OrthancPluginReconstructMainDicomTags(GetOutput().GetContext(), storageArea, 
                                                                        OrthancPluginResourceType_Study);
    if (code == OrthancPluginErrorCode_Success)
    {
      code = OrthancPluginReconstructMainDicomTags(GetOutput().GetContext(), storageArea, 
                                                   OrthancPluginResourceType_Series);
    }

    if (code != OrthancPluginErrorCode_Success)
    {
      throw OrthancPlugins::DatabaseException(code);
    }

    base_.SetGlobalProperty(Orthanc::GlobalProperty_DatabaseSchemaVersion, "6");
  }
}
