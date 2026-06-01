/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
#include "../../OrthancFramework/Sources/Constants.h"
#include "../../OrthancFramework/Sources/DataSource/DataSourceReader.h"
#include "../../OrthancFramework/Sources/DataSource/DicomSequentialReader.h"
#include "../../OrthancFramework/Sources/DicomFormat/DicomElement.h"
#include "../../OrthancFramework/Sources/DicomFormat/DicomImageInformation.h"
#include "../../OrthancFramework/Sources/DicomFormat/DicomStreamReader.h"
#include "../../OrthancFramework/Sources/DicomNetworking/DicomStoreUserConnection.h"
#include "../../OrthancFramework/Sources/DicomParsing/DicomModification.h"
#include "../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"
#include "../../OrthancFramework/Sources/FileStorage/StorageAccessor.h"
#include "../../OrthancFramework/Sources/HttpServer/FilesystemHttpSender.h"
#include "../../OrthancFramework/Sources/HttpServer/HttpStreamTranscoder.h"
#include "../../OrthancFramework/Sources/JobsEngine/SetOfInstancesJob.h"
#include "../../OrthancFramework/Sources/Logging.h"
#include "../../OrthancFramework/Sources/MallocMemoryBuffer.h"
#include "../../OrthancFramework/Sources/MetricsRegistry.h"
#include "../../OrthancFramework/Sources/MultiThreading/ThreadPool.h"
#include "../../OrthancFramework/Sources/SerializationToolbox.h"
#include "../Plugins/Engine/OrthancPlugins.h"

#include "DicomInstanceToStore.h"
#include "OrthancConfiguration.h"
#include "OrthancRestApi/OrthancRestApi.h"
#include "OutgoingDicomInstance.h"
#include "ResourceFinder.h"
#include "Search/DatabaseLookup.h"
#include "ServerJobs/OrthancJobUnserializer.h"
#include "ServerToolbox.h"
#include "ServerTranscoder.h"
#include "SimpleInstanceOrdering.h"
#include "StorageCommitmentReports.h"

#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmnet/dimse.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcuid.h>        /* for variable dcmAllStorageSOPClassUIDs */

#include <boost/regex.hpp>

#if HAVE_MALLOC_TRIM == 1
#  include <malloc.h>
#endif


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
    Logging::ScopedThreadNameSetter setter("MEMORY-TRIM");

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
          malloc_trim(static_cast<size_t>(128) * 1024);
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
    Logging::ScopedThreadNameSetter setter("CHANGES");

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
    Logging::ScopedThreadNameSetter setter("JOB-EVENTS");

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
    Logging::ScopedThreadNameSetter setter("SAVE-JOBS");

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
      static boost::posix_time::ptime lastSerializedModification = boost::posix_time::neg_infin;
      boost::posix_time::ptime lastModification = boost::posix_time::neg_infin;

      jobsEngine_.GetRegistry().GetLastModificationTime(lastModification);

      if (lastModification > lastSerializedModification)
      {
        LOG(TRACE) << "Serializing the content of the jobs engine";
      
        try
        {
          Json::Value value;
          jobsEngine_.GetRegistry().Serialize(value);

          std::string serialized;
          Toolbox::WriteFastJson(serialized, value);

          index_.SetGlobalProperty(GlobalProperty_JobsRegistry, false /* not shared */, serialized);
          lastSerializedModification = lastModification;
        }
        catch (OrthancException& e)
        {
          LOG(ERROR) << "Cannot serialize the jobs engine: " << e.What();
        }
      }
    }
  }


  void ServerContext::PublishCacheMetrics()
  {
    uint64_t tasksMaximumMemory;
    uint64_t tasksCurrentMemory;
    unsigned int tasksReservations;
    size_t cacheCapacity;
    size_t cacheCurrentCount;
    size_t cacheCurrentSize;

    storageAreaReader_->GetStatistics(tasksMaximumMemory, tasksCurrentMemory, tasksReservations,
                                      cacheCapacity, cacheCurrentCount, cacheCurrentSize);
    metricsRegistry_->SetFloatValue("orthanc_storage_cache_size_mb",
                                    static_cast<float>(cacheCurrentSize) / static_cast<float>(MEGABYTE));
    metricsRegistry_->SetIntegerValue("orthanc_storage_cache_count", cacheCurrentCount);

    dicomReader_->GetStatistics(tasksMaximumMemory, tasksCurrentMemory, tasksReservations,
                                cacheCapacity, cacheCurrentCount, cacheCurrentSize);
    metricsRegistry_->SetFloatValue("orthanc_dicom_cache_size_mb",
                                    static_cast<float>(cacheCurrentSize) / static_cast<float>(MEGABYTE));
    metricsRegistry_->SetIntegerValue("orthanc_dicom_cache_count", cacheCurrentCount);

    transcoderReader_->GetStatistics(tasksMaximumMemory, tasksCurrentMemory, tasksReservations,
                                     cacheCapacity, cacheCurrentCount, cacheCurrentSize);
    metricsRegistry_->SetFloatValue("orthanc_transcoder_cache_size_mb",
                                    static_cast<float>(cacheCurrentSize) / static_cast<float>(MEGABYTE));
    metricsRegistry_->SetIntegerValue("orthanc_transcoder_cache_count", cacheCurrentCount);
  }


  ServerContext::ServerContext(IDatabaseWrapper& database,
                               IPluginStorageArea& area,
                               ServerTranscoder* transcoder /* takes ownership */,
                               bool unitTesting,
                               size_t maxCompletedJobs,
                               bool readOnly) :
    index_(*this, database, (unitTesting ? 20 : 500), readOnly),
    area_(area),
    compressionEnabled_(false),
    storeMD5_(true),
    largeDicomThrottler_(1),
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
    overwriteInstances_(OverwriteInstancesMode_Never),
    transcoder_(transcoder),
    transcodeDicomProtocol_(true),
    isIngestTranscoding_(false),
    ingestTranscodingOfUncompressed_(true),
    ingestTranscodingOfCompressed_(true),
    preferredTransferSyntax_(DicomTransferSyntax_LittleEndianExplicit),
    readOnly_(readOnly),
    patientLevelEnabled_(true),
    deidentifyLogs_(false),
    serverStartTimeUtc_(boost::posix_time::second_clock::universal_time())
  {
    try
    {
      bool checkMD5 = true;

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

        std::string s;
        if (lock.GetConfiguration().LookupStringParameter(s, ORTHANC_CONFIG_INGEST_TRANSCODING))
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

        static const char* const REJECTED_SOP_CLASS = "RejectedSopClasses";
        if (lock.GetJson().isMember(REJECTED_SOP_CLASS))
        {
          lock.GetConfiguration().GetSetOfStringsParameter(rejectedSopClasses, REJECTED_SOP_CLASS);
        }
        else
        {
          static const char* const REJECT_SOP_CLASS = "RejectSopClasses";
          if (lock.GetJson().isMember(REJECT_SOP_CLASS))
          {
            /**
             * This is for backward compatibility. In Orthanc 1.12.6,
             * there was a typo: "RejectedSopClasses" was spelled as
             * "RejectSopClasses".
             * https://discourse.orthanc-server.org/t/fix-for-config-param-rejectsopclasses-vs-rejectedsopclasses/5900
             **/
            lock.GetConfiguration().GetSetOfStringsParameter(rejectedSopClasses, REJECT_SOP_CLASS);
          }
        }

        SetAcceptedSopClasses(acceptedSopClasses, rejectedSopClasses);

        defaultDicomRetrieveMethod_ = StringToRetrieveMethod(lock.GetConfiguration().GetDicomDefaultRetrieveMethod());

        checkMD5 = lock.GetConfiguration().GetBooleanParameter("CheckMD5", true);  // TODO-Streaming - Parameter
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

      // For streaming
      {
        boost::shared_ptr<ThreadPool> pool(new ThreadPool);
        pool->SetCountThreads(4);  // TODO-Streaming - Parameter
        pool->SetLoggingThreadName("STORAGE");
        pool->SetDequeueTimeout(100);
        pool->Start();

        storageAreaReader_.reset(new DataSourceReader(pool, new StorageAreaDataSource(area_, checkMD5)));
        storageAreaReader_->SetCapacity(1);  // Put highest pressure
        //storageAreaReader_->SetCapacity(1 * GIGABYTE);  // 1 GB - TODO-Streaming - Parameter
        storageAreaReader_->CreateCache(256 * MEGABYTE); // 256 MB - TODO-Streaming - Parameter
      }

      {
        boost::shared_ptr<ThreadPool> pool(new ThreadPool);
        pool->SetCountThreads(2);  // TODO-Streaming - Parameter
        pool->SetLoggingThreadName("DICOM");
        pool->SetDequeueTimeout(100);
        pool->Start();

        dicomReader_.reset(new DataSourceReader(pool, new DicomDataSource(storageAreaReader_)));
        dicomReader_->SetCapacity(1);  // Put highest pressure
        //dicomReader_->SetCapacity(1 * GIGABYTE);  // 1 GB - TODO-Streaming - Parameter
        dicomReader_->CreateCache(32 * MEGABYTE); // 256 MB - TODO-Streaming - Parameter
      }

      if (transcoder_.get() != NULL)
      {
        transcoderThreadPool_.reset(new ThreadPool);
        transcoderThreadPool_->SetCountThreads(2);  // TODO-Streaming - Parameter
        transcoderThreadPool_->SetLoggingThreadName("TRANSCODER");
        transcoderThreadPool_->SetDequeueTimeout(100);
        transcoderThreadPool_->Start();

        transcoderReader_.reset(new DataSourceReader(transcoderThreadPool_, new TranscoderDataSource(transcoder_, storageAreaReader_)));
        transcoderReader_->SetCapacity(1);  // Put highest pressure
        //transcoderReader_->SetCapacity(1 * GIGABYTE);  // 1 GB - TODO-Streaming - Parameter
        transcoderReader_->CreateCache(256 * MEGABYTE); // 256 MB - TODO-Streaming - Parameter
      }

      {
        unsigned int loaderThreads;

        {
          OrthancConfiguration::ReaderLock lock;
          loaderThreads = lock.GetConfiguration().GetLoaderThreads();
        }

        std::unique_ptr<ThreadPool> pool(new ThreadPool);
        pool->SetCountThreads(loaderThreads);  // TODO-Streaming - Check
        pool->SetLoggingThreadName("READER");
        pool->SetDequeueTimeout(100);
        pool->Start();

        boost::shared_ptr<IExecutorService> executor(pool.release());
        dicomSequentialReaderFactory_.reset(new DicomSequentialReader::Factory(
                                              executor, storageAreaReader_, dicomReader_, transcoderReader_,
                                              4 /* TODO-Streaming: Parameter */,
                                              0  /* TODO-Streaming: Parameter */));
      }
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


  void ServerContext::SetPatientLevelEnabled(bool enabled)
  {
    if (enabled)
      LOG(WARNING) << "Patient level is enabled";
    else
      LOG(WARNING) << "Patient level  is disabled";

    patientLevelEnabled_ = enabled;
  }


  void ServerContext::RemoveFile(const std::string& fileUuid,
                                 FileContentType type,
                                 const std::string& customData)
  {
    StorageAccessor accessor(area_, GetMetricsRegistry());
    accessor.Remove(fileUuid, type, customData);
  }


  ServerContext::StoreResult ServerContext::StoreAfterTranscoding(std::string& resultPublicId,
                                                                  DicomInstanceToStore& dicom,
                                                                  bool isReconstruct)
  {
    FileInfo adoptedFileNotUsed;

    return StoreAfterTranscoding(resultPublicId,
                                 dicom,
                                 isReconstruct,
                                 false,
                                 adoptedFileNotUsed);
  }

  ServerContext::StoreResult ServerContext::AdoptDicomInstance(std::string& resultPublicId,
                                                               DicomInstanceToStore& dicom,
                                                               const FileInfo& adoptedFile)
  {
    return StoreAfterTranscoding(resultPublicId,
                                 dicom,
                                 false,
                                 true,
                                 adoptedFile);
  }


  ServerContext::StoreResult ServerContext::StoreAfterTranscoding(std::string& resultPublicId,
                                                                  DicomInstanceToStore& dicom,
                                                                  bool isReconstruct,
                                                                  bool isAdoption,
                                                                  const FileInfo& adoptedFile)
  {
    OverwriteInstancesMode overwriteMode = overwriteInstances_;
    bool overwriteInDb = IsOverwriteInstances();

    if (isReconstruct)
    {
      overwriteMode = OverwriteInstancesMode_Always;
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
      StorageAccessor accessor(area_, GetMetricsRegistry());

      DicomInstanceHasher hasher(summary);
      resultPublicId = hasher.HashInstance();

      StoreResult result;
      std::string dicomMd5;

      // check if the instance already exists or is identical to quit as soon as possible to avoid unnecessary disk and DB operations
      if (overwriteMode == OverwriteInstancesMode_IfChanged || overwriteMode == OverwriteInstancesMode_Never)
      {
        FileInfo existingDicomAttachment;
        int64_t revision;

        if (index_.LookupAttachmentNoThrowIfResourceNotFound(existingDicomAttachment, revision, ResourceType_Instance, resultPublicId, FileContentType_Dicom))  // avoid throwing exceptions in the happy path (the instance not existing is the nominal case !)
        {
          if (overwriteMode == OverwriteInstancesMode_IfChanged)
          {
            assert(storeMD5_); // this is supposed to be enforced in the configuration
            Toolbox::ComputeMD5(dicomMd5, dicom.GetBufferData(), dicom.GetBufferSize());

            if (existingDicomAttachment.GetUncompressedMD5() == dicomMd5)
            {
              LOG(INFO) << "An incoming instance " << resultPublicId << " has been discarded because it is already stored with the same MD5";
              result.SetStatus(StoreStatus_AlreadyStored);
              result.SetCStoreStatusCode(STATUS_Success); // this is equivalent to a 'success' since the same instance is already stored
              return result;
            }
          }
          else if (overwriteMode == OverwriteInstancesMode_Never)
          {
            LOG(INFO) << "An incoming instance " << resultPublicId << " has been discarded because it is already stored";
            result.SetStatus(StoreStatus_AlreadyStored);
            result.SetCStoreStatusCode(STATUS_Success); // this is equivalent to a 'success' since the same instance is already stored
            return result;
          }
        }
      }

      Json::Value dicomAsJson;
      dicom.GetDicomAsJson(dicomAsJson, allMainDicomTags);  // don't crop any main dicom tags

      Json::Value simplifiedTags;
      Toolbox::SimplifyDicomAsJson(simplifiedTags, dicomAsJson, DicomToJsonFormat_Human);

      // Test if the instance must be filtered out

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

      // TODO Should we use "gzip" instead?
      CompressionType compression = (compressionEnabled_ ? CompressionType_ZlibWithSize : CompressionType_None);

      StatelessDatabaseOperations::Attachments attachments;
      FileInfo dicomInfo;

      if (!isAdoption)
      {
        accessor.Write(dicomInfo, dicom.GetBufferData(), dicom.GetBufferSize(), FileContentType_Dicom, compression, dicomMd5, storeMD5_, &dicom);
        attachments.push_back(dicomInfo);
      }
      else
      {
        attachments.push_back(adoptedFile);
      }

      FileInfo dicomUntilPixelData;
      if (hasPixelDataOffset &&
          (!area_.HasEfficientReadRange() ||
           compressionEnabled_))
      {
        accessor.Write(dicomUntilPixelData, dicom.GetBufferData(), pixelDataOffset, FileContentType_DicomUntilPixelData, compression, storeMD5_, NULL);
        attachments.push_back(dicomUntilPixelData);
      }

      typedef std::map<MetadataType, std::string>  InstanceMetadata;
      InstanceMetadata  instanceMetadata;

      try 
      {
        result.SetStatus(index_.Store(
          instanceMetadata, summary, attachments, dicom.GetMetadata(), dicom.GetOrigin(), overwriteInDb,
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
        if (!isAdoption)
        {
          accessor.Remove(dicomInfo);
        }

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
            if (isAdoption)
            {
              LOG(INFO) << "New instance adopted (" << resultPublicId << ")";
            }
            else
            {
              LOG(INFO) << "New instance stored (" << resultPublicId << ")";
            }
            break;

          case StoreStatus_AlreadyStored:
            LOG(INFO) << "Instance already stored (" << resultPublicId << ")";
            break;

          case StoreStatus_Failure:
            LOG(ERROR) << "Unknown store failure while storing instance " << resultPublicId;
            throw OrthancException(ErrorCode_InternalError).SetHttpStatus(HttpStatus_500_InternalServerError);

          case StoreStatus_StorageFull:
            LOG(ERROR) << "Storage full while storing instance " << resultPublicId;
            throw OrthancException(ErrorCode_FullStorage).SetHttpStatus(HttpStatus_507_InsufficientStorage);

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
        LOG(ERROR) << summary.FormatMissingTagsForStore();
      }
      
      throw;
    }
  }


  ServerContext::StoreResult ServerContext::Store(std::string& resultPublicId,
                                                  DicomInstanceToStore& receivedDicom)
  { 
    DicomInstanceToStore* dicom = &receivedDicom;

#if ORTHANC_ENABLE_PLUGINS == 1
    // WARNING: The scope of "modifiedBuffer" and "modifiedDicom" must
    // be the same as that of "dicom"
    PluginMemoryBuffer64 modifiedBuffer;
    std::unique_ptr<DicomInstanceToStore> modifiedDicom;

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

    return TranscodeAndStore(resultPublicId, dicom);
  }

  ServerContext::StoreResult ServerContext::TranscodeAndStore(std::string& resultPublicId,
                                                              DicomInstanceToStore* dicom,
                                                              bool isReconstruct)
  {

    if (!isIngestTranscoding_ || dicom->SkipIngestTranscoding())
    {
      // No automated transcoding. This was the only path in Orthanc <= 1.6.1.
      return StoreAfterTranscoding(resultPublicId, *dicom, isReconstruct);
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
        
        // If the DICOM does not have any pixel data (e.g. a DICOM SR, an ECG, a PDF, RTSTRUCT, ...), it makes no sense
        // to transcode it to a compressed Transfer Syntax and it might actually be considered invalid or poorly handled
        // by some softwares receiving the data.  Therefore, we skip transcoding in this case unless
        // the ingestTransferSyntax is uncompressed in which case, we might just want to change e.g. from Implicit to Explicit.
        if (transcode && !dicom->HasPixelData() && !IsUncompressedTransferSyntax(ingestTransferSyntax_))
        {
          transcode = false;
        }
      }
      else
      {
        // This is an compressed transfer syntax (new in Orthanc 1.8.2)
        transcode = ingestTranscodingOfCompressed_;
      }

      if (!transcode)
      {
        // No transcoding
        return StoreAfterTranscoding(resultPublicId, *dicom, isReconstruct);
      }
      else
      {
        // Trancoding
        std::set<DicomTransferSyntax> syntaxes;
        syntaxes.insert(ingestTransferSyntax_);
        
        IDicomTranscoder::DicomImage source;
        source.SetExternalBuffer(dicom->GetBufferData(), dicom->GetBufferSize());
        
        IDicomTranscoder::DicomImage transcoded;
        
        if (dicom->HasPixelData())
        {// check that the target image has a valid/reasonable size before transcoding to avoid possible crash or OOB during transcoding
          DicomMap summary;
          dicom->GetSummary(summary);

          DicomImageInformation imageInfo(summary);
          imageInfo.ThrowIfInvalidFrameSize();
        }

        if (GetTranscoder()->Transcode(transcoded, source, syntaxes, TranscodingSopInstanceUidMode_AllowNew /* allow new SOP instance UID */))
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

          StoreResult result = StoreAfterTranscoding(resultPublicId, *toStore, isReconstruct);
          assert(resultPublicId == tmp->GetHasher().HashInstance());

          return result;
        }
        else
        {
          // Cannot transcode => store the original file
          return StoreAfterTranscoding(resultPublicId, *dicom, isReconstruct);
        }
      }
    }
  }

  
  void ServerContext::AnswerAttachment(RestApiOutput& output,
                                       const FileInfo& attachment,
                                       const std::string& filename)
  {
    std::unique_ptr<StorageAreaDataSource::Range> range(ReadAttachment(attachment, true /* uncompress */));

    BufferHttpSender sender;
    sender.SetBuffer(range->GetData(), range->GetSize());
    sender.SetContentType(GetFileContentMime(attachment.GetContentType()));
    sender.SetContentFilename(filename.c_str());

    HttpStreamTranscoder transcoder(sender, CompressionType_None);
    output.AnswerStream(transcoder);
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


    StorageAccessor accessor(area_, GetMetricsRegistry());

    FileInfo modified;

    {
      std::unique_ptr<StorageAreaDataSource::Range> range(ReadAttachment(attachment, true /* uncompress */));
      accessor.Write(modified, range->GetData(), range->GetSize(), attachmentType, compression, storeMD5_, NULL);
    }

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
      std::unique_ptr<DicomDataSource::Dicom> dicom(
        DicomDataSource::Execute(*dicomReader_, DicomDataSource::CreateWholeRequest(attachment)));

      {
        DicomDataSource::Dicom::Lock lock(*dicom);
        OrthancConfiguration::DefaultDicomDatasetToJson(result, lock.GetContent(), ignoreTagLength);
      }

      InjectEmptyPixelData(result);
    }
    else
    {
      /**
       * The truncated DICOM file is not stored as a standalone
       * attachment. Lookup whether the pixel data offset has already
       * been computed for this instance.
       **/
    
      bool hasPixelDataOffset = false;
      uint64_t pixelDataOffset = 0;  // dummy initialization

      {
        std::string s;
        if (LookupMetadata(s, MetadataType_Instance_PixelDataOffset, instanceMetadata)) // This instance was created by a version of Orthanc > 1.9.0
        {
          if (!s.empty())
          {
            hasPixelDataOffset = SerializationToolbox::ParseUnsignedInteger64(pixelDataOffset, s);
          }

          if (!hasPixelDataOffset)
          {
            LOG(ERROR) << "Metadata \"PixelDataOffset\" is corrupted for instance: " << instancePublicId;
          }
        }
      }

      if (hasPixelDataOffset &&
          area_.HasEfficientReadRange() &&
          LookupAttachment(attachment, FileContentType_Dicom, instanceAttachments) &&
          attachment.GetCompressionType() == CompressionType_None)
      {
        /**
         * CASE 2: The pixel data offset is known, AND that a range read
         * can be used to retrieve the truncated DICOM file. Note that
         * this case cannot be used if "StorageCompression" option is
         * "true".
         **/

        std::unique_ptr<DicomDataSource::Dicom> dicom(
          DicomDataSource::Execute(*dicomReader_, DicomDataSource::CreateUntilPixelDataRequest(attachment, pixelDataOffset)));

        {
          DicomDataSource::Dicom::Lock lock(*dicom);
          OrthancConfiguration::DefaultDicomDatasetToJson(result, lock.GetContent(), ignoreTagLength);
        }
        
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

        std::unique_ptr<StorageAreaDataSource::Range> dicomAsJson(ReadAttachment(attachment, true /* uncompress */));

        if (!Toolbox::ReadJson(result, dicomAsJson->GetData(), dicomAsJson->GetSize()))
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

        if (!LookupAttachment(attachment, FileContentType_Dicom, instanceAttachments))
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }

        std::unique_ptr<StorageAreaDataSource::Range> range(ReadAttachment(attachment, true /* uncompress */));

        // We don't use the cache, as this would complexify the code for a rare edge case
        ParsedDicomFile dicom(range->GetData(), range->GetSize());
        OrthancConfiguration::DefaultDicomDatasetToJson(result, dicom, ignoreTagLength);

        if (!hasPixelDataOffset)
        {
          /**
           * (*) The pixel data offset was never computed for this
           * instance, which indicates that it was created using
           * Orthanc <= 1.9.0, or that calls to
           * "LookupPixelDataOffset()" from earlier versions of
           * Orthanc have failed. Try again this precomputation now
           * for future calls.
           **/

          ValueRepresentation pixelDataVR;
          if (DicomStreamReader::LookupPixelDataOffset(
                pixelDataOffset, pixelDataVR, range->GetData(), range->GetSize()) &&
              pixelDataOffset < range->GetSize())
          {
            index_.OverwriteMetadata(instancePublicId, MetadataType_Instance_PixelDataOffset,
                                     boost::lexical_cast<std::string>(pixelDataOffset));

            if (!area_.HasEfficientReadRange() ||
                compressionEnabled_)
            {
              int64_t newRevision;
              AddAttachment(newRevision, instancePublicId, ResourceType_Instance, FileContentType_DicomUntilPixelData,
                            range->GetData(), pixelDataOffset,
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


  FileInfo ServerContext::LookupDicomForInstance(const std::string& instancePublicId)
  {
    FileInfo attachment;
    int64_t revision;

    if (index_.LookupAttachment(attachment, revision, ResourceType_Instance, instancePublicId, FileContentType_Dicom))
    {
      return attachment;
    }
    else
    {
      throw OrthancException(ErrorCode_InternalError,
                             "Unable to read attachment " + EnumerationToString(FileContentType_Dicom) +
                             " of instance " + instancePublicId);
    }
  }


  StorageAreaDataSource::Range* ServerContext::ReadAttachment(const FileInfo& attachment,
                                                              bool uncompress)
  {
    return StorageAreaDataSource::Execute(
      *storageAreaReader_, StorageAreaDataSource::CreateAttachmentRequest(attachment, uncompress));
  }


  StorageAreaDataSource::Range* ServerContext::ReadAttachment(const FileInfo& attachment,
                                                              const StorageRange& range,
                                                              bool uncompress)
  {
    return StorageAreaDataSource::ReadRange(*storageAreaReader_, attachment, range, uncompress);
  }


  StorageAreaDataSource::Range* ServerContext::ReadRawDicom(const std::string& instancePublicId)
  {
    return ReadAttachment(LookupDicomForInstance(instancePublicId), true /* uncompress */);
  }


  DicomDataSource::Dicom* ServerContext::ReadParsedDicom(const std::string& instancePublicId)
  {
    return DicomDataSource::Execute(*dicomReader_, DicomDataSource::CreateWholeRequest(LookupDicomForInstance(instancePublicId)));
  }


  DicomDataSource::Dicom* ServerContext::ReadDicomUntilPixelData(const std::string& instancePublicId)
  {
    FileInfo attachment;
    int64_t revision;  // Ignored

    if (index_.LookupAttachment(attachment, revision, ResourceType_Instance,
                                instancePublicId, FileContentType_DicomUntilPixelData))
    {
      return DicomDataSource::Execute(*dicomReader_, DicomDataSource::CreateWholeRequest(attachment));
    }
    else
    {
      attachment = LookupDicomForInstance(instancePublicId);

      std::string metadata;

      if (attachment.GetCompressionType() == CompressionType_None &&
          index_.LookupMetadata(metadata, instancePublicId, ResourceType_Instance,
                                MetadataType_Instance_PixelDataOffset) &&
          !metadata.empty())
      {
        uint64_t pixelDataOffset = 0;

        if (SerializationToolbox::ParseUnsignedInteger64(pixelDataOffset, metadata))
        {
          return DicomDataSource::Execute(*dicomReader_, DicomDataSource::CreateUntilPixelDataRequest(attachment, pixelDataOffset));
        }
        else
        {
          LOG(ERROR) << "Metadata \"PixelDataOffset\" is corrupted for instance: " << instancePublicId;
        }
      }

      // Fallback: The pixel data offset is not present or cannot be used, the whole DICOM file must be read
      return DicomDataSource::Execute(*dicomReader_, DicomDataSource::CreateWholeRequest(attachment));
    }
  }


  TranscoderDataSource::Transcoded* ServerContext::ReadTranscodedDicom(const std::string& instancePublicId,
                                                                       DicomTransferSyntax targetSyntax,
                                                                       TranscodingSopInstanceUidMode mode,
                                                                       bool hasLossyQuality,
                                                                       unsigned int lossyQuality)
  {
    const FileInfo& attachment = LookupDicomForInstance(instancePublicId);
    return TranscoderDataSource::Execute(*transcoderReader_, TranscoderDataSource::CreateRequest(
                                           attachment, targetSyntax, mode, hasLossyQuality, lossyQuality));
  }


  void ServerContext::SetStoreMD5ForAttachments(bool storeMD5)
  {
    LOG(INFO) << "Storing MD5 for attachments: " << (storeMD5 ? "yes" : "no");
    storeMD5_ = storeMD5;
  }


  bool ServerContext::AddAttachment(int64_t& newRevision,
                                    const std::string& resourceId,
                                    ResourceType resourceType,
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

    StorageAccessor accessor(area_, GetMetricsRegistry());

    assert(attachmentType != FileContentType_Dicom && attachmentType != FileContentType_DicomUntilPixelData); // this method can not be used to store instances

    FileInfo attachment;
    accessor.Write(attachment, data, size, attachmentType, compression, storeMD5_, NULL);

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
    return index_.DeleteResource(remainingAncestor, uuid, expectedType);
  }


  void ServerContext::SignalChange(const ServerIndexChange& change)
  {
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


  void ServerContext::GetOrderedChildInstances(std::vector<std::string>& instancesIds,
                                               std::vector<FileInfo>& filesInfo,
                                               const std::string& publicId,
                                               ResourceType level)
  {
    // don't mix series and, inside a series, order the instances per instance number
    switch (level)
    {
      case ResourceType_Patient:
      case ResourceType_Study:
      {
        std::list<std::string> childrenIds;
        GetIndex().GetChildren(childrenIds, level, publicId);

        for (std::list<std::string>::const_iterator it = childrenIds.begin(); it != childrenIds.end(); ++it)
        {
          GetOrderedChildInstances(instancesIds, filesInfo, *it, GetChildResourceType(level));
        }
      }; break;
      case ResourceType_Series:
      {
        SimpleInstanceOrdering orderedInstances(GetIndex(), publicId);

        instancesIds.reserve(instancesIds.size() + orderedInstances.GetInstancesCount());
        filesInfo.reserve(filesInfo.size() + orderedInstances.GetInstancesCount());
        
        for (size_t i = 0; i < orderedInstances.GetInstancesCount(); ++i)
        {
          instancesIds.push_back(orderedInstances.GetInstanceId(i));
          filesInfo.push_back(orderedInstances.GetInstanceFileInfo(i));
        }
      }; break;
      case ResourceType_Instance:
      {  
        FileInfo fileInfo;
        int64_t revisionNotUsed;

        if (GetIndex().LookupAttachment(fileInfo, revisionNotUsed, level, publicId, FileContentType_Dicom))
        {
          instancesIds.push_back(publicId);
          filesInfo.push_back(fileInfo);
        }
      }; break;
      default:
        throw OrthancException(ErrorCode_InternalError);
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
                                         const DicomConnectionInfo& connection)
  {
#if ORTHANC_ENABLE_PLUGINS == 1
    if (HasPlugins())
    {
      return GetPlugins().CreateStorageCommitment(
        jobId, transactionUid, sopClassUids, sopInstanceUids, connection);
    }
#endif

    return NULL;
  }


  ImageAccessor* ServerContext::DecodeDicomFrame(const std::string& instancePublicId,
                                                 unsigned int frameIndex)
  {
    FileInfo attachment;
    int64_t revision;

    if (!index_.LookupAttachment(attachment, revision, ResourceType_Instance, instancePublicId, FileContentType_Dicom))
    {
      throw OrthancException(ErrorCode_InternalError,
                             "Unable to read attachment " + EnumerationToString(FileContentType_Dicom) +
                             " of instance " + instancePublicId);
    }

    std::unique_ptr<ImageAccessor> decoded;

    decoded.reset(GetTranscoder()->DecodeFrame(dicomReader_, storageAreaReader_, transcoderReader_, attachment, frameIndex));

    if (decoded.get() == NULL)
    {
      OrthancConfiguration::ReaderLock configLock;
      if (configLock.GetConfiguration().IsWarningEnabled(Warnings_003_DecoderFailure))
      {
        LOG(WARNING) << "W003: Unable to decode frame " << frameIndex << " from instance " << instancePublicId;
      }

      throw OrthancException(ErrorCode_NotImplemented);
    }
    else
    {
      return decoded.release();
    }
  }


  void ServerContext::PerformCStoreWithTranscoding(std::string& sopClassUid,
                                                   std::string& sopInstanceUid,
                                                   DicomStoreUserConnection& connection,
                                                   const void* dicomData,
                                                   size_t dicomSize,
                                                   bool hasMoveOriginator,
                                                   const std::string& moveOriginatorAet,
                                                   uint16_t moveOriginatorId)
  {
    const RemoteModalityParameters& modality = connection.GetParameters().GetRemoteModality();

    // Filter out outgoing C-Store instances
    {
      boost::shared_lock<boost::shared_mutex> lock(listenersMutex_);

      std::unique_ptr<OutgoingDicomInstance> outgoingInstance(OutgoingDicomInstance::CreateFromBuffer(dicomData, dicomSize));
      outgoingInstance->SetDestination(DicomInstanceDestination(connection.GetParameters().GetRemoteModality().GetHost(),
                                                                connection.GetParameters().GetRemoteModality().GetApplicationEntityTitle()));

      Json::Value simplifiedTags;
      outgoingInstance->GetSimplifiedJson(simplifiedTags);

      for (ServerListeners::iterator it = listeners_.begin(); it != listeners_.end(); ++it)
      {
        try
        {
          if (!it->GetListener().FilterOutgoingCStoreInstance(*outgoingInstance, simplifiedTags))
          {
            return;
          }
        }
        catch (OrthancException& e)
        {
          LOG(ERROR) << "Error in the " << it->GetDescription()
                      << " callback while sending an instance: " << e.What()
                      << " (code " << e.GetErrorCode() << ")";
          throw;
        }
      }
    }

    if (!transcodeDicomProtocol_ ||
        !modality.IsTranscodingAllowed())
    {
      connection.Store(sopClassUid, sopInstanceUid, dicomData, dicomSize,
                       hasMoveOriginator, moveOriginatorAet, moveOriginatorId);
    }
    else
    {
      connection.Transcode(sopClassUid, sopInstanceUid, *transcoder_, dicomData, dicomSize, preferredTransferSyntax_,
                           hasMoveOriginator, moveOriginatorAet, moveOriginatorId);
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


  const boost::shared_ptr<ServerTranscoder>& ServerContext::GetTranscoder() const
  {
    if (transcoder_.get() != NULL)
    {
      return transcoder_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls, "No transcoder is available");
    }
  }


  DicomSequentialReader::Factory& ServerContext::GetDicomSequentialReaderFactory()
  {
    return *dicomSequentialReaderFactory_;
  }
}
