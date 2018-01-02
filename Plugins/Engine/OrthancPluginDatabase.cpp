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


#include "../../OrthancServer/PrecompiledHeadersServer.h"
#include "OrthancPluginDatabase.h"

#if ORTHANC_ENABLE_PLUGINS != 1
#error The plugin support is disabled
#endif


#include "../../Core/OrthancException.h"
#include "../../Core/Logging.h"
#include "PluginsEnumerations.h"

#include <cassert>

namespace Orthanc
{
  static FileInfo Convert(const OrthancPluginAttachment& attachment)
  {
    return FileInfo(attachment.uuid,
                    static_cast<FileContentType>(attachment.contentType),
                    attachment.uncompressedSize,
                    attachment.uncompressedHash,
                    static_cast<CompressionType>(attachment.compressionType),
                    attachment.compressedSize,
                    attachment.compressedHash);
  }


  void OrthancPluginDatabase::CheckSuccess(OrthancPluginErrorCode code)
  {
    if (code != OrthancPluginErrorCode_Success)
    {
      errorDictionary_.LogError(code, true);
      throw OrthancException(static_cast<ErrorCode>(code));
    }
  }


  void OrthancPluginDatabase::ResetAnswers()
  {
    type_ = _OrthancPluginDatabaseAnswerType_None;

    answerDicomMap_ = NULL;
    answerChanges_ = NULL;
    answerExportedResources_ = NULL;
    answerDone_ = NULL;
  }


  void OrthancPluginDatabase::ForwardAnswers(std::list<int64_t>& target)
  {
    if (type_ != _OrthancPluginDatabaseAnswerType_None &&
        type_ != _OrthancPluginDatabaseAnswerType_Int64)
    {
      throw OrthancException(ErrorCode_DatabasePlugin);
    }

    target.clear();

    if (type_ == _OrthancPluginDatabaseAnswerType_Int64)
    {
      for (std::list<int64_t>::const_iterator 
             it = answerInt64_.begin(); it != answerInt64_.end(); ++it)
      {
        target.push_back(*it);
      }
    }
  }


  void OrthancPluginDatabase::ForwardAnswers(std::list<std::string>& target)
  {
    if (type_ != _OrthancPluginDatabaseAnswerType_None &&
        type_ != _OrthancPluginDatabaseAnswerType_String)
    {
      throw OrthancException(ErrorCode_DatabasePlugin);
    }

    target.clear();

    if (type_ == _OrthancPluginDatabaseAnswerType_String)
    {
      for (std::list<std::string>::const_iterator 
             it = answerStrings_.begin(); it != answerStrings_.end(); ++it)
      {
        target.push_back(*it);
      }
    }
  }


  bool OrthancPluginDatabase::ForwardSingleAnswer(std::string& target)
  {
    if (type_ == _OrthancPluginDatabaseAnswerType_None)
    {
      return false;
    }
    else if (type_ == _OrthancPluginDatabaseAnswerType_String &&
             answerStrings_.size() == 1)
    {
      target = answerStrings_.front();
      return true; 
    }
    else
    {
      throw OrthancException(ErrorCode_DatabasePlugin);
    }
  }


  bool OrthancPluginDatabase::ForwardSingleAnswer(int64_t& target)
  {
    if (type_ == _OrthancPluginDatabaseAnswerType_None)
    {
      return false;
    }
    else if (type_ == _OrthancPluginDatabaseAnswerType_Int64 &&
             answerInt64_.size() == 1)
    {
      target = answerInt64_.front();
      return true; 
    }
    else
    {
      throw OrthancException(ErrorCode_DatabasePlugin);
    }
  }


  OrthancPluginDatabase::OrthancPluginDatabase(SharedLibrary& library,
                                               PluginsErrorDictionary&  errorDictionary,
                                               const OrthancPluginDatabaseBackend& backend,
                                               const OrthancPluginDatabaseExtensions* extensions,
                                               size_t extensionsSize,
                                               void *payload) : 
    library_(library),
    errorDictionary_(errorDictionary),
    type_(_OrthancPluginDatabaseAnswerType_None),
    backend_(backend),
    payload_(payload),
    listener_(NULL),
    answerDicomMap_(NULL),
    answerChanges_(NULL),
    answerExportedResources_(NULL),
    answerDone_(NULL)
  {
    memset(&extensions_, 0, sizeof(extensions_));

    size_t size = sizeof(extensions_);
    if (extensionsSize < size)
    {
      size = extensionsSize;  // Not all the extensions are available
    }

    memcpy(&extensions_, extensions, size);
  }


  void OrthancPluginDatabase::AddAttachment(int64_t id,
                                            const FileInfo& attachment)
  {
    OrthancPluginAttachment tmp;
    tmp.uuid = attachment.GetUuid().c_str();
    tmp.contentType = static_cast<int32_t>(attachment.GetContentType());
    tmp.uncompressedSize = attachment.GetUncompressedSize();
    tmp.uncompressedHash = attachment.GetUncompressedMD5().c_str();
    tmp.compressionType = static_cast<int32_t>(attachment.GetCompressionType());
    tmp.compressedSize = attachment.GetCompressedSize();
    tmp.compressedHash = attachment.GetCompressedMD5().c_str();

    CheckSuccess(backend_.addAttachment(payload_, id, &tmp));
  }


  void OrthancPluginDatabase::AttachChild(int64_t parent,
                                          int64_t child)
  {
    CheckSuccess(backend_.attachChild(payload_, parent, child));
  }


  void OrthancPluginDatabase::ClearChanges()
  {
    CheckSuccess(backend_.clearChanges(payload_));
  }


  void OrthancPluginDatabase::ClearExportedResources()
  {
    CheckSuccess(backend_.clearExportedResources(payload_));
  }


  int64_t OrthancPluginDatabase::CreateResource(const std::string& publicId,
                                                ResourceType type)
  {
    int64_t id;
    CheckSuccess(backend_.createResource(&id, payload_, publicId.c_str(), Plugins::Convert(type)));
    return id;
  }


  void OrthancPluginDatabase::DeleteAttachment(int64_t id,
                                               FileContentType attachment)
  {
    CheckSuccess(backend_.deleteAttachment(payload_, id, static_cast<int32_t>(attachment)));
  }


  void OrthancPluginDatabase::DeleteMetadata(int64_t id,
                                             MetadataType type)
  {
    CheckSuccess(backend_.deleteMetadata(payload_, id, static_cast<int32_t>(type)));
  }


  void OrthancPluginDatabase::DeleteResource(int64_t id)
  {
    CheckSuccess(backend_.deleteResource(payload_, id));
  }


  void OrthancPluginDatabase::GetAllMetadata(std::map<MetadataType, std::string>& target,
                                             int64_t id)
  {
    std::list<MetadataType> metadata;
    ListAvailableMetadata(metadata, id);

    target.clear();

    for (std::list<MetadataType>::const_iterator
           it = metadata.begin(); it != metadata.end(); ++it)
    {
      std::string value;
      if (!LookupMetadata(value, id, *it))
      {
        throw OrthancException(ErrorCode_DatabasePlugin);
      }

      target[*it] = value;
    }
  }


  void OrthancPluginDatabase::GetAllInternalIds(std::list<int64_t>& target,
                                                ResourceType resourceType)
  {
    if (extensions_.getAllInternalIds == NULL)
    {
      LOG(ERROR) << "The database plugin does not implement the GetAllInternalIds primitive";
      throw OrthancException(ErrorCode_DatabasePlugin);
    }

    ResetAnswers();
    CheckSuccess(extensions_.getAllInternalIds(GetContext(), payload_, Plugins::Convert(resourceType)));
    ForwardAnswers(target);
  }


  void OrthancPluginDatabase::GetAllPublicIds(std::list<std::string>& target,
                                              ResourceType resourceType)
  {
    ResetAnswers();
    CheckSuccess(backend_.getAllPublicIds(GetContext(), payload_, Plugins::Convert(resourceType)));
    ForwardAnswers(target);
  }


  void OrthancPluginDatabase::GetAllPublicIds(std::list<std::string>& target,
                                              ResourceType resourceType,
                                              size_t since,
                                              size_t limit)
  {
    if (extensions_.getAllPublicIdsWithLimit != NULL)
    {
      // This extension is available since Orthanc 0.9.4
      ResetAnswers();
      CheckSuccess(extensions_.getAllPublicIdsWithLimit
                   (GetContext(), payload_, Plugins::Convert(resourceType), since, limit));
      ForwardAnswers(target);
    }
    else
    {
      // The extension is not available in the database plugin, use a
      // fallback implementation
      target.clear();

      if (limit == 0)
      {
        return;
      }

      std::list<std::string> tmp;
      GetAllPublicIds(tmp, resourceType);
    
      if (tmp.size() <= since)
      {
        // Not enough results => empty answer
        return;
      }

      std::list<std::string>::iterator current = tmp.begin();
      std::advance(current, since);

      while (limit > 0 && current != tmp.end())
      {
        target.push_back(*current);
        --limit;
        ++current;
      }
    }
  }



  void OrthancPluginDatabase::GetChanges(std::list<ServerIndexChange>& target /*out*/,
                                         bool& done /*out*/,
                                         int64_t since,
                                         uint32_t maxResults)
  {
    ResetAnswers();
    answerChanges_ = &target;
    answerDone_ = &done;
    done = false;

    CheckSuccess(backend_.getChanges(GetContext(), payload_, since, maxResults));
  }


  void OrthancPluginDatabase::GetChildrenInternalId(std::list<int64_t>& target,
                                                    int64_t id)
  {
    ResetAnswers();
    CheckSuccess(backend_.getChildrenInternalId(GetContext(), payload_, id));
    ForwardAnswers(target);
  }


  void OrthancPluginDatabase::GetChildrenPublicId(std::list<std::string>& target,
                                                  int64_t id)
  {
    ResetAnswers();
    CheckSuccess(backend_.getChildrenPublicId(GetContext(), payload_, id));
    ForwardAnswers(target);
  }


  void OrthancPluginDatabase::GetExportedResources(std::list<ExportedResource>& target /*out*/,
                                                   bool& done /*out*/,
                                                   int64_t since,
                                                   uint32_t maxResults)
  {
    ResetAnswers();
    answerExportedResources_ = &target;
    answerDone_ = &done;
    done = false;

    CheckSuccess(backend_.getExportedResources(GetContext(), payload_, since, maxResults));
  }


  void OrthancPluginDatabase::GetLastChange(std::list<ServerIndexChange>& target /*out*/)
  {
    bool ignored = false;

    ResetAnswers();
    answerChanges_ = &target;
    answerDone_ = &ignored;

    CheckSuccess(backend_.getLastChange(GetContext(), payload_));
  }


  void OrthancPluginDatabase::GetLastExportedResource(std::list<ExportedResource>& target /*out*/)
  {
    bool ignored = false;

    ResetAnswers();
    answerExportedResources_ = &target;
    answerDone_ = &ignored;

    CheckSuccess(backend_.getLastExportedResource(GetContext(), payload_));
  }


  void OrthancPluginDatabase::GetMainDicomTags(DicomMap& map,
                                               int64_t id)
  {
    ResetAnswers();
    answerDicomMap_ = &map;

    CheckSuccess(backend_.getMainDicomTags(GetContext(), payload_, id));
  }


  std::string OrthancPluginDatabase::GetPublicId(int64_t resourceId)
  {
    ResetAnswers();
    std::string s;

    CheckSuccess(backend_.getPublicId(GetContext(), payload_, resourceId));

    if (!ForwardSingleAnswer(s))
    {
      throw OrthancException(ErrorCode_DatabasePlugin);
    }

    return s;
  }


  uint64_t OrthancPluginDatabase::GetResourceCount(ResourceType resourceType)
  {
    uint64_t count;
    CheckSuccess(backend_.getResourceCount(&count, payload_, Plugins::Convert(resourceType)));
    return count;
  }


  ResourceType OrthancPluginDatabase::GetResourceType(int64_t resourceId)
  {
    OrthancPluginResourceType type;
    CheckSuccess(backend_.getResourceType(&type, payload_, resourceId));
    return Plugins::Convert(type);
  }


  uint64_t OrthancPluginDatabase::GetTotalCompressedSize()
  {
    uint64_t size;
    CheckSuccess(backend_.getTotalCompressedSize(&size, payload_));
    return size;
  }

    
  uint64_t OrthancPluginDatabase::GetTotalUncompressedSize()
  {
    uint64_t size;
    CheckSuccess(backend_.getTotalUncompressedSize(&size, payload_));
    return size;
  }


  bool OrthancPluginDatabase::IsExistingResource(int64_t internalId)
  {
    int32_t existing;
    CheckSuccess(backend_.isExistingResource(&existing, payload_, internalId));
    return (existing != 0);
  }


  bool OrthancPluginDatabase::IsProtectedPatient(int64_t internalId)
  {
    int32_t isProtected;
    CheckSuccess(backend_.isProtectedPatient(&isProtected, payload_, internalId));
    return (isProtected != 0);
  }


  void OrthancPluginDatabase::ListAvailableMetadata(std::list<MetadataType>& target,
                                                    int64_t id)
  {
    ResetAnswers();
    CheckSuccess(backend_.listAvailableMetadata(GetContext(), payload_, id));

    if (type_ != _OrthancPluginDatabaseAnswerType_None &&
        type_ != _OrthancPluginDatabaseAnswerType_Int32)
    {
      throw OrthancException(ErrorCode_DatabasePlugin);
    }

    target.clear();

    if (type_ == _OrthancPluginDatabaseAnswerType_Int32)
    {
      for (std::list<int32_t>::const_iterator 
             it = answerInt32_.begin(); it != answerInt32_.end(); ++it)
      {
        target.push_back(static_cast<MetadataType>(*it));
      }
    }
  }


  void OrthancPluginDatabase::ListAvailableAttachments(std::list<FileContentType>& target,
                                                       int64_t id)
  {
    ResetAnswers();

    CheckSuccess(backend_.listAvailableAttachments(GetContext(), payload_, id));

    if (type_ != _OrthancPluginDatabaseAnswerType_None &&
        type_ != _OrthancPluginDatabaseAnswerType_Int32)
    {
      throw OrthancException(ErrorCode_DatabasePlugin);
    }

    target.clear();

    if (type_ == _OrthancPluginDatabaseAnswerType_Int32)
    {
      for (std::list<int32_t>::const_iterator 
             it = answerInt32_.begin(); it != answerInt32_.end(); ++it)
      {
        target.push_back(static_cast<FileContentType>(*it));
      }
    }
  }


  void OrthancPluginDatabase::LogChange(int64_t internalId,
                                        const ServerIndexChange& change)
  {
    OrthancPluginChange tmp;
    tmp.seq = change.GetSeq();
    tmp.changeType = static_cast<int32_t>(change.GetChangeType());
    tmp.resourceType = Plugins::Convert(change.GetResourceType());
    tmp.publicId = change.GetPublicId().c_str();
    tmp.date = change.GetDate().c_str();

    CheckSuccess(backend_.logChange(payload_, &tmp));
  }


  void OrthancPluginDatabase::LogExportedResource(const ExportedResource& resource)
  {
    OrthancPluginExportedResource tmp;
    tmp.seq = resource.GetSeq();
    tmp.resourceType = Plugins::Convert(resource.GetResourceType());
    tmp.publicId = resource.GetPublicId().c_str();
    tmp.modality = resource.GetModality().c_str();
    tmp.date = resource.GetDate().c_str();
    tmp.patientId = resource.GetPatientId().c_str();
    tmp.studyInstanceUid = resource.GetStudyInstanceUid().c_str();
    tmp.seriesInstanceUid = resource.GetSeriesInstanceUid().c_str();
    tmp.sopInstanceUid = resource.GetSopInstanceUid().c_str();

    CheckSuccess(backend_.logExportedResource(payload_, &tmp));
  }

    
  bool OrthancPluginDatabase::LookupAttachment(FileInfo& attachment,
                                               int64_t id,
                                               FileContentType contentType)
  {
    ResetAnswers();

    CheckSuccess(backend_.lookupAttachment
                 (GetContext(), payload_, id, static_cast<int32_t>(contentType)));

    if (type_ == _OrthancPluginDatabaseAnswerType_None)
    {
      return false;
    }
    else if (type_ == _OrthancPluginDatabaseAnswerType_Attachment &&
             answerAttachments_.size() == 1)
    {
      attachment = answerAttachments_.front();
      return true; 
    }
    else
    {
      throw OrthancException(ErrorCode_DatabasePlugin);
    }
  }


  bool OrthancPluginDatabase::LookupGlobalProperty(std::string& target,
                                                   GlobalProperty property)
  {
    ResetAnswers();

    CheckSuccess(backend_.lookupGlobalProperty
                 (GetContext(), payload_, static_cast<int32_t>(property)));

    return ForwardSingleAnswer(target);
  }


  void OrthancPluginDatabase::LookupIdentifier(std::list<int64_t>& result,
                                               ResourceType level,
                                               const DicomTag& tag,
                                               IdentifierConstraintType type,
                                               const std::string& value)
  {
    if (extensions_.lookupIdentifier3 == NULL)
    {
      LOG(ERROR) << "The database plugin does not implement the LookupIdentifier3 primitive";
      throw OrthancException(ErrorCode_DatabasePlugin);
    }

    OrthancPluginDicomTag tmp;
    tmp.group = tag.GetGroup();
    tmp.element = tag.GetElement();
    tmp.value = value.c_str();

    ResetAnswers();
    CheckSuccess(extensions_.lookupIdentifier3(GetContext(), payload_, Plugins::Convert(level),
                                               &tmp, Plugins::Convert(type)));
    ForwardAnswers(result);
  }


  bool OrthancPluginDatabase::LookupMetadata(std::string& target,
                                             int64_t id,
                                             MetadataType type)
  {
    ResetAnswers();
    CheckSuccess(backend_.lookupMetadata(GetContext(), payload_, id, static_cast<int32_t>(type)));
    return ForwardSingleAnswer(target);
  }


  bool OrthancPluginDatabase::LookupParent(int64_t& parentId,
                                           int64_t resourceId)
  {
    ResetAnswers();
    CheckSuccess(backend_.lookupParent(GetContext(), payload_, resourceId));
    return ForwardSingleAnswer(parentId);
  }


  bool OrthancPluginDatabase::LookupResource(int64_t& id,
                                             ResourceType& type,
                                             const std::string& publicId)
  {
    ResetAnswers();

    CheckSuccess(backend_.lookupResource(GetContext(), payload_, publicId.c_str()));

    if (type_ == _OrthancPluginDatabaseAnswerType_None)
    {
      return false;
    }
    else if (type_ == _OrthancPluginDatabaseAnswerType_Resource &&
             answerResources_.size() == 1)
    {
      id = answerResources_.front().first;
      type = answerResources_.front().second;
      return true; 
    }
    else
    {
      throw OrthancException(ErrorCode_DatabasePlugin);
    }
  }


  bool OrthancPluginDatabase::SelectPatientToRecycle(int64_t& internalId)
  {
    ResetAnswers();
    CheckSuccess(backend_.selectPatientToRecycle(GetContext(), payload_));
    return ForwardSingleAnswer(internalId);
  }


  bool OrthancPluginDatabase::SelectPatientToRecycle(int64_t& internalId,
                                                     int64_t patientIdToAvoid)
  {
    ResetAnswers();
    CheckSuccess(backend_.selectPatientToRecycle2(GetContext(), payload_, patientIdToAvoid));
    return ForwardSingleAnswer(internalId);
  }


  void OrthancPluginDatabase::SetGlobalProperty(GlobalProperty property,
                                                const std::string& value)
  {
    CheckSuccess(backend_.setGlobalProperty
                 (payload_, static_cast<int32_t>(property), value.c_str()));
  }


  void OrthancPluginDatabase::ClearMainDicomTags(int64_t id)
  {
    if (extensions_.clearMainDicomTags == NULL)
    {
      LOG(ERROR) << "Your custom index plugin does not implement the ClearMainDicomTags() extension";
      throw OrthancException(ErrorCode_DatabasePlugin);
    }

    CheckSuccess(extensions_.clearMainDicomTags(payload_, id));
  }


  void OrthancPluginDatabase::SetMainDicomTag(int64_t id,
                                              const DicomTag& tag,
                                              const std::string& value)
  {
    OrthancPluginDicomTag tmp;
    tmp.group = tag.GetGroup();
    tmp.element = tag.GetElement();
    tmp.value = value.c_str();

    CheckSuccess(backend_.setMainDicomTag(payload_, id, &tmp));
  }


  void OrthancPluginDatabase::SetIdentifierTag(int64_t id,
                                               const DicomTag& tag,
                                               const std::string& value)
  {
    OrthancPluginDicomTag tmp;
    tmp.group = tag.GetGroup();
    tmp.element = tag.GetElement();
    tmp.value = value.c_str();

    CheckSuccess(backend_.setIdentifierTag(payload_, id, &tmp));
  }


  void OrthancPluginDatabase::SetMetadata(int64_t id,
                                          MetadataType type,
                                          const std::string& value)
  {
    CheckSuccess(backend_.setMetadata
                 (payload_, id, static_cast<int32_t>(type), value.c_str()));
  }


  void OrthancPluginDatabase::SetProtectedPatient(int64_t internalId, 
                                                  bool isProtected)
  {
    CheckSuccess(backend_.setProtectedPatient(payload_, internalId, isProtected));
  }


  class OrthancPluginDatabase::Transaction : public SQLite::ITransaction
  {
  private:
    const OrthancPluginDatabaseBackend& backend_;
    void* payload_;
    PluginsErrorDictionary&  errorDictionary_;

    void CheckSuccess(OrthancPluginErrorCode code)
    {
      if (code != OrthancPluginErrorCode_Success)
      {
        errorDictionary_.LogError(code, true);
        throw OrthancException(static_cast<ErrorCode>(code));
      }
    }

  public:
    Transaction(const OrthancPluginDatabaseBackend& backend,
                void* payload,
                PluginsErrorDictionary&  errorDictionary) :
      backend_(backend),
      payload_(payload),
      errorDictionary_(errorDictionary)
    {
    }

    virtual void Begin()
    {
      CheckSuccess(backend_.startTransaction(payload_));
    }

    virtual void Rollback()
    {
      CheckSuccess(backend_.rollbackTransaction(payload_));
    }

    virtual void Commit()
    {
      CheckSuccess(backend_.commitTransaction(payload_));
    }
  };


  SQLite::ITransaction* OrthancPluginDatabase::StartTransaction()
  {
    return new Transaction(backend_, payload_, errorDictionary_);
  }


  static void ProcessEvent(IDatabaseListener& listener,
                           const _OrthancPluginDatabaseAnswer& answer)
  {
    switch (answer.type)
    {
      case _OrthancPluginDatabaseAnswerType_DeletedAttachment:
      {
        const OrthancPluginAttachment& attachment = 
          *reinterpret_cast<const OrthancPluginAttachment*>(answer.valueGeneric);
        listener.SignalFileDeleted(Convert(attachment));
        break;
      }
        
      case _OrthancPluginDatabaseAnswerType_RemainingAncestor:
      {
        ResourceType type = Plugins::Convert(static_cast<OrthancPluginResourceType>(answer.valueInt32));
        listener.SignalRemainingAncestor(type, answer.valueString);
        break;
      }
      
      case _OrthancPluginDatabaseAnswerType_DeletedResource:
      {
        ResourceType type = Plugins::Convert(static_cast<OrthancPluginResourceType>(answer.valueInt32));
        ServerIndexChange change(ChangeType_Deleted, type, answer.valueString);
        listener.SignalChange(change);
        break;
      }

      default:
        throw OrthancException(ErrorCode_DatabasePlugin);
    }
  }


  unsigned int OrthancPluginDatabase::GetDatabaseVersion()
  {
    if (extensions_.getDatabaseVersion != NULL)
    {
      uint32_t version;
      CheckSuccess(extensions_.getDatabaseVersion(&version, payload_));
      return version;
    }
    else
    {
      // Before adding the "GetDatabaseVersion()" extension in plugins
      // (OrthancPostgreSQL <= 1.2), the only supported DB schema was
      // version 5.
      return 5;
    }
  }


  void OrthancPluginDatabase::Upgrade(unsigned int targetVersion,
                                      IStorageArea& storageArea)
  {
    if (extensions_.upgradeDatabase != NULL)
    {
      Transaction transaction(backend_, payload_, errorDictionary_);
      transaction.Begin();

      OrthancPluginErrorCode code = extensions_.upgradeDatabase(
        payload_, targetVersion, 
        reinterpret_cast<OrthancPluginStorageArea*>(&storageArea));

      if (code == OrthancPluginErrorCode_Success)
      {
        transaction.Commit();
      }
      else
      {
        transaction.Rollback();
        errorDictionary_.LogError(code, true);
        throw OrthancException(static_cast<ErrorCode>(code));
      }
    }
  }


  void OrthancPluginDatabase::AnswerReceived(const _OrthancPluginDatabaseAnswer& answer)
  {
    if (answer.type == _OrthancPluginDatabaseAnswerType_None)
    {
      throw OrthancException(ErrorCode_DatabasePlugin);
    }

    if (answer.type == _OrthancPluginDatabaseAnswerType_DeletedAttachment ||
        answer.type == _OrthancPluginDatabaseAnswerType_DeletedResource ||
        answer.type == _OrthancPluginDatabaseAnswerType_RemainingAncestor)
    {
      assert(listener_ != NULL);
      ProcessEvent(*listener_, answer);
      return;
    }

    if (type_ == _OrthancPluginDatabaseAnswerType_None)
    {
      type_ = answer.type;

      switch (type_)
      {
        case _OrthancPluginDatabaseAnswerType_Int32:
          answerInt32_.clear();
          break;

        case _OrthancPluginDatabaseAnswerType_Int64:
          answerInt64_.clear();
          break;

        case _OrthancPluginDatabaseAnswerType_Resource:
          answerResources_.clear();
          break;

        case _OrthancPluginDatabaseAnswerType_Attachment:
          answerAttachments_.clear();
          break;

        case _OrthancPluginDatabaseAnswerType_String:
          answerStrings_.clear();
          break;

        case _OrthancPluginDatabaseAnswerType_DicomTag:
          assert(answerDicomMap_ != NULL);
          answerDicomMap_->Clear();
          break;

        case _OrthancPluginDatabaseAnswerType_Change:
          assert(answerChanges_ != NULL);
          answerChanges_->clear();
          break;

        case _OrthancPluginDatabaseAnswerType_ExportedResource:
          assert(answerExportedResources_ != NULL);
          answerExportedResources_->clear();
          break;

        default:
          LOG(ERROR) << "Unhandled type of answer for custom index plugin: " << answer.type;
          throw OrthancException(ErrorCode_DatabasePlugin);
      }
    }
    else if (type_ != answer.type)
    {
      LOG(ERROR) << "Error in the plugin protocol: Cannot change the answer type";
      throw OrthancException(ErrorCode_DatabasePlugin);
    }

    switch (answer.type)
    {
      case _OrthancPluginDatabaseAnswerType_Int32:
      {
        answerInt32_.push_back(answer.valueInt32);
        break;
      }

      case _OrthancPluginDatabaseAnswerType_Int64:
      {
        answerInt64_.push_back(answer.valueInt64);
        break;
      }

      case _OrthancPluginDatabaseAnswerType_Resource:
      {
        OrthancPluginResourceType type = static_cast<OrthancPluginResourceType>(answer.valueInt32);
        answerResources_.push_back(std::make_pair(answer.valueInt64, Plugins::Convert(type)));
        break;
      }

      case _OrthancPluginDatabaseAnswerType_Attachment:
      {
        const OrthancPluginAttachment& attachment = 
          *reinterpret_cast<const OrthancPluginAttachment*>(answer.valueGeneric);

        answerAttachments_.push_back(Convert(attachment));
        break;
      }

      case _OrthancPluginDatabaseAnswerType_DicomTag:
      {
        const OrthancPluginDicomTag& tag = *reinterpret_cast<const OrthancPluginDicomTag*>(answer.valueGeneric);
        assert(answerDicomMap_ != NULL);
        answerDicomMap_->SetValue(tag.group, tag.element, std::string(tag.value), false);
        break;
      }

      case _OrthancPluginDatabaseAnswerType_String:
      {
        if (answer.valueString == NULL)
        {
          throw OrthancException(ErrorCode_DatabasePlugin);
        }

        if (type_ == _OrthancPluginDatabaseAnswerType_None)
        {
          type_ = _OrthancPluginDatabaseAnswerType_String;
          answerStrings_.clear();
        }
        else if (type_ != _OrthancPluginDatabaseAnswerType_String)
        {
          throw OrthancException(ErrorCode_DatabasePlugin);
        }

        answerStrings_.push_back(std::string(answer.valueString));
        break;
      }

      case _OrthancPluginDatabaseAnswerType_Change:
      {
        assert(answerDone_ != NULL);
        if (answer.valueUint32 == 1)
        {
          *answerDone_ = true;
        }
        else if (*answerDone_)
        {
          throw OrthancException(ErrorCode_DatabasePlugin);
        }
        else
        {
          const OrthancPluginChange& change = *reinterpret_cast<const OrthancPluginChange*>(answer.valueGeneric);
          assert(answerChanges_ != NULL);
          answerChanges_->push_back
            (ServerIndexChange(change.seq,
                               static_cast<ChangeType>(change.changeType),
                               Plugins::Convert(change.resourceType),
                               change.publicId,
                               change.date));                                   
        }

        break;
      }

      case _OrthancPluginDatabaseAnswerType_ExportedResource:
      {
        assert(answerDone_ != NULL);
        if (answer.valueUint32 == 1)
        {
          *answerDone_ = true;
        }
        else if (*answerDone_)
        {
          throw OrthancException(ErrorCode_DatabasePlugin);
        }
        else
        {
          const OrthancPluginExportedResource& exported = 
            *reinterpret_cast<const OrthancPluginExportedResource*>(answer.valueGeneric);
          assert(answerExportedResources_ != NULL);
          answerExportedResources_->push_back
            (ExportedResource(exported.seq,
                              Plugins::Convert(exported.resourceType),
                              exported.publicId,
                              exported.modality,
                              exported.date,
                              exported.patientId,
                              exported.studyInstanceUid,
                              exported.seriesInstanceUid,
                              exported.sopInstanceUid));
        }

        break;
      }

      default:
        LOG(ERROR) << "Unhandled type of answer for custom index plugin: " << answer.type;
        throw OrthancException(ErrorCode_DatabasePlugin);
    }
  }
}
