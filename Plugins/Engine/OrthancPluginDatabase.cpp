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


#include "../../OrthancServer/PrecompiledHeadersServer.h"
#include "OrthancPluginDatabase.h"

#if ORTHANC_ENABLE_PLUGINS != 1
#error The plugin support is disabled
#endif


#include "../../Core/Logging.h"
#include "../../Core/OrthancException.h"
#include "PluginsEnumerations.h"

#include <cassert>

namespace Orthanc
{
  class OrthancPluginDatabase::Transaction : public IDatabaseWrapper::ITransaction
  {
  private:
    OrthancPluginDatabase&  that_;

    void CheckSuccess(OrthancPluginErrorCode code) const
    {
      if (code != OrthancPluginErrorCode_Success)
      {
        that_.errorDictionary_.LogError(code, true);
        throw OrthancException(static_cast<ErrorCode>(code));
      }
    }

  public:
    Transaction(OrthancPluginDatabase& that) :
    that_(that)
    {
    }

    virtual void Begin()
    {
      CheckSuccess(that_.backend_.startTransaction(that_.payload_));
    }

    virtual void Rollback()
    {
      CheckSuccess(that_.backend_.rollbackTransaction(that_.payload_));
    }

    virtual void Commit(int64_t diskSizeDelta)
    {
      if (that_.fastGetTotalSize_)
      {
        CheckSuccess(that_.backend_.commitTransaction(that_.payload_));
      }
      else
      {
        if (static_cast<int64_t>(that_.currentDiskSize_) + diskSizeDelta < 0)
        {
          throw OrthancException(ErrorCode_DatabasePlugin);
        }

        uint64_t newDiskSize = (that_.currentDiskSize_ + diskSizeDelta);

        assert(newDiskSize == that_.GetTotalCompressedSize());

        CheckSuccess(that_.backend_.commitTransaction(that_.payload_));

        // The transaction has succeeded, we can commit the new disk size
        that_.currentDiskSize_ = newDiskSize;
      }
    }
  };


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
    answerMatchingResources_ = NULL;
    answerMatchingInstances_ = NULL;
    answerMetadata_ = NULL;
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
    backend_(backend),
    payload_(payload),
    listener_(NULL)
  {
    static const char* const MISSING = "  Missing extension in database index plugin: ";
    
    ResetAnswers();

    memset(&extensions_, 0, sizeof(extensions_));

    size_t size = sizeof(extensions_);
    if (extensionsSize < size)
    {
      size = extensionsSize;  // Not all the extensions are available
    }

    memcpy(&extensions_, extensions, size);

    bool isOptimal = true;

    if (extensions_.lookupResources == NULL)
    {
      LOG(INFO) << MISSING << "LookupIdentifierRange()";
      isOptimal = false;
    }

    if (extensions_.createInstance == NULL)
    {
      LOG(INFO) << MISSING << "CreateInstance()";
      isOptimal = false;
    }

    if (extensions_.setResourcesContent == NULL)
    {
      LOG(INFO) << MISSING << "SetResourcesContent()";
      isOptimal = false;
    }

    if (extensions_.getChildrenMetadata == NULL)
    {
      LOG(INFO) << MISSING << "GetChildrenMetadata()";
      isOptimal = false;
    }

    if (extensions_.getAllMetadata == NULL)
    {
      LOG(INFO) << MISSING << "GetAllMetadata()";
      isOptimal = false;
    }

    if (extensions_.lookupResourceAndParent == NULL)
    {
      LOG(INFO) << MISSING << "LookupResourceAndParent()";
      isOptimal = false;
    }

    if (isOptimal)
    {
      LOG(INFO) << "The performance of the database index plugin "
                << "is optimal for this version of Orthanc";
    }
    else
    {
      LOG(WARNING) << "Performance warning in the database index: "
                   << "Some extensions are missing in the plugin";
    }

    if (extensions_.getLastChangeIndex == NULL)
    {
      LOG(WARNING) << "The database extension GetLastChangeIndex() is missing";
    }

    if (extensions_.tagMostRecentPatient == NULL)
    {
      LOG(WARNING) << "The database extension TagMostRecentPatient() is missing "
                   << "(affected by issue 58)";
    }
  }


  void OrthancPluginDatabase::Open()
  {
    CheckSuccess(backend_.open(payload_));

    {
      Transaction transaction(*this);
      transaction.Begin();

      std::string tmp;
      fastGetTotalSize_ =
        (LookupGlobalProperty(tmp, GlobalProperty_GetTotalSizeIsFast) &&
         tmp == "1");
      
      if (fastGetTotalSize_)
      {
        currentDiskSize_ = 0;   // Unused
      }
      else
      {
        // This is the case of database plugins using Orthanc SDK <= 1.5.2
        LOG(WARNING) << "Your database index plugin is not compatible with multiple Orthanc writers";
        currentDiskSize_ = GetTotalCompressedSize();
      }

      transaction.Commit(0);
    }
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
    if (extensions_.getAllMetadata == NULL)
    {
      // Fallback implementation if extension is missing
      target.clear();

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
          MetadataType type = static_cast<MetadataType>(*it);

          std::string value;
          if (LookupMetadata(value, id, type))
          {
            target[type] = value;
          }
        }
      }
    }
    else
    {
      ResetAnswers();

      answerMetadata_ = &target;
      target.clear();
      
      CheckSuccess(extensions_.getAllMetadata(GetContext(), payload_, id));

      if (type_ != _OrthancPluginDatabaseAnswerType_None &&
          type_ != _OrthancPluginDatabaseAnswerType_Metadata)
      {
        throw OrthancException(ErrorCode_DatabasePlugin);
      }
    }
  }


  void OrthancPluginDatabase::GetAllInternalIds(std::list<int64_t>& target,
                                                ResourceType resourceType)
  {
    if (extensions_.getAllInternalIds == NULL)
    {
      throw OrthancException(ErrorCode_DatabasePlugin,
                             "The database plugin does not implement the mandatory GetAllInternalIds() extension");
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
      throw OrthancException(ErrorCode_DatabasePlugin,
                             "Your custom index plugin does not implement the mandatory ClearMainDicomTags() extension");
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


  IDatabaseWrapper::ITransaction* OrthancPluginDatabase::StartTransaction()
  {
    return new Transaction(*this);
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
      Transaction transaction(*this);
      transaction.Begin();

      OrthancPluginErrorCode code = extensions_.upgradeDatabase(
        payload_, targetVersion, 
        reinterpret_cast<OrthancPluginStorageArea*>(&storageArea));

      if (code == OrthancPluginErrorCode_Success)
      {
        transaction.Commit(0);
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

        case _OrthancPluginDatabaseAnswerType_MatchingResource:
          assert(answerMatchingResources_ != NULL);
          answerMatchingResources_->clear();

          if (answerMatchingInstances_ != NULL)
          {
            answerMatchingInstances_->clear();
          }
          
          break;

        case _OrthancPluginDatabaseAnswerType_Metadata:
          assert(answerMetadata_ != NULL);
          answerMetadata_->clear();
          break;

        default:
          throw OrthancException(ErrorCode_DatabasePlugin,
                                 "Unhandled type of answer for custom index plugin: " +
                                 boost::lexical_cast<std::string>(answer.type));
      }
    }
    else if (type_ != answer.type)
    {
      throw OrthancException(ErrorCode_DatabasePlugin,
                             "Error in the plugin protocol: Cannot change the answer type");
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
          const OrthancPluginChange& change =
            *reinterpret_cast<const OrthancPluginChange*>(answer.valueGeneric);
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

      case _OrthancPluginDatabaseAnswerType_MatchingResource:
      {
        const OrthancPluginMatchingResource& match = 
          *reinterpret_cast<const OrthancPluginMatchingResource*>(answer.valueGeneric);

        if (match.resourceId == NULL)
        {
          throw OrthancException(ErrorCode_DatabasePlugin);
        }

        assert(answerMatchingResources_ != NULL);
        answerMatchingResources_->push_back(match.resourceId);

        if (answerMatchingInstances_ != NULL)
        {
          if (match.someInstanceId == NULL)
          {
            throw OrthancException(ErrorCode_DatabasePlugin);
          }

          answerMatchingInstances_->push_back(match.someInstanceId);
        }
 
        break;
      }

      case _OrthancPluginDatabaseAnswerType_Metadata:
      {
        const OrthancPluginResourcesContentMetadata& metadata =
          *reinterpret_cast<const OrthancPluginResourcesContentMetadata*>(answer.valueGeneric);

        MetadataType type = static_cast<MetadataType>(metadata.metadata);

        if (metadata.value == NULL)
        {
          throw OrthancException(ErrorCode_DatabasePlugin);
        }

        assert(answerMetadata_ != NULL &&
               answerMetadata_->find(type) == answerMetadata_->end());
        (*answerMetadata_) [type] = metadata.value;
        break;
      }

      default:
        throw OrthancException(ErrorCode_DatabasePlugin,
                               "Unhandled type of answer for custom index plugin: " +
                               boost::lexical_cast<std::string>(answer.type));
    }
  }

    
  bool OrthancPluginDatabase::IsDiskSizeAbove(uint64_t threshold)
  {
    if (fastGetTotalSize_)
    {
      return GetTotalCompressedSize() > threshold;
    }
    else
    {
      assert(GetTotalCompressedSize() == currentDiskSize_);
      return currentDiskSize_ > threshold;
    }      
  }


  void OrthancPluginDatabase::ApplyLookupResources(std::list<std::string>& resourcesId,
                                                   std::list<std::string>* instancesId,
                                                   const std::vector<DatabaseConstraint>& lookup,
                                                   ResourceType queryLevel,
                                                   size_t limit)
  {
    if (extensions_.lookupResources == NULL)
    {
      // Fallback to compatibility mode
      ILookupResources::Apply
        (*this, *this, resourcesId, instancesId, lookup, queryLevel, limit);
    }
    else
    {
      std::vector<OrthancPluginDatabaseConstraint> constraints;
      std::vector< std::vector<const char*> > constraintsValues;

      constraints.resize(lookup.size());
      constraintsValues.resize(lookup.size());

      for (size_t i = 0; i < lookup.size(); i++)
      {
        lookup[i].EncodeForPlugins(constraints[i], constraintsValues[i]);
      }

      ResetAnswers();
      answerMatchingResources_ = &resourcesId;
      answerMatchingInstances_ = instancesId;
      
      CheckSuccess(extensions_.lookupResources(GetContext(), payload_, lookup.size(),
                                               (lookup.empty() ? NULL : &constraints[0]),
                                               Plugins::Convert(queryLevel),
                                               limit, (instancesId == NULL ? 0 : 1)));
    }
  }


  bool OrthancPluginDatabase::CreateInstance(
    IDatabaseWrapper::CreateInstanceResult& result,
    int64_t& instanceId,
    const std::string& patient,
    const std::string& study,
    const std::string& series,
    const std::string& instance)
  {
    if (extensions_.createInstance == NULL)
    {
      // Fallback to compatibility mode
      return ICreateInstance::Apply
        (*this, result, instanceId, patient, study, series, instance);
    }
    else
    {
      OrthancPluginCreateInstanceResult output;
      memset(&output, 0, sizeof(output));

      CheckSuccess(extensions_.createInstance(&output, payload_, patient.c_str(),
                                              study.c_str(), series.c_str(), instance.c_str()));

      instanceId = output.instanceId;
      
      if (output.isNewInstance)
      {
        result.isNewPatient_ = output.isNewPatient;
        result.isNewStudy_ = output.isNewStudy;
        result.isNewSeries_ = output.isNewSeries;
        result.patientId_ = output.patientId;
        result.studyId_ = output.studyId;
        result.seriesId_ = output.seriesId;
        return true;
      }
      else
      {
        return false;
      }
    }
  }


  void OrthancPluginDatabase::LookupIdentifier(std::list<int64_t>& result,
                                               ResourceType level,
                                               const DicomTag& tag,
                                               Compatibility::IdentifierConstraintType type,
                                               const std::string& value)
  {
    if (extensions_.lookupIdentifier3 == NULL)
    {
      throw OrthancException(ErrorCode_DatabasePlugin,
                             "The database plugin does not implement the mandatory LookupIdentifier3() extension");
    }

    OrthancPluginDicomTag tmp;
    tmp.group = tag.GetGroup();
    tmp.element = tag.GetElement();
    tmp.value = value.c_str();

    ResetAnswers();
    CheckSuccess(extensions_.lookupIdentifier3(GetContext(), payload_, Plugins::Convert(level),
                                               &tmp, Compatibility::Convert(type)));
    ForwardAnswers(result);
  }


  void OrthancPluginDatabase::LookupIdentifierRange(std::list<int64_t>& result,
                                                    ResourceType level,
                                                    const DicomTag& tag,
                                                    const std::string& start,
                                                    const std::string& end)
  {
    if (extensions_.lookupIdentifierRange == NULL)
    {
      // Default implementation, for plugins using Orthanc SDK <= 1.3.2

      LookupIdentifier(result, level, tag, Compatibility::IdentifierConstraintType_GreaterOrEqual, start);

      std::list<int64_t> b;
      LookupIdentifier(result, level, tag, Compatibility::IdentifierConstraintType_SmallerOrEqual, end);

      result.splice(result.end(), b);
    }
    else
    {
      ResetAnswers();
      CheckSuccess(extensions_.lookupIdentifierRange(GetContext(), payload_, Plugins::Convert(level),
                                                     tag.GetGroup(), tag.GetElement(),
                                                     start.c_str(), end.c_str()));
      ForwardAnswers(result);
    }
  }


  void OrthancPluginDatabase::SetResourcesContent(const Orthanc::ResourcesContent& content)
  {
    if (extensions_.setResourcesContent == NULL)
    {
      ISetResourcesContent::Apply(*this, content);
    }
    else
    {
      std::vector<OrthancPluginResourcesContentTags> identifierTags;
      std::vector<OrthancPluginResourcesContentTags> mainDicomTags;
      std::vector<OrthancPluginResourcesContentMetadata> metadata;

      identifierTags.reserve(content.GetListTags().size());
      mainDicomTags.reserve(content.GetListTags().size());
      metadata.reserve(content.GetListMetadata().size());

      for (ResourcesContent::ListTags::const_iterator
             it = content.GetListTags().begin(); it != content.GetListTags().end(); ++it)
      {
        OrthancPluginResourcesContentTags tmp;
        tmp.resource = it->resourceId_;
        tmp.group = it->tag_.GetGroup();
        tmp.element = it->tag_.GetElement();
        tmp.value = it->value_.c_str();

        if (it->isIdentifier_)
        {
          identifierTags.push_back(tmp);
        }
        else
        {
          mainDicomTags.push_back(tmp);
        }
      }

      for (ResourcesContent::ListMetadata::const_iterator
             it = content.GetListMetadata().begin(); it != content.GetListMetadata().end(); ++it)
      {
        OrthancPluginResourcesContentMetadata tmp;
        tmp.resource = it->resourceId_;
        tmp.metadata = it->metadata_;
        tmp.value = it->value_.c_str();
        metadata.push_back(tmp);
      }

      assert(identifierTags.size() + mainDicomTags.size() == content.GetListTags().size() &&
             metadata.size() == content.GetListMetadata().size());
       
      CheckSuccess(extensions_.setResourcesContent(
                     payload_,
                     identifierTags.size(),
                     (identifierTags.empty() ? NULL : &identifierTags[0]),
                     mainDicomTags.size(),
                     (mainDicomTags.empty() ? NULL : &mainDicomTags[0]),
                     metadata.size(),
                     (metadata.empty() ? NULL : &metadata[0])));
    }
  }



  void OrthancPluginDatabase::GetChildrenMetadata(std::list<std::string>& target,
                                                  int64_t resourceId,
                                                  MetadataType metadata)
  {
    if (extensions_.getChildrenMetadata == NULL)
    {
      IGetChildrenMetadata::Apply(*this, target, resourceId, metadata);
    }
    else
    {
      ResetAnswers();
      CheckSuccess(extensions_.getChildrenMetadata
                   (GetContext(), payload_, resourceId, static_cast<int32_t>(metadata)));
      ForwardAnswers(target);
    }
  }


  int64_t OrthancPluginDatabase::GetLastChangeIndex()
  {
    if (extensions_.getLastChangeIndex == NULL)
    {
      // This was the default behavior in Orthanc <= 1.5.1
      // https://groups.google.com/d/msg/orthanc-users/QhzB6vxYeZ0/YxabgqpfBAAJ
      return 0;
    }
    else
    {
      int64_t result = 0;
      CheckSuccess(extensions_.getLastChangeIndex(&result, payload_));
      return result;
    }
  }

  
  void OrthancPluginDatabase::TagMostRecentPatient(int64_t patient)
  {
    if (extensions_.tagMostRecentPatient != NULL)
    {
      CheckSuccess(extensions_.tagMostRecentPatient(payload_, patient));
    }
  }


  bool OrthancPluginDatabase::LookupResourceAndParent(int64_t& id,
                                                      ResourceType& type,
                                                      std::string& parentPublicId,
                                                      const std::string& publicId)
  {
    if (extensions_.lookupResourceAndParent == NULL)
    {
      return ILookupResourceAndParent::Apply(*this, id, type, parentPublicId, publicId);
    }
    else
    {
      std::list<std::string> parent;

      uint8_t isExisting;
      OrthancPluginResourceType pluginType = OrthancPluginResourceType_Patient;
      
      ResetAnswers();
      CheckSuccess(extensions_.lookupResourceAndParent
                   (GetContext(), &isExisting, &id, &pluginType, payload_, publicId.c_str()));
      ForwardAnswers(parent);

      if (isExisting)
      {
        type = Plugins::Convert(pluginType);

        if (parent.empty())
        {
          if (type != ResourceType_Patient)
          {
            throw OrthancException(ErrorCode_DatabasePlugin);
          }
        }
        else if (parent.size() == 1)
        {
          if ((type != ResourceType_Study &&
               type != ResourceType_Series &&
               type != ResourceType_Instance) ||
              parent.front().empty())
          {
            throw OrthancException(ErrorCode_DatabasePlugin);
          }

          parentPublicId = parent.front();
        }
        else
        {
          throw OrthancException(ErrorCode_DatabasePlugin);
        }

        return true;
      }
      else
      {
        return false;
      }
    }
  }
}
