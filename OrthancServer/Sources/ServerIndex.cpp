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


#include "PrecompiledHeadersServer.h"
#include "ServerIndex.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "../../OrthancFramework/Sources/DicomFormat/DicomArray.h"
#include "../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"
#include "../../OrthancFramework/Sources/DicomParsing/ParsedDicomFile.h"
#include "../../OrthancFramework/Sources/Logging.h"
#include "../../OrthancFramework/Sources/Toolbox.h"

#include "Database/ResourcesContent.h"
#include "OrthancConfiguration.h"
#include "Search/DatabaseLookup.h"
#include "Search/DicomTagConstraint.h"
#include "ServerContext.h"
#include "ServerIndexChange.h"
#include "ServerToolbox.h"

#include <boost/lexical_cast.hpp>
#include <boost/tuple/tuple.hpp>
#include <stdio.h>
#include <stack>

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
      explicit FileToRemove(const FileInfo& info) :
        uuid_(info.GetUuid()), 
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
    explicit Listener(ServerContext& context) :
      context_(context),
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
      LOG(TRACE) << "Remaining ancestor \"" << publicId << "\" (" << parentType << ")";

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
      LOG(TRACE) << "Change related to resource " << change.GetPublicId() << " of type " 
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
    explicit Transaction(ServerIndex& index) : 
      index_(index),
      isCommitted_(false)
    {
      transaction_.reset(index_.db_.StartTransaction());
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
      publicId_(publicId),
      time_(boost::posix_time::second_clock::local_time())
    {
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


  bool ServerIndex::IsUnstableResource(int64_t id)
  {
    return unstableResources_.Contains(id);
  }


  ServerIndex::ServerIndex(ServerContext& context,
                           IDatabaseWrapper& db,
                           unsigned int threadSleep) : 
    done_(false),
    db_(db),
    maximumStorageSize_(0),
    maximumPatients_(0),
    mainDicomTagsRegistry_(new MainDicomTagsRegistry),
    maxRetries_(10)
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
                                 const DicomMap& dicomSummary,
                                 const Attachments& attachments,
                                 const MetadataMap& metadata,
                                 const DicomInstanceOrigin& origin,
                                 bool overwrite,
                                 bool hasTransferSyntax,
                                 DicomTransferSyntax transferSyntax,
                                 bool hasPixelDataOffset,
                                 uint64_t pixelDataOffset)
  {
    boost::mutex::scoped_lock lock(mutex_);

    int64_t expectedInstances;
    const bool hasExpectedInstances =
      ComputeExpectedNumberOfInstances(expectedInstances, dicomSummary);
    
    instanceMetadata.clear();

    DicomInstanceHasher hasher(dicomSummary);
    const std::string hashPatient = hasher.HashPatient();
    const std::string hashStudy = hasher.HashStudy();
    const std::string hashSeries = hasher.HashSeries();
    const std::string hashInstance = hasher.HashInstance();

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
        
        if (overwrite)
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

        if (status.isNewSeries_)
        {
          if (hasExpectedInstances)
          {
            content.AddMetadata(status.seriesId_, MetadataType_Series_ExpectedNumberOfInstances,
                                boost::lexical_cast<std::string>(expectedInstances));
          }

          // New in Orthanc 1.9.0
          content.AddMetadata(status.seriesId_, MetadataType_RemoteAet,
                              origin.GetRemoteAetC());
        }

        
        // Attach the auto-computed metadata for the instance level,
        // reflecting these additions into the input metadata map
        SetInstanceMetadata(content, instanceMetadata, instanceId,
                            MetadataType_Instance_ReceptionDate, now);
        SetInstanceMetadata(content, instanceMetadata, instanceId, MetadataType_RemoteAet,
                            origin.GetRemoteAetC());
        SetInstanceMetadata(content, instanceMetadata, instanceId, MetadataType_Instance_Origin, 
                            EnumerationToString(origin.GetRequestOrigin()));


        if (hasTransferSyntax)
        {
          // New in Orthanc 1.2.0
          SetInstanceMetadata(content, instanceMetadata, instanceId,
                              MetadataType_Instance_TransferSyntax,
                              GetTransferSyntaxUid(transferSyntax));
        }

        {
          std::string s;

          if (origin.LookupRemoteIp(s))
          {
            // New in Orthanc 1.4.0
            SetInstanceMetadata(content, instanceMetadata, instanceId,
                                MetadataType_Instance_RemoteIp, s);
          }

          if (origin.LookupCalledAet(s))
          {
            // New in Orthanc 1.4.0
            SetInstanceMetadata(content, instanceMetadata, instanceId,
                                MetadataType_Instance_CalledAet, s);
          }

          if (origin.LookupHttpUsername(s))
          {
            // New in Orthanc 1.4.0
            SetInstanceMetadata(content, instanceMetadata, instanceId,
                                MetadataType_Instance_HttpUsername, s);
          }
        }

        if (hasPixelDataOffset)
        {
          // New in Orthanc 1.9.1
          SetInstanceMetadata(content, instanceMetadata, instanceId,
                              MetadataType_Instance_PixelDataOffset,
                              boost::lexical_cast<std::string>(pixelDataOffset));
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
                                MetadataType_Instance_IndexInSeries, Toolbox::StripSpaces(value->GetContent()));
          }
        }

        
        db_.SetResourcesContent(content);
      }

  
      // Check whether the series of this new instance is now completed
      int64_t expectedNumberOfInstances;
      if (ComputeExpectedNumberOfInstances(expectedNumberOfInstances, dicomSummary))
      {
        SeriesStatus seriesStatus = GetSeriesStatus(db_, status.seriesId_, expectedNumberOfInstances);
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


  SeriesStatus ServerIndex::GetSeriesStatus(IDatabaseWrapper& db,
                                            int64_t id,
                                            int64_t expectedNumberOfInstances)
  {
    std::list<std::string> values;
    db.GetChildrenMetadata(values, id, MetadataType_Instance_IndexInSeries);

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
                                        IDatabaseWrapper& db,
                                        int64_t resourceId,
                                        ResourceType resourceType)
  {
    DicomMap tags;
    db.GetMainDicomTags(tags, resourceId);

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
      
      LOG(TRACE) << "Recycling one patient";
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


  void ServerIndex::StandaloneRecycling()
  {
    // WARNING: No mutex here, do not include this as a public method
    Transaction t(*this);
    Recycle(0, "");
    t.Commit(0);
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


  void ServerIndex::ReconstructInstance(const ParsedDicomFile& dicom)
  {
    DicomMap summary;
    OrthancConfiguration::DefaultExtractDicomSummary(summary, dicom);

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
        DicomTransferSyntax s;
        if (dicom.LookupTransferSyntax(s))
        {
          db_.SetMetadata(instance, MetadataType_Instance_TransferSyntax, GetTransferSyntaxUid(s));
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





  /***
   ** PROTOTYPING FOR DB REFACTORING BELOW
   ***/
    
  namespace
  {
    /**
     * Some handy templates to reduce the verbosity in the definitions
     * of the internal classes.
     **/
    
    template <typename Operations,
              typename Tuple>
    class TupleOperationsWrapper : public ServerIndex::IReadOnlyOperations
    {
    protected:
      Operations&   operations_;
      const Tuple&  tuple_;
    
    public:
      TupleOperationsWrapper(Operations& operations,
                             const Tuple& tuple) :
        operations_(operations),
        tuple_(tuple)
      {
      }
    
      virtual void Apply(ServerIndex::ReadOnlyTransaction& transaction) ORTHANC_OVERRIDE
      {
        operations_.ApplyTuple(transaction, tuple_);
      }
    };


    template <typename T1>
    class ReadOnlyOperationsT1 : public boost::noncopyable
    {
    public:
      typedef typename boost::tuple<T1>  Tuple;
      
      virtual ~ReadOnlyOperationsT1()
      {
      }

      virtual void ApplyTuple(ServerIndex::ReadOnlyTransaction& transaction,
                              const Tuple& tuple) = 0;

      void Apply(ServerIndex& index,
                 T1 t1)
      {
        const Tuple tuple(t1);
        TupleOperationsWrapper<ReadOnlyOperationsT1, Tuple> wrapper(*this, tuple);
        index.Apply(wrapper);
      }
    };


    template <typename T1,
              typename T2>
    class ReadOnlyOperationsT2 : public boost::noncopyable
    {
    public:
      typedef typename boost::tuple<T1, T2>  Tuple;
      
      virtual ~ReadOnlyOperationsT2()
      {
      }

      virtual void ApplyTuple(ServerIndex::ReadOnlyTransaction& transaction,
                              const Tuple& tuple) = 0;

      void Apply(ServerIndex& index,
                 T1 t1,
                 T2 t2)
      {
        const Tuple tuple(t1, t2);
        TupleOperationsWrapper<ReadOnlyOperationsT2, Tuple> wrapper(*this, tuple);
        index.Apply(wrapper);
      }
    };


    template <typename T1,
              typename T2,
              typename T3>
    class ReadOnlyOperationsT3 : public boost::noncopyable
    {
    public:
      typedef typename boost::tuple<T1, T2, T3>  Tuple;
      
      virtual ~ReadOnlyOperationsT3()
      {
      }

      virtual void ApplyTuple(ServerIndex::ReadOnlyTransaction& transaction,
                              const Tuple& tuple) = 0;

      void Apply(ServerIndex& index,
                 T1 t1,
                 T2 t2,
                 T3 t3)
      {
        const Tuple tuple(t1, t2, t3);
        TupleOperationsWrapper<ReadOnlyOperationsT3, Tuple> wrapper(*this, tuple);
        index.Apply(wrapper);
      }
    };


    template <typename T1,
              typename T2,
              typename T3,
              typename T4>
    class ReadOnlyOperationsT4 : public boost::noncopyable
    {
    public:
      typedef typename boost::tuple<T1, T2, T3, T4>  Tuple;
      
      virtual ~ReadOnlyOperationsT4()
      {
      }

      virtual void ApplyTuple(ServerIndex::ReadOnlyTransaction& transaction,
                              const Tuple& tuple) = 0;

      void Apply(ServerIndex& index,
                 T1 t1,
                 T2 t2,
                 T3 t3,
                 T4 t4)
      {
        const Tuple tuple(t1, t2, t3, t4);
        TupleOperationsWrapper<ReadOnlyOperationsT4, Tuple> wrapper(*this, tuple);
        index.Apply(wrapper);
      }
    };


    template <typename T1,
              typename T2,
              typename T3,
              typename T4,
              typename T5>
    class ReadOnlyOperationsT5 : public boost::noncopyable
    {
    public:
      typedef typename boost::tuple<T1, T2, T3, T4, T5>  Tuple;
      
      virtual ~ReadOnlyOperationsT5()
      {
      }

      virtual void ApplyTuple(ServerIndex::ReadOnlyTransaction& transaction,
                              const Tuple& tuple) = 0;

      void Apply(ServerIndex& index,
                 T1 t1,
                 T2 t2,
                 T3 t3,
                 T4 t4,
                 T5 t5)
      {
        const Tuple tuple(t1, t2, t3, t4, t5);
        TupleOperationsWrapper<ReadOnlyOperationsT5, Tuple> wrapper(*this, tuple);
        index.Apply(wrapper);
      }
    };


    template <typename T1,
              typename T2,
              typename T3,
              typename T4,
              typename T5,
              typename T6>
    class ReadOnlyOperationsT6 : public boost::noncopyable
    {
    public:
      typedef typename boost::tuple<T1, T2, T3, T4, T5, T6>  Tuple;
      
      virtual ~ReadOnlyOperationsT6()
      {
      }

      virtual void ApplyTuple(ServerIndex::ReadOnlyTransaction& transaction,
                              const Tuple& tuple) = 0;

      void Apply(ServerIndex& index,
                 T1 t1,
                 T2 t2,
                 T3 t3,
                 T4 t4,
                 T5 t5,
                 T6 t6)
      {
        const Tuple tuple(t1, t2, t3, t4, t5, t6);
        TupleOperationsWrapper<ReadOnlyOperationsT6, Tuple> wrapper(*this, tuple);
        index.Apply(wrapper);
      }
    };
  }
  

  void ServerIndex::ApplyInternal(IReadOnlyOperations* readOperations,
                                  IReadWriteOperations* writeOperations)
  {
    if ((readOperations == NULL && writeOperations == NULL) ||
        (readOperations != NULL && writeOperations != NULL))
    {
      throw OrthancException(ErrorCode_InternalError);
    }
    
    unsigned int count = 0;

    for (;;)
    {
      try
      {
        boost::mutex::scoped_lock lock(mutex_);  // TODO - REMOVE

        Transaction transaction(*this);  // TODO - Only if "TransactionType_SingleStatement"

        if (readOperations != NULL)
        {
          ReadOnlyTransaction t(db_);
          readOperations->Apply(t);
        }
        else
        {
          assert(writeOperations != NULL);
          ReadWriteTransaction t(db_, *this);
          writeOperations->Apply(t, *listener_);          
        }

        transaction.Commit(0);
        
        return;  // Success
      }
      catch (OrthancException& e)
      {
        if (e.GetErrorCode() == ErrorCode_DatabaseCannotSerialize)
        {
          if (count == maxRetries_)
          {
            throw;
          }
          else
          {
            count++;
            boost::this_thread::sleep(boost::posix_time::milliseconds(100 * count));
          }          
        }
        else if (e.GetErrorCode() == ErrorCode_DatabaseUnavailable)
        {
          if (count == maxRetries_)
          {
            throw;
          }
          else
          {
            count++;
            boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
          }
        }
        else
        {
          throw;
        }
      }
    }
  }

  
  void ServerIndex::Apply(IReadOnlyOperations& operations)
  {
    ApplyInternal(&operations, NULL);
  }
  

  void ServerIndex::Apply(IReadWriteOperations& operations)
  {
    ApplyInternal(NULL, &operations);
  }
  

  bool ServerIndex::ExpandResource(Json::Value& target,
                                   const std::string& publicId,
                                   ResourceType level)
  {    
    class Operations : public ReadOnlyOperationsT4<bool&, Json::Value&, const std::string&, ResourceType>
    {
    private:
      ServerIndex&  index_;     // TODO - REMOVE

    public:
      explicit Operations(ServerIndex& index) :
        index_(index)
      {
      }
      
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        // Lookup for the requested resource
        int64_t internalId;  // unused
        ResourceType type;
        std::string parent;
        if (!transaction.LookupResourceAndParent(internalId, type, parent, tuple.get<2>()) ||
            type != tuple.get<3>())
        {
          tuple.get<0>() = false;
        }
        else
        {
          Json::Value& target = tuple.get<1>();
          target = Json::objectValue;
        
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
                target["ParentPatient"] = parent;
                break;

              case ResourceType_Series:
                target["ParentStudy"] = parent;
                break;

              case ResourceType_Instance:
                target["ParentSeries"] = parent;
                break;

              default:
                throw OrthancException(ErrorCode_InternalError);
            }
          }

          // List the children resources
          std::list<std::string> children;
          transaction.GetChildrenPublicId(children, internalId);

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
                target["Studies"] = c;
                break;

              case ResourceType_Study:
                target["Series"] = c;
                break;

              case ResourceType_Series:
                target["Instances"] = c;
                break;

              default:
                throw OrthancException(ErrorCode_InternalError);
            }
          }

          // Extract the metadata
          std::map<MetadataType, std::string> metadata;
          transaction.GetAllMetadata(metadata, internalId);

          // Set the resource type
          switch (type)
          {
            case ResourceType_Patient:
              target["Type"] = "Patient";
              break;

            case ResourceType_Study:
              target["Type"] = "Study";
              break;

            case ResourceType_Series:
            {
              target["Type"] = "Series";

              int64_t i;
              if (LookupIntegerMetadata(i, metadata, MetadataType_Series_ExpectedNumberOfInstances))
              {
                target["ExpectedNumberOfInstances"] = static_cast<int>(i);
                target["Status"] = EnumerationToString(transaction.GetSeriesStatus(internalId, i));
              }
              else
              {
                target["ExpectedNumberOfInstances"] = Json::nullValue;
                target["Status"] = EnumerationToString(SeriesStatus_Unknown);
              }

              break;
            }

            case ResourceType_Instance:
            {
              target["Type"] = "Instance";

              FileInfo attachment;
              if (!transaction.LookupAttachment(attachment, internalId, FileContentType_Dicom))
              {
                throw OrthancException(ErrorCode_InternalError);
              }

              target["FileSize"] = static_cast<unsigned int>(attachment.GetUncompressedSize());
              target["FileUuid"] = attachment.GetUuid();

              int64_t i;
              if (LookupIntegerMetadata(i, metadata, MetadataType_Instance_IndexInSeries))
              {
                target["IndexInSeries"] = static_cast<int>(i);
              }
              else
              {
                target["IndexInSeries"] = Json::nullValue;
              }

              break;
            }

            default:
              throw OrthancException(ErrorCode_InternalError);
          }

          // Record the remaining information
          target["ID"] = tuple.get<2>();
          transaction.MainDicomTagsToJson(target, internalId, type);

          std::string tmp;

          if (LookupStringMetadata(tmp, metadata, MetadataType_AnonymizedFrom))
          {
            target["AnonymizedFrom"] = tmp;
          }

          if (LookupStringMetadata(tmp, metadata, MetadataType_ModifiedFrom))
          {
            target["ModifiedFrom"] = tmp;
          }

          if (type == ResourceType_Patient ||
              type == ResourceType_Study ||
              type == ResourceType_Series)
          {
            target["IsStable"] = !index_.IsUnstableResource(internalId);

            if (LookupStringMetadata(tmp, metadata, MetadataType_LastUpdate))
            {
              target["LastUpdate"] = tmp;
            }
          }

          tuple.get<0>() = true;
        }
      }
    };

    bool found;
    Operations operations(*this);
    operations.Apply(*this, found, target, publicId, level);
    return found;
  }


  void ServerIndex::GetAllMetadata(std::map<MetadataType, std::string>& target,
                                   const std::string& publicId,
                                   ResourceType level)
  {
    class Operations : public ReadOnlyOperationsT3<std::map<MetadataType, std::string>&, const std::string&, ResourceType>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        ResourceType type;
        int64_t id;
        if (!transaction.LookupResource(id, type, tuple.get<1>()) ||
            tuple.get<2>() != type)
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }
        else
        {
          transaction.GetAllMetadata(tuple.get<0>(), id);
        }
      }
    };

    Operations operations;
    operations.Apply(*this, target, publicId, level);
  }


  bool ServerIndex::LookupAttachment(FileInfo& attachment,
                                     const std::string& instancePublicId,
                                     FileContentType contentType)
  {
    class Operations : public ReadOnlyOperationsT4<bool&, FileInfo&, const std::string&, FileContentType>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        int64_t internalId;
        ResourceType type;
        if (!transaction.LookupResource(internalId, type, tuple.get<2>()))
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }
        else if (transaction.LookupAttachment(tuple.get<1>(), internalId, tuple.get<3>()))
        {
          assert(tuple.get<1>().GetContentType() == tuple.get<3>());
          tuple.get<0>() = true;
        }
        else
        {
          tuple.get<0>() = false;
        }
      }
    };

    bool found;
    Operations operations;
    operations.Apply(*this, found, attachment, instancePublicId, contentType);
    return found;
  }


  void ServerIndex::GetAllUuids(std::list<std::string>& target,
                                ResourceType resourceType)
  {
    class Operations : public ReadOnlyOperationsT2<std::list<std::string>&, ResourceType>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        // TODO - CANDIDATE FOR "TransactionType_SingleStatement"
        transaction.GetAllPublicIds(tuple.get<0>(), tuple.get<1>());
      }
    };

    Operations operations;
    operations.Apply(*this, target, resourceType);
  }


  void ServerIndex::GetAllUuids(std::list<std::string>& target,
                                ResourceType resourceType,
                                size_t since,
                                size_t limit)
  {
    if (limit == 0)
    {
      target.clear();
    }
    else
    {
      class Operations : public ReadOnlyOperationsT4<std::list<std::string>&, ResourceType, size_t, size_t>
      {
      public:
        virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                                const Tuple& tuple) ORTHANC_OVERRIDE
        {
          // TODO - CANDIDATE FOR "TransactionType_SingleStatement"
          transaction.GetAllPublicIds(tuple.get<0>(), tuple.get<1>(), tuple.get<2>(), tuple.get<3>());
        }
      };

      Operations operations;
      operations.Apply(*this, target, resourceType, since, limit);
    }
  }


  void ServerIndex::GetGlobalStatistics(/* out */ uint64_t& diskSize,
                                        /* out */ uint64_t& uncompressedSize,
                                        /* out */ uint64_t& countPatients, 
                                        /* out */ uint64_t& countStudies, 
                                        /* out */ uint64_t& countSeries, 
                                        /* out */ uint64_t& countInstances)
  {
    class Operations : public ReadOnlyOperationsT6<uint64_t&, uint64_t&, uint64_t&, uint64_t&, uint64_t&, uint64_t&>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        tuple.get<0>() = transaction.GetTotalCompressedSize();
        tuple.get<1>() = transaction.GetTotalUncompressedSize();
        tuple.get<2>() = transaction.GetResourceCount(ResourceType_Patient);
        tuple.get<3>() = transaction.GetResourceCount(ResourceType_Study);
        tuple.get<4>() = transaction.GetResourceCount(ResourceType_Series);
        tuple.get<5>() = transaction.GetResourceCount(ResourceType_Instance);
      }
    };
    
    Operations operations;
    operations.Apply(*this, diskSize, uncompressedSize, countPatients,
                     countStudies, countSeries, countInstances);
  }


  void ServerIndex::GetChanges(Json::Value& target,
                               int64_t since,                               
                               unsigned int maxResults)
  {
    class Operations : public ReadOnlyOperationsT3<Json::Value&, int64_t, unsigned int>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        // NB: In Orthanc <= 1.3.2, a transaction was missing, as
        // "GetLastChange()" involves calls to "GetPublicId()"

        std::list<ServerIndexChange> changes;
        bool done;
        bool hasLast = false;
        int64_t last = 0;

        transaction.GetChanges(changes, done, tuple.get<1>(), tuple.get<2>());
        if (changes.empty())
        {
          last = transaction.GetLastChangeIndex();
          hasLast = true;
        }

        FormatLog(tuple.get<0>(), changes, "Changes", done, tuple.get<1>(), hasLast, last);
      }
    };
    
    Operations operations;
    operations.Apply(*this, target, since, maxResults);
  }


  void ServerIndex::GetLastChange(Json::Value& target)
  {
    class Operations : public ReadOnlyOperationsT1<Json::Value&>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        // NB: In Orthanc <= 1.3.2, a transaction was missing, as
        // "GetLastChange()" involves calls to "GetPublicId()"

        std::list<ServerIndexChange> changes;
        bool hasLast = false;
        int64_t last = 0;

        transaction.GetLastChange(changes);
        if (changes.empty())
        {
          last = transaction.GetLastChangeIndex();
          hasLast = true;
        }

        FormatLog(tuple.get<0>(), changes, "Changes", true, 0, hasLast, last);
      }
    };
    
    Operations operations;
    operations.Apply(*this, target);
  }


  void ServerIndex::GetExportedResources(Json::Value& target,
                                         int64_t since,
                                         unsigned int maxResults)
  {
    class Operations : public ReadOnlyOperationsT3<Json::Value&, int64_t, unsigned int>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        // TODO - CANDIDATE FOR "TransactionType_SingleStatement"

        std::list<ExportedResource> exported;
        bool done;
        transaction.GetExportedResources(exported, done, tuple.get<1>(), tuple.get<2>());
        FormatLog(tuple.get<0>(), exported, "Exports", done, tuple.get<1>(), false, -1);
      }
    };
    
    Operations operations;
    operations.Apply(*this, target, since, maxResults);
  }


  void ServerIndex::GetLastExportedResource(Json::Value& target)
  {
    class Operations : public ReadOnlyOperationsT1<Json::Value&>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        // TODO - CANDIDATE FOR "TransactionType_SingleStatement"

        std::list<ExportedResource> exported;
        transaction.GetLastExportedResource(exported);
        FormatLog(tuple.get<0>(), exported, "Exports", true, 0, false, -1);
      }
    };
    
    Operations operations;
    operations.Apply(*this, target);
  }


  bool ServerIndex::IsProtectedPatient(const std::string& publicId)
  {
    class Operations : public ReadOnlyOperationsT2<bool&, const std::string&>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        // Lookup for the requested resource
        int64_t id;
        ResourceType type;
        if (!transaction.LookupResource(id, type, tuple.get<1>()) ||
            type != ResourceType_Patient)
        {
          throw OrthancException(ErrorCode_ParameterOutOfRange);
        }
        else
        {
          tuple.get<0>() = transaction.IsProtectedPatient(id);
        }
      }
    };

    bool isProtected;
    Operations operations;
    operations.Apply(*this, isProtected, publicId);
    return isProtected;
  }


  void ServerIndex::GetChildren(std::list<std::string>& result,
                                const std::string& publicId)
  {
    class Operations : public ReadOnlyOperationsT2<std::list<std::string>&, const std::string&>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        ResourceType type;
        int64_t resource;
        if (!transaction.LookupResource(resource, type, tuple.get<1>()))
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }
        else if (type == ResourceType_Instance)
        {
          // An instance cannot have a child
          throw OrthancException(ErrorCode_BadParameterType);
        }
        else
        {
          std::list<int64_t> tmp;
          transaction.GetChildrenInternalId(tmp, resource);

          tuple.get<0>().clear();

          for (std::list<int64_t>::const_iterator 
                 it = tmp.begin(); it != tmp.end(); ++it)
          {
            tuple.get<0>().push_back(transaction.GetPublicId(*it));
          }
        }
      }
    };
    
    Operations operations;
    operations.Apply(*this, result, publicId);
  }


  void ServerIndex::GetChildInstances(std::list<std::string>& result,
                                      const std::string& publicId)
  {
    class Operations : public ReadOnlyOperationsT2<std::list<std::string>&, const std::string&>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        tuple.get<0>().clear();
        
        ResourceType type;
        int64_t top;
        if (!transaction.LookupResource(top, type, tuple.get<1>()))
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }
        else if (type == ResourceType_Instance)
        {
          // The resource is already an instance: Do not go down the hierarchy
          tuple.get<0>().push_back(tuple.get<1>());
        }
        else
        {
          std::stack<int64_t> toExplore;
          toExplore.push(top);

          std::list<int64_t> tmp;
          while (!toExplore.empty())
          {
            // Get the internal ID of the current resource
            int64_t resource = toExplore.top();
            toExplore.pop();

            // TODO - This could be optimized by seeing how many
            // levels "type == transaction.GetResourceType(top)" is
            // above the "instances level"
            if (transaction.GetResourceType(resource) == ResourceType_Instance)
            {
              tuple.get<0>().push_back(transaction.GetPublicId(resource));
            }
            else
            {
              // Tag all the children of this resource as to be explored
              transaction.GetChildrenInternalId(tmp, resource);
              for (std::list<int64_t>::const_iterator 
                     it = tmp.begin(); it != tmp.end(); ++it)
              {
                toExplore.push(*it);
              }
            }
          }
        }
      }
    };
    
    Operations operations;
    operations.Apply(*this, result, publicId);
  }


  bool ServerIndex::LookupMetadata(std::string& target,
                                   const std::string& publicId,
                                   ResourceType expectedType,
                                   MetadataType type)
  {
    class Operations : public ReadOnlyOperationsT5<bool&, std::string&, const std::string&, ResourceType, MetadataType>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        ResourceType rtype;
        int64_t id;
        if (!transaction.LookupResource(id, rtype, tuple.get<2>()) ||
            rtype != tuple.get<3>())
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }
        else
        {
          tuple.get<0>() = transaction.LookupMetadata(tuple.get<1>(), id, tuple.get<4>());
        }
      }
    };

    bool found;
    Operations operations;
    operations.Apply(*this, found, target, publicId, expectedType, type);
    return found;
  }


  void ServerIndex::ListAvailableAttachments(std::set<FileContentType>& target,
                                             const std::string& publicId,
                                             ResourceType expectedType)
  {
    class Operations : public ReadOnlyOperationsT3<std::set<FileContentType>&, const std::string&, ResourceType>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        ResourceType type;
        int64_t id;
        if (!transaction.LookupResource(id, type, tuple.get<1>()) ||
            tuple.get<2>() != type)
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }
        else
        {
          transaction.ListAvailableAttachments(tuple.get<0>(), id);
        }
      }
    };
    
    Operations operations;
    operations.Apply(*this, target, publicId, expectedType);
  }


  bool ServerIndex::LookupParent(std::string& target,
                                 const std::string& publicId)
  {
    class Operations : public ReadOnlyOperationsT3<bool&, std::string&, const std::string&>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        ResourceType type;
        int64_t id;
        if (!transaction.LookupResource(id, type, tuple.get<2>()))
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }
        else
        {
          int64_t parentId;
          if (transaction.LookupParent(parentId, id))
          {
            tuple.get<1>() = transaction.GetPublicId(parentId);
            tuple.get<0>() = true;
          }
          else
          {
            tuple.get<0>() = false;
          }
        }
      }
    };

    bool found;
    Operations operations;
    operations.Apply(*this, found, target, publicId);
    return found;
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
    class Operations : public ServerIndex::IReadOnlyOperations
    {
    private:
      ResourceType&      type_;
      uint64_t&          diskSize_; 
      uint64_t&          uncompressedSize_; 
      unsigned int&      countStudies_; 
      unsigned int&      countSeries_; 
      unsigned int&      countInstances_; 
      uint64_t&          dicomDiskSize_; 
      uint64_t&          dicomUncompressedSize_; 
      const std::string& publicId_;
        
    public:
      explicit Operations(ResourceType& type,
                          uint64_t& diskSize, 
                          uint64_t& uncompressedSize, 
                          unsigned int& countStudies, 
                          unsigned int& countSeries, 
                          unsigned int& countInstances, 
                          uint64_t& dicomDiskSize, 
                          uint64_t& dicomUncompressedSize, 
                          const std::string& publicId) :
        type_(type),
        diskSize_(diskSize),
        uncompressedSize_(uncompressedSize),
        countStudies_(countStudies),
        countSeries_(countSeries),
        countInstances_(countInstances),
        dicomDiskSize_(dicomDiskSize),
        dicomUncompressedSize_(dicomUncompressedSize),
        publicId_(publicId)
      {
      }
      
      virtual void Apply(ServerIndex::ReadOnlyTransaction& transaction) ORTHANC_OVERRIDE
      {
        int64_t top;
        if (!transaction.LookupResource(top, type_, publicId_))
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }
        else
        {
          countInstances_ = 0;
          countSeries_ = 0;
          countStudies_ = 0;
          diskSize_ = 0;
          uncompressedSize_ = 0;
          dicomDiskSize_ = 0;
          dicomUncompressedSize_ = 0;

          std::stack<int64_t> toExplore;
          toExplore.push(top);

          while (!toExplore.empty())
          {
            // Get the internal ID of the current resource
            int64_t resource = toExplore.top();
            toExplore.pop();

            ResourceType thisType = transaction.GetResourceType(resource);

            std::set<FileContentType> f;
            transaction.ListAvailableAttachments(f, resource);

            for (std::set<FileContentType>::const_iterator
                   it = f.begin(); it != f.end(); ++it)
            {
              FileInfo attachment;
              if (transaction.LookupAttachment(attachment, resource, *it))
              {
                if (attachment.GetContentType() == FileContentType_Dicom)
                {
                  dicomDiskSize_ += attachment.GetCompressedSize();
                  dicomUncompressedSize_ += attachment.GetUncompressedSize();
                }
          
                diskSize_ += attachment.GetCompressedSize();
                uncompressedSize_ += attachment.GetUncompressedSize();
              }
            }

            if (thisType == ResourceType_Instance)
            {
              countInstances_++;
            }
            else
            {
              switch (thisType)
              {
                case ResourceType_Study:
                  countStudies_++;
                  break;

                case ResourceType_Series:
                  countSeries_++;
                  break;

                default:
                  break;
              }

              // Tag all the children of this resource as to be explored
              std::list<int64_t> tmp;
              transaction.GetChildrenInternalId(tmp, resource);
              for (std::list<int64_t>::const_iterator 
                     it = tmp.begin(); it != tmp.end(); ++it)
              {
                toExplore.push(*it);
              }
            }
          }

          if (countStudies_ == 0)
          {
            countStudies_ = 1;
          }

          if (countSeries_ == 0)
          {
            countSeries_ = 1;
          }
        }
      }
    };

    Operations operations(type, diskSize, uncompressedSize, countStudies, countSeries,
                          countInstances, dicomDiskSize, dicomUncompressedSize, publicId);
    Apply(operations);
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


    class Operations : public ServerIndex::IReadOnlyOperations
    {
    private:
      std::vector<std::string>&               result_;
      const std::vector<DatabaseConstraint>&  query_;
      ResourceType                            level_;
      
    public:
      Operations(std::vector<std::string>& result,
                 const std::vector<DatabaseConstraint>& query,
                 ResourceType level) :
        result_(result),
        query_(query),
        level_(level)
      {
      }

      virtual void Apply(ReadOnlyTransaction& transaction) ORTHANC_OVERRIDE
      {
        // TODO - CANDIDATE FOR "TransactionType_SingleStatement"
        std::list<std::string> tmp;
        transaction.ApplyLookupResources(tmp, NULL, query_, level_, 0);
        CopyListToVector(result_, tmp);
      }
    };

    Operations operations(result, query, level);
    Apply(operations);
  }


  bool ServerIndex::LookupGlobalProperty(std::string& value,
                                         GlobalProperty property)
  {
    class Operations : public ReadOnlyOperationsT3<bool&, std::string&, GlobalProperty>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        // TODO - CANDIDATE FOR "TransactionType_SingleStatement"
        tuple.get<0>() = transaction.LookupGlobalProperty(tuple.get<1>(), tuple.get<2>());
      }
    };

    bool found;
    Operations operations;
    operations.Apply(*this, found, value, property);
    return found;
  }
  

  std::string ServerIndex::GetGlobalProperty(GlobalProperty property,
                                             const std::string& defaultValue)
  {
    std::string s;
    if (LookupGlobalProperty(s, property))
    {
      return s;
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


    class Operations : public ReadOnlyOperationsT5<bool&, DicomMap&, const std::string&, ResourceType, ResourceType>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        // Lookup for the requested resource
        int64_t id;
        ResourceType type;
        if (!transaction.LookupResource(id, type, tuple.get<2>()) ||
            type != tuple.get<3>())
        {
          tuple.get<0>() = false;
        }
        else if (type == ResourceType_Study)
        {
          DicomMap tmp;
          transaction.GetMainDicomTags(tmp, id);

          switch (tuple.get<4>())
          {
            case ResourceType_Patient:
              tmp.ExtractPatientInformation(tuple.get<1>());
              tuple.get<0>() = true;
              break;

            case ResourceType_Study:
              tmp.ExtractStudyInformation(tuple.get<1>());
              tuple.get<0>() = true;
              break;

            default:
              throw OrthancException(ErrorCode_InternalError);
          }
        }
        else
        {
          transaction.GetMainDicomTags(tuple.get<1>(), id);
          tuple.get<0>() = true;
        }    
      }
    };

    result.Clear();

    bool found;
    Operations operations;
    operations.Apply(*this, found, result, publicId, expectedType, levelOfInterest);
    return found;
  }


  bool ServerIndex::GetAllMainDicomTags(DicomMap& result,
                                        const std::string& instancePublicId)
  {
    class Operations : public ReadOnlyOperationsT3<bool&, DicomMap&, const std::string&>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        // Lookup for the requested resource
        int64_t instance;
        ResourceType type;
        if (!transaction.LookupResource(instance, type, tuple.get<2>()) ||
            type != ResourceType_Instance)
        {
          tuple.get<0>() =  false;
        }
        else
        {
          DicomMap tmp;

          transaction.GetMainDicomTags(tmp, instance);
          tuple.get<1>().Merge(tmp);

          int64_t series;
          if (!transaction.LookupParent(series, instance))
          {
            throw OrthancException(ErrorCode_InternalError);
          }

          tmp.Clear();
          transaction.GetMainDicomTags(tmp, series);
          tuple.get<1>().Merge(tmp);

          int64_t study;
          if (!transaction.LookupParent(study, series))
          {
            throw OrthancException(ErrorCode_InternalError);
          }

          tmp.Clear();
          transaction.GetMainDicomTags(tmp, study);
          tuple.get<1>().Merge(tmp);

#ifndef NDEBUG
          {
            // Sanity test to check that all the main DICOM tags from the
            // patient level are copied at the study level
        
            int64_t patient;
            if (!transaction.LookupParent(patient, study))
            {
              throw OrthancException(ErrorCode_InternalError);
            }

            tmp.Clear();
            transaction.GetMainDicomTags(tmp, study);

            std::set<DicomTag> patientTags;
            tmp.GetTags(patientTags);

            for (std::set<DicomTag>::const_iterator
                   it = patientTags.begin(); it != patientTags.end(); ++it)
            {
              assert(tuple.get<1>().HasTag(*it));
            }
          }
#endif
      
          tuple.get<0>() =  true;
        }
      }
    };

    result.Clear();
    
    bool found;
    Operations operations;
    operations.Apply(*this, found, result, instancePublicId);
    return found;
  }


  bool ServerIndex::LookupResourceType(ResourceType& type,
                                       const std::string& publicId)
  {
    class Operations : public ReadOnlyOperationsT3<bool&, ResourceType&, const std::string&>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        // TODO - CANDIDATE FOR "TransactionType_SingleStatement"
        int64_t id;
        tuple.get<0>() = transaction.LookupResource(id, tuple.get<1>(), tuple.get<2>());
      }
    };

    bool found;
    Operations operations;
    operations.Apply(*this, found, type, publicId);
    return found;
  }


  unsigned int ServerIndex::GetDatabaseVersion()
  {
    class Operations : public ReadOnlyOperationsT1<unsigned int&>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        // TODO - CANDIDATE FOR "TransactionType_SingleStatement"
        tuple.get<0>() = transaction.GetDatabaseVersion();
      }
    };

    unsigned int version;
    Operations operations;
    operations.Apply(*this, version);
    return version;
  }


  bool ServerIndex::LookupParent(std::string& target,
                                 const std::string& publicId,
                                 ResourceType parentType)
  {
    class Operations : public ReadOnlyOperationsT4<bool&, std::string&, const std::string&, ResourceType>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        ResourceType type;
        int64_t id;
        if (!transaction.LookupResource(id, type, tuple.get<2>()))
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }

        while (type != tuple.get<3>())
        {
          int64_t parentId;

          if (type == ResourceType_Patient ||    // Cannot further go up in hierarchy
              !transaction.LookupParent(parentId, id))
          {
            tuple.get<0>() = false;
            return;
          }

          id = parentId;
          type = GetParentResourceType(type);
        }

        tuple.get<0>() = true;
        tuple.get<1>() = transaction.GetPublicId(id);
      }
    };

    bool found;
    Operations operations;
    operations.Apply(*this, found, target, publicId, parentType);
    return found;
  }


  void ServerIndex::ApplyLookupResources(std::vector<std::string>& resourcesId,
                                         std::vector<std::string>* instancesId,
                                         const DatabaseLookup& lookup,
                                         ResourceType queryLevel,
                                         size_t limit)
  {
    class Operations : public ReadOnlyOperationsT4<bool, const std::vector<DatabaseConstraint>&, ResourceType, size_t>
    {
    private:
      std::list<std::string>  resourcesList_;
      std::list<std::string>  instancesList_;
      
    public:
      const std::list<std::string>& GetResourcesList() const
      {
        return resourcesList_;
      }

      const std::list<std::string>& GetInstancesList() const
      {
        return instancesList_;
      }

      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        // TODO - CANDIDATE FOR "TransactionType_SingleStatement"
        if (tuple.get<0>())
        {
          transaction.ApplyLookupResources(resourcesList_, &instancesList_, tuple.get<1>(), tuple.get<2>(), tuple.get<3>());
        }
        else
        {
          transaction.ApplyLookupResources(resourcesList_, NULL, tuple.get<1>(), tuple.get<2>(), tuple.get<3>());
        }
      }
    };


    std::vector<DatabaseConstraint> normalized;
    NormalizeLookup(normalized, lookup, queryLevel);

    Operations operations;
    operations.Apply(*this, (instancesId != NULL), normalized, queryLevel, limit);
    
    CopyListToVector(resourcesId, operations.GetResourcesList());

    if (instancesId != NULL)
    { 
      CopyListToVector(*instancesId, operations.GetInstancesList());
    }
  }


  bool ServerIndex::DeleteResource(Json::Value& target,
                                   const std::string& uuid,
                                   ResourceType expectedType)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      bool                found_;
      Json::Value&        target_;
      const std::string&  uuid_;
      ResourceType        expectedType_;
      
    public:
      Operations(Json::Value& target,
                 const std::string& uuid,
                 ResourceType expectedType) :
        found_(false),
        target_(target),
        uuid_(uuid),
        expectedType_(expectedType)
      {
      }

      bool IsFound() const
      {
        return found_;
      }

      virtual void Apply(ReadWriteTransaction& transaction,
                         Listener& listener) ORTHANC_OVERRIDE
      {
        int64_t id;
        ResourceType type;
        if (!transaction.LookupResource(id, type, uuid_) ||
            expectedType_ != type)
        {
          found_ = false;
        }
        else
        {
          found_ = true;
          transaction.DeleteResource(id);

          if (listener.HasRemainingLevel())
          {
            ResourceType remainingType = listener.GetRemainingType();
            const std::string& remainingUuid = listener.GetRemainingPublicId();

            target_["RemainingAncestor"] = Json::Value(Json::objectValue);
            target_["RemainingAncestor"]["Path"] = GetBasePath(remainingType, remainingUuid);
            target_["RemainingAncestor"]["Type"] = EnumerationToString(remainingType);
            target_["RemainingAncestor"]["ID"] = remainingUuid;
          }
          else
          {
            target_["RemainingAncestor"] = Json::nullValue;
          }
        }
      }
    };

    Operations operations(target, uuid, expectedType);
    Apply(operations);
    return operations.IsFound();
  }


  void ServerIndex::LogExportedResource(const std::string& publicId,
                                        const std::string& remoteModality)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      const std::string&  publicId_;
      const std::string&  remoteModality_;

    public:
      Operations(const std::string& publicId,
                 const std::string& remoteModality) :
        publicId_(publicId),
        remoteModality_(remoteModality)
      {
      }
      
      virtual void Apply(ReadWriteTransaction& transaction,
                         Listener& listener) ORTHANC_OVERRIDE
      {
        int64_t id;
        ResourceType type;
        if (!transaction.LookupResource(id, type, publicId_))
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
          transaction.GetMainDicomTags(map, currentId);

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
            bool ok = transaction.LookupParent(currentId, currentId);
            (void) ok;  // Remove warning about unused variable in release builds
            assert(ok);
          }
        }

        ExportedResource resource(-1, 
                                  type,
                                  publicId_,
                                  remoteModality_,
                                  SystemToolbox::GetNowIsoString(true /* use UTC time (not local time) */),
                                  patientId,
                                  studyInstanceUid,
                                  seriesInstanceUid,
                                  sopInstanceUid);

        transaction.LogExportedResource(resource);
      }
    };

    Operations operations(publicId, remoteModality);
    Apply(operations);
  }


  void ServerIndex::SetProtectedPatient(const std::string& publicId,
                                        bool isProtected)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      const std::string&  publicId_;
      bool                isProtected_;

    public:
      Operations(const std::string& publicId,
                 bool isProtected) :
        publicId_(publicId),
        isProtected_(isProtected)
      {
      }

      virtual void Apply(ReadWriteTransaction& transaction,
                         Listener& listener) ORTHANC_OVERRIDE
      {
        // Lookup for the requested resource
        int64_t id;
        ResourceType type;
        if (!transaction.LookupResource(id, type, publicId_) ||
            type != ResourceType_Patient)
        {
          throw OrthancException(ErrorCode_ParameterOutOfRange);
        }
        else
        {
          transaction.SetProtectedPatient(id, isProtected_);
        }
      }
    };

    Operations operations(publicId, isProtected);
    Apply(operations);

    if (isProtected)
    {
      LOG(INFO) << "Patient " << publicId << " has been protected";
    }
    else
    {
      LOG(INFO) << "Patient " << publicId << " has been unprotected";
    }
  }


  void ServerIndex::SetMetadata(const std::string& publicId,
                                MetadataType type,
                                const std::string& value)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      const std::string&  publicId_;
      MetadataType        type_;
      const std::string&  value_;

    public:
      Operations(const std::string& publicId,
                 MetadataType type,
                 const std::string& value) :
        publicId_(publicId),
        type_(type),
        value_(value)
      {
      }

      virtual void Apply(ReadWriteTransaction& transaction,
                         Listener& listener) ORTHANC_OVERRIDE
      {
        ResourceType rtype;
        int64_t id;
        if (!transaction.LookupResource(id, rtype, publicId_))
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }
        else
        {
          transaction.SetMetadata(id, type_, value_);

          if (IsUserMetadata(type_))
          {
            transaction.LogChange(id, ChangeType_UpdatedMetadata, rtype, publicId_);
          }
        }
      }
    };

    Operations operations(publicId, type, value);
    Apply(operations);
  }


  void ServerIndex::DeleteMetadata(const std::string& publicId,
                                   MetadataType type)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      const std::string&  publicId_;
      MetadataType        type_;

    public:
      Operations(const std::string& publicId,
                 MetadataType type) :
        publicId_(publicId),
        type_(type)
      {
      }

      virtual void Apply(ReadWriteTransaction& transaction,
                         Listener& listener) ORTHANC_OVERRIDE
      {
        ResourceType rtype;
        int64_t id;
        if (!transaction.LookupResource(id, rtype, publicId_))
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }
        else
        {
          transaction.DeleteMetadata(id, type_);

          if (IsUserMetadata(type_))
          {
            transaction.LogChange(id, ChangeType_UpdatedMetadata, rtype, publicId_);
          }
        }
      }
    };

    Operations operations(publicId, type);
    Apply(operations);
  }


  uint64_t ServerIndex::IncrementGlobalSequence(GlobalProperty sequence)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      uint64_t       newValue_;
      GlobalProperty sequence_;

    public:
      Operations(GlobalProperty sequence) :
        sequence_(sequence)
      {
      }

      uint64_t GetNewValue() const
      {
        return newValue_;
      }

      virtual void Apply(ReadWriteTransaction& transaction,
                         Listener& listener) ORTHANC_OVERRIDE
      {
        std::string oldString;

        if (transaction.LookupGlobalProperty(oldString, sequence_))
        {
          uint64_t oldValue;
      
          try
          {
            oldValue = boost::lexical_cast<uint64_t>(oldString);
          }
          catch (boost::bad_lexical_cast&)
          {
            LOG(ERROR) << "Cannot read the global sequence "
                       << boost::lexical_cast<std::string>(sequence_) << ", resetting it";
            oldValue = 0;
          }

          newValue_ = oldValue + 1;
        }
        else
        {
          // Initialize the sequence at "1"
          newValue_ = 1;
        }

        transaction.SetGlobalProperty(sequence_, boost::lexical_cast<std::string>(newValue_));
      }
    };

    Operations operations(sequence);
    Apply(operations);
    return operations.GetNewValue();
  }


  void ServerIndex::DeleteChanges()
  {
    class Operations : public IReadWriteOperations
    {
    public:
      virtual void Apply(ReadWriteTransaction& transaction,
                         Listener& listener) ORTHANC_OVERRIDE
      {
        transaction.ClearChanges();
      }
    };

    Operations operations;
    Apply(operations);
  }

  
  void ServerIndex::DeleteExportedResources()
  {
    class Operations : public IReadWriteOperations
    {
    public:
      virtual void Apply(ReadWriteTransaction& transaction,
                         Listener& listener) ORTHANC_OVERRIDE
      {
        transaction.ClearExportedResources();
      }
    };

    Operations operations;
    Apply(operations);
  }


  void ServerIndex::SetGlobalProperty(GlobalProperty property,
                                      const std::string& value)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      GlobalProperty      property_;
      const std::string&  value_;
      
    public:
      Operations(GlobalProperty property,
                 const std::string& value) :
        property_(property),
        value_(value)
      {
      }
        
      virtual void Apply(ReadWriteTransaction& transaction,
                         Listener& listener) ORTHANC_OVERRIDE
      {
        transaction.SetGlobalProperty(property_, value_);
      }
    };

    Operations operations(property, value);
    Apply(operations);
  }


  void ServerIndex::DeleteAttachment(const std::string& publicId,
                                     FileContentType type)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      const std::string&  publicId_;
      FileContentType     type_;
      
    public:
      Operations(const std::string& publicId,
                 FileContentType type) :
        publicId_(publicId),
        type_(type)
      {
      }
        
      virtual void Apply(ReadWriteTransaction& transaction,
                         Listener& listener) ORTHANC_OVERRIDE
      {
        ResourceType rtype;
        int64_t id;
        if (!transaction.LookupResource(id, rtype, publicId_))
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }
        else
        {
          transaction.DeleteAttachment(id, type_);
          
          if (IsUserContentType(type_))
          {
            transaction.LogChange(id, ChangeType_UpdatedAttachment, rtype, publicId_);
          }
        }
      }
    };

    Operations operations(publicId, type);
    Apply(operations);
  }


  void ServerIndex::LogChange(ChangeType changeType,
                              const std::string& publicId)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      ChangeType          changeType_;
      const std::string&  publicId_;
      
    public:
      Operations(ChangeType changeType,
                 const std::string& publicId) :
        changeType_(changeType),
        publicId_(publicId)
      {
      }
        
      virtual void Apply(ReadWriteTransaction& transaction,
                         Listener& listener) ORTHANC_OVERRIDE
      {
        int64_t id;
        ResourceType type;
        if (!transaction.LookupResource(id, type, publicId_))
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }
        else
        {
          transaction.LogChange(id, changeType_, type, publicId_);
        }
      }
    };

    Operations operations(changeType, publicId);
    Apply(operations);
  }
}
