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
#include "ServerContext.h"

#include "../Core/Cache/SharedArchive.h"
#include "../Core/DicomParsing/FromDcmtkBridge.h"
#include "../Core/FileStorage/StorageAccessor.h"
#include "../Core/HttpServer/FilesystemHttpSender.h"
#include "../Core/HttpServer/HttpStreamTranscoder.h"
#include "../Core/JobsEngine/SetOfInstancesJob.h"
#include "../Core/Logging.h"
#include "../Core/MetricsRegistry.h"
#include "../Plugins/Engine/OrthancPlugins.h"

#include "OrthancConfiguration.h"
#include "OrthancRestApi/OrthancRestApi.h"
#include "Search/DatabaseLookup.h"
#include "ServerJobs/OrthancJobUnserializer.h"
#include "ServerToolbox.h"
#include "StorageCommitmentReports.h"

#include <EmbeddedResources.h>
#include <dcmtk/dcmdata/dcfilefo.h>



#define ENABLE_DICOM_CACHE  1

static const size_t DICOM_CACHE_SIZE = 2;

/**
 * IMPORTANT: We make the assumption that the same instance of
 * FileStorage can be accessed from multiple threads. This seems OK
 * since the filesystem implements the required locking mechanisms,
 * but maybe a read-writer lock on the "FileStorage" could be
 * useful. Conversely, "ServerIndex" already implements mutex-based
 * locking.
 **/

namespace Orthanc
{
  void ServerContext::ChangeThread(ServerContext* that,
                                   unsigned int sleepDelay)
  {
    while (!that->done_)
    {
      std::unique_ptr<IDynamicObject> obj(that->pendingChanges_.Dequeue(sleepDelay));
        
      if (obj.get() != NULL)
      {
        const ServerIndexChange& change = dynamic_cast<const ServerIndexChange&>(*obj.get());

        boost::recursive_mutex::scoped_lock lock(that->listenersMutex_);
        for (ServerListeners::iterator it = that->listeners_.begin(); 
             it != that->listeners_.end(); ++it)
        {
          try
          {
            try
            {
              it->GetListener().SignalChange(change);
            }
            catch (std::bad_alloc&)
            {
              LOG(ERROR) << "Not enough memory while signaling a change";
            }
            catch (...)
            {
              throw OrthancException(ErrorCode_InternalError);
            }
          }
          catch (OrthancException& e)
          {
            LOG(ERROR) << "Error in the " << it->GetDescription() 
                       << " callback while signaling a change: " << e.What()
                       << " (code " << e.GetErrorCode() << ")";
          }
        }
      }
    }
  }


  void ServerContext::SaveJobsThread(ServerContext* that,
                                     unsigned int sleepDelay)
  {
    static const boost::posix_time::time_duration PERIODICITY =
      boost::posix_time::seconds(10);
    
    boost::posix_time::ptime next =
      boost::posix_time::microsec_clock::universal_time() + PERIODICITY;
    
    while (!that->done_)
    {
      boost::this_thread::sleep(boost::posix_time::milliseconds(sleepDelay));

      if (that->haveJobsChanged_ ||
          boost::posix_time::microsec_clock::universal_time() >= next)
      {
        that->haveJobsChanged_ = false;
        that->SaveJobsEngine();
        next = boost::posix_time::microsec_clock::universal_time() + PERIODICITY;
      }
    }
  }
  

  void ServerContext::SignalJobSubmitted(const std::string& jobId)
  {
    haveJobsChanged_ = true;
    mainLua_.SignalJobSubmitted(jobId);
  }
  

  void ServerContext::SignalJobSuccess(const std::string& jobId)
  {
    haveJobsChanged_ = true;
    mainLua_.SignalJobSuccess(jobId);
  }

  
  void ServerContext::SignalJobFailure(const std::string& jobId)
  {
    haveJobsChanged_ = true;
    mainLua_.SignalJobFailure(jobId);
  }


  void ServerContext::SetupJobsEngine(bool unitTesting,
                                      bool loadJobsFromDatabase)
  {
    if (loadJobsFromDatabase)
    {
      std::string serialized;
      if (index_.LookupGlobalProperty(serialized, GlobalProperty_JobsRegistry))
      {
        LOG(WARNING) << "Reloading the jobs from the last execution of Orthanc";
        OrthancJobUnserializer unserializer(*this);

        try
        {
          jobsEngine_.LoadRegistryFromString(unserializer, serialized);
        }
        catch (OrthancException& e)
        {
          LOG(WARNING) << "Cannot unserialize the jobs engine, starting anyway: " << e.What();
        }
      }
      else
      {
        LOG(INFO) << "The last execution of Orthanc has archived no job";
      }
    }
    else
    {
      LOG(INFO) << "Not reloading the jobs from the last execution of Orthanc";
    }

    jobsEngine_.GetRegistry().SetObserver(*this);
    jobsEngine_.Start();
    isJobsEngineUnserialized_ = true;

    saveJobsThread_ = boost::thread(SaveJobsThread, this, (unitTesting ? 20 : 100));
  }


  void ServerContext::SaveJobsEngine()
  {
    if (saveJobs_)
    {
      VLOG(1) << "Serializing the content of the jobs engine";
    
      try
      {
        Json::Value value;
        jobsEngine_.GetRegistry().Serialize(value);

        Json::FastWriter writer;
        std::string serialized = writer.write(value);

        index_.SetGlobalProperty(GlobalProperty_JobsRegistry, serialized);
      }
      catch (OrthancException& e)
      {
        LOG(ERROR) << "Cannot serialize the jobs engine: " << e.What();
      }
    }
  }


  ServerContext::ServerContext(IDatabaseWrapper& database,
                               IStorageArea& area,
                               bool unitTesting,
                               size_t maxCompletedJobs) :
    index_(*this, database, (unitTesting ? 20 : 500)),
    area_(area),
    compressionEnabled_(false),
    storeMD5_(true),
    provider_(*this),
    dicomCache_(provider_, DICOM_CACHE_SIZE),
    mainLua_(*this),
    filterLua_(*this),
    luaListener_(*this),
    jobsEngine_(maxCompletedJobs),
#if ORTHANC_ENABLE_PLUGINS == 1
    plugins_(NULL),
#endif
    done_(false),
    haveJobsChanged_(false),
    isJobsEngineUnserialized_(false),
    metricsRegistry_(new MetricsRegistry),
    isHttpServerSecure_(true),
    isExecuteLuaEnabled_(false)
  {
    {
      OrthancConfiguration::ReaderLock lock;

      queryRetrieveArchive_.reset(
        new SharedArchive(lock.GetConfiguration().GetUnsignedIntegerParameter("QueryRetrieveSize", 100)));
      mediaArchive_.reset(
        new SharedArchive(lock.GetConfiguration().GetUnsignedIntegerParameter("MediaArchiveSize", 1)));
      defaultLocalAet_ = lock.GetConfiguration().GetStringParameter("DicomAet", "ORTHANC");
      jobsEngine_.SetWorkersCount(lock.GetConfiguration().GetUnsignedIntegerParameter("ConcurrentJobs", 2));
      saveJobs_ = lock.GetConfiguration().GetBooleanParameter("SaveJobs", true);
      metricsRegistry_->SetEnabled(lock.GetConfiguration().GetBooleanParameter("MetricsEnabled", true));

      // New configuration options in Orthanc 1.5.1
      findStorageAccessMode_ = StringToFindStorageAccessMode(lock.GetConfiguration().GetStringParameter("StorageAccessOnFind", "Always"));
      limitFindInstances_ = lock.GetConfiguration().GetUnsignedIntegerParameter("LimitFindInstances", 0);
      limitFindResults_ = lock.GetConfiguration().GetUnsignedIntegerParameter("LimitFindResults", 0);

      // New configuration option in Orthanc 1.6.0
      storageCommitmentReports_.reset(new StorageCommitmentReports(lock.GetConfiguration().GetUnsignedIntegerParameter("StorageCommitmentReportsSize", 100)));
    }

    jobsEngine_.SetThreadSleep(unitTesting ? 20 : 200);

    listeners_.push_back(ServerListener(luaListener_, "Lua"));
    changeThread_ = boost::thread(ChangeThread, this, (unitTesting ? 20 : 100));
  }


  
  ServerContext::~ServerContext()
  {
    if (!done_)
    {
      LOG(ERROR) << "INTERNAL ERROR: ServerContext::Stop() should be invoked manually to avoid mess in the destruction order!";
      Stop();
    }
  }


  void ServerContext::Stop()
  {
    if (!done_)
    {
      {
        boost::recursive_mutex::scoped_lock lock(listenersMutex_);
        listeners_.clear();
      }

      done_ = true;

      if (changeThread_.joinable())
      {
        changeThread_.join();
      }

      if (saveJobsThread_.joinable())
      {
        saveJobsThread_.join();
      }

      jobsEngine_.GetRegistry().ResetObserver();

      if (isJobsEngineUnserialized_)
      {
        // Avoid losing jobs if the JobsRegistry cannot be unserialized
        SaveJobsEngine();
      }

      // Do not change the order below!
      jobsEngine_.Stop();
      index_.Stop();
    }
  }


  void ServerContext::SetCompressionEnabled(bool enabled)
  {
    if (enabled)
      LOG(WARNING) << "Disk compression is enabled";
    else
      LOG(WARNING) << "Disk compression is disabled";

    compressionEnabled_ = enabled;
  }


  void ServerContext::RemoveFile(const std::string& fileUuid,
                                 FileContentType type)
  {
    StorageAccessor accessor(area_, GetMetricsRegistry());
    accessor.Remove(fileUuid, type);
  }


  StoreStatus ServerContext::Store(std::string& resultPublicId,
                                   DicomInstanceToStore& dicom)
  {
    try
    {
      MetricsRegistry::Timer timer(GetMetricsRegistry(), "orthanc_store_dicom_duration_ms");
      StorageAccessor accessor(area_, GetMetricsRegistry());

      resultPublicId = dicom.GetHasher().HashInstance();

      Json::Value simplifiedTags;
      ServerToolbox::SimplifyTags(simplifiedTags, dicom.GetJson(), DicomToJsonFormat_Human);

      // Test if the instance must be filtered out
      bool accepted = true;

      {
        boost::recursive_mutex::scoped_lock lock(listenersMutex_);

        for (ServerListeners::iterator it = listeners_.begin(); it != listeners_.end(); ++it)
        {
          try
          {
            if (!it->GetListener().FilterIncomingInstance(dicom, simplifiedTags))
            {
              accepted = false;
              break;
            }
          }
          catch (OrthancException& e)
          {
            LOG(ERROR) << "Error in the " << it->GetDescription() 
                       << " callback while receiving an instance: " << e.What()
                       << " (code " << e.GetErrorCode() << ")";
            throw;
          }
        }
      }

      if (!accepted)
      {
        LOG(INFO) << "An incoming instance has been discarded by the filter";
        return StoreStatus_FilteredOut;
      }

      {
        // Remove the file from the DicomCache (useful if
        // "OverwriteInstances" is set to "true")
        boost::mutex::scoped_lock lock(dicomCacheMutex_);
        dicomCache_.Invalidate(resultPublicId);
      }

      // TODO Should we use "gzip" instead?
      CompressionType compression = (compressionEnabled_ ? CompressionType_ZlibWithSize : CompressionType_None);

      FileInfo dicomInfo = accessor.Write(dicom.GetBufferData(), dicom.GetBufferSize(), 
                                          FileContentType_Dicom, compression, storeMD5_);
      FileInfo jsonInfo = accessor.Write(dicom.GetJson().toStyledString(), 
                                         FileContentType_DicomAsJson, compression, storeMD5_);

      ServerIndex::Attachments attachments;
      attachments.push_back(dicomInfo);
      attachments.push_back(jsonInfo);

      typedef std::map<MetadataType, std::string>  InstanceMetadata;
      InstanceMetadata  instanceMetadata;
      StoreStatus status = index_.Store(instanceMetadata, dicom, attachments);

      // Only keep the metadata for the "instance" level
      dicom.GetMetadata().clear();

      for (InstanceMetadata::const_iterator it = instanceMetadata.begin();
           it != instanceMetadata.end(); ++it)
      {
        dicom.GetMetadata().insert(std::make_pair(std::make_pair(ResourceType_Instance, it->first),
                                                  it->second));
      }
            
      if (status != StoreStatus_Success)
      {
        accessor.Remove(dicomInfo);
        accessor.Remove(jsonInfo);
      }

      switch (status)
      {
        case StoreStatus_Success:
          LOG(INFO) << "New instance stored";
          break;

        case StoreStatus_AlreadyStored:
          LOG(INFO) << "Already stored";
          break;

        case StoreStatus_Failure:
          LOG(ERROR) << "Store failure";
          break;

        default:
          // This should never happen
          break;
      }

      if (status == StoreStatus_Success ||
          status == StoreStatus_AlreadyStored)
      {
        boost::recursive_mutex::scoped_lock lock(listenersMutex_);

        for (ServerListeners::iterator it = listeners_.begin(); it != listeners_.end(); ++it)
        {
          try
          {
            it->GetListener().SignalStoredInstance(resultPublicId, dicom, simplifiedTags);
          }
          catch (OrthancException& e)
          {
            LOG(ERROR) << "Error in the " << it->GetDescription() 
                       << " callback while receiving an instance: " << e.What()
                       << " (code " << e.GetErrorCode() << ")";
          }
        }
      }

      return status;
    }
    catch (OrthancException& e)
    {
      if (e.GetErrorCode() == ErrorCode_InexistentTag)
      {
        dicom.GetSummary().LogMissingTagsForStore();
      }

      throw;
    }
  }


  void ServerContext::AnswerAttachment(RestApiOutput& output,
                                       const std::string& resourceId,
                                       FileContentType content)
  {
    FileInfo attachment;
    if (!index_.LookupAttachment(attachment, resourceId, content))
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }

    StorageAccessor accessor(area_, GetMetricsRegistry());
    accessor.AnswerFile(output, attachment, GetFileContentMime(content));
  }


  void ServerContext::ChangeAttachmentCompression(const std::string& resourceId,
                                                  FileContentType attachmentType,
                                                  CompressionType compression)
  {
    LOG(INFO) << "Changing compression type for attachment "
              << EnumerationToString(attachmentType) 
              << " of resource " << resourceId << " to " 
              << compression; 

    FileInfo attachment;
    if (!index_.LookupAttachment(attachment, resourceId, attachmentType))
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }

    if (attachment.GetCompressionType() == compression)
    {
      // Nothing to do
      return;
    }

    std::string content;

    StorageAccessor accessor(area_, GetMetricsRegistry());
    accessor.Read(content, attachment);

    FileInfo modified = accessor.Write(content.empty() ? NULL : content.c_str(),
                                       content.size(), attachmentType, compression, storeMD5_);

    try
    {
      StoreStatus status = index_.AddAttachment(modified, resourceId);
      if (status != StoreStatus_Success)
      {
        accessor.Remove(modified);
        throw OrthancException(ErrorCode_Database);
      }
    }
    catch (OrthancException&)
    {
      accessor.Remove(modified);
      throw;
    }    
  }


  void ServerContext::ReadDicomAsJsonInternal(std::string& result,
                                              const std::string& instancePublicId)
  {
    FileInfo attachment;
    if (index_.LookupAttachment(attachment, instancePublicId, FileContentType_DicomAsJson))
    {
      ReadAttachment(result, attachment);
    }
    else
    {
      // The "DICOM as JSON" summary is not available from the Orthanc
      // store (most probably deleted), reconstruct it from the DICOM file
      std::string dicom;
      ReadDicom(dicom, instancePublicId);

      LOG(INFO) << "Reconstructing the missing DICOM-as-JSON summary for instance: "
                << instancePublicId;
    
      ParsedDicomFile parsed(dicom);

      Json::Value summary;
      parsed.DatasetToJson(summary);

      result = summary.toStyledString();

      if (!AddAttachment(instancePublicId, FileContentType_DicomAsJson,
                         result.c_str(), result.size()))
      {
        throw OrthancException(ErrorCode_InternalError,
                               "Cannot associate the DICOM-as-JSON summary to instance: " + instancePublicId);
      }
    }
  }


  void ServerContext::ReadDicomAsJson(std::string& result,
                                      const std::string& instancePublicId,
                                      const std::set<DicomTag>& ignoreTagLength)
  {
    if (ignoreTagLength.empty())
    {
      ReadDicomAsJsonInternal(result, instancePublicId);
    }
    else
    {
      Json::Value tmp;
      ReadDicomAsJson(tmp, instancePublicId, ignoreTagLength);
      result = tmp.toStyledString();
    }
  }


  void ServerContext::ReadDicomAsJson(Json::Value& result,
                                      const std::string& instancePublicId,
                                      const std::set<DicomTag>& ignoreTagLength)
  {
    if (ignoreTagLength.empty())
    {
      std::string tmp;
      ReadDicomAsJsonInternal(tmp, instancePublicId);

      Json::Reader reader;
      if (!reader.parse(tmp, result))
      {
        throw OrthancException(ErrorCode_CorruptedFile);
      }
    }
    else
    {
      // The "DicomAsJson" attachment might have stored some tags as
      // "too long". We are forced to re-parse the DICOM file.
      std::string dicom;
      ReadDicom(dicom, instancePublicId);

      ParsedDicomFile parsed(dicom);
      parsed.DatasetToJson(result, ignoreTagLength);
    }
  }


  void ServerContext::ReadAttachment(std::string& result,
                                     const std::string& instancePublicId,
                                     FileContentType content,
                                     bool uncompressIfNeeded)
  {
    FileInfo attachment;
    if (!index_.LookupAttachment(attachment, instancePublicId, content))
    {
      throw OrthancException(ErrorCode_InternalError,
                             "Unable to read attachment " + EnumerationToString(content) +
                             " of instance " + instancePublicId);
    }

    assert(attachment.GetContentType() == content);

    if (uncompressIfNeeded)
    {
      ReadAttachment(result, attachment);
    }
    else
    {
      // Do not uncompress the content of the storage area, return the
      // raw data
      StorageAccessor accessor(area_, GetMetricsRegistry());
      accessor.ReadRaw(result, attachment);
    }
  }


  void ServerContext::ReadAttachment(std::string& result,
                                     const FileInfo& attachment)
  {
    // This will decompress the attachment
    StorageAccessor accessor(area_, GetMetricsRegistry());
    accessor.Read(result, attachment);
  }


  IDynamicObject* ServerContext::DicomCacheProvider::Provide(const std::string& instancePublicId)
  {
    std::string content;
    context_.ReadDicom(content, instancePublicId);
    return new ParsedDicomFile(content);
  }


  ServerContext::DicomCacheLocker::DicomCacheLocker(ServerContext& that,
                                                    const std::string& instancePublicId) : 
    that_(that),
    lock_(that_.dicomCacheMutex_)
  {
#if ENABLE_DICOM_CACHE == 0
    static std::unique_ptr<IDynamicObject> p;
    p.reset(provider_.Provide(instancePublicId));
    dicom_ = dynamic_cast<ParsedDicomFile*>(p.get());
#else
    dicom_ = &dynamic_cast<ParsedDicomFile&>(that_.dicomCache_.Access(instancePublicId));
#endif
  }


  ServerContext::DicomCacheLocker::~DicomCacheLocker()
  {
  }


  void ServerContext::SetStoreMD5ForAttachments(bool storeMD5)
  {
    LOG(INFO) << "Storing MD5 for attachments: " << (storeMD5 ? "yes" : "no");
    storeMD5_ = storeMD5;
  }


  bool ServerContext::AddAttachment(const std::string& resourceId,
                                    FileContentType attachmentType,
                                    const void* data,
                                    size_t size)
  {
    LOG(INFO) << "Adding attachment " << EnumerationToString(attachmentType) << " to resource " << resourceId;
    
    // TODO Should we use "gzip" instead?
    CompressionType compression = (compressionEnabled_ ? CompressionType_ZlibWithSize : CompressionType_None);

    StorageAccessor accessor(area_, GetMetricsRegistry());
    FileInfo attachment = accessor.Write(data, size, attachmentType, compression, storeMD5_);

    StoreStatus status = index_.AddAttachment(attachment, resourceId);
    if (status != StoreStatus_Success)
    {
      accessor.Remove(attachment);
      return false;
    }
    else
    {
      return true;
    }
  }


  bool ServerContext::DeleteResource(Json::Value& target,
                                     const std::string& uuid,
                                     ResourceType expectedType)
  {
    if (expectedType == ResourceType_Instance)
    {
      // remove the file from the DicomCache
      boost::mutex::scoped_lock lock(dicomCacheMutex_);
      dicomCache_.Invalidate(uuid);
    }

    return index_.DeleteResource(target, uuid, expectedType);
  }


  void ServerContext::SignalChange(const ServerIndexChange& change)
  {
    pendingChanges_.Enqueue(change.Clone());
  }


#if ORTHANC_ENABLE_PLUGINS == 1
  void ServerContext::SetPlugins(OrthancPlugins& plugins)
  {
    boost::recursive_mutex::scoped_lock lock(listenersMutex_);

    plugins_ = &plugins;

    // TODO REFACTOR THIS
    listeners_.clear();
    listeners_.push_back(ServerListener(luaListener_, "Lua"));
    listeners_.push_back(ServerListener(plugins, "plugin"));
  }


  void ServerContext::ResetPlugins()
  {
    boost::recursive_mutex::scoped_lock lock(listenersMutex_);

    plugins_ = NULL;

    // TODO REFACTOR THIS
    listeners_.clear();
    listeners_.push_back(ServerListener(luaListener_, "Lua"));
  }


  const OrthancPlugins& ServerContext::GetPlugins() const
  {
    if (HasPlugins())
    {
      return *plugins_;
    }
    else
    {
      throw OrthancException(ErrorCode_InternalError);
    }
  }

  OrthancPlugins& ServerContext::GetPlugins()
  {
    if (HasPlugins())
    {
      return *plugins_;
    }
    else
    {
      throw OrthancException(ErrorCode_InternalError);
    }
  }

#endif


  bool ServerContext::HasPlugins() const
  {
#if ORTHANC_ENABLE_PLUGINS == 1
    return (plugins_ != NULL);
#else
    return false;
#endif
  }


  void ServerContext::Apply(ILookupVisitor& visitor,
                            const DatabaseLookup& lookup,
                            ResourceType queryLevel,
                            size_t since,
                            size_t limit)
  {
    unsigned int databaseLimit = (queryLevel == ResourceType_Instance ?
                                  limitFindInstances_ : limitFindResults_);
      
    std::vector<std::string> resources, instances;

    {
      const size_t lookupLimit = (databaseLimit == 0 ? 0 : databaseLimit + 1);      
      GetIndex().ApplyLookupResources(resources, &instances, lookup, queryLevel, lookupLimit);
    }

    bool complete = (databaseLimit == 0 ||
                     resources.size() <= databaseLimit);

    LOG(INFO) << "Number of candidate resources after fast DB filtering on main DICOM tags: " << resources.size();

    /**
     * "resources" contains the Orthanc ID of the resource at level
     * "queryLevel", "instances" contains one the Orthanc ID of one
     * sample instance from this resource.
     **/
    assert(resources.size() == instances.size());

    size_t countResults = 0;
    size_t skipped = 0;

    const bool isDicomAsJsonNeeded = visitor.IsDicomAsJsonNeeded();
    
    for (size_t i = 0; i < instances.size(); i++)
    {
      // Optimization in Orthanc 1.5.1 - Don't read the full JSON from
      // the disk if only "main DICOM tags" are to be returned

      std::unique_ptr<Json::Value> dicomAsJson;

      bool hasOnlyMainDicomTags;
      DicomMap dicom;
      
      if (findStorageAccessMode_ == FindStorageAccessMode_DatabaseOnly ||
          findStorageAccessMode_ == FindStorageAccessMode_DiskOnAnswer ||
          lookup.HasOnlyMainDicomTags())
      {
        // Case (1): The main DICOM tags, as stored in the database,
        // are sufficient to look for match

        DicomMap tmp;
        if (!GetIndex().GetAllMainDicomTags(tmp, instances[i]))
        {
          // The instance has been removed during the execution of the
          // lookup, ignore it
          continue;
        }

#if 1
        // New in Orthanc 1.6.0: Only keep the main DICOM tags at the
        // level of interest for the query
        switch (queryLevel)
        {
          // WARNING: Don't reorder cases below, and don't add "break"
          case ResourceType_Instance:
            dicom.MergeMainDicomTags(tmp, ResourceType_Instance);

          case ResourceType_Series:
            dicom.MergeMainDicomTags(tmp, ResourceType_Series);

          case ResourceType_Study:
            dicom.MergeMainDicomTags(tmp, ResourceType_Study);
            
          case ResourceType_Patient:
            dicom.MergeMainDicomTags(tmp, ResourceType_Patient);
            break;

          default:
            throw OrthancException(ErrorCode_InternalError);
        }

        // Special case of the "Modality" at the study level, in order
        // to deal with C-FIND on "ModalitiesInStudy" (0008,0061).
        // Check out integration test "test_rest_modalities_in_study".
        if (queryLevel == ResourceType_Study)
        {
          dicom.CopyTagIfExists(tmp, DICOM_TAG_MODALITY);
        }
#else
        dicom.Assign(tmp);  // This emulates Orthanc <= 1.5.8
#endif
        
        hasOnlyMainDicomTags = true;
      }
      else
      {
        // Case (2): Need to read the "DICOM-as-JSON" attachment from
        // the storage area
        dicomAsJson.reset(new Json::Value);
        ReadDicomAsJson(*dicomAsJson, instances[i]);

        dicom.FromDicomAsJson(*dicomAsJson);

        // This map contains the entire JSON, i.e. more than the main DICOM tags
        hasOnlyMainDicomTags = false;   
      }
      
      if (lookup.IsMatch(dicom))
      {
        if (skipped < since)
        {
          skipped++;
        }
        else if (limit != 0 &&
                 countResults >= limit)
        {
          // Too many results, don't mark as complete
          complete = false;
          break;
        }
        else
        {
          if ((findStorageAccessMode_ == FindStorageAccessMode_DiskOnLookupAndAnswer ||
               findStorageAccessMode_ == FindStorageAccessMode_DiskOnAnswer) &&
              dicomAsJson.get() == NULL &&
              isDicomAsJsonNeeded)
          {
            dicomAsJson.reset(new Json::Value);
            ReadDicomAsJson(*dicomAsJson, instances[i]);
          }

          if (hasOnlyMainDicomTags)
          {
            // This is Case (1): The variable "dicom" only contains the main DICOM tags
            visitor.Visit(resources[i], instances[i], dicom, dicomAsJson.get());
          }
          else
          {
            // Remove the non-main DICOM tags from "dicom" if Case (2)
            // was used, for consistency with Case (1)

            DicomMap mainDicomTags;
            mainDicomTags.ExtractMainDicomTags(dicom);
            visitor.Visit(resources[i], instances[i], mainDicomTags, dicomAsJson.get());            
          }
            
          countResults ++;
        }
      }
    }

    if (complete)
    {
      visitor.MarkAsComplete();
    }

    LOG(INFO) << "Number of matching resources: " << countResults;
  }


  bool ServerContext::LookupOrReconstructMetadata(std::string& target,
                                                  const std::string& publicId,
                                                  MetadataType metadata)
  {
    // This is a backwards-compatibility function, that can
    // reconstruct metadata that were not generated by an older
    // release of Orthanc

    if (metadata == MetadataType_Instance_SopClassUid ||
        metadata == MetadataType_Instance_TransferSyntax)
    {
      if (index_.LookupMetadata(target, publicId, metadata))
      {
        return true;
      }
      else
      {
        // These metadata are mandatory in DICOM instances, and were
        // introduced in Orthanc 1.2.0. The fact that
        // "LookupMetadata()" has failed indicates that this database
        // comes from an older release of Orthanc.
        
        DicomTag tag(0, 0);
      
        switch (metadata)
        {
          case MetadataType_Instance_SopClassUid:
            tag = DICOM_TAG_SOP_CLASS_UID;
            break;

          case MetadataType_Instance_TransferSyntax:
            tag = DICOM_TAG_TRANSFER_SYNTAX_UID;
            break;

          default:
            throw OrthancException(ErrorCode_InternalError);
        }
      
        Json::Value dicomAsJson;
        ReadDicomAsJson(dicomAsJson, publicId);

        DicomMap tags;
        tags.FromDicomAsJson(dicomAsJson);

        const DicomValue* value = tags.TestAndGetValue(tag);

        if (value != NULL &&
            !value->IsNull() &&
            !value->IsBinary())
        {
          target = value->GetContent();

          // Store for reuse
          index_.SetMetadata(publicId, metadata, target);
          return true;
        }
        else
        {
          // Should never happen
          return false;
        }
      }
    }
    else
    {
      // No backward
      return index_.LookupMetadata(target, publicId, metadata);
    }
  }


  void ServerContext::AddChildInstances(SetOfInstancesJob& job,
                                        const std::string& publicId)
  {
    std::list<std::string> instances;
    GetIndex().GetChildInstances(instances, publicId);

    job.Reserve(job.GetInstancesCount() + instances.size());

    for (std::list<std::string>::const_iterator
           it = instances.begin(); it != instances.end(); ++it)
    {
      job.AddInstance(*it);
    }
  }


  void ServerContext::SignalUpdatedModalities()
  {
#if ORTHANC_ENABLE_PLUGINS == 1
    if (HasPlugins())
    {
      GetPlugins().SignalUpdatedModalities();
    }
#endif
  }

   
  void ServerContext::SignalUpdatedPeers()
  {
#if ORTHANC_ENABLE_PLUGINS == 1
    if (HasPlugins())
    {
      GetPlugins().SignalUpdatedPeers();
    }
#endif
  }


  IStorageCommitmentFactory::ILookupHandler*
  ServerContext::CreateStorageCommitment(const std::string& jobId,
                                         const std::string& transactionUid,
                                         const std::vector<std::string>& sopClassUids,
                                         const std::vector<std::string>& sopInstanceUids,
                                         const std::string& remoteAet,
                                         const std::string& calledAet)
  {
#if ORTHANC_ENABLE_PLUGINS == 1
    if (HasPlugins())
    {
      return GetPlugins().CreateStorageCommitment(
        jobId, transactionUid, sopClassUids, sopInstanceUids, remoteAet, calledAet);
    }
#endif

    return NULL;
  }
}
