/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
#include "ServerContext.h"

#include "../../OrthancFramework/Sources/Cache/SharedArchive.h"
#include "../../OrthancFramework/Sources/DicomFormat/DicomElement.h"
#include "../../OrthancFramework/Sources/DicomFormat/DicomStreamReader.h"
#include "../../OrthancFramework/Sources/DicomParsing/DcmtkTranscoder.h"
#include "../../OrthancFramework/Sources/DicomParsing/DicomModification.h"
#include "../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"
#include "../../OrthancFramework/Sources/DicomParsing/Internals/DicomImageDecoder.h"
#include "../../OrthancFramework/Sources/FileStorage/StorageAccessor.h"
#include "../../OrthancFramework/Sources/HttpServer/FilesystemHttpSender.h"
#include "../../OrthancFramework/Sources/HttpServer/HttpStreamTranscoder.h"
#include "../../OrthancFramework/Sources/JobsEngine/SetOfInstancesJob.h"
#include "../../OrthancFramework/Sources/Logging.h"
#include "../../OrthancFramework/Sources/MallocMemoryBuffer.h"
#include "../../OrthancFramework/Sources/MetricsRegistry.h"
#include "../Plugins/Engine/OrthancPlugins.h"

#include "OrthancConfiguration.h"
#include "OrthancRestApi/OrthancRestApi.h"
#include "ResourceFinder.h"
#include "Search/DatabaseLookup.h"
#include "ServerJobs/OrthancJobUnserializer.h"
#include "ServerToolbox.h"
#include "StorageCommitmentReports.h"

#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmnet/dimse.h>
#include <dcmtk/dcmdata/dcuid.h>        /* for variable dcmAllStorageSOPClassUIDs */

#include <boost/regex.hpp>

#if HAVE_MALLOC_TRIM == 1
#  include <malloc.h>
#endif

static size_t DICOM_CACHE_SIZE = 128 * 1024 * 1024;  // 128 MB


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
  static bool IsUncompressedTransferSyntax(DicomTransferSyntax transferSyntax)
  {
    return (transferSyntax == DicomTransferSyntax_LittleEndianImplicit ||
            transferSyntax == DicomTransferSyntax_LittleEndianExplicit ||
            transferSyntax == DicomTransferSyntax_BigEndianExplicit);
  }


  static bool IsTranscodableTransferSyntax(DicomTransferSyntax transferSyntax)
  {
    return (
      // Do not try to transcode DICOM videos (new in Orthanc 1.8.2)
      transferSyntax != DicomTransferSyntax_MPEG2MainProfileAtMainLevel &&
      transferSyntax != DicomTransferSyntax_MPEG2MainProfileAtHighLevel &&
      transferSyntax != DicomTransferSyntax_MPEG4HighProfileLevel4_1 &&
      transferSyntax != DicomTransferSyntax_MPEG4BDcompatibleHighProfileLevel4_1 &&
      transferSyntax != DicomTransferSyntax_MPEG4HighProfileLevel4_2_For2DVideo &&
      transferSyntax != DicomTransferSyntax_MPEG4HighProfileLevel4_2_For3DVideo &&
      transferSyntax != DicomTransferSyntax_MPEG4StereoHighProfileLevel4_2 &&
      transferSyntax != DicomTransferSyntax_HEVCMainProfileLevel5_1 &&
      transferSyntax != DicomTransferSyntax_HEVCMain10ProfileLevel5_1 &&

      // Do not try to transcode special transfer syntaxes
      transferSyntax != DicomTransferSyntax_RFC2557MimeEncapsulation &&
      transferSyntax != DicomTransferSyntax_XML);
  }


  ServerContext::StoreResult::StoreResult() :
    status_(StoreStatus_Failure),
    cstoreStatusCode_(0)
  {
  }


#if HAVE_MALLOC_TRIM == 1
  void ServerContext::MemoryTrimmingThread(ServerContext* that,
                                           unsigned int intervalInSeconds)
  {
    Logging::SetCurrentThreadName("MEMORY-TRIM");

    boost::posix_time::ptime lastExecution = boost::posix_time::second_clock::universal_time();

    // This thread is started only if malloc_trim is defined
    while (!that->done_)
    {
      boost::posix_time::ptime now = boost::posix_time::second_clock::universal_time();
      boost::posix_time::time_duration elapsed = now - lastExecution;

      if (elapsed.total_seconds() > intervalInSeconds)
      {
        // If possible, gives memory back to the system 
        // (see OrthancServer/Resources/ImplementationNotes/memory_consumption.txt)
        {
          MetricsRegistry::Timer timer(that->GetMetricsRegistry(), "orthanc_memory_trimming_duration_ms");
          malloc_trim(128*1024);
        }
        lastExecution = boost::posix_time::second_clock::universal_time();
      }

      boost::this_thread::sleep(boost::posix_time::milliseconds(100));
    }
  }
#endif

  
  void ServerContext::ChangeThread(ServerContext* that,
                                   unsigned int sleepDelay)
  {
    Logging::SetCurrentThreadName("CHANGES");

    while (!that->done_)
    {
      std::unique_ptr<IDynamicObject> obj(that->pendingChanges_.Dequeue(sleepDelay));
        
      if (obj.get() != NULL)
      {
        const ServerIndexChange& change = dynamic_cast<const ServerIndexChange&>(*obj.get());

        boost::shared_lock<boost::shared_mutex> lock(that->listenersMutex_);
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
              throw OrthancException(ErrorCode_InternalError, "Error while signaling a change");
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


  void ServerContext::JobEventsThread(ServerContext* that,
                                      unsigned int sleepDelay)
  {
    Logging::SetCurrentThreadName("JOB-EVENTS");

    while (!that->done_)
    {
      std::unique_ptr<IDynamicObject> obj(that->pendingJobEvents_.Dequeue(sleepDelay));
        
      if (obj.get() != NULL)
      {
        const JobEvent& event = dynamic_cast<const JobEvent&>(*obj.get());

        boost::shared_lock<boost::shared_mutex> lock(that->listenersMutex_);
        for (ServerListeners::iterator it = that->listeners_.begin(); 
             it != that->listeners_.end(); ++it)
        {
          try
          {
            try
            {
              it->GetListener().SignalJobEvent(event);
            }
            catch (std::bad_alloc&)
            {
              LOG(ERROR) << "Not enough memory while signaling a job event";
            }
            catch (...)
            {
              throw OrthancException(ErrorCode_InternalError, "Error while signaling a job event");
            }
          }
          catch (OrthancException& e)
          {
            LOG(ERROR) << "Error in the " << it->GetDescription() 
                       << " callback while signaling a job event: " << e.What()
                       << " (code " << e.GetErrorCode() << ")";
          }
        }
      }
    }
  }


  void ServerContext::SaveJobsThread(ServerContext* that,
                                     unsigned int sleepDelay)
  {
    Logging::SetCurrentThreadName("SAVE-JOBS");

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
    pendingJobEvents_.Enqueue(new JobEvent(JobEventType_Submitted, jobId));
  }
  

  void ServerContext::SignalJobSuccess(const std::string& jobId)
  {
    haveJobsChanged_ = true;
    pendingJobEvents_.Enqueue(new JobEvent(JobEventType_Success, jobId));
  }

  
  void ServerContext::SignalJobFailure(const std::string& jobId)
  {
    haveJobsChanged_ = true;
    pendingJobEvents_.Enqueue(new JobEvent(JobEventType_Failure, jobId));
  }


  void ServerContext::SetupJobsEngine(bool unitTesting,
                                      bool loadJobsFromDatabase)
  {
    if (loadJobsFromDatabase)
    {
      std::string serialized;
      if (index_.LookupGlobalProperty(serialized, GlobalProperty_JobsRegistry, false /* not shared */))
      {
        LOG(WARNING) << "Reloading the jobs from the last execution of Orthanc";

        try
        {
          OrthancJobUnserializer unserializer(*this);
          jobsEngine_.LoadRegistryFromString(unserializer, serialized);
        }
        catch (OrthancException& e)
        {
          LOG(WARNING) << "Cannot unserialize the jobs engine, starting anyway: " << e.What();
        }
        catch (const std::string& s) 
        {
          LOG(WARNING) << "Cannot unserialize the jobs engine, starting anyway: \"" << s << "\"";
        }
        catch (...)
        {
          LOG(WARNING) << "Cannot unserialize the jobs engine, starting anyway";
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
      LOG(TRACE) << "Serializing the content of the jobs engine";
    
      try
      {
        Json::Value value;
        jobsEngine_.GetRegistry().Serialize(value);

        std::string serialized;
        Toolbox::WriteFastJson(serialized, value);

        index_.SetGlobalProperty(GlobalProperty_JobsRegistry, false /* not shared */, serialized);
      }
      catch (OrthancException& e)
      {
        LOG(ERROR) << "Cannot serialize the jobs engine: " << e.What();
      }
    }
  }


  void ServerContext::PublishCacheMetrics()
  {
    metricsRegistry_->SetFloatValue("orthanc_dicom_cache_size_mb",
                                    static_cast<float>(dicomCache_.GetCurrentSize()) / static_cast<float>(1024 * 1024));
    metricsRegistry_->SetIntegerValue("orthanc_dicom_cache_count", dicomCache_.GetNumberOfItems());

    metricsRegistry_->SetFloatValue("orthanc_storage_cache_size_mb",
                                    static_cast<float>(storageCache_.GetCurrentSize()) / static_cast<float>(1024 * 1024));
    metricsRegistry_->SetIntegerValue("orthanc_storage_cache_count", storageCache_.GetNumberOfItems());
  }


  ServerContext::ServerContext(IDatabaseWrapper& database,
                               IStorageArea& area,
                               bool unitTesting,
                               size_t maxCompletedJobs,
                               bool readOnly,
                               unsigned int maxConcurrentDcmtkTranscoder) :
    index_(*this, database, (unitTesting ? 20 : 500), readOnly),
    area_(area),
    compressionEnabled_(false),
    storeMD5_(true),
    largeDicomThrottler_(1),
    dicomCache_(DICOM_CACHE_SIZE),
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
    isExecuteLuaEnabled_(false),
    isRestApiWriteToFileSystemEnabled_(false),
    overwriteInstances_(false),
    dcmtkTranscoder_(new DcmtkTranscoder(maxConcurrentDcmtkTranscoder)),
    isIngestTranscoding_(false),
    ingestTranscodingOfUncompressed_(true),
    ingestTranscodingOfCompressed_(true),
    preferredTransferSyntax_(DicomTransferSyntax_LittleEndianExplicit),
    readOnly_(readOnly),
    deidentifyLogs_(false),
    serverStartTimeUtc_(boost::posix_time::second_clock::universal_time())
  {
    try
    {
      unsigned int lossyQuality;

      {
        OrthancConfiguration::ReaderLock lock;

        queryRetrieveArchive_.reset(
          new SharedArchive(lock.GetConfiguration().GetUnsignedIntegerParameter("QueryRetrieveSize", 100)));
        mediaArchive_.reset(
          new SharedArchive(lock.GetConfiguration().GetUnsignedIntegerParameter("MediaArchiveSize", 1)));
        defaultLocalAet_ = lock.GetConfiguration().GetOrthancAET();
        jobsEngine_.SetWorkersCount(lock.GetConfiguration().GetUnsignedIntegerParameter("ConcurrentJobs", 2));

        saveJobs_ = lock.GetConfiguration().GetBooleanParameter("SaveJobs", true);
        if (readOnly_ && saveJobs_)
        {
          LOG(WARNING) << "READ-ONLY SYSTEM: SaveJobs = true is incompatible with a ReadOnly system, ignoring this configuration";
          saveJobs_ = false;
        }

        metricsRegistry_->SetEnabled(lock.GetConfiguration().GetBooleanParameter("MetricsEnabled", true));

        // New configuration options in Orthanc 1.5.1
        findStorageAccessMode_ = StringToFindStorageAccessMode(lock.GetConfiguration().GetStringParameter("StorageAccessOnFind", "Always"));
        limitFindInstances_ = lock.GetConfiguration().GetUnsignedIntegerParameter("LimitFindInstances", 0);
        limitFindResults_ = lock.GetConfiguration().GetUnsignedIntegerParameter("LimitFindResults", 0);

        // New configuration option in Orthanc 1.6.0
        storageCommitmentReports_.reset(new StorageCommitmentReports(lock.GetConfiguration().GetUnsignedIntegerParameter("StorageCommitmentReportsSize", 100)));

        // New options in Orthanc 1.7.0
        transcodeDicomProtocol_ = lock.GetConfiguration().GetBooleanParameter("TranscodeDicomProtocol", true);
        builtinDecoderTranscoderOrder_ = StringToBuiltinDecoderTranscoderOrder(lock.GetConfiguration().GetStringParameter("BuiltinDecoderTranscoderOrder", "After"));
        lossyQuality = lock.GetConfiguration().GetUnsignedIntegerParameter("DicomLossyTranscodingQuality", 90);

        std::string s;
        if (lock.GetConfiguration().LookupStringParameter(s, "IngestTranscoding"))
        {
          if (LookupTransferSyntax(ingestTransferSyntax_, s))
          {
            isIngestTranscoding_ = true;
            LOG(WARNING) << "Incoming DICOM instances will automatically be transcoded to "
                         << "transfer syntax: " << GetTransferSyntaxUid(ingestTransferSyntax_);

            // New options in Orthanc 1.8.2
            ingestTranscodingOfUncompressed_ = lock.GetConfiguration().GetBooleanParameter("IngestTranscodingOfUncompressed", true);
            ingestTranscodingOfCompressed_ = lock.GetConfiguration().GetBooleanParameter("IngestTranscodingOfCompressed", true);

            LOG(WARNING) << "  Ingest transcoding will "
                         << (ingestTranscodingOfUncompressed_ ? "be applied" : "*not* be applied")
                         << " to uncompressed transfer syntaxes (Little Endian Implicit/Explicit, Big Endian Explicit)";

            LOG(WARNING) << "  Ingest transcoding will "
                         << (ingestTranscodingOfCompressed_ ? "be applied" : "*not* be applied")
                         << " to compressed transfer syntaxes";
          }
          else
          {
            throw OrthancException(ErrorCode_ParameterOutOfRange,
                                   "Unknown transfer syntax for ingest transcoding: " + s);
          }
        }
        else
        {
          isIngestTranscoding_ = false;
          LOG(INFO) << "Automated transcoding of incoming DICOM instances is disabled";
        }

        // New options in Orthanc 1.8.2
        if (lock.GetConfiguration().GetBooleanParameter("DeidentifyLogs", true))
        {
          deidentifyLogs_ = true;
          CLOG(INFO, DICOM) << "Deidentification of log contents (notably for DIMSE queries) is enabled";

          DicomVersion version = StringToDicomVersion(
              lock.GetConfiguration().GetStringParameter("DeidentifyLogsDicomVersion", "2023b"));
          CLOG(INFO, DICOM) << "Version of DICOM standard used for deidentification is "
                            << EnumerationToString(version);

          logsDeidentifierRules_.SetupAnonymization(version);
        }
        else
        {
          deidentifyLogs_ = false;
          CLOG(INFO, DICOM) << "Deidentification of log contents (notably for DIMSE queries) is disabled";
        }

        // New options in Orthanc 1.9.0
        if (lock.GetConfiguration().LookupStringParameter(s, "DicomScuPreferredTransferSyntax") &&
            !LookupTransferSyntax(preferredTransferSyntax_, s))
        {
          throw OrthancException(ErrorCode_ParameterOutOfRange,
                                 "Unknown preferred transfer syntax: " + s);
        }
        
        CLOG(INFO, DICOM) << "Preferred transfer syntax for Orthanc C-STORE SCU: "
                          << GetTransferSyntaxUid(preferredTransferSyntax_);

        lock.GetConfiguration().GetAcceptedTransferSyntaxes(acceptedTransferSyntaxes_);

        isUnknownSopClassAccepted_ = lock.GetConfiguration().GetBooleanParameter("UnknownSopClassAccepted", false);

        // New options in Orthanc 1.12.6
        std::list<std::string> acceptedSopClasses;
        std::set<std::string> rejectedSopClasses;
        lock.GetConfiguration().GetListOfStringsParameter(acceptedSopClasses, "AcceptedSopClasses");
        lock.GetConfiguration().GetSetOfStringsParameter(rejectedSopClasses, "RejectSopClasses");
        SetAcceptedSopClasses(acceptedSopClasses, rejectedSopClasses);

        defaultDicomRetrieveMethod_ = StringToRetrieveMethod(lock.GetConfiguration().GetStringParameter("DicomDefaultRetrieveMethod", "C-MOVE"));
      }

      jobsEngine_.SetThreadSleep(unitTesting ? 20 : 200);

      listeners_.push_back(ServerListener(luaListener_, "Lua"));
      changeThread_ = boost::thread(ChangeThread, this, (unitTesting ? 20 : 100));
      jobEventsThread_ = boost::thread(JobEventsThread, this, (unitTesting ? 20 : 100));
      
#if HAVE_MALLOC_TRIM == 1
      LOG(INFO) << "Starting memory trimming thread at 30 seconds interval";
      memoryTrimmingThread_ = boost::thread(MemoryTrimmingThread, this, 30);
#else
      LOG(INFO) << "Your platform does not support malloc_trim(), not starting the memory trimming thread";
#endif
      
      dynamic_cast<DcmtkTranscoder&>(*dcmtkTranscoder_).SetLossyQuality(lossyQuality);
    }
    catch (OrthancException&)
    {
      Stop();
      throw;
    }
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
        boost::unique_lock<boost::shared_mutex> lock(listenersMutex_);
        listeners_.clear();
      }

      done_ = true;

      if (changeThread_.joinable())
      {
        changeThread_.join();
      }

      if (jobEventsThread_.joinable())
      {
        jobEventsThread_.join();
      }

      if (saveJobsThread_.joinable())
      {
        saveJobsThread_.join();
      }

      if (memoryTrimmingThread_.joinable())
      {
        memoryTrimmingThread_.join();
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
    StorageAccessor accessor(area_, storageCache_, GetMetricsRegistry());
    accessor.Remove(fileUuid, type);
  }


  ServerContext::StoreResult ServerContext::StoreAfterTranscoding(std::string& resultPublicId,
                                                                  DicomInstanceToStore& dicom,
                                                                  StoreInstanceMode mode,
                                                                  bool isReconstruct)
  {
    bool overwrite;
    switch (mode)
    {
      case StoreInstanceMode_Default:
        overwrite = overwriteInstances_;
        break;
        
      case StoreInstanceMode_OverwriteDuplicate:
        overwrite = true;
        break;
        
      case StoreInstanceMode_IgnoreDuplicate:
        overwrite = false;
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    bool hasPixelDataOffset;
    uint64_t pixelDataOffset;
    ValueRepresentation pixelDataVR;
    hasPixelDataOffset = DicomStreamReader::LookupPixelDataOffset(
      pixelDataOffset, pixelDataVR, dicom.GetBufferData(), dicom.GetBufferSize());

    DicomTransferSyntax transferSyntax;
    bool hasTransferSyntax = dicom.LookupTransferSyntax(transferSyntax);
    
    DicomMap summary;
    dicom.GetSummary(summary);   // -> from Orthanc 1.11.1, this includes the leaf nodes and sequences

    std::set<DicomTag> allMainDicomTags;
    DicomMap::GetAllMainDicomTags(allMainDicomTags);

    try
    {
      MetricsRegistry::Timer timer(GetMetricsRegistry(), "orthanc_store_dicom_duration_ms");
      StorageAccessor accessor(area_, storageCache_, GetMetricsRegistry());

      DicomInstanceHasher hasher(summary);
      resultPublicId = hasher.HashInstance();

      Json::Value dicomAsJson;
      dicom.GetDicomAsJson(dicomAsJson, allMainDicomTags);  // don't crop any main dicom tags

      Json::Value simplifiedTags;
      Toolbox::SimplifyDicomAsJson(simplifiedTags, dicomAsJson, DicomToJsonFormat_Human);

      // Test if the instance must be filtered out
      StoreResult result;

      if (!isReconstruct) // skip all filters if this is a reconstruction
      {
        boost::shared_lock<boost::shared_mutex> lock(listenersMutex_);

        for (ServerListeners::iterator it = listeners_.begin(); it != listeners_.end(); ++it)
        {
          try
          {
            if (!it->GetListener().FilterIncomingInstance(dicom, simplifiedTags))
            {
              result.SetStatus(StoreStatus_FilteredOut);
              result.SetCStoreStatusCode(STATUS_Success); // to keep backward compatibility, we still return 'success'
              break;
            }

            if (dicom.GetOrigin().GetRequestOrigin() == Orthanc::RequestOrigin_DicomProtocol)
            {
              uint16_t filterResult = STATUS_Success;
              if (!it->GetListener().FilterIncomingCStoreInstance(filterResult, dicom, simplifiedTags))
              {
                // The instance is to be discarded
                result.SetStatus(StoreStatus_FilteredOut);
                result.SetCStoreStatusCode(filterResult);
                break;
              }
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

      if (result.GetStatus() == StoreStatus_FilteredOut)
      {
        LOG(INFO) << "An incoming instance has been discarded by the filter";
        return result;
      }

      // Remove the file from the DicomCache (useful if
      // "OverwriteInstances" is set to "true")
      dicomCache_.Invalidate(resultPublicId);

      // TODO Should we use "gzip" instead?
      CompressionType compression = (compressionEnabled_ ? CompressionType_ZlibWithSize : CompressionType_None);

      FileInfo dicomInfo = accessor.Write(dicom.GetBufferData(), dicom.GetBufferSize(), 
                                          FileContentType_Dicom, compression, storeMD5_);

      ServerIndex::Attachments attachments;
      attachments.push_back(dicomInfo);

      FileInfo dicomUntilPixelData;
      if (hasPixelDataOffset &&
          (!area_.HasReadRange() ||
           compressionEnabled_))
      {
        dicomUntilPixelData = accessor.Write(dicom.GetBufferData(), pixelDataOffset, 
                                             FileContentType_DicomUntilPixelData, compression, storeMD5_);
        attachments.push_back(dicomUntilPixelData);
      }

      typedef std::map<MetadataType, std::string>  InstanceMetadata;
      InstanceMetadata  instanceMetadata;

      try 
      {
        result.SetStatus(index_.Store(
          instanceMetadata, summary, attachments, dicom.GetMetadata(), dicom.GetOrigin(), overwrite,
          hasTransferSyntax, transferSyntax, hasPixelDataOffset, pixelDataOffset, pixelDataVR, isReconstruct));
      }
      catch (OrthancException& ex)
      {
        if (ex.GetErrorCode() == ErrorCode_DuplicateResource)
        {
          LOG(WARNING) << "Duplicate instance, deleting the attachments";
        }
        else
        {
          LOG(ERROR) << "Unexpected error while storing an instance in DB, cancelling and deleting the attachments: " << ex.GetDetails();
        }

        accessor.Remove(dicomInfo);

        if (dicomUntilPixelData.IsValid())
        {
          accessor.Remove(dicomUntilPixelData);
        }
        
        throw;
      }

      // Only keep the metadata for the "instance" level
      dicom.ClearMetadata();

      for (InstanceMetadata::const_iterator it = instanceMetadata.begin();
           it != instanceMetadata.end(); ++it)
      {
        dicom.AddMetadata(ResourceType_Instance, it->first, it->second);
      }
            
      if (result.GetStatus() != StoreStatus_Success)
      {
        accessor.Remove(dicomInfo);

        if (dicomUntilPixelData.IsValid())
        {
          accessor.Remove(dicomUntilPixelData);
        }
      }

      if (!isReconstruct) 
      {
        // skip logs in case of reconstruction
        switch (result.GetStatus())
        {
          case StoreStatus_Success:
            LOG(INFO) << "New instance stored (" << resultPublicId << ")";
            break;

          case StoreStatus_AlreadyStored:
            LOG(INFO) << "Instance already stored (" << resultPublicId << ")";
            break;

          case StoreStatus_Failure:
            LOG(ERROR) << "Unknown store failure while storing instance " << resultPublicId;
            throw OrthancException(ErrorCode_InternalError, HttpStatus_500_InternalServerError);

          case StoreStatus_StorageFull:
            LOG(ERROR) << "Storage full while storing instance " << resultPublicId;
            throw OrthancException(ErrorCode_FullStorage, HttpStatus_507_InsufficientStorage);

          default:
            // This should never happen
            break;
        }

        // skip all signals if this is a reconstruction
        if (result.GetStatus() == StoreStatus_Success ||
            result.GetStatus() == StoreStatus_AlreadyStored)
        {
          boost::shared_lock<boost::shared_mutex> lock(listenersMutex_);

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
      }

      return result;
    }
    catch (OrthancException& e)
    {
      if (e.GetErrorCode() == ErrorCode_InexistentTag)
      {
        summary.LogMissingTagsForStore();
      }
      
      throw;
    }
  }


  ServerContext::StoreResult ServerContext::Store(std::string& resultPublicId,
                                                  DicomInstanceToStore& receivedDicom,
                                                  StoreInstanceMode mode)
  { 
    DicomInstanceToStore* dicom = &receivedDicom;

    // WARNING: The scope of "modifiedBuffer" and "modifiedDicom" must
    // be the same as that of "dicom"
    MallocMemoryBuffer modifiedBuffer;
    std::unique_ptr<DicomInstanceToStore> modifiedDicom;

#if ORTHANC_ENABLE_PLUGINS == 1
    if (HasPlugins())
    {
      // New in Orthanc 1.10.0

      OrthancPluginReceivedInstanceAction action = GetPlugins().ApplyReceivedInstanceCallbacks(
        modifiedBuffer, receivedDicom.GetBufferData(), receivedDicom.GetBufferSize(), receivedDicom.GetOrigin().GetRequestOrigin());

      switch (action)
      {
        case OrthancPluginReceivedInstanceAction_Discard:
        {
          CLOG(INFO, PLUGINS) << "A plugin has discarded the instance in its ReceivedInstanceCallback";
          StoreResult result;
          result.SetStatus(StoreStatus_FilteredOut);
          return result;
        }
          
        case OrthancPluginReceivedInstanceAction_KeepAsIs:
          // This path is also used when no ReceivedInstanceCallback is installed by the plugins
          break;

        case OrthancPluginReceivedInstanceAction_Modify:
          if (modifiedBuffer.GetSize() > 0 &&
              modifiedBuffer.GetData() != NULL)
          {
            CLOG(INFO, PLUGINS) << "A plugin has modified the instance in its ReceivedInstanceCallback";        
            modifiedDicom.reset(DicomInstanceToStore::CreateFromBuffer(modifiedBuffer.GetData(), modifiedBuffer.GetSize()));
            modifiedDicom->SetOrigin(receivedDicom.GetOrigin());
            dicom = modifiedDicom.get();
          }
          else
          {
            throw OrthancException(ErrorCode_Plugin, "The ReceivedInstanceCallback plugin is not returning a modified buffer while it has modified the instance");
          }
          break;
          
        default:
          throw OrthancException(ErrorCode_Plugin, "The ReceivedInstanceCallback has returned an invalid value");
      }
    }
#endif

    return TranscodeAndStore(resultPublicId, dicom, mode);
  }

  ServerContext::StoreResult ServerContext::TranscodeAndStore(std::string& resultPublicId,
                                                              DicomInstanceToStore* dicom,
                                                              StoreInstanceMode mode,
                                                              bool isReconstruct)
  {

    if (!isIngestTranscoding_ || dicom->SkipIngestTranscoding())
    {
      // No automated transcoding. This was the only path in Orthanc <= 1.6.1.
      return StoreAfterTranscoding(resultPublicId, *dicom, mode, isReconstruct);
    }
    else
    {
      // Automated transcoding of incoming DICOM instance

      bool transcode = false;

      DicomTransferSyntax sourceSyntax;
      if (!dicom->LookupTransferSyntax(sourceSyntax) ||
          sourceSyntax == ingestTransferSyntax_)
      {
        // Don't transcode if the incoming DICOM is already in the proper transfer syntax
        transcode = false;
      }
      else if (!IsTranscodableTransferSyntax(sourceSyntax))
      {
        // Don't try to transcode video files, this is useless (new in
        // Orthanc 1.8.2). This could be accepted in the future if
        // video transcoding gets implemented.
        transcode = false;
      }
      else if (IsUncompressedTransferSyntax(sourceSyntax))
      {
        // This is an uncompressed transfer syntax (new in Orthanc 1.8.2)
        transcode = ingestTranscodingOfUncompressed_;
      }
      else
      {
        // This is an compressed transfer syntax (new in Orthanc 1.8.2)
        transcode = ingestTranscodingOfCompressed_;
      }

      if (!transcode)
      {
        // No transcoding
        return StoreAfterTranscoding(resultPublicId, *dicom, mode, isReconstruct);
      }
      else
      {
        // Trancoding
        std::set<DicomTransferSyntax> syntaxes;
        syntaxes.insert(ingestTransferSyntax_);
        
        IDicomTranscoder::DicomImage source;
        source.SetExternalBuffer(dicom->GetBufferData(), dicom->GetBufferSize());
        
        IDicomTranscoder::DicomImage transcoded;
        
        if (Transcode(transcoded, source, syntaxes, true /* allow new SOP instance UID */))
        {
          std::unique_ptr<ParsedDicomFile> tmp(transcoded.ReleaseAsParsedDicomFile());

          if (isReconstruct)
          {
            // when reconstructing, we always want to keep the DICOM IDs untouched even if the transcoding has generated a new SOPInstanceUID.
            const Orthanc::ParsedDicomFile& sourceDicom = dicom->GetParsedDicomFile();
            std::string sopInstanceUid;
            sourceDicom.GetTagValue(sopInstanceUid, DICOM_TAG_SOP_INSTANCE_UID);
            tmp->ReplacePlainString(DICOM_TAG_SOP_INSTANCE_UID, sopInstanceUid);
          }

          std::unique_ptr<DicomInstanceToStore> toStore(DicomInstanceToStore::CreateFromParsedDicomFile(*tmp));
          toStore->SetOrigin(dicom->GetOrigin());
          toStore->CopyMetadata(dicom->GetMetadata());

          StoreResult result = StoreAfterTranscoding(resultPublicId, *toStore, mode, isReconstruct);
          assert(resultPublicId == tmp->GetHasher().HashInstance());

          return result;
        }
        else
        {
          // Cannot transcode => store the original file
          return StoreAfterTranscoding(resultPublicId, *dicom, mode, isReconstruct);
        }
      }
    }
  }

  
  void ServerContext::AnswerAttachment(RestApiOutput& output,
                                       const FileInfo& attachment)
  {
    StorageAccessor accessor(area_, storageCache_, GetMetricsRegistry());
    accessor.AnswerFile(output, attachment, GetFileContentMime(attachment.GetContentType()));
  }


  void ServerContext::ChangeAttachmentCompression(ResourceType level,
                                                  const std::string& resourceId,
                                                  FileContentType attachmentType,
                                                  CompressionType compression)
  {
    LOG(INFO) << "Changing compression type for attachment "
              << EnumerationToString(attachmentType) 
              << " of resource " << resourceId << " to " 
              << compression; 

    FileInfo attachment;
    int64_t revision;
    if (!index_.LookupAttachment(attachment, revision, level, resourceId, attachmentType))
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }

    if (attachment.GetCompressionType() == compression)
    {
      // Nothing to do
      return;
    }

    std::string content;

    StorageAccessor accessor(area_, storageCache_, GetMetricsRegistry());
    accessor.Read(content, attachment);

    FileInfo modified = accessor.Write(content.empty() ? NULL : content.c_str(),
                                       content.size(), attachmentType, compression, storeMD5_);

    try
    {
      int64_t newRevision;  // ignored
      StoreStatus status = index_.AddAttachment(newRevision, modified, resourceId,
                                                true, revision, modified.GetUncompressedMD5());
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


  static void InjectEmptyPixelData(Json::Value& dicomAsJson)
  {
    // This is for backward compatibility with Orthanc <= 1.9.0
    Json::Value pixelData = Json::objectValue;
    pixelData["Name"] = "PixelData";
    pixelData["Type"] = "Null";
    pixelData["Value"] = Json::nullValue;

    dicomAsJson["7fe0,0010"] = pixelData;
  }


  static bool LookupMetadata(std::string& value,
                             MetadataType key,
                             const std::map<MetadataType, std::string>& instanceMetadata)
  {
    std::map<MetadataType, std::string>::const_iterator found = instanceMetadata.find(key);

    if (found == instanceMetadata.end())
    {
      return false;
    }
    else
    {
      value = found->second;
      return true;
    }
  }


  static bool LookupAttachment(FileInfo& target,
                               FileContentType type,
                               const std::map<FileContentType, FileInfo>& attachments)
  {
    std::map<FileContentType, FileInfo>::const_iterator found = attachments.find(type);

    if (found == attachments.end())
    {
      return false;
    }
    else if (found->second.GetContentType() == type)
    {
      target = found->second;
      return true;
    }
    else
    {
      throw OrthancException(ErrorCode_DatabasePlugin);
    }
  }

  
  void ServerContext::ReadDicomAsJson(Json::Value& result,
                                      const std::string& instancePublicId,
                                      const std::set<DicomTag>& ignoreTagLength)
  {
    // TODO-FIND: This is a compatibility method, should be removed

    std::map<MetadataType, std::string> metadata;
    std::map<FileContentType, FileInfo> attachments;

    FileInfo attachment;
    int64_t revision;  // Ignored

    if (index_.LookupAttachment(attachment, revision, ResourceType_Instance, instancePublicId, FileContentType_Dicom))
    {
      attachments[FileContentType_Dicom] = attachment;
    }

    if (index_.LookupAttachment(attachment, revision, ResourceType_Instance, instancePublicId, FileContentType_DicomUntilPixelData))
    {
      attachments[FileContentType_DicomUntilPixelData] = attachment;
    }

    if (index_.LookupAttachment(attachment, revision, ResourceType_Instance, instancePublicId, FileContentType_DicomAsJson))
    {
      attachments[FileContentType_DicomAsJson] = attachment;
    }

    std::string s;
    if (index_.LookupMetadata(s, instancePublicId, ResourceType_Instance,
                              MetadataType_Instance_PixelDataOffset))
    {
      metadata[MetadataType_Instance_PixelDataOffset] = s;
    }

    ReadDicomAsJson(result, instancePublicId, metadata, attachments, ignoreTagLength);
  }


  void ServerContext::ReadDicomAsJson(Json::Value& result,
                                      const std::string& instancePublicId,
                                      const std::map<MetadataType, std::string>& instanceMetadata,
                                      const std::map<FileContentType, FileInfo>& instanceAttachments,
                                      const std::set<DicomTag>& ignoreTagLength)
  {
    /**
     * CASE 1: The DICOM file, truncated at pixel data, is available
     * as an attachment (it was created either because the storage
     * area does not support range reads, or if "StorageCompression"
     * is enabled). Simply return this attachment.
     **/
    
    FileInfo attachment;

    if (LookupAttachment(attachment, FileContentType_DicomUntilPixelData, instanceAttachments))
    {
      std::string dicom;

      {
        StorageAccessor accessor(area_, storageCache_, GetMetricsRegistry());
        accessor.Read(dicom, attachment);
      }

      ParsedDicomFile parsed(dicom);
      OrthancConfiguration::DefaultDicomDatasetToJson(result, parsed, ignoreTagLength);
      InjectEmptyPixelData(result);
    }
    else
    {
      /**
       * The truncated DICOM file is not stored as a standalone
       * attachment. Lookup whether the pixel data offset has already
       * been computed for this instance.
       **/
    
      bool hasPixelDataOffset;
      uint64_t pixelDataOffset = 0;  // dummy initialization

      {
        std::string s;
        if (LookupMetadata(s, MetadataType_Instance_PixelDataOffset, instanceMetadata))
        {
          hasPixelDataOffset = false;

          if (!s.empty())
          {
            try
            {
              pixelDataOffset = boost::lexical_cast<uint64_t>(s);
              hasPixelDataOffset = true;
            }
            catch (boost::bad_lexical_cast&)
            {
            }
          }

          if (!hasPixelDataOffset)
          {
            LOG(ERROR) << "Metadata \"PixelDataOffset\" is corrupted for instance: " << instancePublicId;
          }
        }
        else
        {
          // This instance was created by a version of Orthanc <= 1.9.0
          hasPixelDataOffset = false;
        }
      }


      if (hasPixelDataOffset &&
          area_.HasReadRange() &&
          LookupAttachment(attachment, FileContentType_Dicom, instanceAttachments) &&
          attachment.GetCompressionType() == CompressionType_None)
      {
        /**
         * CASE 2: The pixel data offset is known, AND that a range read
         * can be used to retrieve the truncated DICOM file. Note that
         * this case cannot be used if "StorageCompression" option is
         * "true".
         **/
      
        std::string dicom;
        
        {
          StorageAccessor accessor(area_, storageCache_, GetMetricsRegistry());
          accessor.ReadStartRange(dicom, attachment, pixelDataOffset);
        }
        
        assert(dicom.size() == pixelDataOffset);
        ParsedDicomFile parsed(dicom);
        OrthancConfiguration::DefaultDicomDatasetToJson(result, parsed, ignoreTagLength);
        InjectEmptyPixelData(result);
      }
      else if (ignoreTagLength.empty() &&
               LookupAttachment(attachment, FileContentType_DicomAsJson, instanceAttachments))
      {
        /**
         * CASE 3: This instance was created using Orthanc <=
         * 1.9.0. Reuse the old "DICOM-as-JSON" attachment if available.
         * This is for backward compatibility: A call to
         * "/tools/invalidate-tags" or to one flavors of
         * "/.../.../reconstruct" will disable this case.
         **/
      
        std::string dicomAsJson;

        {
          StorageAccessor accessor(area_, storageCache_, GetMetricsRegistry());
          accessor.Read(dicomAsJson, attachment);
        }

        if (!Toolbox::ReadJson(result, dicomAsJson))
        {
          throw OrthancException(ErrorCode_CorruptedFile,
                                 "Corrupted DICOM-as-JSON attachment of instance: " + instancePublicId);
        }
      }
      else
      {
        /**
         * CASE 4: Neither the truncated DICOM file is accessible, nor
         * the DICOM-as-JSON summary. We have to retrieve the full DICOM
         * file from the storage area.
         **/

        std::string dicom;
        ReadDicom(dicom, instancePublicId);

        ParsedDicomFile parsed(dicom);
        OrthancConfiguration::DefaultDicomDatasetToJson(result, parsed, ignoreTagLength);

        if (!hasPixelDataOffset)
        {
          /**
           * The pixel data offset was never computed for this
           * instance, which indicates that it was created using
           * Orthanc <= 1.9.0, or that calls to
           * "LookupPixelDataOffset()" from earlier versions of
           * Orthanc have failed. Try again this precomputation now
           * for future calls.
           **/
          ValueRepresentation pixelDataVR;
          if (DicomStreamReader::LookupPixelDataOffset(pixelDataOffset, pixelDataVR, dicom) &&
              pixelDataOffset < dicom.size())
          {
            index_.OverwriteMetadata(instancePublicId, MetadataType_Instance_PixelDataOffset,
                                     boost::lexical_cast<std::string>(pixelDataOffset));

            if (!area_.HasReadRange() ||
                compressionEnabled_)
            {
              int64_t newRevision;
              AddAttachment(newRevision, instancePublicId, FileContentType_DicomUntilPixelData,
                            dicom.empty() ? NULL: dicom.c_str(), pixelDataOffset,
                            false /* no old revision */, -1 /* dummy revision */, "" /* dummy MD5 */);
            }
          }
        }
      }
    }
  }


  void ServerContext::ReadDicomAsJson(Json::Value& result,
                                      const std::string& instancePublicId)
  {
    std::set<DicomTag> ignoreTagLength;
    ReadDicomAsJson(result, instancePublicId, ignoreTagLength);
  }


  void ServerContext::ReadDicom(std::string& dicom,
                                std::string& attachmentId,
                                const std::string& instancePublicId)
  {
    FileInfo attachment;
    int64_t revision;

    if (!index_.LookupAttachment(attachment, revision, ResourceType_Instance, instancePublicId, FileContentType_Dicom))
    {
      throw OrthancException(ErrorCode_InternalError,
                             "Unable to read attachment " + EnumerationToString(FileContentType_Dicom) +
                             " of instance " + instancePublicId);
    }

    assert(attachment.GetContentType() == FileContentType_Dicom);
    attachmentId = attachment.GetUuid();

    ReadAttachment(dicom, attachment, true /* uncompress */);
  }


  void ServerContext::ReadDicom(std::string& dicom,
                                const std::string& instancePublicId)
  {
    std::string attachmentId;
    ReadDicom(dicom, attachmentId, instancePublicId);    
  }

  void ServerContext::ReadDicomForHeader(std::string& dicom,
                                         const std::string& instancePublicId)
  {
    if (!ReadDicomUntilPixelData(dicom, instancePublicId))
    {
      ReadDicom(dicom, instancePublicId);
    }
  }

  bool ServerContext::ReadDicomUntilPixelData(std::string& dicom,
                                              const std::string& instancePublicId)
  {
    FileInfo attachment;
    int64_t revision;  // Ignored
    if (index_.LookupAttachment(attachment, revision, ResourceType_Instance, instancePublicId, FileContentType_DicomUntilPixelData))
    {
      StorageAccessor accessor(area_, storageCache_, GetMetricsRegistry());

      accessor.Read(dicom, attachment);
      assert(dicom.size() == attachment.GetUncompressedSize());

      return true;
    }

    if (!area_.HasReadRange())
    {
      return false;
    }
    
    if (!index_.LookupAttachment(attachment, revision, ResourceType_Instance, instancePublicId, FileContentType_Dicom))
    {
      throw OrthancException(ErrorCode_InternalError,
                             "Unable to read the DICOM file of instance " + instancePublicId);
    }

    std::string s;

    if (attachment.GetCompressionType() == CompressionType_None &&
        index_.LookupMetadata(s, instancePublicId, ResourceType_Instance,
                              MetadataType_Instance_PixelDataOffset) &&
        !s.empty())
    {
      try
      {
        uint64_t pixelDataOffset = boost::lexical_cast<uint64_t>(s);

        StorageAccessor accessor(area_, storageCache_, GetMetricsRegistry());

        accessor.ReadStartRange(dicom, attachment, pixelDataOffset);
        assert(dicom.size() == pixelDataOffset);
        
        return true;   // Success
      }
      catch (boost::bad_lexical_cast&)
      {
        LOG(ERROR) << "Metadata \"PixelDataOffset\" is corrupted for instance: " << instancePublicId;
      }
    }

    return false;
  }
  

  void ServerContext::ReadAttachment(std::string& result,
                                     const FileInfo& attachment,
                                     bool uncompressIfNeeded,
                                     bool skipCache)
  {
    std::unique_ptr<StorageAccessor> accessor;
      
    if (skipCache)
    {
      accessor.reset(new StorageAccessor(area_, GetMetricsRegistry()));
    }
    else
    {
      accessor.reset(new StorageAccessor(area_, storageCache_, GetMetricsRegistry()));
    }

    if (uncompressIfNeeded)
    {
      accessor->Read(result, attachment);
    }
    else
    {
      // Do not uncompress the content of the storage area, return the
      // raw data
      accessor->ReadRaw(result, attachment);
    }
  }

  void ServerContext::ReadAttachmentRange(std::string &result,
                                          const FileInfo &attachment,
                                          const StorageAccessor::Range &range,
                                          bool uncompressIfNeeded)
  {
    StorageAccessor accessor(area_, storageCache_, GetMetricsRegistry());
    accessor.ReadRange(result, attachment, range, uncompressIfNeeded);
  }


  ServerContext::DicomCacheLocker::DicomCacheLocker(ServerContext& context,
                                                    const std::string& instancePublicId) :
    context_(context),
    instancePublicId_(instancePublicId)
  {
    accessor_.reset(new ParsedDicomCache::Accessor(context_.dicomCache_, instancePublicId));
    
    if (!accessor_->IsValid())
    {
      accessor_.reset(NULL);

      // Throttle to avoid loading several large DICOM files simultaneously
      largeDicomLocker_.reset(new Semaphore::Locker(context.largeDicomThrottler_));
      
      context_.ReadDicom(buffer_, instancePublicId_);

      // Release the throttle if loading "small" DICOM files (under
      // 50MB, which is an arbitrary value)
      if (buffer_.size() < 50 * 1024 * 1024)
      {
        largeDicomLocker_.reset(NULL);
      }
      
      dicom_.reset(new ParsedDicomFile(buffer_));
      dicomSize_ = buffer_.size();
    }

    assert(accessor_.get() != NULL ||
           dicom_.get() != NULL);
  }


  ServerContext::DicomCacheLocker::~DicomCacheLocker()
  {
    if (dicom_.get() != NULL)
    {
      try
      {
        context_.dicomCache_.Acquire(instancePublicId_, dicom_.release(), dicomSize_);
      }
      catch (OrthancException&)
      {
      }
    }
  }


  ParsedDicomFile& ServerContext::DicomCacheLocker::GetDicom() const
  {
    if (dicom_.get() != NULL)
    {
      return *dicom_;
    }
    else
    {
      assert(accessor_.get() != NULL);
      return accessor_->GetDicom();
    }
  }

  const std::string& ServerContext::DicomCacheLocker::GetBuffer()
  {
    if (buffer_.size() > 0)
    {
      return buffer_;
    }
    else
    {
      context_.ReadDicom(buffer_, instancePublicId_);
      return buffer_;
    }
  }
  
  void ServerContext::SetStoreMD5ForAttachments(bool storeMD5)
  {
    LOG(INFO) << "Storing MD5 for attachments: " << (storeMD5 ? "yes" : "no");
    storeMD5_ = storeMD5;
  }


  bool ServerContext::AddAttachment(int64_t& newRevision,
                                    const std::string& resourceId,
                                    FileContentType attachmentType,
                                    const void* data,
                                    size_t size,
                                    bool hasOldRevision,
                                    int64_t oldRevision,
                                    const std::string& oldMD5)
  {
    LOG(INFO) << "Adding attachment " << EnumerationToString(attachmentType) << " to resource " << resourceId;
    
    // TODO Should we use "gzip" instead?
    CompressionType compression = (compressionEnabled_ ? CompressionType_ZlibWithSize : CompressionType_None);

    StorageAccessor accessor(area_, storageCache_, GetMetricsRegistry());
    FileInfo attachment = accessor.Write(data, size, attachmentType, compression, storeMD5_);

    try
    {
      StoreStatus status = index_.AddAttachment(
        newRevision, attachment, resourceId, hasOldRevision, oldRevision, oldMD5);
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
    catch (OrthancException&)
    {
      // Fixed in Orthanc 1.9.6
      accessor.Remove(attachment);
      throw;
    }
  }


  bool ServerContext::DeleteResource(Json::Value& remainingAncestor,
                                     const std::string& uuid,
                                     ResourceType expectedType)
  {
    if (expectedType == ResourceType_Instance)
    {
      // remove the file from the DicomCache
      dicomCache_.Invalidate(uuid);
    }

    return index_.DeleteResource(remainingAncestor, uuid, expectedType);
  }


  void ServerContext::SignalChange(const ServerIndexChange& change)
  {
    if (change.GetResourceType() == ResourceType_Instance &&
        change.GetChangeType() == ChangeType_Deleted)
    {
      dicomCache_.Invalidate(change.GetPublicId());
    }
    
    pendingChanges_.Enqueue(change.Clone());
  }


#if ORTHANC_ENABLE_PLUGINS == 1
  void ServerContext::SetPlugins(OrthancPlugins& plugins)
  {
    boost::unique_lock<boost::shared_mutex> lock(listenersMutex_);

    plugins_ = &plugins;

    // TODO REFACTOR THIS
    listeners_.clear();
    listeners_.push_back(ServerListener(luaListener_, "Lua"));
    listeners_.push_back(ServerListener(plugins, "plugin"));
  }


  void ServerContext::ResetPlugins()
  {
    boost::unique_lock<boost::shared_mutex> lock(listenersMutex_);

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


  bool ServerContext::LookupOrReconstructMetadata(std::string& target,
                                                  const std::string& publicId,
                                                  ResourceType level,
                                                  MetadataType metadata)
  {
    // This is a backwards-compatibility function, that can
    // reconstruct metadata that were not generated by an older
    // release of Orthanc

    if (metadata == MetadataType_Instance_SopClassUid ||
        metadata == MetadataType_Instance_TransferSyntax)
    {
      if (index_.LookupMetadata(target, publicId, level, metadata))
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
          index_.OverwriteMetadata(publicId, metadata, target);
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
      return index_.LookupMetadata(target, publicId, level, metadata);
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





  ImageAccessor* ServerContext::DecodeDicomFrame(const std::string& publicId,
                                                 unsigned int frameIndex)
  {
    ServerContext::DicomCacheLocker locker(*this, publicId);
    std::unique_ptr<ImageAccessor> decoded(DecodeDicomFrame(locker.GetDicom(), locker.GetBuffer().c_str(), locker.GetBuffer().size(), frameIndex));

    if (decoded.get() == NULL)
    {
      OrthancConfiguration::ReaderLock configLock;
      if (configLock.GetConfiguration().IsWarningEnabled(Warnings_003_DecoderFailure))
      {
        LOG(WARNING) << "W003: Unable to decode frame " << frameIndex << " from instance " << publicId;
      }
      return NULL;
    }

    return decoded.release();
  }


  ImageAccessor* ServerContext::DecodeDicomFrame(const ParsedDicomFile& parsedDicom,
                                                 const void* buffer,  // actually the buffer that is the source of the ParsedDicomFile
                                                 size_t size,
                                                 unsigned int frameIndex)
  {
    std::unique_ptr<ImageAccessor> decoded;

    if (builtinDecoderTranscoderOrder_ == BuiltinDecoderTranscoderOrder_Before)
    {
      // Use Orthanc's built-in decoder

      try
      {
        decoded.reset(parsedDicom.DecodeFrame(frameIndex));
        if (decoded.get() != NULL)
        {
          return decoded.release();
        }
      }
      catch (OrthancException& e)
      { // ignore, we'll try other alternatives
      }
    }

#if ORTHANC_ENABLE_PLUGINS == 1
    if (HasPlugins() &&
        GetPlugins().HasCustomImageDecoder())
    {
      try
      {
        decoded.reset(GetPlugins().Decode(buffer, size, frameIndex));
      }
      catch (OrthancException& e)
      {
      }
      
      if (decoded.get() != NULL)
      {
        return decoded.release();
      }
      else if (builtinDecoderTranscoderOrder_ == BuiltinDecoderTranscoderOrder_After)
      {
        LOG(INFO) << "The installed image decoding plugins cannot handle an image, "
                  << "fallback to the built-in DCMTK decoder";
      }
    }
#endif

    if (builtinDecoderTranscoderOrder_ == BuiltinDecoderTranscoderOrder_After)
    {
      try
      { 
        decoded.reset(parsedDicom.DecodeFrame(frameIndex));
        if (decoded.get() != NULL)
        {
          return decoded.release();
        }
      }
      catch (OrthancException& e)
      {
        LOG(INFO) << e.GetDetails();
      }
    }

#if ORTHANC_ENABLE_PLUGINS == 1
    if (HasPlugins() && GetPlugins().HasCustomTranscoder())
    {
      LOG(INFO) << "The plugins and built-in image decoders failed to decode a frame, "
                << "trying to transcode the file to LittleEndianExplicit using the plugins.";
      DicomImage explicitTemporaryImage;
      DicomImage source;
      std::set<DicomTransferSyntax> allowedSyntaxes;

      source.SetExternalBuffer(buffer, size);
      allowedSyntaxes.insert(DicomTransferSyntax_LittleEndianExplicit);

      if (Transcode(explicitTemporaryImage, source, allowedSyntaxes, true))
      {
        std::unique_ptr<ParsedDicomFile> file(explicitTemporaryImage.ReleaseAsParsedDicomFile());
        return file->DecodeFrame(frameIndex);
      }
    }
#endif

    return NULL;
  }


  ImageAccessor* ServerContext::DecodeDicomFrame(const DicomInstanceToStore& dicom,
                                                 unsigned int frameIndex)
  {
    return DecodeDicomFrame(dicom.GetParsedDicomFile(),
                            dicom.GetBufferData(),
                            dicom.GetBufferSize(),
                            frameIndex);

  }


  ImageAccessor* ServerContext::DecodeDicomFrame(const void* dicom,
                                                 size_t size,
                                                 unsigned int frameIndex)
  {
    std::unique_ptr<ParsedDicomFile> instance(new ParsedDicomFile(dicom, size));
    return DecodeDicomFrame(*instance, dicom, size, frameIndex);
  }
  

  void ServerContext::StoreWithTranscoding(std::string& sopClassUid,
                                           std::string& sopInstanceUid,
                                           DicomStoreUserConnection& connection,
                                           const std::string& dicom,
                                           bool hasMoveOriginator,
                                           const std::string& moveOriginatorAet,
                                           uint16_t moveOriginatorId)
  {
    const void* data = dicom.empty() ? NULL : dicom.c_str();
    const RemoteModalityParameters& modality = connection.GetParameters().GetRemoteModality();

    if (!transcodeDicomProtocol_ ||
        !modality.IsTranscodingAllowed())
    {
      connection.Store(sopClassUid, sopInstanceUid, data, dicom.size(),
                       hasMoveOriginator, moveOriginatorAet, moveOriginatorId);
    }
    else
    {
      connection.Transcode(sopClassUid, sopInstanceUid, *this, data, dicom.size(), preferredTransferSyntax_,
                           hasMoveOriginator, moveOriginatorAet, moveOriginatorId);
    }
  }


  bool ServerContext::TranscodeWithCache(std::string& target,
                                         const std::string& source,
                                         const std::string& sourceInstanceId,
                                         const std::string& attachmentId,
                                         DicomTransferSyntax targetSyntax)
  {
    StorageCache::Accessor cacheAccessor(storageCache_);

    if (!cacheAccessor.FetchTranscodedInstance(target, attachmentId, targetSyntax))
    {
      IDicomTranscoder::DicomImage sourceDicom;
      sourceDicom.SetExternalBuffer(source);

      IDicomTranscoder::DicomImage targetDicom;
      std::set<DicomTransferSyntax> syntaxes;
      syntaxes.insert(targetSyntax);

      if (Transcode(targetDicom, sourceDicom, syntaxes, true))
      {
        cacheAccessor.AddTranscodedInstance(attachmentId, targetSyntax, reinterpret_cast<const char*>(targetDicom.GetBufferData()), targetDicom.GetBufferSize());
        target = std::string(reinterpret_cast<const char*>(targetDicom.GetBufferData()), targetDicom.GetBufferSize());
        return true;
      }

      return false;
    }

    return true;
  }


  bool ServerContext::Transcode(DicomImage& target,
                                DicomImage& source /* in, "GetParsed()" possibly modified */,
                                const std::set<DicomTransferSyntax>& allowedSyntaxes,
                                bool allowNewSopInstanceUid)
  {
    if (builtinDecoderTranscoderOrder_ == BuiltinDecoderTranscoderOrder_Before)
    {
      if (dcmtkTranscoder_->Transcode(target, source, allowedSyntaxes, allowNewSopInstanceUid))
      {
        return true;
      }
    }
      
#if ORTHANC_ENABLE_PLUGINS == 1
    if (HasPlugins() &&
        GetPlugins().HasCustomTranscoder())
    {
      if (GetPlugins().Transcode(target, source, allowedSyntaxes, allowNewSopInstanceUid))
      {
        return true;
      }
      else if (builtinDecoderTranscoderOrder_ == BuiltinDecoderTranscoderOrder_After)
      {
        LOG(INFO) << "The installed transcoding plugins cannot handle an image, "
                  << "fallback to the built-in DCMTK transcoder";
      }
    }
#endif

    if (builtinDecoderTranscoderOrder_ == BuiltinDecoderTranscoderOrder_After)
    {
      return dcmtkTranscoder_->Transcode(target, source, allowedSyntaxes, allowNewSopInstanceUid);
    }
    else
    {
      return false;
    }
  }

  const std::string& ServerContext::GetDeidentifiedContent(const DicomElement &element) const
  {
    static const std::string redactedContent = "*** POTENTIAL PHI ***";
    static const std::string emptyContent = "";

    const DicomTag& tag = element.GetTag();
    if (element.GetValue().IsSequence())
    {
      return emptyContent;
    }
    
    if (deidentifyLogs_ &&
        !element.GetValue().GetContent().empty() &&
        logsDeidentifierRules_.IsAlteredTag(tag))
    {
      return redactedContent;
    }
    else
    {
      return element.GetValue().GetContent();
    }
  }

  void ServerContext::SetAcceptedSopClasses(const std::list<std::string>& acceptedSopClasses,
                                            const std::set<std::string>& rejectedSopClasses)
  {
    boost::mutex::scoped_lock lock(dynamicOptionsMutex_);
    acceptedSopClasses_.clear();

    size_t count = 0;
    std::set<std::string> allDcmtkSopClassUids;
    std::set<std::string> shortDcmtkSopClassUids;

    // we actually take a list of default 120 most common storage SOP classes defined in DCMTK
    while (dcmLongSCUStorageSOPClassUIDs[count] != NULL)
    {
      shortDcmtkSopClassUids.insert(dcmLongSCUStorageSOPClassUIDs[count++]);
    }

    count = 0;
    while (dcmAllStorageSOPClassUIDs[count] != NULL)
    {
      allDcmtkSopClassUids.insert(dcmAllStorageSOPClassUIDs[count++]);
    }

    if (acceptedSopClasses.size() == 0)
    {
      // by default, include the short list first and then all the others
      for (std::set<std::string>::const_iterator it = shortDcmtkSopClassUids.begin(); it != shortDcmtkSopClassUids.end(); ++it)
      {
        acceptedSopClasses_.push_back(*it);
      }

      for (std::set<std::string>::const_iterator it = allDcmtkSopClassUids.begin(); it != allDcmtkSopClassUids.end(); ++it)
      {
        if (shortDcmtkSopClassUids.find(*it) == shortDcmtkSopClassUids.end()) // don't add the classes that we have already added
        {
          acceptedSopClasses_.push_back(*it);
        }
      }
    }
    else
    {
      std::set<std::string> addedSopClasses;

      for (std::list<std::string>::const_iterator it = acceptedSopClasses.begin(); it != acceptedSopClasses.end(); ++it)
      {
        if (it->find('*') != std::string::npos || it->find('?') != std::string::npos)
        {
          // if it contains wildcard, add all the matching SOP classes known by DCMTK
          boost::regex pattern(Toolbox::WildcardToRegularExpression(*it));

          for (std::set<std::string>::const_iterator itall = allDcmtkSopClassUids.begin(); itall != allDcmtkSopClassUids.end(); ++itall)
          {
            if (regex_match(*itall, pattern) && addedSopClasses.find(*itall) == addedSopClasses.end())
            {
              acceptedSopClasses_.push_back(*itall);
              addedSopClasses.insert(*itall);
            }
          }
        }
        else
        {
          // if it is a SOP Class UID, add it without checking if it is known by DCMTK
          acceptedSopClasses_.push_back(*it);
          addedSopClasses.insert(*it);
        }
      }
    }
    
    // now remove all rejected syntaxes
    if (rejectedSopClasses.size() > 0)
    {
      for (std::set<std::string>::const_iterator it = rejectedSopClasses.begin(); it != rejectedSopClasses.end(); ++it)
      {
        if (it->find('*') != std::string::npos || it->find('?') != std::string::npos)
        {
          // if it contains wildcard, get all the matching SOP classes known by DCMTK
          boost::regex pattern(Toolbox::WildcardToRegularExpression(*it));

          for (std::set<std::string>::const_iterator itall = allDcmtkSopClassUids.begin(); itall != allDcmtkSopClassUids.end(); ++itall)
          {
            if (regex_match(*itall, pattern))
            {
              acceptedSopClasses_.remove(*itall);
            }
          }
        }
        else
        {
          // if it is a SOP Class UID, remove it without checking if it is known by DCMTK
          acceptedSopClasses_.remove(*it);
        }
      }
    }
  }

  void ServerContext::GetAcceptedSopClasses(std::set<std::string>& sopClasses, size_t maxCount) const
  {
    sopClasses.clear();

    boost::mutex::scoped_lock lock(dynamicOptionsMutex_);

    size_t count = 0;
    std::list<std::string>::const_iterator it = acceptedSopClasses_.begin();

    while (it != acceptedSopClasses_.end() && (maxCount == 0 || count < maxCount))
    {
      sopClasses.insert(*it);
      count++;
      ++it;
    }
  }

  void ServerContext::GetAcceptedTransferSyntaxes(std::set<DicomTransferSyntax>& syntaxes) const
  {
    boost::mutex::scoped_lock lock(dynamicOptionsMutex_);
    syntaxes = acceptedTransferSyntaxes_;
  }
  

  void ServerContext::SetAcceptedTransferSyntaxes(const std::set<DicomTransferSyntax>& syntaxes)
  {
    boost::mutex::scoped_lock lock(dynamicOptionsMutex_);
    acceptedTransferSyntaxes_ = syntaxes;
  }


  void ServerContext::GetProposedStorageTransferSyntaxes(std::list<DicomTransferSyntax>& syntaxes) const
  {
    boost::mutex::scoped_lock lock(dynamicOptionsMutex_);
    
    // // TODO: investigate: actually, neither Orthanc 1.12.4 nor DCM4CHEE will accept to send a LittleEndianExplicit file
    // //                    while e.g., Jpeg-LS has been presented (and accepted) as the preferred TS for the C-Store SCP.
    // // if we have defined IngestTranscoding, let's propose this TS first to avoid any unnecessary transcoding
    // if (isIngestTranscoding_)
    // {
    //   syntaxes.push_back(ingestTransferSyntax_);
    // }
    
    // then, propose the default ones
    syntaxes.push_back(DicomTransferSyntax_LittleEndianExplicit);
    syntaxes.push_back(DicomTransferSyntax_LittleEndianImplicit);
  }
  

  bool ServerContext::IsUnknownSopClassAccepted() const
  {
    boost::mutex::scoped_lock lock(dynamicOptionsMutex_);
    return isUnknownSopClassAccepted_;
  }

  
  void ServerContext::SetUnknownSopClassAccepted(bool accepted)
  {
    boost::mutex::scoped_lock lock(dynamicOptionsMutex_);
    isUnknownSopClassAccepted_ = accepted;
  }

  int64_t ServerContext::GetServerUpTime() const
  {
    boost::posix_time::ptime nowUtc = boost::posix_time::second_clock::universal_time();
    boost::posix_time::time_duration elapsed = nowUtc - serverStartTimeUtc_;

    return elapsed.total_seconds();
  }
}
