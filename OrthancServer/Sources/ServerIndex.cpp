/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "PrecompiledHeadersServer.h"
#include "ServerIndex.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "../../OrthancFramework/Sources/Logging.h"
#include "../../OrthancFramework/Sources/Toolbox.h"

#include "OrthancConfiguration.h"
#include "ServerContext.h"
#include "ServerIndexChange.h"
#include "ServerToolbox.h"


static const uint64_t MEGA_BYTES = 1024 * 1024;

namespace Orthanc
{
  class ServerIndex::TransactionContext : public StatelessDatabaseOperations::ITransactionContext
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
    uint64_t sizeOfAddedAttachments_;

    void Reset()
    {
      sizeOfFilesToRemove_ = 0;
      hasRemainingLevel_ = false;
      remainingType_ = ResourceType_Instance;  // dummy initialization
      pendingFilesToRemove_.clear();
      pendingChanges_.clear();
      sizeOfAddedAttachments_ = 0;
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

  public:
    explicit TransactionContext(ServerContext& context) :
      context_(context)
    {
      Reset();
      assert(ResourceType_Patient < ResourceType_Study &&
             ResourceType_Study < ResourceType_Series &&
             ResourceType_Series < ResourceType_Instance);
    }

    virtual void SignalRemainingAncestor(ResourceType parentType,
                                         const std::string& publicId) ORTHANC_OVERRIDE
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

    virtual void SignalAttachmentDeleted(const FileInfo& info) ORTHANC_OVERRIDE
    {
      assert(Toolbox::IsUuid(info.GetUuid()));
      pendingFilesToRemove_.push_back(FileToRemove(info));
      sizeOfFilesToRemove_ += info.GetCompressedSize();
    }

    virtual void SignalResourceDeleted(ResourceType type,
                                       const std::string& publicId) ORTHANC_OVERRIDE
    {
      SignalChange(ServerIndexChange(ChangeType_Deleted, type, publicId));
    }

    virtual void SignalChange(const ServerIndexChange& change) ORTHANC_OVERRIDE
    {
      LOG(TRACE) << "Change related to resource " << change.GetPublicId() << " of type " 
                 << EnumerationToString(change.GetResourceType()) << ": " 
                 << EnumerationToString(change.GetChangeType());

      pendingChanges_.push_back(change);
    }

    virtual void SignalAttachmentsAdded(uint64_t compressedSize) ORTHANC_OVERRIDE
    {
      sizeOfAddedAttachments_ += compressedSize;
    }

    virtual bool LookupRemainingLevel(std::string& remainingPublicId /* out */,
                                      ResourceType& remainingLevel   /* out */) ORTHANC_OVERRIDE
    {
      if (hasRemainingLevel_)
      {
        remainingPublicId = remainingPublicId_;
        remainingLevel = remainingType_;
        return true;
      }
      else
      {
        return false;
      }        
    };

    virtual void MarkAsUnstable(int64_t id,
                                Orthanc::ResourceType type,
                                const std::string& publicId) ORTHANC_OVERRIDE
    {
      context_.GetIndex().MarkAsUnstable(id, type, publicId);
    }

    virtual bool IsUnstableResource(int64_t id) ORTHANC_OVERRIDE
    {
      return context_.GetIndex().IsUnstableResource(id);
    }

    virtual void Commit() ORTHANC_OVERRIDE
    {
      // We can remove the files once the SQLite transaction has
      // been successfully committed. Some files might have to be
      // deleted because of recycling.
      CommitFilesToRemove();
      
      // Send all the pending changes to the Orthanc plugins
      CommitChanges();
    }

    virtual int64_t GetCompressedSizeDelta() ORTHANC_OVERRIDE
    {
      return (static_cast<int64_t>(sizeOfAddedAttachments_) -
              static_cast<int64_t>(sizeOfFilesToRemove_));
    }
  };


  class ServerIndex::TransactionContextFactory : public ITransactionContextFactory
  {
  private:
    ServerContext& context_;
      
  public:
    explicit TransactionContextFactory(ServerContext& context) :
      context_(context)
    {
    }

    virtual ITransactionContext* Create()
    {
      // There can be concurrent calls to this method, which is not an
      // issue because we simply create an object
      return new TransactionContext(context_);
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


  void ServerIndex::FlushThread(ServerIndex* that,
                                unsigned int threadSleepGranularityMilliseconds)
  {
    // By default, wait for 10 seconds before flushing
    static const unsigned int SLEEP_SECONDS = 10;

    if (threadSleepGranularityMilliseconds > 1000)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    LOG(INFO) << "Starting the database flushing thread (sleep = " << SLEEP_SECONDS << " seconds)";

    unsigned int count = 0;
    unsigned int countThreshold = (1000 * SLEEP_SECONDS) / threadSleepGranularityMilliseconds;

    while (!that->done_)
    {
      boost::this_thread::sleep(boost::posix_time::milliseconds(threadSleepGranularityMilliseconds));
      count++;
      
      if (count >= countThreshold)
      {
        Logging::Flush();
        that->FlushToDisk();
        
        count = 0;
      }
    }

    LOG(INFO) << "Stopping the database flushing thread";
  }


  bool ServerIndex::IsUnstableResource(int64_t id)
  {
    boost::mutex::scoped_lock lock(monitoringMutex_);
    return unstableResources_.Contains(id);
  }


  ServerIndex::ServerIndex(ServerContext& context,
                           IDatabaseWrapper& db,
                           unsigned int threadSleepGranularityMilliseconds) :
    StatelessDatabaseOperations(db),
    done_(false),
    maximumStorageSize_(0),
    maximumPatients_(0)
  {
    SetTransactionContextFactory(new TransactionContextFactory(context));

    // Initial recycling if the parameters have changed since the last
    // execution of Orthanc
    StandaloneRecycling(maximumStorageSize_, maximumPatients_);

    if (HasFlushToDisk())
    {
      flushThread_ = boost::thread(FlushThread, this, threadSleepGranularityMilliseconds);
    }

    unstableResourcesMonitorThread_ = boost::thread
      (UnstableResourcesMonitorThread, this, threadSleepGranularityMilliseconds);
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

      if (flushThread_.joinable())
      {
        flushThread_.join();
      }

      if (unstableResourcesMonitorThread_.joinable())
      {
        unstableResourcesMonitorThread_.join();
      }
    }
  }


  void ServerIndex::SetMaximumPatientCount(unsigned int count) 
  {
    {
      boost::mutex::scoped_lock lock(monitoringMutex_);
      maximumPatients_ = count;
      
      if (count == 0)
      {
        LOG(WARNING) << "No limit on the number of stored patients";
      }
      else
      {
        LOG(WARNING) << "At most " << count << " patients will be stored";
      }
    }

    StandaloneRecycling(maximumStorageSize_, maximumPatients_);
  }

  
  void ServerIndex::SetMaximumStorageSize(uint64_t size) 
  {
    {
      boost::mutex::scoped_lock lock(monitoringMutex_);
      maximumStorageSize_ = size;
      
      if (size == 0)
      {
        LOG(WARNING) << "No limit on the size of the storage area";
      }
      else
      {
        LOG(WARNING) << "At most " << (size / MEGA_BYTES) << "MB will be used for the storage area";
      }
    }

    StandaloneRecycling(maximumStorageSize_, maximumPatients_);
  }


  void ServerIndex::UnstableResourcesMonitorThread(ServerIndex* that,
                                                   unsigned int threadSleepGranularityMilliseconds)
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
      boost::this_thread::sleep(boost::posix_time::milliseconds(threadSleepGranularityMilliseconds));

      for (;;)
      {
        UnstableResourcePayload stableResource;
        int64_t stableId;

        {      
          boost::mutex::scoped_lock lock(that->monitoringMutex_);

          if (!that->unstableResources_.IsEmpty() &&
              that->unstableResources_.GetOldestPayload().GetAge() > static_cast<unsigned int>(stableAge))
          {
            // This DICOM resource has not received any new instance for
            // some time. It can be considered as stable.
            stableId = that->unstableResources_.RemoveOldest(stableResource);
            //LOG(TRACE) << "Stable resource: " << EnumerationToString(stableResource.GetResourceType()) << " " << stableId;
          }
          else
          {
            // No more stable DICOM resource, leave the internal loop
            break;
          }
        }

        try
        {
          /**
           * WARNING: Don't protect the calls to "LogChange()" using
           * "monitoringMutex_", as this could lead to deadlocks in
           * other threads (typically, if "Store()" is being running in
           * another thread, which leads to calls to "MarkAsUnstable()",
           * which leads to two lockings of "monitoringMutex_").
           **/
          switch (stableResource.GetResourceType())
          {
            case ResourceType_Patient:
              that->LogChange(stableId, ChangeType_StablePatient, stableResource.GetPublicId(), ResourceType_Patient);
              break;
            
            case ResourceType_Study:
              that->LogChange(stableId, ChangeType_StableStudy, stableResource.GetPublicId(), ResourceType_Study);
              break;
            
            case ResourceType_Series:
              that->LogChange(stableId, ChangeType_StableSeries, stableResource.GetPublicId(), ResourceType_Series);
              break;
            
            default:
              throw OrthancException(ErrorCode_InternalError);
          }
        }
        catch (OrthancException& e)
        {
          LOG(ERROR) << "Cannot log a change about a stable resource into the database";
        }          
      }
    }

    LOG(INFO) << "Closing the monitor thread for stable resources";
  }
  

  void ServerIndex::MarkAsUnstable(int64_t id,
                                   Orthanc::ResourceType type,
                                   const std::string& publicId)
  {
    assert(type == Orthanc::ResourceType_Patient ||
           type == Orthanc::ResourceType_Study ||
           type == Orthanc::ResourceType_Series);

    {
      boost::mutex::scoped_lock lock(monitoringMutex_);
      UnstableResourcePayload payload(type, publicId);
      unstableResources_.AddOrMakeMostRecent(id, payload);
      //LOG(INFO) << "Unstable resource: " << EnumerationToString(type) << " " << id;
    }
  }


  StoreStatus ServerIndex::Store(std::map<MetadataType, std::string>& instanceMetadata,
                                 const DicomMap& dicomSummary,
                                 const ServerIndex::Attachments& attachments,
                                 const ServerIndex::MetadataMap& metadata,
                                 const DicomInstanceOrigin& origin,
                                 bool overwrite,
                                 bool hasTransferSyntax,
                                 DicomTransferSyntax transferSyntax,
                                 bool hasPixelDataOffset,
                                 uint64_t pixelDataOffset,
                                 bool isReconstruct)
  {
    uint64_t maximumStorageSize;
    unsigned int maximumPatients;
    
    {
      boost::mutex::scoped_lock lock(monitoringMutex_);
      maximumStorageSize = maximumStorageSize_;
      maximumPatients = maximumPatients_;
    }

    return StatelessDatabaseOperations::Store(
      instanceMetadata, dicomSummary, attachments, metadata, origin, overwrite, hasTransferSyntax,
      transferSyntax, hasPixelDataOffset, pixelDataOffset, maximumStorageSize, maximumPatients, isReconstruct);
  }

  
  StoreStatus ServerIndex::AddAttachment(int64_t& newRevision,
                                         const FileInfo& attachment,
                                         const std::string& publicId,
                                         bool hasOldRevision,
                                         int64_t oldRevision,
                                         const std::string& oldMD5)
  {
    uint64_t maximumStorageSize;
    unsigned int maximumPatients;
    
    {
      boost::mutex::scoped_lock lock(monitoringMutex_);
      maximumStorageSize = maximumStorageSize_;
      maximumPatients = maximumPatients_;
    }

    return StatelessDatabaseOperations::AddAttachment(
      newRevision, attachment, publicId, maximumStorageSize, maximumPatients,
      hasOldRevision, oldRevision, oldMD5);
  }
}
