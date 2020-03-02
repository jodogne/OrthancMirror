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


#include "PrecompiledHeadersServer.h"
#include "ServerIndex.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "../Core/DicomFormat/DicomArray.h"
#include "../Core/DicomParsing/FromDcmtkBridge.h"
#include "../Core/DicomParsing/ParsedDicomFile.h"
#include "../Core/Logging.h"
#include "../Core/Toolbox.h"

#include "Database/ResourcesContent.h"
#include "DicomInstanceToStore.h"
#include "EmbeddedResources.h"
#include "OrthancConfiguration.h"
#include "Search/DatabaseLookup.h"
#include "Search/DicomTagConstraint.h"
#include "ServerContext.h"
#include "ServerIndexChange.h"
#include "ServerToolbox.h"

#include <boost/lexical_cast.hpp>
#include <stdio.h>

static const uint64_t MEGA_BYTES = 1024 * 1024;

namespace Orthanc
{
  static void CopyListToVector(std::vector<std::string>& target,
                               const std::list<std::string>& source)
  {
    target.resize(source.size());

    size_t pos = 0;
    
    for (std::list<std::string>::const_iterator
           it = source.begin(); it != source.end(); ++it)
    {
      target[pos] = *it;
      pos ++;
    }      
  }

  
  class ServerIndex::Listener : public IDatabaseListener
  {
  private:
    struct FileToRemove
    {
    private:
      std::string  uuid_;
      FileContentType  type_;

    public:
      FileToRemove(const FileInfo& info) : uuid_(info.GetUuid()), 
                                           type_(info.GetContentType())
      {
      }

      const std::string& GetUuid() const
      {
        return uuid_;
      }

      FileContentType GetContentType() const 
      {
        return type_;
      }
    };

    ServerContext& context_;
    bool hasRemainingLevel_;
    ResourceType remainingType_;
    std::string remainingPublicId_;
    std::list<FileToRemove> pendingFilesToRemove_;
    std::list<ServerIndexChange> pendingChanges_;
    uint64_t sizeOfFilesToRemove_;
    bool insideTransaction_;

    void Reset()
    {
      sizeOfFilesToRemove_ = 0;
      hasRemainingLevel_ = false;
      pendingFilesToRemove_.clear();
      pendingChanges_.clear();
    }

  public:
    Listener(ServerContext& context) : context_(context),
                                       insideTransaction_(false)      
    {
      Reset();
      assert(ResourceType_Patient < ResourceType_Study &&
             ResourceType_Study < ResourceType_Series &&
             ResourceType_Series < ResourceType_Instance);
    }

    void StartTransaction()
    {
      Reset();
      insideTransaction_ = true;
    }

    void EndTransaction()
    {
      insideTransaction_ = false;
    }

    uint64_t GetSizeOfFilesToRemove()
    {
      return sizeOfFilesToRemove_;
    }

    void CommitFilesToRemove()
    {
      for (std::list<FileToRemove>::const_iterator 
             it = pendingFilesToRemove_.begin();
           it != pendingFilesToRemove_.end(); ++it)
      {
        try
        {
          context_.RemoveFile(it->GetUuid(), it->GetContentType());
        }
        catch (OrthancException& e)
        {
          LOG(ERROR) << "Unable to remove an attachment from the storage area: "
                     << it->GetUuid() << " (type: " << EnumerationToString(it->GetContentType()) << ")";
        }
      }
    }

    void CommitChanges()
    {
      for (std::list<ServerIndexChange>::const_iterator 
             it = pendingChanges_.begin(); 
           it != pendingChanges_.end(); ++it)
      {
        context_.SignalChange(*it);
      }
    }

    virtual void SignalRemainingAncestor(ResourceType parentType,
                                         const std::string& publicId)
    {
      VLOG(1) << "Remaining ancestor \"" << publicId << "\" (" << parentType << ")";

      if (hasRemainingLevel_)
      {
        if (parentType < remainingType_)
        {
          remainingType_ = parentType;
          remainingPublicId_ = publicId;
        }
      }
      else
      {
        hasRemainingLevel_ = true;
        remainingType_ = parentType;
        remainingPublicId_ = publicId;
      }        
    }

    virtual void SignalFileDeleted(const FileInfo& info)
    {
      assert(Toolbox::IsUuid(info.GetUuid()));
      pendingFilesToRemove_.push_back(FileToRemove(info));
      sizeOfFilesToRemove_ += info.GetCompressedSize();
    }

    virtual void SignalChange(const ServerIndexChange& change)
    {
      VLOG(1) << "Change related to resource " << change.GetPublicId() << " of type " 
              << EnumerationToString(change.GetResourceType()) << ": " 
              << EnumerationToString(change.GetChangeType());

      if (insideTransaction_)
      {
        pendingChanges_.push_back(change);
      }
      else
      {
        context_.SignalChange(change);
      }
    }

    bool HasRemainingLevel() const
    {
      return hasRemainingLevel_;
    }

    ResourceType GetRemainingType() const
    {
      assert(HasRemainingLevel());
      return remainingType_;
    }

    const std::string& GetRemainingPublicId() const
    {
      assert(HasRemainingLevel());
      return remainingPublicId_;
    }                                 
  };


  class ServerIndex::Transaction
  {
  private:
    ServerIndex& index_;
    std::unique_ptr<IDatabaseWrapper::ITransaction> transaction_;
    bool isCommitted_;

  public:
    Transaction(ServerIndex& index) : 
      index_(index),
      isCommitted_(false)
    {
      transaction_.reset(index_.db_.StartTransaction());
      transaction_->Begin();

      index_.listener_->StartTransaction();
    }

    ~Transaction()
    {
      index_.listener_->EndTransaction();

      if (!isCommitted_)
      {
        transaction_->Rollback();
      }
    }

    void Commit(uint64_t sizeOfAddedFiles)
    {
      if (!isCommitted_)
      {
        int64_t delta = (static_cast<int64_t>(sizeOfAddedFiles) -
                         static_cast<int64_t>(index_.listener_->GetSizeOfFilesToRemove()));

        transaction_->Commit(delta);

        // We can remove the files once the SQLite transaction has
        // been successfully committed. Some files might have to be
        // deleted because of recycling.
        index_.listener_->CommitFilesToRemove();

        // Send all the pending changes to the Orthanc plugins
        index_.listener_->CommitChanges();

        isCommitted_ = true;
      }
    }
  };


  class ServerIndex::UnstableResourcePayload
  {
  private:
    ResourceType type_;
    std::string publicId_;
    boost::posix_time::ptime time_;

  public:
    UnstableResourcePayload() : type_(ResourceType_Instance)
    {
    }

    UnstableResourcePayload(Orthanc::ResourceType type,
                            const std::string& publicId) : 
      type_(type),
      publicId_(publicId)
    {
      time_ = boost::posix_time::second_clock::local_time();
    }

    unsigned int GetAge() const
    {
      return (boost::posix_time::second_clock::local_time() - time_).total_seconds();
    }

    ResourceType GetResourceType() const
    {
      return type_;
    }
    
    const std::string& GetPublicId() const
    {
      return publicId_;
    }
  };


  class ServerIndex::MainDicomTagsRegistry : public boost::noncopyable
  {
  private:
    class TagInfo
    {
    private:
      ResourceType  level_;
      DicomTagType  type_;

    public:
      TagInfo()
      {
      }

      TagInfo(ResourceType level,
              DicomTagType type) :
        level_(level),
        type_(type)
      {
      }

      ResourceType GetLevel() const
      {
        return level_;
      }

      DicomTagType GetType() const
      {
        return type_;
      }
    };
      
    typedef std::map<DicomTag, TagInfo>   Registry;


    Registry  registry_;
      
    void LoadTags(ResourceType level)
    {
      {
        const DicomTag* tags = NULL;
        size_t size;
  
        ServerToolbox::LoadIdentifiers(tags, size, level);
  
        for (size_t i = 0; i < size; i++)
        {
          if (registry_.find(tags[i]) == registry_.end())
          {
            registry_[tags[i]] = TagInfo(level, DicomTagType_Identifier);
          }
          else
          {
            // These patient-level tags are copied in the study level
            assert(level == ResourceType_Study &&
                   (tags[i] == DICOM_TAG_PATIENT_ID ||
                    tags[i] == DICOM_TAG_PATIENT_NAME ||
                    tags[i] == DICOM_TAG_PATIENT_BIRTH_DATE));
          }
        }
      }

      {
        std::set<DicomTag> tags;
        DicomMap::GetMainDicomTags(tags, level);

        for (std::set<DicomTag>::const_iterator
               tag = tags.begin(); tag != tags.end(); ++tag)
        {
          if (registry_.find(*tag) == registry_.end())
          {
            registry_[*tag] = TagInfo(level, DicomTagType_Main);
          }
        }
      }
    }

  public:
    MainDicomTagsRegistry()
    {
      LoadTags(ResourceType_Patient);
      LoadTags(ResourceType_Study);
      LoadTags(ResourceType_Series);
      LoadTags(ResourceType_Instance); 
    }

    void LookupTag(ResourceType& level,
                   DicomTagType& type,
                   const DicomTag& tag) const
    {
      Registry::const_iterator it = registry_.find(tag);

      if (it == registry_.end())
      {
        // Default values
        level = ResourceType_Instance;
        type = DicomTagType_Generic;
      }
      else
      {
        level = it->second.GetLevel();
        type = it->second.GetType();
      }
    }
  };


  bool ServerIndex::DeleteResource(Json::Value& target,
                                   const std::string& uuid,
                                   ResourceType expectedType)
  {
    boost::mutex::scoped_lock lock(mutex_);

    Transaction t(*this);

    int64_t id;
    ResourceType type;
    if (!db_.LookupResource(id, type, uuid) ||
        expectedType != type)
    {
      return false;
    }
      
    db_.DeleteResource(id);

    if (listener_->HasRemainingLevel())
    {
      ResourceType type = listener_->GetRemainingType();
      const std::string& uuid = listener_->GetRemainingPublicId();

      target["RemainingAncestor"] = Json::Value(Json::objectValue);
      target["RemainingAncestor"]["Path"] = GetBasePath(type, uuid);
      target["RemainingAncestor"]["Type"] = EnumerationToString(type);
      target["RemainingAncestor"]["ID"] = uuid;
    }
    else
    {
      target["RemainingAncestor"] = Json::nullValue;
    }

    t.Commit(0);

    return true;
  }


  void ServerIndex::FlushThread(ServerIndex* that,
                                unsigned int threadSleep)
  {
    // By default, wait for 10 seconds before flushing
    unsigned int sleep = 10;

    try
    {
      boost::mutex::scoped_lock lock(that->mutex_);
      std::string sleepString;

      if (that->db_.LookupGlobalProperty(sleepString, GlobalProperty_FlushSleep) &&
          Toolbox::IsInteger(sleepString))
      {
        sleep = boost::lexical_cast<unsigned int>(sleepString);
      }
    }
    catch (boost::bad_lexical_cast&)
    {
    }

    LOG(INFO) << "Starting the database flushing thread (sleep = " << sleep << ")";

    unsigned int count = 0;

    while (!that->done_)
    {
      boost::this_thread::sleep(boost::posix_time::milliseconds(threadSleep));
      count++;
      if (count < sleep)
      {
        continue;
      }

      Logging::Flush();

      boost::mutex::scoped_lock lock(that->mutex_);

      try
      {
        that->db_.FlushToDisk();
      }
      catch (OrthancException&)
      {
        LOG(ERROR) << "Cannot flush the SQLite database to the disk (is your filesystem full?)";
      }
          
      count = 0;
    }

    LOG(INFO) << "Stopping the database flushing thread";
  }


  static bool ComputeExpectedNumberOfInstances(int64_t& target,
                                               const DicomMap& dicomSummary)
  {
    try
    {
      const DicomValue* value;
      const DicomValue* value2;
          
      if ((value = dicomSummary.TestAndGetValue(DICOM_TAG_IMAGES_IN_ACQUISITION)) != NULL &&
          !value->IsNull() &&
          !value->IsBinary() &&
          (value2 = dicomSummary.TestAndGetValue(DICOM_TAG_NUMBER_OF_TEMPORAL_POSITIONS)) != NULL &&
          !value2->IsNull() &&
          !value2->IsBinary())
      {
        // Patch for series with temporal positions thanks to Will Ryder
        int64_t imagesInAcquisition = boost::lexical_cast<int64_t>(value->GetContent());
        int64_t countTemporalPositions = boost::lexical_cast<int64_t>(value2->GetContent());
        target = imagesInAcquisition * countTemporalPositions;
        return (target > 0);
      }

      else if ((value = dicomSummary.TestAndGetValue(DICOM_TAG_NUMBER_OF_SLICES)) != NULL &&
               !value->IsNull() &&
               !value->IsBinary() &&
               (value2 = dicomSummary.TestAndGetValue(DICOM_TAG_NUMBER_OF_TIME_SLICES)) != NULL &&
               !value2->IsBinary() &&
               !value2->IsNull())
      {
        // Support of Cardio-PET images
        int64_t numberOfSlices = boost::lexical_cast<int64_t>(value->GetContent());
        int64_t numberOfTimeSlices = boost::lexical_cast<int64_t>(value2->GetContent());
        target = numberOfSlices * numberOfTimeSlices;
        return (target > 0);
      }

      else if ((value = dicomSummary.TestAndGetValue(DICOM_TAG_CARDIAC_NUMBER_OF_IMAGES)) != NULL &&
               !value->IsNull() &&
               !value->IsBinary())
      {
        target = boost::lexical_cast<int64_t>(value->GetContent());
        return (target > 0);
      }
    }
    catch (OrthancException&)
    {
    }
    catch (boost::bad_lexical_cast&)
    {
    }

    return false;
  }




  static bool LookupStringMetadata(std::string& result,
                                   const std::map<MetadataType, std::string>& metadata,
                                   MetadataType type)
  {
    std::map<MetadataType, std::string>::const_iterator found = metadata.find(type);

    if (found == metadata.end())
    {
      return false;
    }
    else
    {
      result = found->second;
      return true;
    }
  }


  static bool LookupIntegerMetadata(int64_t& result,
                                    const std::map<MetadataType, std::string>& metadata,
                                    MetadataType type)
  {
    std::string s;
    if (!LookupStringMetadata(s, metadata, type))
    {
      return false;
    }

    try
    {
      result = boost::lexical_cast<int64_t>(s);
      return true;
    }
    catch (boost::bad_lexical_cast&)
    {
      return false;
    }
  }


  void ServerIndex::LogChange(int64_t internalId,
                              ChangeType changeType,
                              ResourceType resourceType,
                              const std::string& publicId)
  {
    ServerIndexChange change(changeType, resourceType, publicId);

    if (changeType <= ChangeType_INTERNAL_LastLogged)
    {
      db_.LogChange(internalId, change);
    }

    assert(listener_.get() != NULL);
    listener_->SignalChange(change);
  }


  uint64_t ServerIndex::IncrementGlobalSequenceInternal(GlobalProperty property)
  {
    std::string oldValue;

    if (db_.LookupGlobalProperty(oldValue, property))
    {
      uint64_t oldNumber;

      try
      {
        oldNumber = boost::lexical_cast<uint64_t>(oldValue);
        db_.SetGlobalProperty(property, boost::lexical_cast<std::string>(oldNumber + 1));
        return oldNumber + 1;
      }
      catch (boost::bad_lexical_cast&)
      {
        throw OrthancException(ErrorCode_InternalError);
      }
    }
    else
    {
      // Initialize the sequence at "1"
      db_.SetGlobalProperty(property, "1");
      return 1;
    }
  }



  ServerIndex::ServerIndex(ServerContext& context,
                           IDatabaseWrapper& db,
                           unsigned int threadSleep) : 
    done_(false),
    db_(db),
    maximumStorageSize_(0),
    maximumPatients_(0),
    overwrite_(false),
    mainDicomTagsRegistry_(new MainDicomTagsRegistry)
  {
    listener_.reset(new Listener(context));
    db_.SetListener(*listener_);

    // Initial recycling if the parameters have changed since the last
    // execution of Orthanc
    StandaloneRecycling();

    if (db.HasFlushToDisk())
    {
      flushThread_ = boost::thread(FlushThread, this, threadSleep);
    }

    unstableResourcesMonitorThread_ = boost::thread
      (UnstableResourcesMonitorThread, this, threadSleep);
  }



  ServerIndex::~ServerIndex()
  {
    if (!done_)
    {
      LOG(ERROR) << "INTERNAL ERROR: ServerIndex::Stop() should be invoked manually to avoid mess in the destruction order!";
      Stop();
    }
  }



  void ServerIndex::Stop()
  {
    if (!done_)
    {
      done_ = true;

      if (db_.HasFlushToDisk() &&
          flushThread_.joinable())
      {
        flushThread_.join();
      }

      if (unstableResourcesMonitorThread_.joinable())
      {
        unstableResourcesMonitorThread_.join();
      }
    }
  }


  static void SetInstanceMetadata(ResourcesContent& content,
                                  std::map<MetadataType, std::string>& instanceMetadata,
                                  int64_t instance,
                                  MetadataType metadata,
                                  const std::string& value)
  {
    content.AddMetadata(instance, metadata, value);
    instanceMetadata[metadata] = value;
  }


  void ServerIndex::SignalNewResource(ChangeType changeType,
                                      ResourceType level,
                                      const std::string& publicId,
                                      int64_t internalId)
  {
    ServerIndexChange change(changeType, level, publicId);
    db_.LogChange(internalId, change);
    
    assert(listener_.get() != NULL);
    listener_->SignalChange(change);
  }

  
  StoreStatus ServerIndex::Store(std::map<MetadataType, std::string>& instanceMetadata,
                                 DicomInstanceToStore& instanceToStore,
                                 const Attachments& attachments)
  {
    boost::mutex::scoped_lock lock(mutex_);

    const DicomMap& dicomSummary = instanceToStore.GetSummary();
    const ServerIndex::MetadataMap& metadata = instanceToStore.GetMetadata();

    int64_t expectedInstances;
    const bool hasExpectedInstances =
      ComputeExpectedNumberOfInstances(expectedInstances, dicomSummary);
    
    instanceMetadata.clear();

    const std::string hashPatient = instanceToStore.GetHasher().HashPatient();
    const std::string hashStudy = instanceToStore.GetHasher().HashStudy();
    const std::string hashSeries = instanceToStore.GetHasher().HashSeries();
    const std::string hashInstance = instanceToStore.GetHasher().HashInstance();

    try
    {
      Transaction t(*this);

      IDatabaseWrapper::CreateInstanceResult status;
      int64_t instanceId;

      // Check whether this instance is already stored
      if (!db_.CreateInstance(status, instanceId, hashPatient,
                              hashStudy, hashSeries, hashInstance))
      {
        // The instance already exists
        
        if (overwrite_)
        {
          // Overwrite the old instance
          LOG(INFO) << "Overwriting instance: " << hashInstance;
          db_.DeleteResource(instanceId);

          // Re-create the instance, now that the old one is removed
          if (!db_.CreateInstance(status, instanceId, hashPatient,
                                  hashStudy, hashSeries, hashInstance))
          {
            throw OrthancException(ErrorCode_InternalError);
          }
        }
        else
        {
          // Do nothing if the instance already exists and overwriting is disabled
          db_.GetAllMetadata(instanceMetadata, instanceId);
          return StoreStatus_AlreadyStored;
        }
      }


      // Warn about the creation of new resources. The order must be
      // from instance to patient.

      // NB: In theory, could be sped up by grouping the underlying
      // calls to "db_.LogChange()". However, this would only have an
      // impact when new patient/study/series get created, which
      // occurs far less often that creating new instances. The
      // positive impact looks marginal in practice.
      SignalNewResource(ChangeType_NewInstance, ResourceType_Instance, hashInstance, instanceId);

      if (status.isNewSeries_)
      {
        SignalNewResource(ChangeType_NewSeries, ResourceType_Series, hashSeries, status.seriesId_);
      }
      
      if (status.isNewStudy_)
      {
        SignalNewResource(ChangeType_NewStudy, ResourceType_Study, hashStudy, status.studyId_);
      }
      
      if (status.isNewPatient_)
      {
        SignalNewResource(ChangeType_NewPatient, ResourceType_Patient, hashPatient, status.patientId_);
      }
      
      
      // Ensure there is enough room in the storage for the new instance
      uint64_t instanceSize = 0;
      for (Attachments::const_iterator it = attachments.begin();
           it != attachments.end(); ++it)
      {
        instanceSize += it->GetCompressedSize();
      }

      Recycle(instanceSize, hashPatient /* don't consider the current patient for recycling */);
      
     
      // Attach the files to the newly created instance
      for (Attachments::const_iterator it = attachments.begin();
           it != attachments.end(); ++it)
      {
        db_.AddAttachment(instanceId, *it);
      }

      
      {
        ResourcesContent content;
      
        // Populate the tags of the newly-created resources

        content.AddResource(instanceId, ResourceType_Instance, dicomSummary);

        if (status.isNewSeries_)
        {
          content.AddResource(status.seriesId_, ResourceType_Series, dicomSummary);
        }

        if (status.isNewStudy_)
        {
          content.AddResource(status.studyId_, ResourceType_Study, dicomSummary);
        }

        if (status.isNewPatient_)
        {
          content.AddResource(status.patientId_, ResourceType_Patient, dicomSummary);
        }


        // Attach the user-specified metadata

        for (MetadataMap::const_iterator 
               it = metadata.begin(); it != metadata.end(); ++it)
        {
          switch (it->first.first)
          {
            case ResourceType_Patient:
              content.AddMetadata(status.patientId_, it->first.second, it->second);
              break;

            case ResourceType_Study:
              content.AddMetadata(status.studyId_, it->first.second, it->second);
              break;

            case ResourceType_Series:
              content.AddMetadata(status.seriesId_, it->first.second, it->second);
              break;

            case ResourceType_Instance:
              SetInstanceMetadata(content, instanceMetadata, instanceId,
                                  it->first.second, it->second);
              break;

            default:
              throw OrthancException(ErrorCode_ParameterOutOfRange);
          }
        }

        
        // Attach the auto-computed metadata for the patient/study/series levels
        std::string now = SystemToolbox::GetNowIsoString(true /* use UTC time (not local time) */);
        content.AddMetadata(status.seriesId_, MetadataType_LastUpdate, now);
        content.AddMetadata(status.studyId_, MetadataType_LastUpdate, now);
        content.AddMetadata(status.patientId_, MetadataType_LastUpdate, now);

        if (status.isNewSeries_ &&
            hasExpectedInstances)
        {
          content.AddMetadata(status.seriesId_, MetadataType_Series_ExpectedNumberOfInstances,
                              boost::lexical_cast<std::string>(expectedInstances));
        }

        
        // Attach the auto-computed metadata for the instance level,
        // reflecting these additions into the input metadata map
        SetInstanceMetadata(content, instanceMetadata, instanceId,
                            MetadataType_Instance_ReceptionDate, now);
        SetInstanceMetadata(content, instanceMetadata, instanceId, MetadataType_Instance_RemoteAet,
                            instanceToStore.GetOrigin().GetRemoteAetC());
        SetInstanceMetadata(content, instanceMetadata, instanceId, MetadataType_Instance_Origin, 
                            EnumerationToString(instanceToStore.GetOrigin().GetRequestOrigin()));


        {
          std::string s;

          if (instanceToStore.LookupTransferSyntax(s))
          {
            // New in Orthanc 1.2.0
            SetInstanceMetadata(content, instanceMetadata, instanceId,
                                MetadataType_Instance_TransferSyntax, s);
          }

          if (instanceToStore.GetOrigin().LookupRemoteIp(s))
          {
            // New in Orthanc 1.4.0
            SetInstanceMetadata(content, instanceMetadata, instanceId,
                                MetadataType_Instance_RemoteIp, s);
          }

          if (instanceToStore.GetOrigin().LookupCalledAet(s))
          {
            // New in Orthanc 1.4.0
            SetInstanceMetadata(content, instanceMetadata, instanceId,
                                MetadataType_Instance_CalledAet, s);
          }

          if (instanceToStore.GetOrigin().LookupHttpUsername(s))
          {
            // New in Orthanc 1.4.0
            SetInstanceMetadata(content, instanceMetadata, instanceId,
                                MetadataType_Instance_HttpUsername, s);
          }
        }

        
        const DicomValue* value;
        if ((value = dicomSummary.TestAndGetValue(DICOM_TAG_SOP_CLASS_UID)) != NULL &&
            !value->IsNull() &&
            !value->IsBinary())
        {
          SetInstanceMetadata(content, instanceMetadata, instanceId,
                              MetadataType_Instance_SopClassUid, value->GetContent());
        }


        if ((value = dicomSummary.TestAndGetValue(DICOM_TAG_INSTANCE_NUMBER)) != NULL ||
            (value = dicomSummary.TestAndGetValue(DICOM_TAG_IMAGE_INDEX)) != NULL)
        {
          if (!value->IsNull() && 
              !value->IsBinary())
          {
            SetInstanceMetadata(content, instanceMetadata, instanceId,
                                MetadataType_Instance_IndexInSeries, value->GetContent());
          }
        }

        
        db_.SetResourcesContent(content);
      }

  
      // Check whether the series of this new instance is now completed
      int64_t expectedNumberOfInstances;
      if (ComputeExpectedNumberOfInstances(expectedNumberOfInstances, dicomSummary))
      {
        SeriesStatus seriesStatus = GetSeriesStatus(status.seriesId_, expectedNumberOfInstances);
        if (seriesStatus == SeriesStatus_Complete)
        {
          LogChange(status.seriesId_, ChangeType_CompletedSeries, ResourceType_Series, hashSeries);
        }
      }
      

      // Mark the parent resources of this instance as unstable
      MarkAsUnstable(status.seriesId_, ResourceType_Series, hashSeries);
      MarkAsUnstable(status.studyId_, ResourceType_Study, hashStudy);
      MarkAsUnstable(status.patientId_, ResourceType_Patient, hashPatient);

      t.Commit(instanceSize);

      return StoreStatus_Success;
    }
    catch (OrthancException& e)
    {
      LOG(ERROR) << "EXCEPTION [" << e.What() << "]";
    }

    return StoreStatus_Failure;
  }


  void ServerIndex::GetGlobalStatistics(/* out */ uint64_t& diskSize,
                                        /* out */ uint64_t& uncompressedSize,
                                        /* out */ uint64_t& countPatients, 
                                        /* out */ uint64_t& countStudies, 
                                        /* out */ uint64_t& countSeries, 
                                        /* out */ uint64_t& countInstances)
  {
    boost::mutex::scoped_lock lock(mutex_);
    diskSize = db_.GetTotalCompressedSize();
    uncompressedSize = db_.GetTotalUncompressedSize();
    countPatients = db_.GetResourceCount(ResourceType_Patient);
    countStudies = db_.GetResourceCount(ResourceType_Study);
    countSeries = db_.GetResourceCount(ResourceType_Series);
    countInstances = db_.GetResourceCount(ResourceType_Instance);
  }

  
  SeriesStatus ServerIndex::GetSeriesStatus(int64_t id,
                                            int64_t expectedNumberOfInstances)
  {
    std::list<std::string> values;
    db_.GetChildrenMetadata(values, id, MetadataType_Instance_IndexInSeries);

    std::set<int64_t> instances;

    for (std::list<std::string>::const_iterator
           it = values.begin(); it != values.end(); ++it)
    {
      int64_t index;

      try
      {
        index = boost::lexical_cast<int64_t>(*it);
      }
      catch (boost::bad_lexical_cast&)
      {
        return SeriesStatus_Unknown;
      }
      
      if (!(index > 0 && index <= expectedNumberOfInstances))
      {
        // Out-of-range instance index
        return SeriesStatus_Inconsistent;
      }

      if (instances.find(index) != instances.end())
      {
        // Twice the same instance index
        return SeriesStatus_Inconsistent;
      }

      instances.insert(index);
    }

    if (static_cast<int64_t>(instances.size()) == expectedNumberOfInstances)
    {
      return SeriesStatus_Complete;
    }
    else
    {
      return SeriesStatus_Missing;
    }
  }


  void ServerIndex::MainDicomTagsToJson(Json::Value& target,
                                        int64_t resourceId,
                                        ResourceType resourceType)
  {
    DicomMap tags;
    db_.GetMainDicomTags(tags, resourceId);

    if (resourceType == ResourceType_Study)
    {
      DicomMap t1, t2;
      tags.ExtractStudyInformation(t1);
      tags.ExtractPatientInformation(t2);

      target["MainDicomTags"] = Json::objectValue;
      FromDcmtkBridge::ToJson(target["MainDicomTags"], t1, true);

      target["PatientMainDicomTags"] = Json::objectValue;
      FromDcmtkBridge::ToJson(target["PatientMainDicomTags"], t2, true);
    }
    else
    {
      target["MainDicomTags"] = Json::objectValue;
      FromDcmtkBridge::ToJson(target["MainDicomTags"], tags, true);
    }
  }

  
  bool ServerIndex::LookupResource(Json::Value& result,
                                   const std::string& publicId,
                                   ResourceType expectedType)
  {
    result = Json::objectValue;

    boost::mutex::scoped_lock lock(mutex_);

    // Lookup for the requested resource
    int64_t id;
    ResourceType type;
    std::string parent;
    if (!db_.LookupResourceAndParent(id, type, parent, publicId) ||
        type != expectedType)
    {
      return false;
    }

    // Set information about the parent resource (if it exists)
    if (type == ResourceType_Patient)
    {
      if (!parent.empty())
      {
        throw OrthancException(ErrorCode_DatabasePlugin);
      }
    }
    else
    {
      if (parent.empty())
      {
        throw OrthancException(ErrorCode_DatabasePlugin);
      }

      switch (type)
      {
        case ResourceType_Study:
          result["ParentPatient"] = parent;
          break;

        case ResourceType_Series:
          result["ParentStudy"] = parent;
          break;

        case ResourceType_Instance:
          result["ParentSeries"] = parent;
          break;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }
    }

    // List the children resources
    std::list<std::string> children;
    db_.GetChildrenPublicId(children, id);

    if (type != ResourceType_Instance)
    {
      Json::Value c = Json::arrayValue;

      for (std::list<std::string>::const_iterator
             it = children.begin(); it != children.end(); ++it)
      {
        c.append(*it);
      }

      switch (type)
      {
        case ResourceType_Patient:
          result["Studies"] = c;
          break;

        case ResourceType_Study:
          result["Series"] = c;
          break;

        case ResourceType_Series:
          result["Instances"] = c;
          break;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }
    }

    // Extract the metadata
    std::map<MetadataType, std::string> metadata;
    db_.GetAllMetadata(metadata, id);

    // Set the resource type
    switch (type)
    {
      case ResourceType_Patient:
        result["Type"] = "Patient";
        break;

      case ResourceType_Study:
        result["Type"] = "Study";
        break;

      case ResourceType_Series:
      {
        result["Type"] = "Series";

        int64_t i;
        if (LookupIntegerMetadata(i, metadata, MetadataType_Series_ExpectedNumberOfInstances))
        {
          result["ExpectedNumberOfInstances"] = static_cast<int>(i);
          result["Status"] = EnumerationToString(GetSeriesStatus(id, i));
        }
        else
        {
          result["ExpectedNumberOfInstances"] = Json::nullValue;
          result["Status"] = EnumerationToString(SeriesStatus_Unknown);
        }

        break;
      }

      case ResourceType_Instance:
      {
        result["Type"] = "Instance";

        FileInfo attachment;
        if (!db_.LookupAttachment(attachment, id, FileContentType_Dicom))
        {
          throw OrthancException(ErrorCode_InternalError);
        }

        result["FileSize"] = static_cast<unsigned int>(attachment.GetUncompressedSize());
        result["FileUuid"] = attachment.GetUuid();

        int64_t i;
        if (LookupIntegerMetadata(i, metadata, MetadataType_Instance_IndexInSeries))
        {
          result["IndexInSeries"] = static_cast<int>(i);
        }
        else
        {
          result["IndexInSeries"] = Json::nullValue;
        }

        break;
      }

      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    // Record the remaining information
    result["ID"] = publicId;
    MainDicomTagsToJson(result, id, type);

    std::string tmp;

    if (LookupStringMetadata(tmp, metadata, MetadataType_AnonymizedFrom))
    {
      result["AnonymizedFrom"] = tmp;
    }

    if (LookupStringMetadata(tmp, metadata, MetadataType_ModifiedFrom))
    {
      result["ModifiedFrom"] = tmp;
    }

    if (type == ResourceType_Patient ||
        type == ResourceType_Study ||
        type == ResourceType_Series)
    {
      result["IsStable"] = !unstableResources_.Contains(id);

      if (LookupStringMetadata(tmp, metadata, MetadataType_LastUpdate))
      {
        result["LastUpdate"] = tmp;
      }
    }

    return true;
  }


  bool ServerIndex::LookupAttachment(FileInfo& attachment,
                                     const std::string& instanceUuid,
                                     FileContentType contentType)
  {
    boost::mutex::scoped_lock lock(mutex_);

    int64_t id;
    ResourceType type;
    if (!db_.LookupResource(id, type, instanceUuid))
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }

    if (db_.LookupAttachment(attachment, id, contentType))
    {
      assert(attachment.GetContentType() == contentType);
      return true;
    }
    else
    {
      return false;
    }
  }



  void ServerIndex::GetAllUuids(std::list<std::string>& target,
                                ResourceType resourceType)
  {
    boost::mutex::scoped_lock lock(mutex_);
    db_.GetAllPublicIds(target, resourceType);
  }


  void ServerIndex::GetAllUuids(std::list<std::string>& target,
                                ResourceType resourceType,
                                size_t since,
                                size_t limit)
  {
    if (limit == 0)
    {
      target.clear();
      return;
    }

    boost::mutex::scoped_lock lock(mutex_);
    db_.GetAllPublicIds(target, resourceType, since, limit);
  }


  template <typename T>
  static void FormatLog(Json::Value& target,
                        const std::list<T>& log,
                        const std::string& name,
                        bool done,
                        int64_t since,
                        bool hasLast,
                        int64_t last)
  {
    Json::Value items = Json::arrayValue;
    for (typename std::list<T>::const_iterator
           it = log.begin(); it != log.end(); ++it)
    {
      Json::Value item;
      it->Format(item);
      items.append(item);
    }

    target = Json::objectValue;
    target[name] = items;
    target["Done"] = done;

    if (!hasLast)
    {
      // Best-effort guess of the last index in the sequence
      if (log.empty())
      {
        last = since;
      }
      else
      {
        last = log.back().GetSeq();
      }
    }
    
    target["Last"] = static_cast<int>(last);
  }


  void ServerIndex::GetChanges(Json::Value& target,
                               int64_t since,                               
                               unsigned int maxResults)
  {
    std::list<ServerIndexChange> changes;
    bool done;
    bool hasLast = false;
    int64_t last = 0;

    {
      boost::mutex::scoped_lock lock(mutex_);

      // Fix wrt. Orthanc <= 1.3.2: A transaction was missing, as
      // "GetLastChange()" involves calls to "GetPublicId()"
      Transaction transaction(*this);

      db_.GetChanges(changes, done, since, maxResults);
      if (changes.empty())
      {
        last = db_.GetLastChangeIndex();
        hasLast = true;
      }
      
      transaction.Commit(0);
    }

    FormatLog(target, changes, "Changes", done, since, hasLast, last);
  }


  void ServerIndex::GetLastChange(Json::Value& target)
  {
    std::list<ServerIndexChange> changes;
    bool hasLast = false;
    int64_t last = 0;

    {
      boost::mutex::scoped_lock lock(mutex_);

      // Fix wrt. Orthanc <= 1.3.2: A transaction was missing, as
      // "GetLastChange()" involves calls to "GetPublicId()"
      Transaction transaction(*this);

      db_.GetLastChange(changes);
      if (changes.empty())
      {
        last = db_.GetLastChangeIndex();
        hasLast = true;
      }

      transaction.Commit(0);
    }

    FormatLog(target, changes, "Changes", true, 0, hasLast, last);
  }


  void ServerIndex::LogExportedResource(const std::string& publicId,
                                        const std::string& remoteModality)
  {
    boost::mutex::scoped_lock lock(mutex_);
    Transaction transaction(*this);

    int64_t id;
    ResourceType type;
    if (!db_.LookupResource(id, type, publicId))
    {
      throw OrthancException(ErrorCode_InexistentItem);
    }

    std::string patientId;
    std::string studyInstanceUid;
    std::string seriesInstanceUid;
    std::string sopInstanceUid;

    int64_t currentId = id;
    ResourceType currentType = type;

    // Iteratively go up inside the patient/study/series/instance hierarchy
    bool done = false;
    while (!done)
    {
      DicomMap map;
      db_.GetMainDicomTags(map, currentId);

      switch (currentType)
      {
        case ResourceType_Patient:
          if (map.HasTag(DICOM_TAG_PATIENT_ID))
          {
            patientId = map.GetValue(DICOM_TAG_PATIENT_ID).GetContent();
          }
          done = true;
          break;

        case ResourceType_Study:
          if (map.HasTag(DICOM_TAG_STUDY_INSTANCE_UID))
          {
            studyInstanceUid = map.GetValue(DICOM_TAG_STUDY_INSTANCE_UID).GetContent();
          }
          currentType = ResourceType_Patient;
          break;

        case ResourceType_Series:
          if (map.HasTag(DICOM_TAG_SERIES_INSTANCE_UID))
          {
            seriesInstanceUid = map.GetValue(DICOM_TAG_SERIES_INSTANCE_UID).GetContent();
          }
          currentType = ResourceType_Study;
          break;

        case ResourceType_Instance:
          if (map.HasTag(DICOM_TAG_SOP_INSTANCE_UID))
          {
            sopInstanceUid = map.GetValue(DICOM_TAG_SOP_INSTANCE_UID).GetContent();
          }
          currentType = ResourceType_Series;
          break;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }

      // If we have not reached the Patient level, find the parent of
      // the current resource
      if (!done)
      {
        bool ok = db_.LookupParent(currentId, currentId);
        assert(ok);
      }
    }

    ExportedResource resource(-1, 
                              type,
                              publicId,
                              remoteModality,
                              SystemToolbox::GetNowIsoString(true /* use UTC time (not local time) */),
                              patientId,
                              studyInstanceUid,
                              seriesInstanceUid,
                              sopInstanceUid);

    db_.LogExportedResource(resource);
    transaction.Commit(0);
  }


  void ServerIndex::GetExportedResources(Json::Value& target,
                                         int64_t since,
                                         unsigned int maxResults)
  {
    std::list<ExportedResource> exported;
    bool done;

    {
      boost::mutex::scoped_lock lock(mutex_);
      db_.GetExportedResources(exported, done, since, maxResults);
    }

    FormatLog(target, exported, "Exports", done, since, false, -1);
  }


  void ServerIndex::GetLastExportedResource(Json::Value& target)
  {
    std::list<ExportedResource> exported;

    {
      boost::mutex::scoped_lock lock(mutex_);
      db_.GetLastExportedResource(exported);
    }

    FormatLog(target, exported, "Exports", true, 0, false, -1);
  }


  bool ServerIndex::IsRecyclingNeeded(uint64_t instanceSize)
  {
    if (maximumStorageSize_ != 0)
    {
      assert(maximumStorageSize_ >= instanceSize);
      
      if (db_.IsDiskSizeAbove(maximumStorageSize_ - instanceSize))
      {
        return true;
      }
    }

    if (maximumPatients_ != 0)
    {
      uint64_t patientCount = db_.GetResourceCount(ResourceType_Patient);
      if (patientCount > maximumPatients_)
      {
        return true;
      }
    }

    return false;
  }

  
  void ServerIndex::Recycle(uint64_t instanceSize,
                            const std::string& newPatientId)
  {
    if (!IsRecyclingNeeded(instanceSize))
    {
      return;
    }

    // Check whether other DICOM instances from this patient are
    // already stored
    int64_t patientToAvoid;
    ResourceType type;
    bool hasPatientToAvoid = db_.LookupResource(patientToAvoid, type, newPatientId);

    if (hasPatientToAvoid && type != ResourceType_Patient)
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    // Iteratively select patient to remove until there is enough
    // space in the DICOM store
    int64_t patientToRecycle;
    while (true)
    {
      // If other instances of this patient are already in the store,
      // we must avoid to recycle them
      bool ok = hasPatientToAvoid ?
        db_.SelectPatientToRecycle(patientToRecycle, patientToAvoid) :
        db_.SelectPatientToRecycle(patientToRecycle);
        
      if (!ok)
      {
        throw OrthancException(ErrorCode_FullStorage);
      }
      
      VLOG(1) << "Recycling one patient";
      db_.DeleteResource(patientToRecycle);

      if (!IsRecyclingNeeded(instanceSize))
      {
        // OK, we're done
        break;
      }
    }
  }  

  void ServerIndex::SetMaximumPatientCount(unsigned int count) 
  {
    boost::mutex::scoped_lock lock(mutex_);
    maximumPatients_ = count;

    if (count == 0)
    {
      LOG(WARNING) << "No limit on the number of stored patients";
    }
    else
    {
      LOG(WARNING) << "At most " << count << " patients will be stored";
    }

    StandaloneRecycling();
  }

  void ServerIndex::SetMaximumStorageSize(uint64_t size) 
  {
    boost::mutex::scoped_lock lock(mutex_);
    maximumStorageSize_ = size;

    if (size == 0)
    {
      LOG(WARNING) << "No limit on the size of the storage area";
    }
    else
    {
      LOG(WARNING) << "At most " << (size / MEGA_BYTES) << "MB will be used for the storage area";
    }

    StandaloneRecycling();
  }

  void ServerIndex::SetOverwriteInstances(bool overwrite)
  {
    boost::mutex::scoped_lock lock(mutex_);
    overwrite_ = overwrite;
  }


  void ServerIndex::StandaloneRecycling()
  {
    // WARNING: No mutex here, do not include this as a public method
    Transaction t(*this);
    Recycle(0, "");
    t.Commit(0);
  }


  bool ServerIndex::IsProtectedPatient(const std::string& publicId)
  {
    boost::mutex::scoped_lock lock(mutex_);

    // Lookup for the requested resource
    int64_t id;
    ResourceType type;
    if (!db_.LookupResource(id, type, publicId) ||
        type != ResourceType_Patient)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    return db_.IsProtectedPatient(id);
  }
     

  void ServerIndex::SetProtectedPatient(const std::string& publicId,
                                        bool isProtected)
  {
    boost::mutex::scoped_lock lock(mutex_);
    Transaction transaction(*this);

    // Lookup for the requested resource
    int64_t id;
    ResourceType type;
    if (!db_.LookupResource(id, type, publicId) ||
        type != ResourceType_Patient)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    db_.SetProtectedPatient(id, isProtected);
    transaction.Commit(0);

    if (isProtected)
      LOG(INFO) << "Patient " << publicId << " has been protected";
    else
      LOG(INFO) << "Patient " << publicId << " has been unprotected";
  }


  void ServerIndex::GetChildren(std::list<std::string>& result,
                                const std::string& publicId)
  {
    result.clear();

    boost::mutex::scoped_lock lock(mutex_);

    ResourceType type;
    int64_t resource;
    if (!db_.LookupResource(resource, type, publicId))
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }

    if (type == ResourceType_Instance)
    {
      // An instance cannot have a child
      throw OrthancException(ErrorCode_BadParameterType);
    }

    std::list<int64_t> tmp;
    db_.GetChildrenInternalId(tmp, resource);

    for (std::list<int64_t>::const_iterator 
           it = tmp.begin(); it != tmp.end(); ++it)
    {
      result.push_back(db_.GetPublicId(*it));
    }
  }


  void ServerIndex::GetChildInstances(std::list<std::string>& result,
                                      const std::string& publicId)
  {
    result.clear();

    boost::mutex::scoped_lock lock(mutex_);

    ResourceType type;
    int64_t top;
    if (!db_.LookupResource(top, type, publicId))
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }

    if (type == ResourceType_Instance)
    {
      // The resource is already an instance: Do not go down the hierarchy
      result.push_back(publicId);
      return;
    }

    std::stack<int64_t> toExplore;
    toExplore.push(top);

    std::list<int64_t> tmp;

    while (!toExplore.empty())
    {
      // Get the internal ID of the current resource
      int64_t resource = toExplore.top();
      toExplore.pop();

      if (db_.GetResourceType(resource) == ResourceType_Instance)
      {
        result.push_back(db_.GetPublicId(resource));
      }
      else
      {
        // Tag all the children of this resource as to be explored
        db_.GetChildrenInternalId(tmp, resource);
        for (std::list<int64_t>::const_iterator 
               it = tmp.begin(); it != tmp.end(); ++it)
        {
          toExplore.push(*it);
        }
      }
    }
  }


  void ServerIndex::SetMetadata(const std::string& publicId,
                                MetadataType type,
                                const std::string& value)
  {
    boost::mutex::scoped_lock lock(mutex_);
    Transaction t(*this);

    ResourceType rtype;
    int64_t id;
    if (!db_.LookupResource(id, rtype, publicId))
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }

    db_.SetMetadata(id, type, value);

    if (IsUserMetadata(type))
    {
      LogChange(id, ChangeType_UpdatedMetadata, rtype, publicId);
    }

    t.Commit(0);
  }


  void ServerIndex::DeleteMetadata(const std::string& publicId,
                                   MetadataType type)
  {
    boost::mutex::scoped_lock lock(mutex_);
    Transaction t(*this);

    ResourceType rtype;
    int64_t id;
    if (!db_.LookupResource(id, rtype, publicId))
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }

    db_.DeleteMetadata(id, type);

    if (IsUserMetadata(type))
    {
      LogChange(id, ChangeType_UpdatedMetadata, rtype, publicId);
    }

    t.Commit(0);
  }


  bool ServerIndex::LookupMetadata(std::string& target,
                                   const std::string& publicId,
                                   MetadataType type)
  {
    boost::mutex::scoped_lock lock(mutex_);

    ResourceType rtype;
    int64_t id;
    if (!db_.LookupResource(id, rtype, publicId))
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }

    return db_.LookupMetadata(target, id, type);
  }


  void ServerIndex::GetAllMetadata(std::map<MetadataType, std::string>& target,
                                   const std::string& publicId)
  {
    boost::mutex::scoped_lock lock(mutex_);

    ResourceType type;
    int64_t id;
    if (!db_.LookupResource(id, type, publicId))
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }

    return db_.GetAllMetadata(target, id);
  }


  void ServerIndex::ListAvailableAttachments(std::list<FileContentType>& target,
                                             const std::string& publicId,
                                             ResourceType expectedType)
  {
    boost::mutex::scoped_lock lock(mutex_);

    ResourceType type;
    int64_t id;
    if (!db_.LookupResource(id, type, publicId) ||
        expectedType != type)
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }

    db_.ListAvailableAttachments(target, id);
  }


  bool ServerIndex::LookupParent(std::string& target,
                                 const std::string& publicId)
  {
    boost::mutex::scoped_lock lock(mutex_);

    ResourceType type;
    int64_t id;
    if (!db_.LookupResource(id, type, publicId))
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }

    int64_t parentId;
    if (db_.LookupParent(parentId, id))
    {
      target = db_.GetPublicId(parentId);
      return true;
    }
    else
    {
      return false;
    }
  }


  uint64_t ServerIndex::IncrementGlobalSequence(GlobalProperty sequence)
  {
    boost::mutex::scoped_lock lock(mutex_);
    Transaction transaction(*this);

    uint64_t seq = IncrementGlobalSequenceInternal(sequence);
    transaction.Commit(0);

    return seq;
  }



  void ServerIndex::LogChange(ChangeType changeType,
                              const std::string& publicId)
  {
    boost::mutex::scoped_lock lock(mutex_);
    Transaction transaction(*this);

    int64_t id;
    ResourceType type;
    if (!db_.LookupResource(id, type, publicId))
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }

    LogChange(id, changeType, type, publicId);
    transaction.Commit(0);
  }


  void ServerIndex::DeleteChanges()
  {
    boost::mutex::scoped_lock lock(mutex_);

    Transaction transaction(*this);
    db_.ClearChanges();
    transaction.Commit(0);
  }

  void ServerIndex::DeleteExportedResources()
  {
    boost::mutex::scoped_lock lock(mutex_);

    Transaction transaction(*this);
    db_.ClearExportedResources();
    transaction.Commit(0);
  }


  void ServerIndex::GetResourceStatistics(/* out */ ResourceType& type,
                                          /* out */ uint64_t& diskSize, 
                                          /* out */ uint64_t& uncompressedSize, 
                                          /* out */ unsigned int& countStudies, 
                                          /* out */ unsigned int& countSeries, 
                                          /* out */ unsigned int& countInstances, 
                                          /* out */ uint64_t& dicomDiskSize, 
                                          /* out */ uint64_t& dicomUncompressedSize, 
                                          const std::string& publicId)
  {
    boost::mutex::scoped_lock lock(mutex_);

    int64_t top;
    if (!db_.LookupResource(top, type, publicId))
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }

    std::stack<int64_t> toExplore;
    toExplore.push(top);

    countInstances = 0;
    countSeries = 0;
    countStudies = 0;
    diskSize = 0;
    uncompressedSize = 0;
    dicomDiskSize = 0;
    dicomUncompressedSize = 0;

    while (!toExplore.empty())
    {
      // Get the internal ID of the current resource
      int64_t resource = toExplore.top();
      toExplore.pop();

      ResourceType thisType = db_.GetResourceType(resource);

      std::list<FileContentType> f;
      db_.ListAvailableAttachments(f, resource);

      for (std::list<FileContentType>::const_iterator
             it = f.begin(); it != f.end(); ++it)
      {
        FileInfo attachment;
        if (db_.LookupAttachment(attachment, resource, *it))
        {
          if (attachment.GetContentType() == FileContentType_Dicom)
          {
            dicomDiskSize += attachment.GetCompressedSize();
            dicomUncompressedSize += attachment.GetUncompressedSize();
          }
          
          diskSize += attachment.GetCompressedSize();
          uncompressedSize += attachment.GetUncompressedSize();
        }
      }

      if (thisType == ResourceType_Instance)
      {
        countInstances++;
      }
      else
      {
        switch (thisType)
        {
          case ResourceType_Study:
            countStudies++;
            break;

          case ResourceType_Series:
            countSeries++;
            break;

          default:
            break;
        }

        // Tag all the children of this resource as to be explored
        std::list<int64_t> tmp;
        db_.GetChildrenInternalId(tmp, resource);
        for (std::list<int64_t>::const_iterator 
               it = tmp.begin(); it != tmp.end(); ++it)
        {
          toExplore.push(*it);
        }
      }
    }

    if (countStudies == 0)
    {
      countStudies = 1;
    }

    if (countSeries == 0)
    {
      countSeries = 1;
    }
  }


  void ServerIndex::UnstableResourcesMonitorThread(ServerIndex* that,
                                                   unsigned int threadSleep)
  {
    int stableAge;
    
    {
      OrthancConfiguration::ReaderLock lock;
      stableAge = lock.GetConfiguration().GetUnsignedIntegerParameter("StableAge", 60);
    }

    if (stableAge <= 0)
    {
      stableAge = 60;
    }

    LOG(INFO) << "Starting the monitor for stable resources (stable age = " << stableAge << ")";

    while (!that->done_)
    {
      // Check for stable resources each few seconds
      boost::this_thread::sleep(boost::posix_time::milliseconds(threadSleep));

      boost::mutex::scoped_lock lock(that->mutex_);

      while (!that->unstableResources_.IsEmpty() &&
             that->unstableResources_.GetOldestPayload().GetAge() > static_cast<unsigned int>(stableAge))
      {
        // This DICOM resource has not received any new instance for
        // some time. It can be considered as stable.
          
        UnstableResourcePayload payload;
        int64_t id = that->unstableResources_.RemoveOldest(payload);

        // Ensure that the resource is still existing before logging the change
        if (that->db_.IsExistingResource(id))
        {
          switch (payload.GetResourceType())
          {
            case ResourceType_Patient:
              that->LogChange(id, ChangeType_StablePatient, ResourceType_Patient, payload.GetPublicId());
              break;

            case ResourceType_Study:
              that->LogChange(id, ChangeType_StableStudy, ResourceType_Study, payload.GetPublicId());
              break;

            case ResourceType_Series:
              that->LogChange(id, ChangeType_StableSeries, ResourceType_Series, payload.GetPublicId());
              break;

            default:
              throw OrthancException(ErrorCode_InternalError);
          }

          //LOG(INFO) << "Stable resource: " << EnumerationToString(payload.type_) << " " << id;
        }
      }
    }

    LOG(INFO) << "Closing the monitor thread for stable resources";
  }
  

  void ServerIndex::MarkAsUnstable(int64_t id,
                                   Orthanc::ResourceType type,
                                   const std::string& publicId)
  {
    // WARNING: Before calling this method, "mutex_" must be locked.

    assert(type == Orthanc::ResourceType_Patient ||
           type == Orthanc::ResourceType_Study ||
           type == Orthanc::ResourceType_Series);

    UnstableResourcePayload payload(type, publicId);
    unstableResources_.AddOrMakeMostRecent(id, payload);
    //LOG(INFO) << "Unstable resource: " << EnumerationToString(type) << " " << id;

    LogChange(id, ChangeType_NewChildInstance, type, publicId);
  }



  void ServerIndex::LookupIdentifierExact(std::vector<std::string>& result,
                                          ResourceType level,
                                          const DicomTag& tag,
                                          const std::string& value)
  {
    assert((level == ResourceType_Patient && tag == DICOM_TAG_PATIENT_ID) ||
           (level == ResourceType_Study && tag == DICOM_TAG_STUDY_INSTANCE_UID) ||
           (level == ResourceType_Study && tag == DICOM_TAG_ACCESSION_NUMBER) ||
           (level == ResourceType_Series && tag == DICOM_TAG_SERIES_INSTANCE_UID) ||
           (level == ResourceType_Instance && tag == DICOM_TAG_SOP_INSTANCE_UID));
    
    result.clear();

    DicomTagConstraint c(tag, ConstraintType_Equal, value, true, true);

    std::vector<DatabaseConstraint> query;
    query.push_back(c.ConvertToDatabaseConstraint(level, DicomTagType_Identifier));

    std::list<std::string> tmp;
    
    {
      boost::mutex::scoped_lock lock(mutex_);
      db_.ApplyLookupResources(tmp, NULL, query, level, 0);
    }

    CopyListToVector(result, tmp);
  }


  StoreStatus ServerIndex::AddAttachment(const FileInfo& attachment,
                                         const std::string& publicId)
  {
    boost::mutex::scoped_lock lock(mutex_);

    Transaction t(*this);

    ResourceType resourceType;
    int64_t resourceId;
    if (!db_.LookupResource(resourceId, resourceType, publicId))
    {
      return StoreStatus_Failure;  // Inexistent resource
    }

    // Remove possible previous attachment
    db_.DeleteAttachment(resourceId, attachment.GetContentType());

    // Locate the patient of the target resource
    int64_t patientId = resourceId;
    for (;;)
    {
      int64_t parent;
      if (db_.LookupParent(parent, patientId))
      {
        // We have not reached the patient level yet
        patientId = parent;
      }
      else
      {
        // We have reached the patient level
        break;
      }
    }

    // Possibly apply the recycling mechanism while preserving this patient
    assert(db_.GetResourceType(patientId) == ResourceType_Patient);
    Recycle(attachment.GetCompressedSize(), db_.GetPublicId(patientId));

    db_.AddAttachment(resourceId, attachment);

    if (IsUserContentType(attachment.GetContentType()))
    {
      LogChange(resourceId, ChangeType_UpdatedAttachment, resourceType, publicId);
    }

    t.Commit(attachment.GetCompressedSize());

    return StoreStatus_Success;
  }


  void ServerIndex::DeleteAttachment(const std::string& publicId,
                                     FileContentType type)
  {
    boost::mutex::scoped_lock lock(mutex_);
    Transaction t(*this);

    ResourceType rtype;
    int64_t id;
    if (!db_.LookupResource(id, rtype, publicId))
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }

    db_.DeleteAttachment(id, type);

    if (IsUserContentType(type))
    {
      LogChange(id, ChangeType_UpdatedAttachment, rtype, publicId);
    }

    t.Commit(0);
  }


  void ServerIndex::SetGlobalProperty(GlobalProperty property,
                                      const std::string& value)
  {
    boost::mutex::scoped_lock lock(mutex_);

    Transaction transaction(*this);
    db_.SetGlobalProperty(property, value);
    transaction.Commit(0);
  }


  bool ServerIndex::LookupGlobalProperty(std::string& value,
                                         GlobalProperty property)
  {
    boost::mutex::scoped_lock lock(mutex_);
    return db_.LookupGlobalProperty(value, property);
  }
  

  std::string ServerIndex::GetGlobalProperty(GlobalProperty property,
                                             const std::string& defaultValue)
  {
    std::string value;

    if (LookupGlobalProperty(value, property))
    {
      return value;
    }
    else
    {
      return defaultValue;
    }
  }


  bool ServerIndex::GetMainDicomTags(DicomMap& result,
                                     const std::string& publicId,
                                     ResourceType expectedType,
                                     ResourceType levelOfInterest)
  {
    // Yes, the following test could be shortened, but we wish to make it as clear as possible
    if (!(expectedType == ResourceType_Patient  && levelOfInterest == ResourceType_Patient) &&
        !(expectedType == ResourceType_Study    && levelOfInterest == ResourceType_Patient) &&
        !(expectedType == ResourceType_Study    && levelOfInterest == ResourceType_Study)   &&
        !(expectedType == ResourceType_Series   && levelOfInterest == ResourceType_Series)  &&
        !(expectedType == ResourceType_Instance && levelOfInterest == ResourceType_Instance))
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    result.Clear();

    boost::mutex::scoped_lock lock(mutex_);

    // Lookup for the requested resource
    int64_t id;
    ResourceType type;
    if (!db_.LookupResource(id, type, publicId) ||
        type != expectedType)
    {
      return false;
    }

    if (type == ResourceType_Study)
    {
      DicomMap tmp;
      db_.GetMainDicomTags(tmp, id);

      switch (levelOfInterest)
      {
        case ResourceType_Patient:
          tmp.ExtractPatientInformation(result);
          return true;

        case ResourceType_Study:
          tmp.ExtractStudyInformation(result);
          return true;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }
    }
    else
    {
      db_.GetMainDicomTags(result, id);
      return true;
    }    
  }


  bool ServerIndex::GetAllMainDicomTags(DicomMap& result,
                                        const std::string& instancePublicId)
  {
    result.Clear();
    
    boost::mutex::scoped_lock lock(mutex_);

    // Lookup for the requested resource
    int64_t instance;
    ResourceType type;
    if (!db_.LookupResource(instance, type, instancePublicId) ||
        type != ResourceType_Instance)
    {
      return false;
    }
    else
    {
      DicomMap tmp;

      db_.GetMainDicomTags(tmp, instance);
      result.Merge(tmp);

      int64_t series;
      if (!db_.LookupParent(series, instance))
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      tmp.Clear();
      db_.GetMainDicomTags(tmp, series);
      result.Merge(tmp);

      int64_t study;
      if (!db_.LookupParent(study, series))
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      tmp.Clear();
      db_.GetMainDicomTags(tmp, study);
      result.Merge(tmp);

#ifndef NDEBUG
      {
        // Sanity test to check that all the main DICOM tags from the
        // patient level are copied at the study level
        
        int64_t patient;
        if (!db_.LookupParent(patient, study))
        {
          throw OrthancException(ErrorCode_InternalError);
        }

        tmp.Clear();
        db_.GetMainDicomTags(tmp, study);

        std::set<DicomTag> patientTags;
        tmp.GetTags(patientTags);

        for (std::set<DicomTag>::const_iterator
               it = patientTags.begin(); it != patientTags.end(); ++it)
        {
          assert(result.HasTag(*it));
        }
      }
#endif
      
      return true;
    }
  }


  bool ServerIndex::LookupResourceType(ResourceType& type,
                                       const std::string& publicId)
  {
    boost::mutex::scoped_lock lock(mutex_);

    int64_t id;
    return db_.LookupResource(id, type, publicId);
  }


  unsigned int ServerIndex::GetDatabaseVersion()
  {
    boost::mutex::scoped_lock lock(mutex_);
    return db_.GetDatabaseVersion();
  }


  bool ServerIndex::LookupParent(std::string& target,
                                 const std::string& publicId,
                                 ResourceType parentType)
  {
    boost::mutex::scoped_lock lock(mutex_);

    ResourceType type;
    int64_t id;
    if (!db_.LookupResource(id, type, publicId))
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }

    while (type != parentType)
    {
      int64_t parentId;

      if (type == ResourceType_Patient ||    // Cannot further go up in hierarchy
          !db_.LookupParent(parentId, id))
      {
        return false;
      }

      id = parentId;
      type = GetParentResourceType(type);
    }

    target = db_.GetPublicId(id);
    return true;
  }


  void ServerIndex::ReconstructInstance(ParsedDicomFile& dicom)
  {
    DicomMap summary;
    dicom.ExtractDicomSummary(summary);

    DicomInstanceHasher hasher(summary);

    boost::mutex::scoped_lock lock(mutex_);

    try
    {
      Transaction t(*this);

      int64_t patient = -1, study = -1, series = -1, instance = -1;

      ResourceType dummy;      
      if (!db_.LookupResource(patient, dummy, hasher.HashPatient()) ||
          !db_.LookupResource(study, dummy, hasher.HashStudy()) ||
          !db_.LookupResource(series, dummy, hasher.HashSeries()) ||
          !db_.LookupResource(instance, dummy, hasher.HashInstance()) ||
          patient == -1 ||
          study == -1 ||
          series == -1 ||
          instance == -1)
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      db_.ClearMainDicomTags(patient);
      db_.ClearMainDicomTags(study);
      db_.ClearMainDicomTags(series);
      db_.ClearMainDicomTags(instance);

      {
        ResourcesContent content;
        content.AddResource(patient, ResourceType_Patient, summary);
        content.AddResource(study, ResourceType_Study, summary);
        content.AddResource(series, ResourceType_Series, summary);
        content.AddResource(instance, ResourceType_Instance, summary);
        db_.SetResourcesContent(content);
      }

      {
        std::string s;
        if (dicom.LookupTransferSyntax(s))
        {
          db_.SetMetadata(instance, MetadataType_Instance_TransferSyntax, s);
        }
      }

      const DicomValue* value;
      if ((value = summary.TestAndGetValue(DICOM_TAG_SOP_CLASS_UID)) != NULL &&
          !value->IsNull() &&
          !value->IsBinary())
      {
        db_.SetMetadata(instance, MetadataType_Instance_SopClassUid, value->GetContent());
      }

      t.Commit(0);  // No change in the DB size
    }
    catch (OrthancException& e)
    {
      LOG(ERROR) << "EXCEPTION [" << e.What() << "]";
    }
  }


  void ServerIndex::NormalizeLookup(std::vector<DatabaseConstraint>& target,
                                    const DatabaseLookup& source,
                                    ResourceType queryLevel) const
  {
    assert(mainDicomTagsRegistry_.get() != NULL);

    target.clear();
    target.reserve(source.GetConstraintsCount());

    for (size_t i = 0; i < source.GetConstraintsCount(); i++)
    {
      ResourceType level;
      DicomTagType type;
      
      mainDicomTagsRegistry_->LookupTag(level, type, source.GetConstraint(i).GetTag());

      if (type == DicomTagType_Identifier ||
          type == DicomTagType_Main)
      {
        // Use the fact that patient-level tags are copied at the study level
        if (level == ResourceType_Patient &&
            queryLevel != ResourceType_Patient)
        {
          level = ResourceType_Study;
        }
        
        target.push_back(source.GetConstraint(i).ConvertToDatabaseConstraint(level, type));
      }
    }
  }


  void ServerIndex::ApplyLookupResources(std::vector<std::string>& resourcesId,
                                         std::vector<std::string>* instancesId,
                                         const DatabaseLookup& lookup,
                                         ResourceType queryLevel,
                                         size_t limit)
  {
    std::vector<DatabaseConstraint> normalized;
    NormalizeLookup(normalized, lookup, queryLevel);

    std::list<std::string> resourcesList, instancesList;
    
    {
      boost::mutex::scoped_lock lock(mutex_);

      if (instancesId == NULL)
      {
        db_.ApplyLookupResources(resourcesList, NULL, normalized, queryLevel, limit);
      }
      else
      {
        db_.ApplyLookupResources(resourcesList, &instancesList, normalized, queryLevel, limit);
      }
    }

    CopyListToVector(resourcesId, resourcesList);

    if (instancesId != NULL)
    { 
      CopyListToVector(*instancesId, instancesList);
    }
  }
}
