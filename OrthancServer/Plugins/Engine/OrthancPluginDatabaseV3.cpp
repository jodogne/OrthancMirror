/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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


#include "../../Sources/PrecompiledHeadersServer.h"
#include "OrthancPluginDatabaseV3.h"

#if ORTHANC_ENABLE_PLUGINS != 1
#  error The plugin support is disabled
#endif

#include "../../../OrthancFramework/Sources/Logging.h"
#include "../../../OrthancFramework/Sources/OrthancException.h"
#include "../../Sources/Database/ResourcesContent.h"
#include "../../Sources/Database/VoidDatabaseListener.h"
#include "PluginsEnumerations.h"

#include <cassert>


#define CHECK_FUNCTION_EXISTS(backend, func)                            \
  if (backend.func == NULL)                                             \
  {                                                                     \
    throw OrthancException(                                             \
      ErrorCode_DatabasePlugin, "Missing primitive: " #func "()");      \
  }

namespace Orthanc
{
  class OrthancPluginDatabaseV3::Transaction : public IDatabaseWrapper::ITransaction
  {
  private:
    OrthancPluginDatabaseV3&           that_;
    IDatabaseListener&                 listener_;
    OrthancPluginDatabaseTransaction*  transaction_;

    
    void CheckSuccess(OrthancPluginErrorCode code) const
    {
      that_.CheckSuccess(code);
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


    void ReadStringAnswers(std::list<std::string>& target)
    {
      uint32_t count;
      CheckSuccess(that_.backend_.readAnswersCount(transaction_, &count));

      target.clear();
      for (uint32_t i = 0; i < count; i++)
      {
        const char* value = NULL;
        CheckSuccess(that_.backend_.readAnswerString(transaction_, &value, i));
        if (value == NULL)
        {
          throw OrthancException(ErrorCode_DatabasePlugin);
        }
        else
        {
          target.push_back(value);
        }
      }
    }


    bool ReadSingleStringAnswer(std::string& target)
    {
      uint32_t count;
      CheckSuccess(that_.backend_.readAnswersCount(transaction_, &count));

      if (count == 0)
      {
        return false;
      }
      else if (count == 1)
      {
        const char* value = NULL;
        CheckSuccess(that_.backend_.readAnswerString(transaction_, &value, 0));
        if (value == NULL)
        {
          throw OrthancException(ErrorCode_DatabasePlugin);
        }
        else
        {
          target.assign(value);
          return true;
        }
      }
      else
      {
        throw OrthancException(ErrorCode_DatabasePlugin);
      }
    }


    bool ReadSingleInt64Answer(int64_t& target)
    {
      uint32_t count;
      CheckSuccess(that_.backend_.readAnswersCount(transaction_, &count));

      if (count == 0)
      {
        return false;
      }
      else if (count == 1)
      {
        CheckSuccess(that_.backend_.readAnswerInt64(transaction_, &target, 0));
        return true;
      }
      else
      {
        throw OrthancException(ErrorCode_DatabasePlugin);
      }
    }

    
    ExportedResource ReadAnswerExportedResource(uint32_t answerIndex)
    {
      OrthancPluginExportedResource exported;
      CheckSuccess(that_.backend_.readAnswerExportedResource(transaction_, &exported, answerIndex));

      if (exported.publicId == NULL ||
          exported.modality == NULL ||
          exported.date == NULL ||
          exported.patientId == NULL ||
          exported.studyInstanceUid == NULL ||
          exported.seriesInstanceUid == NULL ||
          exported.sopInstanceUid == NULL)
      {
        throw OrthancException(ErrorCode_DatabasePlugin);
      }
      else
      {
        return ExportedResource(exported.seq,
                                Plugins::Convert(exported.resourceType),
                                exported.publicId,
                                exported.modality,
                                exported.date,
                                exported.patientId,
                                exported.studyInstanceUid,
                                exported.seriesInstanceUid,
                                exported.sopInstanceUid);
      }
    }
    

    ServerIndexChange ReadAnswerChange(uint32_t answerIndex)
    {
      OrthancPluginChange change;
      CheckSuccess(that_.backend_.readAnswerChange(transaction_, &change, answerIndex));

      if (change.publicId == NULL ||
          change.date == NULL)
      {
        throw OrthancException(ErrorCode_DatabasePlugin);
      }
      else
      {
        return ServerIndexChange(change.seq,
                                 static_cast<ChangeType>(change.changeType),
                                 Plugins::Convert(change.resourceType),
                                 change.publicId,
                                 change.date);
      }
    }


    void CheckNoEvent()
    {
      uint32_t count;
      CheckSuccess(that_.backend_.readEventsCount(transaction_, &count));
      if (count != 0)
      {
        throw OrthancException(ErrorCode_DatabasePlugin);
      }
    }


    void ProcessEvents(bool isDeletingAttachment)
    {
      uint32_t count;
      CheckSuccess(that_.backend_.readEventsCount(transaction_, &count));

      for (uint32_t i = 0; i < count; i++)
      {
        OrthancPluginDatabaseEvent event;
        CheckSuccess(that_.backend_.readEvent(transaction_, &event, i));

        switch (event.type)
        {
          case OrthancPluginDatabaseEventType_DeletedAttachment:
            listener_.SignalAttachmentDeleted(Convert(event.content.attachment));
            break;
            
          case OrthancPluginDatabaseEventType_DeletedResource:
            if (isDeletingAttachment)
            {
              // This event should only be triggered by "DeleteResource()"
              throw OrthancException(ErrorCode_DatabasePlugin);
            }
            else
            {
              listener_.SignalResourceDeleted(Plugins::Convert(event.content.resource.level), event.content.resource.publicId);
            }            
            break;
            
          case OrthancPluginDatabaseEventType_RemainingAncestor:
            if (isDeletingAttachment)
            {
              // This event should only triggered by "DeleteResource()"
              throw OrthancException(ErrorCode_DatabasePlugin);
            }
            else
            {
              listener_.SignalRemainingAncestor(Plugins::Convert(event.content.resource.level), event.content.resource.publicId);
            }
            break;

          default:
            break;  // Unhandled event
        }
      }
    }


  public:
    Transaction(OrthancPluginDatabaseV3& that,
                IDatabaseListener& listener,
                OrthancPluginDatabaseTransactionType type) :
      that_(that),
      listener_(listener)
    {
      CheckSuccess(that.backend_.startTransaction(that.database_, &transaction_, type));
      if (transaction_ == NULL)
      {
        throw OrthancException(ErrorCode_DatabasePlugin);
      }
    }

    
    virtual ~Transaction()
    {
      OrthancPluginErrorCode code = that_.backend_.destructTransaction(transaction_);
      if (code != OrthancPluginErrorCode_Success)
      {
        // Don't throw exception in destructors
        that_.errorDictionary_.LogError(code, true);
      }
    }
    

    virtual void Rollback() ORTHANC_OVERRIDE
    {
      CheckSuccess(that_.backend_.rollback(transaction_));
      CheckNoEvent();
    }
    

    virtual void Commit(int64_t fileSizeDelta) ORTHANC_OVERRIDE
    {
      CheckSuccess(that_.backend_.commit(transaction_, fileSizeDelta));
      CheckNoEvent();
    }

    
    virtual void AddAttachment(int64_t id,
                               const FileInfo& attachment) ORTHANC_OVERRIDE
    {
      OrthancPluginAttachment tmp;
      tmp.uuid = attachment.GetUuid().c_str();
      tmp.contentType = static_cast<int32_t>(attachment.GetContentType());
      tmp.uncompressedSize = attachment.GetUncompressedSize();
      tmp.uncompressedHash = attachment.GetUncompressedMD5().c_str();
      tmp.compressionType = static_cast<int32_t>(attachment.GetCompressionType());
      tmp.compressedSize = attachment.GetCompressedSize();
      tmp.compressedHash = attachment.GetCompressedMD5().c_str();

      CheckSuccess(that_.backend_.addAttachment(transaction_, id, &tmp));
      CheckNoEvent();
    }


    virtual void ClearChanges() ORTHANC_OVERRIDE
    {
      CheckSuccess(that_.backend_.clearChanges(transaction_));
      CheckNoEvent();
    }

    
    virtual void ClearExportedResources() ORTHANC_OVERRIDE
    {
      CheckSuccess(that_.backend_.clearExportedResources(transaction_));
      CheckNoEvent();
    }

    
    virtual void DeleteAttachment(int64_t id,
                                  FileContentType attachment) ORTHANC_OVERRIDE
    {
      CheckSuccess(that_.backend_.deleteAttachment(transaction_, id, static_cast<int32_t>(attachment)));
      ProcessEvents(true);
    }

    
    virtual void DeleteMetadata(int64_t id,
                                MetadataType type) ORTHANC_OVERRIDE
    {
      CheckSuccess(that_.backend_.deleteMetadata(transaction_, id, static_cast<int32_t>(type)));
      CheckNoEvent();
    }

    
    virtual void DeleteResource(int64_t id) ORTHANC_OVERRIDE
    {
      CheckSuccess(that_.backend_.deleteResource(transaction_, id));
      ProcessEvents(false);
    }

    
    virtual void GetAllMetadata(std::map<MetadataType, std::string>& target,
                                int64_t id) ORTHANC_OVERRIDE
    {
      CheckSuccess(that_.backend_.getAllMetadata(transaction_, id));
      CheckNoEvent();

      uint32_t count;
      CheckSuccess(that_.backend_.readAnswersCount(transaction_, &count));
      
      target.clear();
      for (uint32_t i = 0; i < count; i++)
      {
        int32_t metadata;
        const char* value = NULL;
        CheckSuccess(that_.backend_.readAnswerMetadata(transaction_, &metadata, &value, i));

        if (value == NULL)
        {
          throw OrthancException(ErrorCode_DatabasePlugin);
        }
        else
        {
          target[static_cast<MetadataType>(metadata)] = value;
        }
      }
    }

    
    virtual void GetAllPublicIds(std::list<std::string>& target,
                                 ResourceType resourceType) ORTHANC_OVERRIDE
    {
      CheckSuccess(that_.backend_.getAllPublicIds(transaction_, Plugins::Convert(resourceType)));
      CheckNoEvent();

      ReadStringAnswers(target);
    }

    
    virtual void GetAllPublicIds(std::list<std::string>& target,
                                 ResourceType resourceType,
                                 size_t since,
                                 size_t limit) ORTHANC_OVERRIDE
    {
      CheckSuccess(that_.backend_.getAllPublicIdsWithLimit(
                     transaction_, Plugins::Convert(resourceType),
                     static_cast<uint64_t>(since), static_cast<uint64_t>(limit)));
      CheckNoEvent();

      ReadStringAnswers(target);
    }

    
    virtual void GetChanges(std::list<ServerIndexChange>& target /*out*/,
                            bool& done /*out*/,
                            int64_t since,
                            uint32_t maxResults) ORTHANC_OVERRIDE
    {
      uint8_t tmpDone = true;
      CheckSuccess(that_.backend_.getChanges(transaction_, &tmpDone, since, maxResults));
      CheckNoEvent();

      done = (tmpDone != 0);
      
      uint32_t count;
      CheckSuccess(that_.backend_.readAnswersCount(transaction_, &count));
      
      target.clear();
      for (uint32_t i = 0; i < count; i++)
      {
        target.push_back(ReadAnswerChange(i));
      }
    }

    
    virtual void GetChildrenInternalId(std::list<int64_t>& target,
                                       int64_t id) ORTHANC_OVERRIDE
    {
      CheckSuccess(that_.backend_.getChildrenInternalId(transaction_, id));
      CheckNoEvent();

      uint32_t count;
      CheckSuccess(that_.backend_.readAnswersCount(transaction_, &count));
      
      target.clear();
      for (uint32_t i = 0; i < count; i++)
      {
        int64_t value;
        CheckSuccess(that_.backend_.readAnswerInt64(transaction_, &value, i));
        target.push_back(value);
      }
    }

    
    virtual void GetChildrenPublicId(std::list<std::string>& target,
                                     int64_t id) ORTHANC_OVERRIDE
    {
      CheckSuccess(that_.backend_.getChildrenPublicId(transaction_, id));
      CheckNoEvent();

      ReadStringAnswers(target);
    }

    
    virtual void GetExportedResources(std::list<ExportedResource>& target /*out*/,
                                      bool& done /*out*/,
                                      int64_t since,
                                      uint32_t maxResults) ORTHANC_OVERRIDE
    {
      uint8_t tmpDone = true;
      CheckSuccess(that_.backend_.getExportedResources(transaction_, &tmpDone, since, maxResults));
      CheckNoEvent();

      done = (tmpDone != 0);
      
      uint32_t count;
      CheckSuccess(that_.backend_.readAnswersCount(transaction_, &count));
      
      target.clear();
      for (uint32_t i = 0; i < count; i++)
      {
        target.push_back(ReadAnswerExportedResource(i));
      }
    }


    virtual void GetLastChange(std::list<ServerIndexChange>& target /*out*/) ORTHANC_OVERRIDE
    {
      CheckSuccess(that_.backend_.getLastChange(transaction_));
      CheckNoEvent();

      uint32_t count;
      CheckSuccess(that_.backend_.readAnswersCount(transaction_, &count));

      target.clear();
      if (count == 1)
      {
        target.push_back(ReadAnswerChange(0));
      }
      else if (count > 1)
      {
        throw OrthancException(ErrorCode_DatabasePlugin);
      }
    }


    virtual void GetLastExportedResource(std::list<ExportedResource>& target /*out*/) ORTHANC_OVERRIDE
    {
      CheckSuccess(that_.backend_.getLastExportedResource(transaction_));
      CheckNoEvent();

      uint32_t count;
      CheckSuccess(that_.backend_.readAnswersCount(transaction_, &count));

      target.clear();
      if (count == 1)
      {
        target.push_back(ReadAnswerExportedResource(0));
      }
      else if (count > 1)
      {
        throw OrthancException(ErrorCode_DatabasePlugin);
      }
    }

    
    virtual void GetMainDicomTags(DicomMap& target,
                                  int64_t id) ORTHANC_OVERRIDE
    {
      CheckSuccess(that_.backend_.getMainDicomTags(transaction_, id));
      CheckNoEvent();

      uint32_t count;
      CheckSuccess(that_.backend_.readAnswersCount(transaction_, &count));

      target.Clear();
      for (uint32_t i = 0; i < count; i++)
      {
        uint16_t group, element;
        const char* value = NULL;
        CheckSuccess(that_.backend_.readAnswerDicomTag(transaction_, &group, &element, &value, i));

        if (value == NULL)
        {
          throw OrthancException(ErrorCode_DatabasePlugin);
        }
        else
        {
          target.SetValue(group, element, std::string(value), false);
        }
      }
    }

    
    virtual std::string GetPublicId(int64_t resourceId) ORTHANC_OVERRIDE
    {
      CheckSuccess(that_.backend_.getPublicId(transaction_, resourceId));
      CheckNoEvent();

      std::string s;
      if (ReadSingleStringAnswer(s))
      {
        return s;
      }
      else
      {
        throw OrthancException(ErrorCode_InexistentItem);
      }
    }

    
    virtual uint64_t GetResourcesCount(ResourceType resourceType) ORTHANC_OVERRIDE
    {
      uint64_t value;
      CheckSuccess(that_.backend_.getResourcesCount(transaction_, &value, Plugins::Convert(resourceType)));
      CheckNoEvent();
      return value;
    }

    
    virtual ResourceType GetResourceType(int64_t resourceId) ORTHANC_OVERRIDE
    {
      OrthancPluginResourceType type;
      CheckSuccess(that_.backend_.getResourceType(transaction_, &type, resourceId));
      CheckNoEvent();
      return Plugins::Convert(type);
    }

    
    virtual uint64_t GetTotalCompressedSize() ORTHANC_OVERRIDE
    {
      uint64_t s;
      CheckSuccess(that_.backend_.getTotalCompressedSize(transaction_, &s));
      CheckNoEvent();
      return s;
    }

    
    virtual uint64_t GetTotalUncompressedSize() ORTHANC_OVERRIDE
    {
      uint64_t s;
      CheckSuccess(that_.backend_.getTotalUncompressedSize(transaction_, &s));
      CheckNoEvent();
      return s;
    }

    
    virtual bool IsExistingResource(int64_t internalId) ORTHANC_OVERRIDE
    {
      uint8_t b;
      CheckSuccess(that_.backend_.isExistingResource(transaction_, &b, internalId));
      CheckNoEvent();
      return (b != 0);
    }

    
    virtual bool IsProtectedPatient(int64_t internalId) ORTHANC_OVERRIDE
    {
      uint8_t b;
      CheckSuccess(that_.backend_.isProtectedPatient(transaction_, &b, internalId));
      CheckNoEvent();
      return (b != 0);
    }

    
    virtual void ListAvailableAttachments(std::set<FileContentType>& target,
                                          int64_t id) ORTHANC_OVERRIDE
    {
      CheckSuccess(that_.backend_.listAvailableAttachments(transaction_, id));
      CheckNoEvent();

      uint32_t count;
      CheckSuccess(that_.backend_.readAnswersCount(transaction_, &count));
      
      target.clear();
      for (uint32_t i = 0; i < count; i++)
      {
        int32_t value;
        CheckSuccess(that_.backend_.readAnswerInt32(transaction_, &value, i));
        target.insert(static_cast<FileContentType>(value));
      }
    }

    
    virtual void LogChange(int64_t internalId,
                           const ServerIndexChange& change) ORTHANC_OVERRIDE
    {
      CheckSuccess(that_.backend_.logChange(transaction_, static_cast<int32_t>(change.GetChangeType()),
                                            internalId, Plugins::Convert(change.GetResourceType()),
                                            change.GetDate().c_str()));
      CheckNoEvent();
    }

    
    virtual void LogExportedResource(const ExportedResource& resource) ORTHANC_OVERRIDE
    {
      CheckSuccess(that_.backend_.logExportedResource(transaction_, Plugins::Convert(resource.GetResourceType()),
                                                      resource.GetPublicId().c_str(),
                                                      resource.GetModality().c_str(),
                                                      resource.GetDate().c_str(),
                                                      resource.GetPatientId().c_str(),
                                                      resource.GetStudyInstanceUid().c_str(),
                                                      resource.GetSeriesInstanceUid().c_str(),
                                                      resource.GetSopInstanceUid().c_str()));
      CheckNoEvent();
    }

    
    virtual bool LookupAttachment(FileInfo& attachment,
                                  int64_t id,
                                  FileContentType contentType) ORTHANC_OVERRIDE
    {
      CheckSuccess(that_.backend_.lookupAttachment(transaction_, id, static_cast<int32_t>(contentType)));
      CheckNoEvent();

      uint32_t count;
      CheckSuccess(that_.backend_.readAnswersCount(transaction_, &count));

      if (count == 0)
      {
        return false;
      }
      else if (count == 1)
      {
        OrthancPluginAttachment tmp;
        CheckSuccess(that_.backend_.readAnswerAttachment(transaction_, &tmp, 0));
        attachment = Convert(tmp);
        return true;
      }
      else
      {
        throw OrthancException(ErrorCode_DatabasePlugin);
      }
    }

    
    virtual bool LookupGlobalProperty(std::string& target,
                                      GlobalProperty property,
                                      bool shared) ORTHANC_OVERRIDE
    {
      const char* id = (shared ? "" : that_.serverIdentifier_.c_str());
      
      CheckSuccess(that_.backend_.lookupGlobalProperty(transaction_, id, static_cast<int32_t>(property)));
      CheckNoEvent();
      return ReadSingleStringAnswer(target);      
    }

    
    virtual bool LookupMetadata(std::string& target,
                                int64_t& revision,
                                int64_t id,
                                MetadataType type) ORTHANC_OVERRIDE
    {
      CheckSuccess(that_.backend_.lookupMetadata(transaction_, &revision, id, static_cast<int32_t>(type)));
      CheckNoEvent();
      return ReadSingleStringAnswer(target);      
    }

    
    virtual bool LookupParent(int64_t& parentId,
                              int64_t resourceId) ORTHANC_OVERRIDE
    {
      uint8_t existing;
      CheckSuccess(that_.backend_.lookupParent(transaction_, &existing, &parentId, resourceId));
      CheckNoEvent();
      return (existing != 0);
    }

    
    virtual bool LookupResource(int64_t& id,
                                ResourceType& type,
                                const std::string& publicId) ORTHANC_OVERRIDE
    {
      uint8_t existing;
      OrthancPluginResourceType t;
      CheckSuccess(that_.backend_.lookupResource(transaction_, &existing, &id, &t, publicId.c_str()));
      CheckNoEvent();

      if (existing == 0)
      {
        return false;
      }
      else
      {
        type = Plugins::Convert(t);
        return true;
      }
    }

    
    virtual bool SelectPatientToRecycle(int64_t& internalId) ORTHANC_OVERRIDE
    {
      uint8_t available;      
      CheckSuccess(that_.backend_.selectPatientToRecycle(transaction_, &available, &internalId));
      CheckNoEvent();
      return (available != 0);
    }

    
    virtual bool SelectPatientToRecycle(int64_t& internalId,
                                        int64_t patientIdToAvoid) ORTHANC_OVERRIDE
    {
      uint8_t available;      
      CheckSuccess(that_.backend_.selectPatientToRecycle2(transaction_, &available, &internalId, patientIdToAvoid));
      CheckNoEvent();
      return (available != 0);
    }

    
    virtual void SetGlobalProperty(GlobalProperty property,
                                   bool shared,
                                   const std::string& value) ORTHANC_OVERRIDE
    {
      const char* id = (shared ? "" : that_.serverIdentifier_.c_str());
      
      CheckSuccess(that_.backend_.setGlobalProperty(transaction_, id, static_cast<int32_t>(property), value.c_str()));
      CheckNoEvent();
    }

    
    virtual void ClearMainDicomTags(int64_t id) ORTHANC_OVERRIDE
    {
      CheckSuccess(that_.backend_.clearMainDicomTags(transaction_, id));
      CheckNoEvent();
    }

    
    virtual void SetMetadata(int64_t id,
                             MetadataType type,
                             const std::string& value,
                             int64_t revision) ORTHANC_OVERRIDE
    {
      CheckSuccess(that_.backend_.setMetadata(transaction_, id, static_cast<int32_t>(type), value.c_str(), revision));
      CheckNoEvent();
    }

    
    virtual void SetProtectedPatient(int64_t internalId, 
                                     bool isProtected) ORTHANC_OVERRIDE
    {
      CheckSuccess(that_.backend_.setProtectedPatient(transaction_, internalId, (isProtected ? 1 : 0)));
      CheckNoEvent();
    }


    virtual bool IsDiskSizeAbove(uint64_t threshold) ORTHANC_OVERRIDE
    {
      uint8_t tmp;
      CheckSuccess(that_.backend_.isDiskSizeAbove(transaction_, &tmp, threshold));
      CheckNoEvent();
      return (tmp != 0);
    }

    
    virtual void ApplyLookupResources(std::list<std::string>& resourcesId,
                                      std::list<std::string>* instancesId, // Can be NULL if not needed
                                      const std::vector<DatabaseConstraint>& lookup,
                                      ResourceType queryLevel,
                                      size_t limit) ORTHANC_OVERRIDE
    {
      std::vector<OrthancPluginDatabaseConstraint> constraints;
      std::vector< std::vector<const char*> > constraintsValues;

      constraints.resize(lookup.size());
      constraintsValues.resize(lookup.size());

      for (size_t i = 0; i < lookup.size(); i++)
      {
        lookup[i].EncodeForPlugins(constraints[i], constraintsValues[i]);
      }

      CheckSuccess(that_.backend_.lookupResources(transaction_, lookup.size(),
                                                  (lookup.empty() ? NULL : &constraints[0]),
                                                  Plugins::Convert(queryLevel),
                                                  limit, (instancesId == NULL ? 0 : 1)));
      CheckNoEvent();

      uint32_t count;
      CheckSuccess(that_.backend_.readAnswersCount(transaction_, &count));
      
      resourcesId.clear();

      if (instancesId != NULL)
      {
        instancesId->clear();
      }
      
      for (uint32_t i = 0; i < count; i++)
      {
        OrthancPluginMatchingResource resource;
        CheckSuccess(that_.backend_.readAnswerMatchingResource(transaction_, &resource, i));

        if (resource.resourceId == NULL)
        {
          throw OrthancException(ErrorCode_DatabasePlugin);
        }
        
        resourcesId.push_back(resource.resourceId);

        if (instancesId != NULL)
        {
          if (resource.someInstanceId == NULL)
          {
            throw OrthancException(ErrorCode_DatabasePlugin);
          }
          else
          {
            instancesId->push_back(resource.someInstanceId);
          }
        }
      }
    }

    
    virtual bool CreateInstance(CreateInstanceResult& result, /* out */
                                int64_t& instanceId,          /* out */
                                const std::string& patient,
                                const std::string& study,
                                const std::string& series,
                                const std::string& instance) ORTHANC_OVERRIDE
    {
      OrthancPluginCreateInstanceResult output;
      memset(&output, 0, sizeof(output));

      CheckSuccess(that_.backend_.createInstance(transaction_, &output, patient.c_str(),
                                                 study.c_str(), series.c_str(), instance.c_str()));
      CheckNoEvent();

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

    
    virtual void SetResourcesContent(const ResourcesContent& content) ORTHANC_OVERRIDE
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
       
      CheckSuccess(that_.backend_.setResourcesContent(transaction_,
                                                      identifierTags.size(),
                                                      (identifierTags.empty() ? NULL : &identifierTags[0]),
                                                      mainDicomTags.size(),
                                                      (mainDicomTags.empty() ? NULL : &mainDicomTags[0]),
                                                      metadata.size(),
                                                      (metadata.empty() ? NULL : &metadata[0])));
      CheckNoEvent();
    }

    
    virtual void GetChildrenMetadata(std::list<std::string>& target,
                                     int64_t resourceId,
                                     MetadataType metadata) ORTHANC_OVERRIDE
    {
      CheckSuccess(that_.backend_.getChildrenMetadata(transaction_, resourceId, static_cast<int32_t>(metadata)));
      CheckNoEvent();
      ReadStringAnswers(target);
    }

    
    virtual int64_t GetLastChangeIndex() ORTHANC_OVERRIDE
    {
      int64_t tmp;
      CheckSuccess(that_.backend_.getLastChangeIndex(transaction_, &tmp));
      CheckNoEvent();
      return tmp;
    }

    
    virtual bool LookupResourceAndParent(int64_t& id,
                                         ResourceType& type,
                                         std::string& parentPublicId,
                                         const std::string& publicId) ORTHANC_OVERRIDE
    {
      uint8_t isExisting;
      OrthancPluginResourceType tmpType;
      CheckSuccess(that_.backend_.lookupResourceAndParent(transaction_, &isExisting, &id, &tmpType, publicId.c_str()));
      CheckNoEvent();

      if (isExisting)
      {
        type = Plugins::Convert(tmpType);
        
        uint32_t count;
        CheckSuccess(that_.backend_.readAnswersCount(transaction_, &count));

        if (count > 1)
        {
          throw OrthancException(ErrorCode_DatabasePlugin);
        }

        switch (type)
        {
          case ResourceType_Patient:
            // A patient has no parent
            if (count == 1)
            {
              throw OrthancException(ErrorCode_DatabasePlugin);
            }
            break;

          case ResourceType_Study:
          case ResourceType_Series:
          case ResourceType_Instance:
            if (count == 0)
            {
              throw OrthancException(ErrorCode_DatabasePlugin);
            }
            else
            {
              const char* value = NULL;
              CheckSuccess(that_.backend_.readAnswerString(transaction_, &value, 0));
              if (value == NULL)
              {
                throw OrthancException(ErrorCode_DatabasePlugin);
              }
              else
              {
                parentPublicId.assign(value);
              }              
            }
            break;

          default:
            throw OrthancException(ErrorCode_DatabasePlugin);
        }
        
        return true;
      }
      else
      {
        return false;
      }
    }
  };

  
  void OrthancPluginDatabaseV3::CheckSuccess(OrthancPluginErrorCode code)
  {
    if (code != OrthancPluginErrorCode_Success)
    {
      errorDictionary_.LogError(code, true);
      throw OrthancException(static_cast<ErrorCode>(code));
    }
  }


  OrthancPluginDatabaseV3::OrthancPluginDatabaseV3(SharedLibrary& library,
                                                   PluginsErrorDictionary&  errorDictionary,
                                                   const OrthancPluginDatabaseBackendV3* backend,
                                                   size_t backendSize,
                                                   void* database,
                                                   const std::string& serverIdentifier) :
    library_(library),
    errorDictionary_(errorDictionary),
    database_(database),
    serverIdentifier_(serverIdentifier)
  {
    CLOG(INFO, PLUGINS) << "Identifier of this Orthanc server for the global properties "
                        << "of the custom database: \"" << serverIdentifier << "\"";
    
    if (backendSize >= sizeof(backend_))
    {
      memcpy(&backend_, backend, sizeof(backend_));
    }
    else
    {
      // Not all the primitives are implemented by the plugin
      memset(&backend_, 0, sizeof(backend_));
      memcpy(&backend_, backend, backendSize);
    }

    // Sanity checks
    CHECK_FUNCTION_EXISTS(backend_, readAnswersCount);
    CHECK_FUNCTION_EXISTS(backend_, readAnswerAttachment);
    CHECK_FUNCTION_EXISTS(backend_, readAnswerChange);
    CHECK_FUNCTION_EXISTS(backend_, readAnswerDicomTag);
    CHECK_FUNCTION_EXISTS(backend_, readAnswerExportedResource);
    CHECK_FUNCTION_EXISTS(backend_, readAnswerInt32);
    CHECK_FUNCTION_EXISTS(backend_, readAnswerInt64);
    CHECK_FUNCTION_EXISTS(backend_, readAnswerMatchingResource);
    CHECK_FUNCTION_EXISTS(backend_, readAnswerMetadata);
    CHECK_FUNCTION_EXISTS(backend_, readAnswerString);
    
    CHECK_FUNCTION_EXISTS(backend_, readEventsCount);
    CHECK_FUNCTION_EXISTS(backend_, readEvent);

    CHECK_FUNCTION_EXISTS(backend_, open);
    CHECK_FUNCTION_EXISTS(backend_, close);
    CHECK_FUNCTION_EXISTS(backend_, destructDatabase);
    CHECK_FUNCTION_EXISTS(backend_, getDatabaseVersion);
    CHECK_FUNCTION_EXISTS(backend_, upgradeDatabase);
    CHECK_FUNCTION_EXISTS(backend_, startTransaction);
    CHECK_FUNCTION_EXISTS(backend_, destructTransaction);

    CHECK_FUNCTION_EXISTS(backend_, rollback);
    CHECK_FUNCTION_EXISTS(backend_, commit);
    
    CHECK_FUNCTION_EXISTS(backend_, addAttachment);
    CHECK_FUNCTION_EXISTS(backend_, clearChanges);
    CHECK_FUNCTION_EXISTS(backend_, clearExportedResources);
    CHECK_FUNCTION_EXISTS(backend_, clearMainDicomTags);
    CHECK_FUNCTION_EXISTS(backend_, createInstance);
    CHECK_FUNCTION_EXISTS(backend_, deleteAttachment);
    CHECK_FUNCTION_EXISTS(backend_, deleteMetadata);
    CHECK_FUNCTION_EXISTS(backend_, deleteResource);
    CHECK_FUNCTION_EXISTS(backend_, getAllMetadata);
    CHECK_FUNCTION_EXISTS(backend_, getAllPublicIds);
    CHECK_FUNCTION_EXISTS(backend_, getAllPublicIdsWithLimit);
    CHECK_FUNCTION_EXISTS(backend_, getChanges);
    CHECK_FUNCTION_EXISTS(backend_, getChildrenInternalId);
    CHECK_FUNCTION_EXISTS(backend_, getChildrenMetadata);
    CHECK_FUNCTION_EXISTS(backend_, getChildrenPublicId);
    CHECK_FUNCTION_EXISTS(backend_, getExportedResources);
    CHECK_FUNCTION_EXISTS(backend_, getLastChange);
    CHECK_FUNCTION_EXISTS(backend_, getLastChangeIndex);
    CHECK_FUNCTION_EXISTS(backend_, getLastExportedResource);
    CHECK_FUNCTION_EXISTS(backend_, getMainDicomTags);
    CHECK_FUNCTION_EXISTS(backend_, getPublicId);
    CHECK_FUNCTION_EXISTS(backend_, getResourcesCount);
    CHECK_FUNCTION_EXISTS(backend_, getResourceType);
    CHECK_FUNCTION_EXISTS(backend_, getTotalCompressedSize);
    CHECK_FUNCTION_EXISTS(backend_, getTotalUncompressedSize);
    CHECK_FUNCTION_EXISTS(backend_, isDiskSizeAbove);
    CHECK_FUNCTION_EXISTS(backend_, isExistingResource);
    CHECK_FUNCTION_EXISTS(backend_, isProtectedPatient);
    CHECK_FUNCTION_EXISTS(backend_, listAvailableAttachments);
    CHECK_FUNCTION_EXISTS(backend_, logChange);
    CHECK_FUNCTION_EXISTS(backend_, logExportedResource);
    CHECK_FUNCTION_EXISTS(backend_, lookupAttachment);
    CHECK_FUNCTION_EXISTS(backend_, lookupGlobalProperty);
    CHECK_FUNCTION_EXISTS(backend_, lookupMetadata);
    CHECK_FUNCTION_EXISTS(backend_, lookupParent);
    CHECK_FUNCTION_EXISTS(backend_, lookupResource);
    CHECK_FUNCTION_EXISTS(backend_, lookupResources);
    CHECK_FUNCTION_EXISTS(backend_, lookupResourceAndParent);
    CHECK_FUNCTION_EXISTS(backend_, selectPatientToRecycle);
    CHECK_FUNCTION_EXISTS(backend_, selectPatientToRecycle2);
    CHECK_FUNCTION_EXISTS(backend_, setGlobalProperty);
    CHECK_FUNCTION_EXISTS(backend_, setMetadata);
    CHECK_FUNCTION_EXISTS(backend_, setProtectedPatient);
    CHECK_FUNCTION_EXISTS(backend_, setResourcesContent);
  }

  
  OrthancPluginDatabaseV3::~OrthancPluginDatabaseV3()
  {
    if (database_ != NULL)
    {
      OrthancPluginErrorCode code = backend_.destructDatabase(database_);
      if (code != OrthancPluginErrorCode_Success)
      {
        // Don't throw exception in destructors
        errorDictionary_.LogError(code, true);
      }
    }
  }

  
  void OrthancPluginDatabaseV3::Open()
  {
    CheckSuccess(backend_.open(database_));
  }


  void OrthancPluginDatabaseV3::Close()
  {
    CheckSuccess(backend_.close(database_));
  }
  

  IDatabaseWrapper::ITransaction* OrthancPluginDatabaseV3::StartTransaction(TransactionType type,
                                                                            IDatabaseListener& listener)
  {
    switch (type)
    {
      case TransactionType_ReadOnly:
        return new Transaction(*this, listener, OrthancPluginDatabaseTransactionType_ReadOnly);

      case TransactionType_ReadWrite:
        return new Transaction(*this, listener, OrthancPluginDatabaseTransactionType_ReadWrite);

      default:
        throw OrthancException(ErrorCode_InternalError);
    }
  }

  
  unsigned int OrthancPluginDatabaseV3::GetDatabaseVersion()
  {
    uint32_t version = 0;
    CheckSuccess(backend_.getDatabaseVersion(database_, &version));
    return version;
  }

  
  void OrthancPluginDatabaseV3::Upgrade(unsigned int targetVersion,
                                        IStorageArea& storageArea)
  {
    VoidDatabaseListener listener;
    
    if (backend_.upgradeDatabase != NULL)
    {
      Transaction transaction(*this, listener, OrthancPluginDatabaseTransactionType_ReadWrite);

      OrthancPluginErrorCode code = backend_.upgradeDatabase(
        database_, reinterpret_cast<OrthancPluginStorageArea*>(&storageArea),
        static_cast<uint32_t>(targetVersion));

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
}
