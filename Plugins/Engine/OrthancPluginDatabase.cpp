/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
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


#include "../../Core/PrecompiledHeaders.h"
#include "OrthancPluginDatabase.h"

#include "../../Core/OrthancException.h"
#include "../../Core/Logging.h"

#include <cassert>

namespace Orthanc
{
  static OrthancPluginResourceType Convert(ResourceType type)
  {
    switch (type)
    {
      case ResourceType_Patient:
        return OrthancPluginResourceType_Patient;

      case ResourceType_Study:
        return OrthancPluginResourceType_Study;

      case ResourceType_Series:
        return OrthancPluginResourceType_Series;

      case ResourceType_Instance:
        return OrthancPluginResourceType_Instance;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }
  }


  static ResourceType Convert(OrthancPluginResourceType type)
  {
    switch (type)
    {
      case OrthancPluginResourceType_Patient:
        return ResourceType_Patient;

      case OrthancPluginResourceType_Study:
        return ResourceType_Study;

      case OrthancPluginResourceType_Series:
        return ResourceType_Series;

      case OrthancPluginResourceType_Instance:
        return ResourceType_Instance;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }
  }


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
      throw OrthancException(ErrorCode_Plugin);
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
      throw OrthancException(ErrorCode_Plugin);
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
      throw OrthancException(ErrorCode_Plugin);
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
      throw OrthancException(ErrorCode_Plugin);
    }
  }


  OrthancPluginDatabase::OrthancPluginDatabase(const OrthancPluginDatabaseBackend& backend,
                                               const OrthancPluginDatabaseExtensions* extensions,
                                               size_t extensionsSize,
                                               void *payload) : 
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

    if (backend_.addAttachment(payload_, id, &tmp) != 0)
    {
      throw OrthancException(ErrorCode_Plugin);
    }
  }


  void OrthancPluginDatabase::AttachChild(int64_t parent,
                                          int64_t child)
  {
    if (backend_.attachChild(payload_, parent, child) != 0)
    {
      throw OrthancException(ErrorCode_Plugin);
    }
  }


  void OrthancPluginDatabase::ClearChanges()
  {
    if (backend_.clearChanges(payload_) != 0)
    {
      throw OrthancException(ErrorCode_Plugin);
    }
  }


  void OrthancPluginDatabase::ClearExportedResources()
  {
    if (backend_.clearExportedResources(payload_) != 0)
    {
      throw OrthancException(ErrorCode_Plugin);
    }
  }


  int64_t OrthancPluginDatabase::CreateResource(const std::string& publicId,
                                                ResourceType type)
  {
    int64_t id;

    if (backend_.createResource(&id, payload_, publicId.c_str(), Convert(type)) != 0)
    {
      throw OrthancException(ErrorCode_Plugin);
    }

    return id;
  }


  void OrthancPluginDatabase::DeleteAttachment(int64_t id,
                                               FileContentType attachment)
  {
    if (backend_.deleteAttachment(payload_, id, static_cast<int32_t>(attachment)) != 0)
    {
      throw OrthancException(ErrorCode_Plugin);
    }
  }


  void OrthancPluginDatabase::DeleteMetadata(int64_t id,
                                             MetadataType type)
  {
    if (backend_.deleteMetadata(payload_, id, static_cast<int32_t>(type)) != 0)
    {
      throw OrthancException(ErrorCode_Plugin);
    }
  }


  void OrthancPluginDatabase::DeleteResource(int64_t id)
  {
    if (backend_.deleteResource(payload_, id) != 0)
    {
      throw OrthancException(ErrorCode_Plugin);
    }
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
        throw OrthancException(ErrorCode_Plugin);
      }

      target[*it] = value;
    }
  }


  void OrthancPluginDatabase::GetAllPublicIds(std::list<std::string>& target,
                                              ResourceType resourceType)
  {
    ResetAnswers();

    if (backend_.getAllPublicIds(GetContext(), payload_, Convert(resourceType)) != 0)
    {
      throw OrthancException(ErrorCode_Plugin);
    }

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

      if (extensions_.getAllPublicIdsWithLimit(GetContext(), payload_, Convert(resourceType), since, limit) != 0)
      {
        throw OrthancException(ErrorCode_Plugin);
      }

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

    if (backend_.getChanges(GetContext(), payload_, since, maxResults) != 0)
    {
      throw OrthancException(ErrorCode_Plugin);
    }
  }


  void OrthancPluginDatabase::GetChildrenInternalId(std::list<int64_t>& target,
                                                    int64_t id)
  {
    ResetAnswers();

    if (backend_.getChildrenInternalId(GetContext(), payload_, id) != 0)
    {
      throw OrthancException(ErrorCode_Plugin);
    }

    ForwardAnswers(target);
  }


  void OrthancPluginDatabase::GetChildrenPublicId(std::list<std::string>& target,
                                                  int64_t id)
  {
    ResetAnswers();

    if (backend_.getChildrenPublicId(GetContext(), payload_, id) != 0)
    {
      throw OrthancException(ErrorCode_Plugin);
    }

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

    if (backend_.getExportedResources(GetContext(), payload_, since, maxResults) != 0)
    {
      throw OrthancException(ErrorCode_Plugin);
    }
  }


  void OrthancPluginDatabase::GetLastChange(std::list<ServerIndexChange>& target /*out*/)
  {
    bool ignored = false;

    ResetAnswers();
    answerChanges_ = &target;
    answerDone_ = &ignored;

    if (backend_.getLastChange(GetContext(), payload_) != 0)
    {
      throw OrthancException(ErrorCode_Plugin);
    }
  }


  void OrthancPluginDatabase::GetLastExportedResource(std::list<ExportedResource>& target /*out*/)
  {
    bool ignored = false;

    ResetAnswers();
    answerExportedResources_ = &target;
    answerDone_ = &ignored;

    if (backend_.getLastExportedResource(GetContext(), payload_) != 0)
    {
      throw OrthancException(ErrorCode_Plugin);
    }
  }


  void OrthancPluginDatabase::GetMainDicomTags(DicomMap& map,
                                               int64_t id)
  {
    ResetAnswers();
    answerDicomMap_ = &map;

    if (backend_.getMainDicomTags(GetContext(), payload_, id) != 0)
    {
      throw OrthancException(ErrorCode_Plugin);
    }
  }


  std::string OrthancPluginDatabase::GetPublicId(int64_t resourceId)
  {
    ResetAnswers();
    std::string s;

    if (backend_.getPublicId(GetContext(), payload_, resourceId) != 0 ||
        !ForwardSingleAnswer(s))
    {
      throw OrthancException(ErrorCode_Plugin);
    }

    return s;
  }


  uint64_t OrthancPluginDatabase::GetResourceCount(ResourceType resourceType)
  {
    uint64_t count;

    if (backend_.getResourceCount(&count, payload_, Convert(resourceType)) != 0)
    {
      throw OrthancException(ErrorCode_Plugin);
    }

    return count;
  }


  ResourceType OrthancPluginDatabase::GetResourceType(int64_t resourceId)
  {
    OrthancPluginResourceType type;

    if (backend_.getResourceType(&type, payload_, resourceId) != 0)
    {
      throw OrthancException(ErrorCode_Plugin);
    }

    return Convert(type);
  }


  uint64_t OrthancPluginDatabase::GetTotalCompressedSize()
  {
    uint64_t size;

    if (backend_.getTotalCompressedSize(&size, payload_) != 0)
    {
      throw OrthancException(ErrorCode_Plugin);
    }

    return size;
  }

    
  uint64_t OrthancPluginDatabase::GetTotalUncompressedSize()
  {
    uint64_t size;

    if (backend_.getTotalUncompressedSize(&size, payload_) != 0)
    {
      throw OrthancException(ErrorCode_Plugin);
    }

    return size;
  }


  bool OrthancPluginDatabase::IsExistingResource(int64_t internalId)
  {
    int32_t existing;

    if (backend_.isExistingResource(&existing, payload_, internalId) != 0)
    {
      throw OrthancException(ErrorCode_Plugin);
    }

    return (existing != 0);
  }


  bool OrthancPluginDatabase::IsProtectedPatient(int64_t internalId)
  {
    int32_t isProtected;

    if (backend_.isProtectedPatient(&isProtected, payload_, internalId) != 0)
    {
      throw OrthancException(ErrorCode_Plugin);
    }

    return (isProtected != 0);
  }


  void OrthancPluginDatabase::ListAvailableMetadata(std::list<MetadataType>& target,
                                                    int64_t id)
  {
    ResetAnswers();

    if (backend_.listAvailableMetadata(GetContext(), payload_, id) != 0)
    {
      throw OrthancException(ErrorCode_Plugin);
    }

    if (type_ != _OrthancPluginDatabaseAnswerType_None &&
        type_ != _OrthancPluginDatabaseAnswerType_Int32)
    {
      throw OrthancException(ErrorCode_Plugin);
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

    if (backend_.listAvailableAttachments(GetContext(), payload_, id) != 0)
    {
      throw OrthancException(ErrorCode_Plugin);
    }

    if (type_ != _OrthancPluginDatabaseAnswerType_None &&
        type_ != _OrthancPluginDatabaseAnswerType_Int32)
    {
      throw OrthancException(ErrorCode_Plugin);
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
    tmp.resourceType = Convert(change.GetResourceType());
    tmp.publicId = change.GetPublicId().c_str();
    tmp.date = change.GetDate().c_str();

    if (backend_.logChange(payload_, &tmp) != 0)
    {
      throw OrthancException(ErrorCode_Plugin);
    }
  }


  void OrthancPluginDatabase::LogExportedResource(const ExportedResource& resource)
  {
    OrthancPluginExportedResource tmp;
    tmp.seq = resource.GetSeq();
    tmp.resourceType = Convert(resource.GetResourceType());
    tmp.publicId = resource.GetPublicId().c_str();
    tmp.modality = resource.GetModality().c_str();
    tmp.date = resource.GetDate().c_str();
    tmp.patientId = resource.GetPatientId().c_str();
    tmp.studyInstanceUid = resource.GetStudyInstanceUid().c_str();
    tmp.seriesInstanceUid = resource.GetSeriesInstanceUid().c_str();
    tmp.sopInstanceUid = resource.GetSopInstanceUid().c_str();

    if (backend_.logExportedResource(payload_, &tmp) != 0)
    {
      throw OrthancException(ErrorCode_Plugin);
    }
  }

    
  bool OrthancPluginDatabase::LookupAttachment(FileInfo& attachment,
                                               int64_t id,
                                               FileContentType contentType)
  {
    ResetAnswers();

    if (backend_.lookupAttachment(GetContext(), payload_, id, static_cast<int32_t>(contentType)))
    {
      throw OrthancException(ErrorCode_Plugin);
    }

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
      throw OrthancException(ErrorCode_Plugin);
    }
  }


  bool OrthancPluginDatabase::LookupGlobalProperty(std::string& target,
                                                   GlobalProperty property)
  {
    ResetAnswers();

    if (backend_.lookupGlobalProperty(GetContext(), payload_, 
                                      static_cast<int32_t>(property)))
    {
      throw OrthancException(ErrorCode_Plugin);
    }

    return ForwardSingleAnswer(target);
  }


  void OrthancPluginDatabase::LookupIdentifier(std::list<int64_t>& target,
                                               const DicomTag& tag,
                                               const std::string& value)
  {
    ResetAnswers();

    OrthancPluginDicomTag tmp;
    tmp.group = tag.GetGroup();
    tmp.element = tag.GetElement();
    tmp.value = value.c_str();

    if (backend_.lookupIdentifier(GetContext(), payload_, &tmp) != 0)
    {
      throw OrthancException(ErrorCode_Plugin);
    }

    ForwardAnswers(target);
  }


  void OrthancPluginDatabase::LookupIdentifier(std::list<int64_t>& target,
                                               const std::string& value)
  {
    ResetAnswers();

    if (backend_.lookupIdentifier2(GetContext(), payload_, value.c_str()) != 0)
    {
      throw OrthancException(ErrorCode_Plugin);
    }

    ForwardAnswers(target);
  }


  bool OrthancPluginDatabase::LookupMetadata(std::string& target,
                                             int64_t id,
                                             MetadataType type)
  {
    ResetAnswers();

    if (backend_.lookupMetadata(GetContext(), payload_, id, static_cast<int32_t>(type)))
    {
      throw OrthancException(ErrorCode_Plugin);
    }

    return ForwardSingleAnswer(target);
  }


  bool OrthancPluginDatabase::LookupParent(int64_t& parentId,
                                           int64_t resourceId)
  {
    ResetAnswers();

    if (backend_.lookupParent(GetContext(), payload_, resourceId))
    {
      throw OrthancException(ErrorCode_Plugin);
    }

    return ForwardSingleAnswer(parentId);
  }


  bool OrthancPluginDatabase::LookupResource(int64_t& id,
                                             ResourceType& type,
                                             const std::string& publicId)
  {
    ResetAnswers();

    if (backend_.lookupResource(GetContext(), payload_, publicId.c_str()))
    {
      throw OrthancException(ErrorCode_Plugin);
    }

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
      throw OrthancException(ErrorCode_Plugin);
    }
  }


  bool OrthancPluginDatabase::SelectPatientToRecycle(int64_t& internalId)
  {
    ResetAnswers();

    if (backend_.selectPatientToRecycle(GetContext(), payload_))
    {
      throw OrthancException(ErrorCode_Plugin);
    }

    return ForwardSingleAnswer(internalId);
  }


  bool OrthancPluginDatabase::SelectPatientToRecycle(int64_t& internalId,
                                                     int64_t patientIdToAvoid)
  {
    ResetAnswers();

    if (backend_.selectPatientToRecycle2(GetContext(), payload_, patientIdToAvoid))
    {
      throw OrthancException(ErrorCode_Plugin);
    }

    return ForwardSingleAnswer(internalId);
  }


  void OrthancPluginDatabase::SetGlobalProperty(GlobalProperty property,
                                                const std::string& value)
  {
    if (backend_.setGlobalProperty(payload_, static_cast<int32_t>(property), 
                                   value.c_str()) != 0)
    {
      throw OrthancException(ErrorCode_Plugin);
    }
  }


  void OrthancPluginDatabase::SetMainDicomTag(int64_t id,
                                              const DicomTag& tag,
                                              const std::string& value)
  {
    int32_t status;
    OrthancPluginDicomTag tmp;
    tmp.group = tag.GetGroup();
    tmp.element = tag.GetElement();
    tmp.value = value.c_str();

    if (tag.IsIdentifier())
    {
      status = backend_.setIdentifierTag(payload_, id, &tmp);
    }
    else
    {
      status = backend_.setMainDicomTag(payload_, id, &tmp);
    }

    if (status != 0)
    {
      throw OrthancException(ErrorCode_Plugin);
    }
  }


  void OrthancPluginDatabase::SetMetadata(int64_t id,
                                          MetadataType type,
                                          const std::string& value)
  {
    if (backend_.setMetadata(payload_, id, static_cast<int32_t>(type), 
                             value.c_str()) != 0)
    {
      throw OrthancException(ErrorCode_Plugin);
    }
  }


  void OrthancPluginDatabase::SetProtectedPatient(int64_t internalId, 
                                                  bool isProtected)
  {
    if (backend_.setProtectedPatient(payload_, internalId, isProtected) != 0)
    {
      throw OrthancException(ErrorCode_Plugin);
    }
  }


  class OrthancPluginDatabase::Transaction : public SQLite::ITransaction
  {
  private:
    const OrthancPluginDatabaseBackend& backend_;
    void* payload_;

  public:
    Transaction(const OrthancPluginDatabaseBackend& backend,
                void* payload) :
      backend_(backend),
      payload_(payload)
    {
    }

    virtual void Begin()
    {
      if (backend_.startTransaction(payload_) != 0)
      {
        throw OrthancException(ErrorCode_Plugin);
      }
    }

    virtual void Rollback()
    {
      if (backend_.rollbackTransaction(payload_) != 0)
      {
        throw OrthancException(ErrorCode_Plugin);
      }
    }

    virtual void Commit()
    {
      if (backend_.commitTransaction(payload_) != 0)
      {
        throw OrthancException(ErrorCode_Plugin);
      }
    }
  };


  SQLite::ITransaction* OrthancPluginDatabase::StartTransaction()
  {
    return new Transaction(backend_, payload_);
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
        ResourceType type = Convert(static_cast<OrthancPluginResourceType>(answer.valueInt32));
        listener.SignalRemainingAncestor(type, answer.valueString);
        break;
      }
      
      case _OrthancPluginDatabaseAnswerType_DeletedResource:
      {
        ResourceType type = Convert(static_cast<OrthancPluginResourceType>(answer.valueInt32));
        ServerIndexChange change(ChangeType_Deleted, type, answer.valueString);
        listener.SignalChange(change);
        break;
      }

      default:
        throw OrthancException(ErrorCode_Plugin);
    }
  }


  unsigned int OrthancPluginDatabase::GetDatabaseVersion()
  {
    if (extensions_.getDatabaseVersion != NULL)
    {
      uint32_t version;
      if (extensions_.getDatabaseVersion(&version, payload_) != 0)
      {
        throw OrthancException(ErrorCode_Plugin);
      }

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
      if (extensions_.upgradeDatabase(
            payload_, targetVersion, 
            reinterpret_cast<OrthancPluginStorageArea*>(&storageArea)) != 0)
      {
        throw OrthancException(ErrorCode_Plugin);
      }
    }
  }


  void OrthancPluginDatabase::AnswerReceived(const _OrthancPluginDatabaseAnswer& answer)
  {
    if (answer.type == _OrthancPluginDatabaseAnswerType_None)
    {
      throw OrthancException(ErrorCode_Plugin);
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
          throw OrthancException(ErrorCode_Plugin);
      }
    }
    else if (type_ != answer.type)
    {
      LOG(ERROR) << "Error in the plugin protocol: Cannot change the answer type";
      throw OrthancException(ErrorCode_Plugin);
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
        answerResources_.push_back(std::make_pair(answer.valueInt64, Convert(type)));
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
        answerDicomMap_->SetValue(tag.group, tag.element, std::string(tag.value));
        break;
      }

      case _OrthancPluginDatabaseAnswerType_String:
      {
        if (answer.valueString == NULL)
        {
          throw OrthancException(ErrorCode_Plugin);
        }

        if (type_ == _OrthancPluginDatabaseAnswerType_None)
        {
          type_ = _OrthancPluginDatabaseAnswerType_String;
          answerStrings_.clear();
        }
        else if (type_ != _OrthancPluginDatabaseAnswerType_String)
        {
          throw OrthancException(ErrorCode_Plugin);
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
          throw OrthancException(ErrorCode_Plugin);
        }
        else
        {
          const OrthancPluginChange& change = *reinterpret_cast<const OrthancPluginChange*>(answer.valueGeneric);
          assert(answerChanges_ != NULL);
          answerChanges_->push_back
            (ServerIndexChange(change.seq,
                               static_cast<ChangeType>(change.changeType),
                               Convert(change.resourceType),
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
          throw OrthancException(ErrorCode_Plugin);
        }
        else
        {
          const OrthancPluginExportedResource& exported = 
            *reinterpret_cast<const OrthancPluginExportedResource*>(answer.valueGeneric);
          assert(answerExportedResources_ != NULL);
          answerExportedResources_->push_back
            (ExportedResource(exported.seq,
                              Convert(exported.resourceType),
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
        throw OrthancException(ErrorCode_Plugin);
    }
  }
}
