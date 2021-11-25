/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
 * Copyright (C) 2021-2021 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
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
#include "OrthancPluginDatabase.h"

#if ORTHANC_ENABLE_PLUGINS != 1
#  error The plugin support is disabled
#endif


#include "../../../OrthancFramework/Sources/Logging.h"
#include "../../../OrthancFramework/Sources/OrthancException.h"
#include "../../Sources/Database/Compatibility/ICreateInstance.h"
#include "../../Sources/Database/Compatibility/IGetChildrenMetadata.h"
#include "../../Sources/Database/Compatibility/ILookupResourceAndParent.h"
#include "../../Sources/Database/Compatibility/ILookupResources.h"
#include "../../Sources/Database/Compatibility/ISetResourcesContent.h"
#include "../../Sources/Database/VoidDatabaseListener.h"
#include "PluginsEnumerations.h"

#include <cassert>

namespace Orthanc
{
  class OrthancPluginDatabase::Transaction :
    public IDatabaseWrapper::ITransaction,
    public Compatibility::ICreateInstance,
    public Compatibility::IGetChildrenMetadata,
    public Compatibility::ILookupResources,
    public Compatibility::ILookupResourceAndParent,
    public Compatibility::ISetResourcesContent
  {
  private:
    typedef std::pair<int64_t, ResourceType>     AnswerResource;
    typedef std::map<MetadataType, std::string>  AnswerMetadata;

    OrthancPluginDatabase&               that_;
    boost::recursive_mutex::scoped_lock  lock_;
    IDatabaseListener&                   listener_;
    _OrthancPluginDatabaseAnswerType     type_;

    std::list<std::string>         answerStrings_;
    std::list<int32_t>             answerInt32_;
    std::list<int64_t>             answerInt64_;
    std::list<AnswerResource>      answerResources_;
    std::list<FileInfo>            answerAttachments_;

    DicomMap*                      answerDicomMap_;
    std::list<ServerIndexChange>*  answerChanges_;
    std::list<ExportedResource>*   answerExportedResources_;
    bool*                          answerDone_;
    bool                           answerDoneIgnored_;
    std::list<std::string>*        answerMatchingResources_;
    std::list<std::string>*        answerMatchingInstances_;
    AnswerMetadata*                answerMetadata_;

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


    void ResetAnswers()
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


    void ForwardAnswers(std::list<int64_t>& target)
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


    void ForwardAnswers(std::list<std::string>& target)
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


    bool ForwardSingleAnswer(std::string& target)
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


    bool ForwardSingleAnswer(int64_t& target)
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


    void ProcessEvent(const _OrthancPluginDatabaseAnswer& answer)
    {
      switch (answer.type)
      {
        case _OrthancPluginDatabaseAnswerType_DeletedAttachment:
        {
          const OrthancPluginAttachment& attachment = 
            *reinterpret_cast<const OrthancPluginAttachment*>(answer.valueGeneric);
          listener_.SignalAttachmentDeleted(Convert(attachment));
          break;
        }
        
        case _OrthancPluginDatabaseAnswerType_RemainingAncestor:
        {
          ResourceType type = Plugins::Convert(static_cast<OrthancPluginResourceType>(answer.valueInt32));
          listener_.SignalRemainingAncestor(type, answer.valueString);
          break;
        }
      
        case _OrthancPluginDatabaseAnswerType_DeletedResource:
        {
          ResourceType type = Plugins::Convert(static_cast<OrthancPluginResourceType>(answer.valueInt32));
          listener_.SignalResourceDeleted(type, answer.valueString);
          break;
        }

        default:
          throw OrthancException(ErrorCode_DatabasePlugin);
      }
    }


  public:
    explicit Transaction(OrthancPluginDatabase& that,
                         IDatabaseListener& listener) :
      that_(that),
      lock_(that.mutex_),
      listener_(listener),
      type_(_OrthancPluginDatabaseAnswerType_None),
      answerDoneIgnored_(false)
    {
      if (that_.activeTransaction_ != NULL)
      {
        throw OrthancException(ErrorCode_InternalError);
      }
      
      that_.activeTransaction_ = this;

      ResetAnswers();
    }

    virtual ~Transaction()
    {
      assert(that_.activeTransaction_ != NULL);    
      that_.activeTransaction_ = NULL;
    }

    IDatabaseListener& GetDatabaseListener() const
    {
      return listener_;
    }

    void Begin()
    {
      CheckSuccess(that_.backend_.startTransaction(that_.payload_));
    }

    virtual void Rollback() ORTHANC_OVERRIDE
    {
      CheckSuccess(that_.backend_.rollbackTransaction(that_.payload_));
    }

    virtual void Commit(int64_t diskSizeDelta) ORTHANC_OVERRIDE
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

        assert(newDiskSize == GetTotalCompressedSize());

        CheckSuccess(that_.backend_.commitTransaction(that_.payload_));

        // The transaction has succeeded, we can commit the new disk size
        that_.currentDiskSize_ = newDiskSize;
      }
    }


    void AnswerReceived(const _OrthancPluginDatabaseAnswer& answer)
    {
      if (answer.type == _OrthancPluginDatabaseAnswerType_None)
      {
        throw OrthancException(ErrorCode_DatabasePlugin);
      }

      if (answer.type == _OrthancPluginDatabaseAnswerType_DeletedAttachment ||
          answer.type == _OrthancPluginDatabaseAnswerType_DeletedResource ||
          answer.type == _OrthancPluginDatabaseAnswerType_RemainingAncestor)
      {
        ProcessEvent(answer);
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
    

    // From the "ILookupResources" interface
    virtual void LookupIdentifier(std::list<int64_t>& result,
                                  ResourceType level,
                                  const DicomTag& tag,
                                  Compatibility::IdentifierConstraintType type,
                                  const std::string& value) ORTHANC_OVERRIDE
    {
      if (that_.extensions_.lookupIdentifier3 == NULL)
      {
        throw OrthancException(ErrorCode_DatabasePlugin,
                               "The database plugin does not implement the mandatory LookupIdentifier3() extension");
      }

      OrthancPluginDicomTag tmp;
      tmp.group = tag.GetGroup();
      tmp.element = tag.GetElement();
      tmp.value = value.c_str();

      ResetAnswers();
      CheckSuccess(that_.extensions_.lookupIdentifier3(that_.GetContext(), that_.payload_, Plugins::Convert(level),
                                                       &tmp, Compatibility::Convert(type)));
      ForwardAnswers(result);
    }

    
    virtual void ApplyLookupResources(std::list<std::string>& resourcesId,
                                      std::list<std::string>* instancesId,
                                      const std::vector<DatabaseConstraint>& lookup,
                                      ResourceType queryLevel,
                                      size_t limit) ORTHANC_OVERRIDE
    {
      if (that_.extensions_.lookupResources == NULL)
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
      
        CheckSuccess(that_.extensions_.lookupResources(that_.GetContext(), that_.payload_, lookup.size(),
                                                       (lookup.empty() ? NULL : &constraints[0]),
                                                       Plugins::Convert(queryLevel),
                                                       limit, (instancesId == NULL ? 0 : 1)));
      }
    }


    virtual bool CreateInstance(IDatabaseWrapper::CreateInstanceResult& result,
                                int64_t& instanceId,
                                const std::string& patient,
                                const std::string& study,
                                const std::string& series,
                                const std::string& instance) ORTHANC_OVERRIDE
    {
      if (that_.extensions_.createInstance == NULL)
      {
        // Fallback to compatibility mode
        return ICreateInstance::Apply
          (*this, result, instanceId, patient, study, series, instance);
      }
      else
      {
        OrthancPluginCreateInstanceResult output;
        memset(&output, 0, sizeof(output));

        CheckSuccess(that_.extensions_.createInstance(&output, that_.payload_, patient.c_str(),
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
    

    virtual void AddAttachment(int64_t id,
                               const FileInfo& attachment,
                               int64_t revision) ORTHANC_OVERRIDE
    {
      // "revision" is not used, as it was added in Orthanc 1.9.2
      OrthancPluginAttachment tmp;
      tmp.uuid = attachment.GetUuid().c_str();
      tmp.contentType = static_cast<int32_t>(attachment.GetContentType());
      tmp.uncompressedSize = attachment.GetUncompressedSize();
      tmp.uncompressedHash = attachment.GetUncompressedMD5().c_str();
      tmp.compressionType = static_cast<int32_t>(attachment.GetCompressionType());
      tmp.compressedSize = attachment.GetCompressedSize();
      tmp.compressedHash = attachment.GetCompressedMD5().c_str();

      CheckSuccess(that_.backend_.addAttachment(that_.payload_, id, &tmp));
    }


    // From the "ICreateInstance" interface
    virtual void AttachChild(int64_t parent,
                             int64_t child) ORTHANC_OVERRIDE
    {
      CheckSuccess(that_.backend_.attachChild(that_.payload_, parent, child));
    }


    virtual void ClearChanges() ORTHANC_OVERRIDE
    {
      CheckSuccess(that_.backend_.clearChanges(that_.payload_));
    }


    virtual void ClearExportedResources() ORTHANC_OVERRIDE
    {
      CheckSuccess(that_.backend_.clearExportedResources(that_.payload_));
    }


    virtual void ClearMainDicomTags(int64_t id) ORTHANC_OVERRIDE
    {
      if (that_.extensions_.clearMainDicomTags == NULL)
      {
        throw OrthancException(ErrorCode_DatabasePlugin,
                               "Your custom index plugin does not implement the mandatory ClearMainDicomTags() extension");
      }

      CheckSuccess(that_.extensions_.clearMainDicomTags(that_.payload_, id));
    }


    // From the "ICreateInstance" interface
    virtual int64_t CreateResource(const std::string& publicId,
                                   ResourceType type) ORTHANC_OVERRIDE
    {
      int64_t id;
      CheckSuccess(that_.backend_.createResource(&id, that_.payload_, publicId.c_str(), Plugins::Convert(type)));
      return id;
    }


    virtual void DeleteAttachment(int64_t id,
                                  FileContentType attachment) ORTHANC_OVERRIDE
    {
      CheckSuccess(that_.backend_.deleteAttachment(that_.payload_, id, static_cast<int32_t>(attachment)));
    }


    virtual void DeleteMetadata(int64_t id,
                                MetadataType type) ORTHANC_OVERRIDE
    {
      CheckSuccess(that_.backend_.deleteMetadata(that_.payload_, id, static_cast<int32_t>(type)));
    }


    virtual void DeleteResource(int64_t id) ORTHANC_OVERRIDE
    {
      CheckSuccess(that_.backend_.deleteResource(that_.payload_, id));
    }


    // From the "ILookupResources" interface
    void GetAllInternalIds(std::list<int64_t>& target,
                           ResourceType resourceType) ORTHANC_OVERRIDE
    {
      if (that_.extensions_.getAllInternalIds == NULL)
      {
        throw OrthancException(ErrorCode_DatabasePlugin,
                               "The database plugin does not implement the mandatory GetAllInternalIds() extension");
      }

      ResetAnswers();
      CheckSuccess(that_.extensions_.getAllInternalIds(that_.GetContext(), that_.payload_, Plugins::Convert(resourceType)));
      ForwardAnswers(target);
    }



    virtual void GetAllMetadata(std::map<MetadataType, std::string>& target,
                                int64_t id) ORTHANC_OVERRIDE
    {
      if (that_.extensions_.getAllMetadata == NULL)
      {
        // Fallback implementation if extension is missing
        target.clear();

        ResetAnswers();
        CheckSuccess(that_.backend_.listAvailableMetadata(that_.GetContext(), that_.payload_, id));

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
            int64_t revision;  // Ignored
            if (LookupMetadata(value, revision, id, type))
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
      
        CheckSuccess(that_.extensions_.getAllMetadata(that_.GetContext(), that_.payload_, id));

        if (type_ != _OrthancPluginDatabaseAnswerType_None &&
            type_ != _OrthancPluginDatabaseAnswerType_Metadata)
        {
          throw OrthancException(ErrorCode_DatabasePlugin);
        }
      }
    }


    virtual void GetAllPublicIds(std::list<std::string>& target,
                                 ResourceType resourceType) ORTHANC_OVERRIDE
    {
      ResetAnswers();
      CheckSuccess(that_.backend_.getAllPublicIds(that_.GetContext(), that_.payload_, Plugins::Convert(resourceType)));
      ForwardAnswers(target);
    }


    virtual void GetAllPublicIds(std::list<std::string>& target,
                                 ResourceType resourceType,
                                 size_t since,
                                 size_t limit) ORTHANC_OVERRIDE
    {
      if (that_.extensions_.getAllPublicIdsWithLimit != NULL)
      {
        // This extension is available since Orthanc 0.9.4
        ResetAnswers();
        CheckSuccess(that_.extensions_.getAllPublicIdsWithLimit
                     (that_.GetContext(), that_.payload_, Plugins::Convert(resourceType), since, limit));
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


    virtual void GetChanges(std::list<ServerIndexChange>& target /*out*/,
                            bool& done /*out*/,
                            int64_t since,
                            uint32_t maxResults) ORTHANC_OVERRIDE
    {
      ResetAnswers();
      answerChanges_ = &target;
      answerDone_ = &done;
      done = false;

      CheckSuccess(that_.backend_.getChanges(that_.GetContext(), that_.payload_, since, maxResults));
    }


    virtual void GetChildrenInternalId(std::list<int64_t>& target,
                                       int64_t id) ORTHANC_OVERRIDE
    {
      ResetAnswers();
      CheckSuccess(that_.backend_.getChildrenInternalId(that_.GetContext(), that_.payload_, id));
      ForwardAnswers(target);
    }


    virtual void GetChildrenMetadata(std::list<std::string>& target,
                                     int64_t resourceId,
                                     MetadataType metadata) ORTHANC_OVERRIDE
    {
      if (that_.extensions_.getChildrenMetadata == NULL)
      {
        IGetChildrenMetadata::Apply(*this, target, resourceId, metadata);
      }
      else
      {
        ResetAnswers();
        CheckSuccess(that_.extensions_.getChildrenMetadata
                     (that_.GetContext(), that_.payload_, resourceId, static_cast<int32_t>(metadata)));
        ForwardAnswers(target);
      }
    }


    virtual void GetChildrenPublicId(std::list<std::string>& target,
                                     int64_t id) ORTHANC_OVERRIDE
    {
      ResetAnswers();
      CheckSuccess(that_.backend_.getChildrenPublicId(that_.GetContext(), that_.payload_, id));
      ForwardAnswers(target);
    }


    virtual void GetExportedResources(std::list<ExportedResource>& target /*out*/,
                                      bool& done /*out*/,
                                      int64_t since,
                                      uint32_t maxResults) ORTHANC_OVERRIDE
    {
      ResetAnswers();
      answerExportedResources_ = &target;
      answerDone_ = &done;
      done = false;

      CheckSuccess(that_.backend_.getExportedResources(that_.GetContext(), that_.payload_, since, maxResults));
    }


    virtual void GetLastChange(std::list<ServerIndexChange>& target /*out*/) ORTHANC_OVERRIDE
    {
      answerDoneIgnored_ = false;

      ResetAnswers();
      answerChanges_ = &target;
      answerDone_ = &answerDoneIgnored_;

      CheckSuccess(that_.backend_.getLastChange(that_.GetContext(), that_.payload_));
    }


    int64_t GetLastChangeIndex() ORTHANC_OVERRIDE
    {
      if (that_.extensions_.getLastChangeIndex == NULL)
      {
        // This was the default behavior in Orthanc <= 1.5.1
        // https://groups.google.com/d/msg/orthanc-users/QhzB6vxYeZ0/YxabgqpfBAAJ
        return 0;
      }
      else
      {
        int64_t result = 0;
        CheckSuccess(that_.extensions_.getLastChangeIndex(&result, that_.payload_));
        return result;
      }
    }

  
    virtual void GetLastExportedResource(std::list<ExportedResource>& target /*out*/) ORTHANC_OVERRIDE
    {
      answerDoneIgnored_ = false;

      ResetAnswers();
      answerExportedResources_ = &target;
      answerDone_ = &answerDoneIgnored_;

      CheckSuccess(that_.backend_.getLastExportedResource(that_.GetContext(), that_.payload_));
    }


    virtual void GetMainDicomTags(DicomMap& map,
                                  int64_t id) ORTHANC_OVERRIDE
    {
      ResetAnswers();
      answerDicomMap_ = &map;

      CheckSuccess(that_.backend_.getMainDicomTags(that_.GetContext(), that_.payload_, id));
    }


    virtual std::string GetPublicId(int64_t resourceId) ORTHANC_OVERRIDE
    {
      ResetAnswers();
      std::string s;

      CheckSuccess(that_.backend_.getPublicId(that_.GetContext(), that_.payload_, resourceId));

      if (!ForwardSingleAnswer(s))
      {
        throw OrthancException(ErrorCode_DatabasePlugin);
      }

      return s;
    }


    virtual uint64_t GetResourcesCount(ResourceType resourceType) ORTHANC_OVERRIDE
    {
      uint64_t count;
      CheckSuccess(that_.backend_.getResourceCount(&count, that_.payload_, Plugins::Convert(resourceType)));
      return count;
    }


    virtual ResourceType GetResourceType(int64_t resourceId) ORTHANC_OVERRIDE
    {
      OrthancPluginResourceType type;
      CheckSuccess(that_.backend_.getResourceType(&type, that_.payload_, resourceId));
      return Plugins::Convert(type);
    }


    virtual uint64_t GetTotalCompressedSize() ORTHANC_OVERRIDE
    {
      uint64_t size;
      CheckSuccess(that_.backend_.getTotalCompressedSize(&size, that_.payload_));
      return size;
    }

    
    virtual uint64_t GetTotalUncompressedSize() ORTHANC_OVERRIDE
    {
      uint64_t size;
      CheckSuccess(that_.backend_.getTotalUncompressedSize(&size, that_.payload_));
      return size;
    }
    

    virtual bool IsDiskSizeAbove(uint64_t threshold) ORTHANC_OVERRIDE
    {
      if (that_.fastGetTotalSize_)
      {
        return GetTotalCompressedSize() > threshold;
      }
      else
      {
        assert(GetTotalCompressedSize() == that_.currentDiskSize_);
        return that_.currentDiskSize_ > threshold;
      }      
    }


    virtual bool IsExistingResource(int64_t internalId) ORTHANC_OVERRIDE
    {
      int32_t existing;
      CheckSuccess(that_.backend_.isExistingResource(&existing, that_.payload_, internalId));
      return (existing != 0);
    }


    virtual bool IsProtectedPatient(int64_t internalId) ORTHANC_OVERRIDE
    {
      int32_t isProtected;
      CheckSuccess(that_.backend_.isProtectedPatient(&isProtected, that_.payload_, internalId));
      return (isProtected != 0);
    }


    virtual void ListAvailableAttachments(std::set<FileContentType>& target,
                                          int64_t id) ORTHANC_OVERRIDE
    {
      ResetAnswers();

      CheckSuccess(that_.backend_.listAvailableAttachments(that_.GetContext(), that_.payload_, id));

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
          target.insert(static_cast<FileContentType>(*it));
        }
      }
    }


    virtual void LogChange(int64_t internalId,
                           const ServerIndexChange& change) ORTHANC_OVERRIDE
    {
      OrthancPluginChange tmp;
      tmp.seq = change.GetSeq();
      tmp.changeType = static_cast<int32_t>(change.GetChangeType());
      tmp.resourceType = Plugins::Convert(change.GetResourceType());
      tmp.publicId = change.GetPublicId().c_str();
      tmp.date = change.GetDate().c_str();

      CheckSuccess(that_.backend_.logChange(that_.payload_, &tmp));
    }


    virtual void LogExportedResource(const ExportedResource& resource) ORTHANC_OVERRIDE
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

      CheckSuccess(that_.backend_.logExportedResource(that_.payload_, &tmp));
    }

    
    virtual bool LookupAttachment(FileInfo& attachment,
                                  int64_t& revision,
                                  int64_t id,
                                  FileContentType contentType) ORTHANC_OVERRIDE
    {
      ResetAnswers();

      CheckSuccess(that_.backend_.lookupAttachment
                   (that_.GetContext(), that_.payload_, id, static_cast<int32_t>(contentType)));
      
      revision = 0;  // Dummy value, as revisions were added in Orthanc 1.9.2

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


    virtual bool LookupGlobalProperty(std::string& target,
                                      GlobalProperty property,
                                      bool shared) ORTHANC_OVERRIDE
    {
      // "shared" is unused, as database plugins using Orthanc SDK <=
      // 1.9.1 are not compatible with multiple readers/writers
      
      ResetAnswers();

      CheckSuccess(that_.backend_.lookupGlobalProperty
                   (that_.GetContext(), that_.payload_, static_cast<int32_t>(property)));

      return ForwardSingleAnswer(target);
    }


    // From the "ILookupResources" interface
    virtual void LookupIdentifierRange(std::list<int64_t>& result,
                                       ResourceType level,
                                       const DicomTag& tag,
                                       const std::string& start,
                                       const std::string& end) ORTHANC_OVERRIDE
    {
      if (that_.extensions_.lookupIdentifierRange == NULL)
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
        CheckSuccess(that_.extensions_.lookupIdentifierRange(that_.GetContext(), that_.payload_, Plugins::Convert(level),
                                                             tag.GetGroup(), tag.GetElement(),
                                                             start.c_str(), end.c_str()));
        ForwardAnswers(result);
      }
    }


    virtual bool LookupMetadata(std::string& target,
                                int64_t& revision,
                                int64_t id,
                                MetadataType type) ORTHANC_OVERRIDE
    {
      ResetAnswers();
      CheckSuccess(that_.backend_.lookupMetadata(that_.GetContext(), that_.payload_, id, static_cast<int32_t>(type)));
      revision = 0;  // Dummy value, as revisions were added in Orthanc 1.9.2
      return ForwardSingleAnswer(target);
    }


    virtual bool LookupParent(int64_t& parentId,
                              int64_t resourceId) ORTHANC_OVERRIDE
    {
      ResetAnswers();
      CheckSuccess(that_.backend_.lookupParent(that_.GetContext(), that_.payload_, resourceId));
      return ForwardSingleAnswer(parentId);
    }


    virtual bool LookupResource(int64_t& id,
                                ResourceType& type,
                                const std::string& publicId) ORTHANC_OVERRIDE
    {
      ResetAnswers();

      CheckSuccess(that_.backend_.lookupResource(that_.GetContext(), that_.payload_, publicId.c_str()));

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


    virtual bool LookupResourceAndParent(int64_t& id,
                                         ResourceType& type,
                                         std::string& parentPublicId,
                                         const std::string& publicId) ORTHANC_OVERRIDE
    {
      if (that_.extensions_.lookupResourceAndParent == NULL)
      {
        return ILookupResourceAndParent::Apply(*this, id, type, parentPublicId, publicId);
      }
      else
      {
        std::list<std::string> parent;

        uint8_t isExisting;
        OrthancPluginResourceType pluginType = OrthancPluginResourceType_Patient;
      
        ResetAnswers();
        CheckSuccess(that_.extensions_.lookupResourceAndParent
                     (that_.GetContext(), &isExisting, &id, &pluginType, that_.payload_, publicId.c_str()));
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


    virtual bool SelectPatientToRecycle(int64_t& internalId) ORTHANC_OVERRIDE
    {
      ResetAnswers();
      CheckSuccess(that_.backend_.selectPatientToRecycle(that_.GetContext(), that_.payload_));
      return ForwardSingleAnswer(internalId);
    }


    virtual bool SelectPatientToRecycle(int64_t& internalId,
                                        int64_t patientIdToAvoid) ORTHANC_OVERRIDE
    {
      ResetAnswers();
      CheckSuccess(that_.backend_.selectPatientToRecycle2(that_.GetContext(), that_.payload_, patientIdToAvoid));
      return ForwardSingleAnswer(internalId);
    }


    virtual void SetGlobalProperty(GlobalProperty property,
                                   bool shared,
                                   const std::string& value) ORTHANC_OVERRIDE
    {
      // "shared" is unused, as database plugins using Orthanc SDK <=
      // 1.9.1 are not compatible with multiple readers/writers
      
      CheckSuccess(that_.backend_.setGlobalProperty
                   (that_.payload_, static_cast<int32_t>(property), value.c_str()));
    }


    // From the "ISetResourcesContent" interface
    virtual void SetIdentifierTag(int64_t id,
                                  const DicomTag& tag,
                                  const std::string& value) ORTHANC_OVERRIDE
    {
      OrthancPluginDicomTag tmp;
      tmp.group = tag.GetGroup();
      tmp.element = tag.GetElement();
      tmp.value = value.c_str();

      CheckSuccess(that_.backend_.setIdentifierTag(that_.payload_, id, &tmp));
    }


    // From the "ISetResourcesContent" interface
    virtual void SetMainDicomTag(int64_t id,
                                 const DicomTag& tag,
                                 const std::string& value) ORTHANC_OVERRIDE
    {
      OrthancPluginDicomTag tmp;
      tmp.group = tag.GetGroup();
      tmp.element = tag.GetElement();
      tmp.value = value.c_str();

      CheckSuccess(that_.backend_.setMainDicomTag(that_.payload_, id, &tmp));
    }


    virtual void SetMetadata(int64_t id,
                             MetadataType type,
                             const std::string& value,
                             int64_t revision) ORTHANC_OVERRIDE
    {
      // "revision" is not used, as it was added in Orthanc 1.9.2
      CheckSuccess(that_.backend_.setMetadata
                   (that_.payload_, id, static_cast<int32_t>(type), value.c_str()));
    }


    virtual void SetProtectedPatient(int64_t internalId, 
                                     bool isProtected) ORTHANC_OVERRIDE
    {
      CheckSuccess(that_.backend_.setProtectedPatient(that_.payload_, internalId, isProtected));
    }


    // From the "ISetResourcesContent" interface
    virtual void SetResourcesContent(const Orthanc::ResourcesContent& content) ORTHANC_OVERRIDE
    {
      if (that_.extensions_.setResourcesContent == NULL)
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
       
        CheckSuccess(that_.extensions_.setResourcesContent(
                       that_.payload_,
                       identifierTags.size(),
                       (identifierTags.empty() ? NULL : &identifierTags[0]),
                       mainDicomTags.size(),
                       (mainDicomTags.empty() ? NULL : &mainDicomTags[0]),
                       metadata.size(),
                       (metadata.empty() ? NULL : &metadata[0])));
      }
    }


    // From the "ICreateInstance" interface
    virtual void TagMostRecentPatient(int64_t patient) ORTHANC_OVERRIDE
    {
      if (that_.extensions_.tagMostRecentPatient != NULL)
      {
        CheckSuccess(that_.extensions_.tagMostRecentPatient(that_.payload_, patient));
      }
    }
  };


  void OrthancPluginDatabase::CheckSuccess(OrthancPluginErrorCode code)
  {
    if (code != OrthancPluginErrorCode_Success)
    {
      errorDictionary_.LogError(code, true);
      throw OrthancException(static_cast<ErrorCode>(code));
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
    activeTransaction_(NULL),
    fastGetTotalSize_(false),
    currentDiskSize_(0)
  {
    static const char* const MISSING = "  Missing extension in database index plugin: ";
    
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
      CLOG(INFO, PLUGINS) << MISSING << "LookupIdentifierRange()";
      isOptimal = false;
    }

    if (extensions_.createInstance == NULL)
    {
      CLOG(INFO, PLUGINS) << MISSING << "CreateInstance()";
      isOptimal = false;
    }

    if (extensions_.setResourcesContent == NULL)
    {
      CLOG(INFO, PLUGINS) << MISSING << "SetResourcesContent()";
      isOptimal = false;
    }

    if (extensions_.getChildrenMetadata == NULL)
    {
      CLOG(INFO, PLUGINS) << MISSING << "GetChildrenMetadata()";
      isOptimal = false;
    }

    if (extensions_.getAllMetadata == NULL)
    {
      CLOG(INFO, PLUGINS) << MISSING << "GetAllMetadata()";
      isOptimal = false;
    }

    if (extensions_.lookupResourceAndParent == NULL)
    {
      CLOG(INFO, PLUGINS) << MISSING << "LookupResourceAndParent()";
      isOptimal = false;
    }

    if (isOptimal)
    {
      CLOG(INFO, PLUGINS) << "The performance of the database index plugin "
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
    {
      boost::recursive_mutex::scoped_lock lock(mutex_);
      CheckSuccess(backend_.open(payload_));
    }

    VoidDatabaseListener listener;
    
    {
      Transaction transaction(*this, listener);
      transaction.Begin();

      std::string tmp;
      fastGetTotalSize_ =
        (transaction.LookupGlobalProperty(tmp, GlobalProperty_GetTotalSizeIsFast, true /* unused in old databases */) &&
         tmp == "1");
      
      if (fastGetTotalSize_)
      {
        currentDiskSize_ = 0;   // Unused
      }
      else
      {
        // This is the case of database plugins using Orthanc SDK <= 1.5.2
        LOG(WARNING) << "Your database index plugin is not compatible with multiple Orthanc writers";
        currentDiskSize_ = transaction.GetTotalCompressedSize();
      }

      transaction.Commit(0);
    }
  }


  void OrthancPluginDatabase::Close()
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);
    CheckSuccess(backend_.close(payload_));
  }


  IDatabaseWrapper::ITransaction* OrthancPluginDatabase::StartTransaction(TransactionType type,
                                                                          IDatabaseListener& listener)
  {
    // TODO - Take advantage of "type"

    std::unique_ptr<Transaction> transaction(new Transaction(*this, listener));
    transaction->Begin();
    return transaction.release();
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
    VoidDatabaseListener listener;
    
    if (extensions_.upgradeDatabase != NULL)
    {
      Transaction transaction(*this, listener);
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
    boost::recursive_mutex::scoped_lock lock(mutex_);

    if (activeTransaction_ != NULL)
    {
      activeTransaction_->AnswerReceived(answer);
    }
    else
    {
      LOG(WARNING) << "Received an answer from the database index plugin, but not transaction is active";
    }
  }
}
