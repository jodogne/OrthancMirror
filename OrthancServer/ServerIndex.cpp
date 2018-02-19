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
#include "ServerIndex.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "ServerIndexChange.h"
#include "EmbeddedResources.h"
#include "OrthancInitialization.h"
#include "../Core/DicomParsing/ParsedDicomFile.h"
#include "ServerToolbox.h"
#include "../Core/Toolbox.h"
#include "../Core/Logging.h"
#include "../Core/DicomFormat/DicomArray.h"

#include "../Core/DicomParsing/FromDcmtkBridge.h"
#include "ServerContext.h"
#include "DicomInstanceToStore.h"
#include "Search/LookupResource.h"

#include <boost/lexical_cast.hpp>
#include <stdio.h>

static const uint64_t MEGA_BYTES = 1024 * 1024;

namespace Orthanc
{
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
        context_.RemoveFile(it->GetUuid(), it->GetContentType());
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
    std::auto_ptr<SQLite::ITransaction> transaction_;
    bool isCommitted_;

  public:
    Transaction(ServerIndex& index) : 
      index_(index),
      isCommitted_(false)
    {
      transaction_.reset(index_.db_.StartTransaction());
      transaction_->Begin();

      assert(index_.currentStorageSize_ == index_.db_.GetTotalCompressedSize());

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
        transaction_->Commit();

        // We can remove the files once the SQLite transaction has
        // been successfully committed. Some files might have to be
        // deleted because of recycling.
        index_.listener_->CommitFilesToRemove();

        index_.currentStorageSize_ += sizeOfAddedFiles;

        assert(index_.currentStorageSize_ >= index_.listener_->GetSizeOfFilesToRemove());
        index_.currentStorageSize_ -= index_.listener_->GetSizeOfFilesToRemove();

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


  void ServerIndex::FlushThread(ServerIndex* that)
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
      boost::this_thread::sleep(boost::posix_time::seconds(1));
      count++;
      if (count < sleep)
      {
        continue;
      }

      Logging::Flush();

      boost::mutex::scoped_lock lock(that->mutex_);
      that->db_.FlushToDisk();
      count = 0;
    }

    LOG(INFO) << "Stopping the database flushing thread";
  }


  static void ComputeExpectedNumberOfInstances(IDatabaseWrapper& db,
                                               int64_t series,
                                               const DicomMap& dicomSummary)
  {
    try
    {
      const DicomValue* value;
      const DicomValue* value2;
          
      if ((value = dicomSummary.TestAndGetValue(DICOM_TAG_IMAGES_IN_ACQUISITION)) != NULL &&
          (value2 = dicomSummary.TestAndGetValue(DICOM_TAG_NUMBER_OF_TEMPORAL_POSITIONS)) != NULL)
      {
        // Patch for series with temporal positions thanks to Will Ryder
        int64_t imagesInAcquisition = boost::lexical_cast<int64_t>(value->GetContent());
        int64_t countTemporalPositions = boost::lexical_cast<int64_t>(value2->GetContent());
        std::string expected = boost::lexical_cast<std::string>(imagesInAcquisition * countTemporalPositions);
        db.SetMetadata(series, MetadataType_Series_ExpectedNumberOfInstances, expected);
      }

      else if ((value = dicomSummary.TestAndGetValue(DICOM_TAG_NUMBER_OF_SLICES)) != NULL &&
               (value2 = dicomSummary.TestAndGetValue(DICOM_TAG_NUMBER_OF_TIME_SLICES)) != NULL)
      {
        // Support of Cardio-PET images
        int64_t numberOfSlices = boost::lexical_cast<int64_t>(value->GetContent());
        int64_t numberOfTimeSlices = boost::lexical_cast<int64_t>(value2->GetContent());
        std::string expected = boost::lexical_cast<std::string>(numberOfSlices * numberOfTimeSlices);
        db.SetMetadata(series, MetadataType_Series_ExpectedNumberOfInstances, expected);
      }

      else if ((value = dicomSummary.TestAndGetValue(DICOM_TAG_CARDIAC_NUMBER_OF_IMAGES)) != NULL)
      {
        db.SetMetadata(series, MetadataType_Series_ExpectedNumberOfInstances, value->GetContent());
      }
    }
    catch (OrthancException&)
    {
    }
    catch (boost::bad_lexical_cast&)
    {
    }
  }




  bool ServerIndex::GetMetadataAsInteger(int64_t& result,
                                         int64_t id,
                                         MetadataType type)
  {
    std::string s;
    if (!db_.LookupMetadata(s, id, type))
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



  int64_t ServerIndex::CreateResource(const std::string& publicId,
                                      ResourceType type)
  {
    int64_t id = db_.CreateResource(publicId, type);

    ChangeType changeType;
    switch (type)
    {
    case ResourceType_Patient: 
      changeType = ChangeType_NewPatient; 
      break;

    case ResourceType_Study: 
      changeType = ChangeType_NewStudy; 
      break;

    case ResourceType_Series: 
      changeType = ChangeType_NewSeries; 
      break;

    case ResourceType_Instance: 
      changeType = ChangeType_NewInstance; 
      break;

    default:
      throw OrthancException(ErrorCode_InternalError);
    }

    ServerIndexChange change(changeType, type, publicId);
    db_.LogChange(id, change);

    assert(listener_.get() != NULL);
    listener_->SignalChange(change);

    return id;
  }


  ServerIndex::ServerIndex(ServerContext& context,
                           IDatabaseWrapper& db) : 
    done_(false),
    db_(db),
    maximumStorageSize_(0),
    maximumPatients_(0)
  {
    listener_.reset(new Listener(context));
    db_.SetListener(*listener_);

    currentStorageSize_ = db_.GetTotalCompressedSize();

    // Initial recycling if the parameters have changed since the last
    // execution of Orthanc
    StandaloneRecycling();

    if (db.HasFlushToDisk())
    {
      flushThread_ = boost::thread(FlushThread, this);
    }

    unstableResourcesMonitorThread_ = boost::thread(UnstableResourcesMonitorThread, this);
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



  void ServerIndex::SetInstanceMetadata(std::map<MetadataType, std::string>& instanceMetadata,
                                        int64_t instance,
                                        MetadataType metadata,
                                        const std::string& value)
  {
    db_.SetMetadata(instance, metadata, value);
    instanceMetadata[metadata] = value;
  }



  StoreStatus ServerIndex::Store(std::map<MetadataType, std::string>& instanceMetadata,
                                 DicomInstanceToStore& instanceToStore,
                                 const Attachments& attachments)
  {
    boost::mutex::scoped_lock lock(mutex_);

    const DicomMap& dicomSummary = instanceToStore.GetSummary();
    const ServerIndex::MetadataMap& metadata = instanceToStore.GetMetadata();

    instanceMetadata.clear();

    DicomInstanceHasher hasher(instanceToStore.GetSummary());

    try
    {
      Transaction t(*this);

      // Do nothing if the instance already exists
      {
        ResourceType type;
        int64_t tmp;
        if (db_.LookupResource(tmp, type, hasher.HashInstance()))
        {
          assert(type == ResourceType_Instance);
          db_.GetAllMetadata(instanceMetadata, tmp);
          return StoreStatus_AlreadyStored;
        }
      }

      // Ensure there is enough room in the storage for the new instance
      uint64_t instanceSize = 0;
      for (Attachments::const_iterator it = attachments.begin();
           it != attachments.end(); ++it)
      {
        instanceSize += it->GetCompressedSize();
      }

      Recycle(instanceSize, hasher.HashPatient());

      // Create the instance
      int64_t instance = CreateResource(hasher.HashInstance(), ResourceType_Instance);
      ServerToolbox::StoreMainDicomTags(db_, instance, ResourceType_Instance, dicomSummary);

      // Detect up to which level the patient/study/series/instance
      // hierarchy must be created
      int64_t patient = -1, study = -1, series = -1;
      bool isNewPatient = false;
      bool isNewStudy = false;
      bool isNewSeries = false;

      {
        ResourceType dummy;

        if (db_.LookupResource(series, dummy, hasher.HashSeries()))
        {
          assert(dummy == ResourceType_Series);
          // The patient, the study and the series already exist

          bool ok = (db_.LookupResource(patient, dummy, hasher.HashPatient()) &&
                     db_.LookupResource(study, dummy, hasher.HashStudy()));
          assert(ok);
        }
        else if (db_.LookupResource(study, dummy, hasher.HashStudy()))
        {
          assert(dummy == ResourceType_Study);

          // New series: The patient and the study already exist
          isNewSeries = true;

          bool ok = db_.LookupResource(patient, dummy, hasher.HashPatient());
          assert(ok);
        }
        else if (db_.LookupResource(patient, dummy, hasher.HashPatient()))
        {
          assert(dummy == ResourceType_Patient);

          // New study and series: The patient already exist
          isNewStudy = true;
          isNewSeries = true;
        }
        else
        {
          // New patient, study and series: Nothing exists
          isNewPatient = true;
          isNewStudy = true;
          isNewSeries = true;
        }
      }

      // Create the series if needed
      if (isNewSeries)
      {
        series = CreateResource(hasher.HashSeries(), ResourceType_Series);
        ServerToolbox::StoreMainDicomTags(db_, series, ResourceType_Series, dicomSummary);
      }

      // Create the study if needed
      if (isNewStudy)
      {
        study = CreateResource(hasher.HashStudy(), ResourceType_Study);
        ServerToolbox::StoreMainDicomTags(db_, study, ResourceType_Study, dicomSummary);
      }

      // Create the patient if needed
      if (isNewPatient)
      {
        patient = CreateResource(hasher.HashPatient(), ResourceType_Patient);
        ServerToolbox::StoreMainDicomTags(db_, patient, ResourceType_Patient, dicomSummary);
      }

      // Create the parent-to-child links
      db_.AttachChild(series, instance);

      if (isNewSeries)
      {
        db_.AttachChild(study, series);
      }

      if (isNewStudy)
      {
        db_.AttachChild(patient, study);
      }

      // Sanity checks
      assert(patient != -1);
      assert(study != -1);
      assert(series != -1);
      assert(instance != -1);

      // Attach the files to the newly created instance
      for (Attachments::const_iterator it = attachments.begin();
           it != attachments.end(); ++it)
      {
        db_.AddAttachment(instance, *it);
      }

      // Attach the user-specified metadata
      for (MetadataMap::const_iterator 
             it = metadata.begin(); it != metadata.end(); ++it)
      {
        switch (it->first.first)
        {
          case ResourceType_Patient:
            db_.SetMetadata(patient, it->first.second, it->second);
            break;

          case ResourceType_Study:
            db_.SetMetadata(study, it->first.second, it->second);
            break;

          case ResourceType_Series:
            db_.SetMetadata(series, it->first.second, it->second);
            break;

          case ResourceType_Instance:
            SetInstanceMetadata(instanceMetadata, instance, it->first.second, it->second);
            break;

          default:
            throw OrthancException(ErrorCode_ParameterOutOfRange);
        }
      }

      // Attach the auto-computed metadata for the patient/study/series levels
      std::string now = SystemToolbox::GetNowIsoString(true /* use UTC time (not local time) */);
      db_.SetMetadata(series, MetadataType_LastUpdate, now);
      db_.SetMetadata(study, MetadataType_LastUpdate, now);
      db_.SetMetadata(patient, MetadataType_LastUpdate, now);

      // Attach the auto-computed metadata for the instance level,
      // reflecting these additions into the input metadata map
      SetInstanceMetadata(instanceMetadata, instance, MetadataType_Instance_ReceptionDate, now);
      SetInstanceMetadata(instanceMetadata, instance, MetadataType_Instance_RemoteAet, instanceToStore.GetRemoteAet());
      SetInstanceMetadata(instanceMetadata, instance, MetadataType_Instance_Origin, 
                          EnumerationToString(instanceToStore.GetRequestOrigin()));
        
      {
        std::string s;
        if (instanceToStore.LookupTransferSyntax(s))
        {
          SetInstanceMetadata(instanceMetadata, instance, MetadataType_Instance_TransferSyntax, s);
        }
      }

      const DicomValue* value;
      if ((value = dicomSummary.TestAndGetValue(DICOM_TAG_SOP_CLASS_UID)) != NULL &&
          !value->IsNull() &&
          !value->IsBinary())
      {
        SetInstanceMetadata(instanceMetadata, instance, MetadataType_Instance_SopClassUid, value->GetContent());
      }

      if ((value = dicomSummary.TestAndGetValue(DICOM_TAG_INSTANCE_NUMBER)) != NULL ||
          (value = dicomSummary.TestAndGetValue(DICOM_TAG_IMAGE_INDEX)) != NULL)
      {
        if (!value->IsNull() && 
            !value->IsBinary())
        {
          SetInstanceMetadata(instanceMetadata, instance, MetadataType_Instance_IndexInSeries, value->GetContent());
        }
      }

      // Check whether the series of this new instance is now completed
      if (isNewSeries)
      {
        ComputeExpectedNumberOfInstances(db_, series, dicomSummary);
      }

      SeriesStatus seriesStatus = GetSeriesStatus(series);
      if (seriesStatus == SeriesStatus_Complete)
      {
        LogChange(series, ChangeType_CompletedSeries, ResourceType_Series, hasher.HashSeries());
      }

      // Mark the parent resources of this instance as unstable
      MarkAsUnstable(series, ResourceType_Series, hasher.HashSeries());
      MarkAsUnstable(study, ResourceType_Study, hasher.HashStudy());
      MarkAsUnstable(patient, ResourceType_Patient, hasher.HashPatient());

      t.Commit(instanceSize);

      return StoreStatus_Success;
    }
    catch (OrthancException& e)
    {
      LOG(ERROR) << "EXCEPTION [" << e.What() << "]";
    }

    return StoreStatus_Failure;
  }


  void ServerIndex::ComputeStatistics(Json::Value& target)
  {
    boost::mutex::scoped_lock lock(mutex_);
    target = Json::objectValue;

    uint64_t cs = currentStorageSize_;
    assert(cs == db_.GetTotalCompressedSize());
    uint64_t us = db_.GetTotalUncompressedSize();
    target["TotalDiskSize"] = boost::lexical_cast<std::string>(cs);
    target["TotalUncompressedSize"] = boost::lexical_cast<std::string>(us);
    target["TotalDiskSizeMB"] = static_cast<unsigned int>(cs / MEGA_BYTES);
    target["TotalUncompressedSizeMB"] = static_cast<unsigned int>(us / MEGA_BYTES);

    target["CountPatients"] = static_cast<unsigned int>(db_.GetResourceCount(ResourceType_Patient));
    target["CountStudies"] = static_cast<unsigned int>(db_.GetResourceCount(ResourceType_Study));
    target["CountSeries"] = static_cast<unsigned int>(db_.GetResourceCount(ResourceType_Series));
    target["CountInstances"] = static_cast<unsigned int>(db_.GetResourceCount(ResourceType_Instance));
  }          



  SeriesStatus ServerIndex::GetSeriesStatus(int64_t id)
  {
    // Get the expected number of instances in this series (from the metadata)
    int64_t expected;
    if (!GetMetadataAsInteger(expected, id, MetadataType_Series_ExpectedNumberOfInstances))
    {
      return SeriesStatus_Unknown;
    }

    // Loop over the instances of this series
    std::list<int64_t> children;
    db_.GetChildrenInternalId(children, id);

    std::set<int64_t> instances;
    for (std::list<int64_t>::const_iterator 
           it = children.begin(); it != children.end(); ++it)
    {
      // Get the index of this instance in the series
      int64_t index;
      if (!GetMetadataAsInteger(index, *it, MetadataType_Instance_IndexInSeries))
      {
        return SeriesStatus_Unknown;
      }

      if (!(index > 0 && index <= expected))
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

    if (static_cast<int64_t>(instances.size()) == expected)
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
    if (!db_.LookupResource(id, type, publicId) ||
        type != expectedType)
    {
      return false;
    }

    // Find the parent resource (if it exists)
    if (type != ResourceType_Patient)
    {
      int64_t parentId;
      if (!db_.LookupParent(parentId, id))
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      std::string parent = db_.GetPublicId(parentId);

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
        result["Status"] = EnumerationToString(GetSeriesStatus(id));

        int64_t i;
        if (GetMetadataAsInteger(i, id, MetadataType_Series_ExpectedNumberOfInstances))
          result["ExpectedNumberOfInstances"] = static_cast<int>(i);
        else
          result["ExpectedNumberOfInstances"] = Json::nullValue;

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
        if (GetMetadataAsInteger(i, id, MetadataType_Instance_IndexInSeries))
          result["IndexInSeries"] = static_cast<int>(i);
        else
          result["IndexInSeries"] = Json::nullValue;

        break;
      }

      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    // Record the remaining information
    result["ID"] = publicId;
    MainDicomTagsToJson(result, id, type);

    std::string tmp;

    if (db_.LookupMetadata(tmp, id, MetadataType_AnonymizedFrom))
    {
      result["AnonymizedFrom"] = tmp;
    }

    if (db_.LookupMetadata(tmp, id, MetadataType_ModifiedFrom))
    {
      result["ModifiedFrom"] = tmp;
    }

    if (type == ResourceType_Patient ||
        type == ResourceType_Study ||
        type == ResourceType_Series)
    {
      result["IsStable"] = !unstableResources_.Contains(id);

      if (db_.LookupMetadata(tmp, id, MetadataType_LastUpdate))
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
                        int64_t since)
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

    int64_t last = (log.empty() ? since : log.back().GetSeq());
    target["Last"] = static_cast<int>(last);
  }


  void ServerIndex::GetChanges(Json::Value& target,
                               int64_t since,                               
                               unsigned int maxResults)
  {
    std::list<ServerIndexChange> changes;
    bool done;

    {
      boost::mutex::scoped_lock lock(mutex_);
      db_.GetChanges(changes, done, since, maxResults);
    }

    FormatLog(target, changes, "Changes", done, since);
  }


  void ServerIndex::GetLastChange(Json::Value& target)
  {
    std::list<ServerIndexChange> changes;

    {
      boost::mutex::scoped_lock lock(mutex_);
      db_.GetLastChange(changes);
    }

    FormatLog(target, changes, "Changes", true, 0);
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
      throw OrthancException(ErrorCode_InternalError);
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

    FormatLog(target, exported, "Exports", done, since);
  }


  void ServerIndex::GetLastExportedResource(Json::Value& target)
  {
    std::list<ExportedResource> exported;

    {
      boost::mutex::scoped_lock lock(mutex_);
      db_.GetLastExportedResource(exported);
    }

    FormatLog(target, exported, "Exports", true, 0);
  }


  bool ServerIndex::IsRecyclingNeeded(uint64_t instanceSize)
  {
    if (maximumStorageSize_ != 0)
    {
      uint64_t currentSize = currentStorageSize_ - listener_->GetSizeOfFilesToRemove();
      assert(db_.GetTotalCompressedSize() == currentSize);

      if (currentSize + instanceSize > maximumStorageSize_)
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


  void ServerIndex::ListAvailableMetadata(std::list<MetadataType>& target,
                                          const std::string& publicId)
  {
    boost::mutex::scoped_lock lock(mutex_);

    ResourceType rtype;
    int64_t id;
    if (!db_.LookupResource(id, rtype, publicId))
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }

    db_.ListAvailableMetadata(target, id);
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
    db_.ClearChanges();
  }

  void ServerIndex::DeleteExportedResources()
  {
    boost::mutex::scoped_lock lock(mutex_);
    db_.ClearExportedResources();
  }


  void ServerIndex::GetStatisticsInternal(/* out */ uint64_t& compressedSize, 
                                          /* out */ uint64_t& uncompressedSize, 
                                          /* out */ unsigned int& countStudies, 
                                          /* out */ unsigned int& countSeries, 
                                          /* out */ unsigned int& countInstances, 
                                          /* in  */ int64_t id,
                                          /* in  */ ResourceType type)
  {
    std::stack<int64_t> toExplore;
    toExplore.push(id);

    countInstances = 0;
    countSeries = 0;
    countStudies = 0;
    compressedSize = 0;
    uncompressedSize = 0;

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
          compressedSize += attachment.GetCompressedSize();
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



  void ServerIndex::GetStatistics(Json::Value& target,
                                  const std::string& publicId)
  {
    boost::mutex::scoped_lock lock(mutex_);

    ResourceType type;
    int64_t top;
    if (!db_.LookupResource(top, type, publicId))
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }

    uint64_t uncompressedSize;
    uint64_t compressedSize;
    unsigned int countStudies;
    unsigned int countSeries;
    unsigned int countInstances;
    GetStatisticsInternal(compressedSize, uncompressedSize, countStudies, 
                          countSeries, countInstances, top, type);

    target = Json::objectValue;
    target["DiskSize"] = boost::lexical_cast<std::string>(compressedSize);
    target["DiskSizeMB"] = static_cast<unsigned int>(compressedSize / MEGA_BYTES);
    target["UncompressedSize"] = boost::lexical_cast<std::string>(uncompressedSize);
    target["UncompressedSizeMB"] = static_cast<unsigned int>(uncompressedSize / MEGA_BYTES);

    switch (type)
    {
      // Do NOT add "break" below this point!
      case ResourceType_Patient:
        target["CountStudies"] = countStudies;

      case ResourceType_Study:
        target["CountSeries"] = countSeries;

      case ResourceType_Series:
        target["CountInstances"] = countInstances;

      case ResourceType_Instance:
      default:
        break;
    }
  }


  void ServerIndex::GetStatistics(/* out */ uint64_t& compressedSize, 
                                  /* out */ uint64_t& uncompressedSize, 
                                  /* out */ unsigned int& countStudies, 
                                  /* out */ unsigned int& countSeries, 
                                  /* out */ unsigned int& countInstances, 
                                  const std::string& publicId)
  {
    boost::mutex::scoped_lock lock(mutex_);

    ResourceType type;
    int64_t top;
    if (!db_.LookupResource(top, type, publicId))
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }

    GetStatisticsInternal(compressedSize, uncompressedSize, countStudies, 
                          countSeries, countInstances, top, type);    
  }


  void ServerIndex::UnstableResourcesMonitorThread(ServerIndex* that)
  {
    int stableAge = Configuration::GetGlobalUnsignedIntegerParameter("StableAge", 60);
    if (stableAge <= 0)
    {
      stableAge = 60;
    }

    LOG(INFO) << "Starting the monitor for stable resources (stable age = " << stableAge << ")";

    while (!that->done_)
    {
      // Check for stable resources each second
      boost::this_thread::sleep(boost::posix_time::seconds(1));

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



  void ServerIndex::LookupIdentifierExact(std::list<std::string>& result,
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

    boost::mutex::scoped_lock lock(mutex_);

    LookupIdentifierQuery query(level);
    query.AddConstraint(tag, IdentifierConstraintType_Equal, value);
    query.Apply(result, db_);
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


  bool ServerIndex::GetMetadata(Json::Value& target,
                                const std::string& publicId)
  {
    boost::mutex::scoped_lock lock(mutex_);

    target = Json::objectValue;

    ResourceType type;
    int64_t id;
    if (!db_.LookupResource(id, type, publicId))
    {
      return false;
    }

    std::list<MetadataType> metadata;
    db_.ListAvailableMetadata(metadata, id);

    for (std::list<MetadataType>::const_iterator
           it = metadata.begin(); it != metadata.end(); ++it)
    {
      std::string key = EnumerationToString(*it);

      std::string value;
      if (!db_.LookupMetadata(value, id, *it))
      {
        value.clear();
      }

      target[key] = value;
    }

    return true;
  }


  void ServerIndex::SetGlobalProperty(GlobalProperty property,
                                      const std::string& value)
  {
    boost::mutex::scoped_lock lock(mutex_);
    db_.SetGlobalProperty(property, value);
  }


  std::string ServerIndex::GetGlobalProperty(GlobalProperty property,
                                             const std::string& defaultValue)
  {
    boost::mutex::scoped_lock lock(mutex_);

    std::string value;
    if (db_.LookupGlobalProperty(value, property))
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


  void ServerIndex::FindCandidates(std::vector<std::string>& resources,
                                   std::vector<std::string>& instances,
                                   const ::Orthanc::LookupResource& lookup)
  {
    boost::mutex::scoped_lock lock(mutex_);
   
    std::list<int64_t> tmp;
    lookup.FindCandidates(tmp, db_);

    resources.resize(tmp.size());
    instances.resize(tmp.size());

    size_t pos = 0;
    for (std::list<int64_t>::const_iterator
           it = tmp.begin(); it != tmp.end(); ++it, pos++)
    {
      assert(db_.GetResourceType(*it) == lookup.GetLevel());
      
      int64_t instance;
      if (!ServerToolbox::FindOneChildInstance(instance, db_, *it, lookup.GetLevel()))
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      resources[pos] = db_.GetPublicId(*it);
      instances[pos] = db_.GetPublicId(instance);
    }
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

      ServerToolbox::StoreMainDicomTags(db_, patient, ResourceType_Patient, summary);
      ServerToolbox::StoreMainDicomTags(db_, study, ResourceType_Study, summary);
      ServerToolbox::StoreMainDicomTags(db_, series, ResourceType_Series, summary);
      ServerToolbox::StoreMainDicomTags(db_, instance, ResourceType_Instance, summary);

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
}
