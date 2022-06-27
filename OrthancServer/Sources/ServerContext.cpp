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
#include "Search/DatabaseLookup.h"
#include "ServerJobs/OrthancJobUnserializer.h"
#include "ServerToolbox.h"
#include "StorageCommitmentReports.h"

#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmnet/dimse.h>


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
  static void ComputeStudyTags(ExpandedResource& resource,
                               ServerContext& context,
                               const std::string& studyPublicId,
                               const std::set<DicomTag>& requestedTags);


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

  
  void ServerContext::ChangeThread(ServerContext* that,
                                   unsigned int sleepDelay)
  {
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

#if ORTHANC_ENABLE_PLUGINS == 1
    if (HasPlugins())
    {
      GetPlugins().SignalJobSubmitted(jobId);
    }
#endif
  }
  

  void ServerContext::SignalJobSuccess(const std::string& jobId)
  {
    haveJobsChanged_ = true;
    mainLua_.SignalJobSuccess(jobId);

#if ORTHANC_ENABLE_PLUGINS == 1
    if (HasPlugins())
    {
      GetPlugins().SignalJobSuccess(jobId);
    }
#endif
  }

  
  void ServerContext::SignalJobFailure(const std::string& jobId)
  {
    haveJobsChanged_ = true;
    mainLua_.SignalJobFailure(jobId);

#if ORTHANC_ENABLE_PLUGINS == 1
    if (HasPlugins())
    {
      GetPlugins().SignalJobFailure(jobId);
    }
#endif
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


  void ServerContext::PublishDicomCacheMetrics()
  {
    metricsRegistry_->SetValue("orthanc_dicom_cache_size",
                               static_cast<float>(dicomCache_.GetCurrentSize()) / static_cast<float>(1024 * 1024));
    metricsRegistry_->SetValue("orthanc_dicom_cache_count",
                               static_cast<float>(dicomCache_.GetNumberOfItems()));
  }


  ServerContext::ServerContext(IDatabaseWrapper& database,
                               IStorageArea& area,
                               bool unitTesting,
                               size_t maxCompletedJobs) :
    index_(*this, database, (unitTesting ? 20 : 500)),
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
    overwriteInstances_(false),
    dcmtkTranscoder_(new DcmtkTranscoder),
    isIngestTranscoding_(false),
    ingestTranscodingOfUncompressed_(true),
    ingestTranscodingOfCompressed_(true),
    preferredTransferSyntax_(DicomTransferSyntax_LittleEndianExplicit),
    deidentifyLogs_(false)
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
              lock.GetConfiguration().GetStringParameter("DeidentifyLogsDicomVersion", "2021b"));
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
      }

      jobsEngine_.SetThreadSleep(unitTesting ? 20 : 200);

      listeners_.push_back(ServerListener(luaListener_, "Lua"));
      changeThread_ = boost::thread(ChangeThread, this, (unitTesting ? 20 : 100));
    
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
    hasPixelDataOffset = DicomStreamReader::LookupPixelDataOffset(
      pixelDataOffset, dicom.GetBufferData(), dicom.GetBufferSize());

    DicomTransferSyntax transferSyntax;
    bool hasTransferSyntax = dicom.LookupTransferSyntax(transferSyntax);
    
    DicomMap summary;
    dicom.GetSummary(summary);   // -> this includes only the leaf nodes

    std::set<DicomTag> allMainDicomTags = DicomMap::GetAllMainDicomTags();
    std::set<DicomTag> mainDicomSequences;
    DicomMap::ExtractSequences(mainDicomSequences, allMainDicomTags);
    DicomSequencesMap sequencesToStore;

    try
    {
      MetricsRegistry::Timer timer(GetMetricsRegistry(), "orthanc_store_dicom_duration_ms");
      StorageAccessor accessor(area_, storageCache_, GetMetricsRegistry());

      DicomInstanceHasher hasher(summary);
      resultPublicId = hasher.HashInstance();

      Json::Value dicomAsJson;    // -> this includes the sequences

      dicom.GetDicomAsJson(dicomAsJson, mainDicomSequences /*ignoreTagLength*/);  // make sure that sequences that we wish to store in DB are not 'cropped'
      sequencesToStore.FromDicomAsJson(dicomAsJson, mainDicomSequences);

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
      PublishDicomCacheMetrics();

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
      result.SetStatus(index_.Store(
        instanceMetadata, summary, sequencesToStore, attachments, dicom.GetMetadata(), dicom.GetOrigin(), overwrite,
        hasTransferSyntax, transferSyntax, hasPixelDataOffset, pixelDataOffset, isReconstruct));

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

    if (!isIngestTranscoding_)
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

          std::unique_ptr<DicomInstanceToStore> toStore(DicomInstanceToStore::CreateFromParsedDicomFile(*tmp));
          toStore->SetOrigin(dicom->GetOrigin());

          if (isReconstruct) // the initial instance to store already has its own metadata
          {
            toStore->CopyMetadata(dicom->GetMetadata());
          }

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
                                       const std::string& resourceId,
                                       FileContentType content)
  {
    FileInfo attachment;
    int64_t revision;
    if (!index_.LookupAttachment(attachment, revision, resourceId, content))
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }
    else
    {
      StorageAccessor accessor(area_, storageCache_, GetMetricsRegistry());
      accessor.AnswerFile(output, attachment, GetFileContentMime(content));
    }
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
    int64_t revision;
    if (!index_.LookupAttachment(attachment, revision, resourceId, attachmentType))
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

  
  void ServerContext::ReadDicomAsJson(Json::Value& result,
                                      const std::string& instancePublicId,
                                      const std::set<DicomTag>& ignoreTagLength)
  {
    /**
     * CASE 1: The DICOM file, truncated at pixel data, is available
     * as an attachment (it was created either because the storage
     * area does not support range reads, or if "StorageCompression"
     * is enabled). Simply return this attachment.
     **/
    
    FileInfo attachment;
    int64_t revision;  // Ignored

    if (index_.LookupAttachment(attachment, revision, instancePublicId, FileContentType_DicomUntilPixelData))
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
        if (index_.LookupMetadata(s, revision, instancePublicId, ResourceType_Instance,
                                  MetadataType_Instance_PixelDataOffset))
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
          index_.LookupAttachment(attachment, revision, instancePublicId, FileContentType_Dicom) &&
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
          accessor.ReadStartRange(dicom, attachment.GetUuid(), FileContentType_Dicom, pixelDataOffset);
        }
        
        assert(dicom.size() == pixelDataOffset);
        ParsedDicomFile parsed(dicom);
        OrthancConfiguration::DefaultDicomDatasetToJson(result, parsed, ignoreTagLength);
        InjectEmptyPixelData(result);
      }
      else if (ignoreTagLength.empty() &&
               index_.LookupAttachment(attachment, revision, instancePublicId, FileContentType_DicomAsJson))
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
          if (DicomStreamReader::LookupPixelDataOffset(pixelDataOffset, dicom) &&
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
                                const std::string& instancePublicId)
  {
    int64_t revision;
    ReadAttachment(dicom, revision, instancePublicId, FileContentType_Dicom, true /* uncompress */);
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
    if (!area_.HasReadRange())
    {
      return false;
    }
    
    FileInfo attachment;
    int64_t revision;  // Ignored
    if (!index_.LookupAttachment(attachment, revision, instancePublicId, FileContentType_Dicom))
    {
      throw OrthancException(ErrorCode_InternalError,
                             "Unable to read the DICOM file of instance " + instancePublicId);
    }

    std::string s;

    if (attachment.GetCompressionType() == CompressionType_None &&
        index_.LookupMetadata(s, revision, instancePublicId, ResourceType_Instance,
                              MetadataType_Instance_PixelDataOffset) &&
        !s.empty())
    {
      try
      {
        uint64_t pixelDataOffset = boost::lexical_cast<uint64_t>(s);

        StorageAccessor accessor(area_, storageCache_, GetMetricsRegistry());

        accessor.ReadStartRange(dicom, attachment.GetUuid(), attachment.GetContentType(), pixelDataOffset);
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
                                     int64_t& revision,
                                     const std::string& instancePublicId,
                                     FileContentType content,
                                     bool uncompressIfNeeded)
  {
    FileInfo attachment;
    if (!index_.LookupAttachment(attachment, revision, instancePublicId, content))
    {
      throw OrthancException(ErrorCode_InternalError,
                             "Unable to read attachment " + EnumerationToString(content) +
                             " of instance " + instancePublicId);
    }

    assert(attachment.GetContentType() == content);

    {
      StorageAccessor accessor(area_, storageCache_, GetMetricsRegistry());

      if (uncompressIfNeeded)
      {
        accessor.Read(result, attachment);
      }
      else
      {
        // Do not uncompress the content of the storage area, return the
        // raw data
        accessor.ReadRaw(result, attachment);
      }
    }
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
      
      std::string content;
      context_.ReadDicom(content, instancePublicId);

      // Release the throttle if loading "small" DICOM files (under
      // 50MB, which is an arbitrary value)
      if (content.size() < 50 * 1024 * 1024)
      {
        largeDicomLocker_.reset(NULL);
      }
      
      dicom_.reset(new ParsedDicomFile(content));
      dicomSize_ = content.size();
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
        context_.PublishDicomCacheMetrics();
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
      PublishDicomCacheMetrics();
    }

    return index_.DeleteResource(remainingAncestor, uuid, expectedType);
  }


  void ServerContext::SignalChange(const ServerIndexChange& change)
  {
    if (change.GetResourceType() == ResourceType_Instance &&
        change.GetChangeType() == ChangeType_Deleted)
    {
      dicomCache_.Invalidate(change.GetPublicId());
      PublishDicomCacheMetrics();
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


  void ServerContext::Apply(ILookupVisitor& visitor,
                            const DatabaseLookup& lookup,
                            ResourceType queryLevel,
                            size_t since,
                            size_t limit)
  {    
    unsigned int databaseLimit = (queryLevel == ResourceType_Instance ?
                                  limitFindInstances_ : limitFindResults_);
      
    std::vector<std::string> resources, instances;
    const DicomTagConstraint* dicomModalitiesConstraint = NULL;

    bool hasModalitiesInStudyLookup = (queryLevel == ResourceType_Study &&
          lookup.GetConstraint(dicomModalitiesConstraint, DICOM_TAG_MODALITIES_IN_STUDY) &&
          ((dicomModalitiesConstraint->GetConstraintType() == ConstraintType_Equal && !dicomModalitiesConstraint->GetValue().empty()) ||
          (dicomModalitiesConstraint->GetConstraintType() == ConstraintType_List && !dicomModalitiesConstraint->GetValues().empty())));

    std::unique_ptr<DatabaseLookup> fastLookup(lookup.Clone());
    
    if (hasModalitiesInStudyLookup)
    {
      fastLookup->RemoveConstraint(DICOM_TAG_MODALITIES_IN_STUDY);
    }

    {
      const size_t lookupLimit = (databaseLimit == 0 ? 0 : databaseLimit + 1);      
      GetIndex().ApplyLookupResources(resources, &instances, *fastLookup, queryLevel, lookupLimit);
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
      DicomMap allMainDicomTagsFromDB;
      
      if (findStorageAccessMode_ == FindStorageAccessMode_DatabaseOnly ||
          findStorageAccessMode_ == FindStorageAccessMode_DiskOnAnswer ||
          fastLookup->HasOnlyMainDicomTags())
      {
        // Case (1): The main DICOM tags, as stored in the database,
        // are sufficient to look for match

        if (!GetIndex().GetAllMainDicomTags(allMainDicomTagsFromDB, instances[i]))
        {
          // The instance has been removed during the execution of the
          // lookup, ignore it
          continue;
        }

        // New in Orthanc 1.6.0: Only keep the main DICOM tags at the
        // level of interest for the query
        switch (queryLevel)
        {
          // WARNING: Don't reorder cases below, and don't add "break"
          case ResourceType_Instance:
            dicom.MergeMainDicomTags(allMainDicomTagsFromDB, ResourceType_Instance);

          case ResourceType_Series:
            dicom.MergeMainDicomTags(allMainDicomTagsFromDB, ResourceType_Series);

          case ResourceType_Study:
            dicom.MergeMainDicomTags(allMainDicomTagsFromDB, ResourceType_Study);
            
          case ResourceType_Patient:
            dicom.MergeMainDicomTags(allMainDicomTagsFromDB, ResourceType_Patient);
            break;

          default:
            throw OrthancException(ErrorCode_InternalError);
        }
        
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
      
      if (fastLookup->IsMatch(dicom))
      {
        bool isMatch = true;

        if (hasModalitiesInStudyLookup)
        {
          std::set<DicomTag> requestedTags;
          requestedTags.insert(DICOM_TAG_MODALITIES_IN_STUDY);
          ExpandedResource resource;
          ComputeStudyTags(resource, *this, resources[i], requestedTags);

          std::vector<std::string> modalities;
          Toolbox::TokenizeString(modalities, resource.tags_.GetValue(DICOM_TAG_MODALITIES_IN_STUDY).GetContent(), '\\');
          bool hasAtLeastOneModalityMatching = false;
          for (size_t m = 0; m < modalities.size(); m++)
          {
            hasAtLeastOneModalityMatching |= dicomModalitiesConstraint->IsMatch(modalities[m]);
          }

          isMatch = isMatch && hasAtLeastOneModalityMatching;
          // copy the value of ModalitiesInStudy such that it can be reused to build the answer
          allMainDicomTagsFromDB.SetValue(DICOM_TAG_MODALITIES_IN_STUDY, resource.tags_.GetValue(DICOM_TAG_MODALITIES_IN_STUDY));
        }

        if (isMatch)
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
              visitor.Visit(resources[i], instances[i], allMainDicomTagsFromDB, dicomAsJson.get());
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
    }

    if (complete)
    {
      visitor.MarkAsComplete();
    }

    LOG(INFO) << "Number of matching resources: " << countResults;
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
      int64_t revision;  // Ignored
      if (index_.LookupMetadata(target, revision, publicId, level, metadata))
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
      int64_t revision;  // Ignored
      return index_.LookupMetadata(target, revision, publicId, level, metadata);
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
    if (builtinDecoderTranscoderOrder_ == BuiltinDecoderTranscoderOrder_Before)
    {
      // Use Orthanc's built-in decoder, using the cache to speed-up
      // things on multi-frame images

      std::unique_ptr<ImageAccessor> decoded;
      try
      {
        ServerContext::DicomCacheLocker locker(*this, publicId);
        decoded.reset(locker.GetDicom().DecodeFrame(frameIndex));
      }
      catch (OrthancException& e)
      {
      }
      
      if (decoded.get() != NULL)
      {
        return decoded.release();
      }
    }

#if ORTHANC_ENABLE_PLUGINS == 1
    if (HasPlugins() &&
        GetPlugins().HasCustomImageDecoder())
    {
      // TODO: Store the raw buffer in the DicomCacheLocker
      std::string dicomContent;
      ReadDicom(dicomContent, publicId);
      
      std::unique_ptr<ImageAccessor> decoded;
      try
      {
        decoded.reset(GetPlugins().Decode(dicomContent.c_str(), dicomContent.size(), frameIndex));
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
      ServerContext::DicomCacheLocker locker(*this, publicId);        
      return locker.GetDicom().DecodeFrame(frameIndex);
    }
    else
    {
      return NULL;  // Built-in decoder is disabled
    }
  }


  ImageAccessor* ServerContext::DecodeDicomFrame(const DicomInstanceToStore& dicom,
                                                 unsigned int frameIndex)
  {
    if (builtinDecoderTranscoderOrder_ == BuiltinDecoderTranscoderOrder_Before)
    {
      std::unique_ptr<ImageAccessor> decoded;
      try
      {
        decoded.reset(dicom.DecodeFrame(frameIndex));
      }
      catch (OrthancException& e)
      {
      }
        
      if (decoded.get() != NULL)
      {
        return decoded.release();
      }
    }

#if ORTHANC_ENABLE_PLUGINS == 1
    if (HasPlugins() &&
        GetPlugins().HasCustomImageDecoder())
    {
      std::unique_ptr<ImageAccessor> decoded;
      try
      {
        decoded.reset(GetPlugins().Decode(dicom.GetBufferData(), dicom.GetBufferSize(), frameIndex));
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
      return dicom.DecodeFrame(frameIndex);
    }
    else
    {
      return NULL;
    }
  }


  ImageAccessor* ServerContext::DecodeDicomFrame(const void* dicom,
                                                 size_t size,
                                                 unsigned int frameIndex)
  {
    std::unique_ptr<DicomInstanceToStore> instance(DicomInstanceToStore::CreateFromBuffer(dicom, size));
    return DecodeDicomFrame(*instance, frameIndex);
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
    
    if (!transcodeDicomProtocol_ ||
        !connection.GetParameters().GetRemoteModality().IsTranscodingAllowed())
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

    const DicomTag& tag = element.GetTag();
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


  void ServerContext::GetAcceptedTransferSyntaxes(std::set<DicomTransferSyntax>& syntaxes)
  {
    boost::mutex::scoped_lock lock(dynamicOptionsMutex_);
    syntaxes = acceptedTransferSyntaxes_;
  }
  

  void ServerContext::SetAcceptedTransferSyntaxes(const std::set<DicomTransferSyntax>& syntaxes)
  {
    boost::mutex::scoped_lock lock(dynamicOptionsMutex_);
    acceptedTransferSyntaxes_ = syntaxes;
  }


  bool ServerContext::IsUnknownSopClassAccepted()
  {
    boost::mutex::scoped_lock lock(dynamicOptionsMutex_);
    return isUnknownSopClassAccepted_;
  }

  
  void ServerContext::SetUnknownSopClassAccepted(bool accepted)
  {
    boost::mutex::scoped_lock lock(dynamicOptionsMutex_);
    isUnknownSopClassAccepted_ = accepted;
  }


  static void SerializeExpandedResource(Json::Value& target,
                                        const ExpandedResource& resource,
                                        DicomToJsonFormat format,
                                        const std::set<DicomTag>& requestedTags)
  {
    target = Json::objectValue;

    target["Type"] = GetResourceTypeText(resource.type_, false, true);
    target["ID"] = resource.id_;

    switch (resource.type_)
    {
      case ResourceType_Patient:
        break;

      case ResourceType_Study:
        target["ParentPatient"] = resource.parentId_;
        break;

      case ResourceType_Series:
        target["ParentStudy"] = resource.parentId_;
        break;

      case ResourceType_Instance:
        target["ParentSeries"] = resource.parentId_;
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    switch (resource.type_)
    {
      case ResourceType_Patient:
      case ResourceType_Study:
      case ResourceType_Series:
      {
        Json::Value c = Json::arrayValue;

        for (std::list<std::string>::const_iterator
                it = resource.childrenIds_.begin(); it != resource.childrenIds_.end(); ++it)
        {
          c.append(*it);
        }

        if (resource.type_ == ResourceType_Patient)
        {
          target["Studies"] = c;
        }
        else if (resource.type_ == ResourceType_Study)
        {
          target["Series"] = c;
        }
        else
        {
          target["Instances"] = c;
        }
        break;
      }

      case ResourceType_Instance:
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    switch (resource.type_)
    {
      case ResourceType_Patient:
      case ResourceType_Study:
        break;

      case ResourceType_Series:
        if (resource.expectedNumberOfInstances_ < 0)
        {
          target["ExpectedNumberOfInstances"] = Json::nullValue;
        }
        else
        {
          target["ExpectedNumberOfInstances"] = resource.expectedNumberOfInstances_;
        }
        target["Status"] = resource.status_;
        break;

      case ResourceType_Instance:
      {
        target["FileSize"] = static_cast<unsigned int>(resource.fileSize_);
        target["FileUuid"] = resource.fileUuid_;

        if (resource.indexInSeries_ < 0)
        {
          target["IndexInSeries"] = Json::nullValue;
        }
        else
        {
          target["IndexInSeries"] = resource.indexInSeries_;
        }

        break;
      }

      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    if (!resource.anonymizedFrom_.empty())
    {
      target["AnonymizedFrom"] = resource.anonymizedFrom_;
    }
    
    if (!resource.modifiedFrom_.empty())
    {
      target["ModifiedFrom"] = resource.modifiedFrom_;
    }

    if (resource.type_ == ResourceType_Patient ||
        resource.type_ == ResourceType_Study ||
        resource.type_ == ResourceType_Series)
    {
      target["IsStable"] = resource.isStable_;

      if (!resource.lastUpdate_.empty())
      {
        target["LastUpdate"] = resource.lastUpdate_;
      }
    }

    // serialize tags

    static const char* const MAIN_DICOM_TAGS = "MainDicomTags";
    static const char* const PATIENT_MAIN_DICOM_TAGS = "PatientMainDicomTags";

    DicomMap mainDicomTags;
    resource.tags_.ExtractResourceInformation(mainDicomTags, resource.type_);

    target[MAIN_DICOM_TAGS] = Json::objectValue;
    FromDcmtkBridge::ToJson(target[MAIN_DICOM_TAGS], mainDicomTags, format);
    
    if (resource.type_ == ResourceType_Study)
    {
      DicomMap patientMainDicomTags;
      resource.tags_.ExtractPatientInformation(patientMainDicomTags);

      target[PATIENT_MAIN_DICOM_TAGS] = Json::objectValue;
      FromDcmtkBridge::ToJson(target[PATIENT_MAIN_DICOM_TAGS], patientMainDicomTags, format);
    }

    if (requestedTags.size() > 0)
    {
      static const char* const REQUESTED_TAGS = "RequestedTags";

      DicomMap tags;
      resource.tags_.ExtractTags(tags, requestedTags);

      target[REQUESTED_TAGS] = Json::objectValue;
      FromDcmtkBridge::ToJson(target[REQUESTED_TAGS], tags, format);

      // add the sequences to the requested tags
      resource.sequences_.ToJson(target[REQUESTED_TAGS], format);
    }

  }


  static void ComputeInstanceTags(ExpandedResource& resource,
                                  ServerContext& context,
                                  const std::string& instancePublicId,
                                  const std::set<DicomTag>& requestedTags)
  {
    if (requestedTags.count(DICOM_TAG_INSTANCE_AVAILABILITY) > 0)
    {
      resource.tags_.SetValue(DICOM_TAG_INSTANCE_AVAILABILITY, "ONLINE", false);
      resource.missingRequestedTags_.erase(DICOM_TAG_INSTANCE_AVAILABILITY);
    }
  }


  static void ComputeSeriesTags(ExpandedResource& resource,
                                ServerContext& context,
                                const std::string& seriesPublicId,
                                const std::set<DicomTag>& requestedTags)
  {
    if (requestedTags.count(DICOM_TAG_NUMBER_OF_SERIES_RELATED_INSTANCES) > 0)
    {
      ServerIndex& index = context.GetIndex();
      std::list<std::string> instances;

      index.GetChildren(instances, seriesPublicId);

      resource.tags_.SetValue(DICOM_TAG_NUMBER_OF_SERIES_RELATED_INSTANCES,
                              boost::lexical_cast<std::string>(instances.size()), false);
      resource.missingRequestedTags_.erase(DICOM_TAG_NUMBER_OF_SERIES_RELATED_INSTANCES);
    }
  }

  static void ComputeStudyTags(ExpandedResource& resource,
                               ServerContext& context,
                               const std::string& studyPublicId,
                               const std::set<DicomTag>& requestedTags)
  {
    ServerIndex& index = context.GetIndex();
    std::list<std::string> series;
    std::list<std::string> instances;

    bool hasNbRelatedSeries = requestedTags.count(DICOM_TAG_NUMBER_OF_STUDY_RELATED_SERIES) > 0;
    bool hasNbRelatedInstances = requestedTags.count(DICOM_TAG_NUMBER_OF_STUDY_RELATED_INSTANCES) > 0;
    bool hasModalitiesInStudy = requestedTags.count(DICOM_TAG_MODALITIES_IN_STUDY) > 0;
    bool hasSopClassesInStudy = requestedTags.count(DICOM_TAG_SOP_CLASSES_IN_STUDY) > 0;

    index.GetChildren(series, studyPublicId);

    if (hasModalitiesInStudy)
    {
      std::set<std::string> values;

      for (std::list<std::string>::const_iterator
            it = series.begin(); it != series.end(); ++it)
      {
        DicomMap tags;
        index.GetMainDicomTags(tags, *it, ResourceType_Series, ResourceType_Series);

        const DicomValue* value = tags.TestAndGetValue(DICOM_TAG_MODALITY);

        if (value != NULL &&
            !value->IsNull() &&
            !value->IsBinary())
        {
          values.insert(value->GetContent());
        }
      }

      std::string modalities;
      Toolbox::JoinStrings(modalities, values, "\\");

      resource.tags_.SetValue(DICOM_TAG_MODALITIES_IN_STUDY, modalities, false);
      resource.missingRequestedTags_.erase(DICOM_TAG_MODALITIES_IN_STUDY);
    }

    if (hasNbRelatedSeries)
    {
      resource.tags_.SetValue(DICOM_TAG_NUMBER_OF_STUDY_RELATED_SERIES,
                              boost::lexical_cast<std::string>(series.size()), false);
      resource.missingRequestedTags_.erase(DICOM_TAG_NUMBER_OF_STUDY_RELATED_SERIES);
    }

    if (hasNbRelatedInstances || hasSopClassesInStudy)
    {
      for (std::list<std::string>::const_iterator
            it = series.begin(); it != series.end(); ++it)
      {
        std::list<std::string> seriesInstancesIds;
        index.GetChildren(seriesInstancesIds, *it);

        instances.splice(instances.end(), seriesInstancesIds);
      }

      if (hasNbRelatedInstances)
      {
        resource.tags_.SetValue(DICOM_TAG_NUMBER_OF_STUDY_RELATED_INSTANCES,
                                boost::lexical_cast<std::string>(instances.size()), false);      
        resource.missingRequestedTags_.erase(DICOM_TAG_NUMBER_OF_STUDY_RELATED_INSTANCES);
      }

      if (hasSopClassesInStudy)
      {
        std::set<std::string> values;

        for (std::list<std::string>::const_iterator
              it = instances.begin(); it != instances.end(); ++it)
        {
          std::string value;

          if (context.LookupOrReconstructMetadata(value, *it, ResourceType_Instance, MetadataType_Instance_SopClassUid))
          {
            values.insert(value);
          }
        }

        if (values.size() > 0)
        {
          std::string sopClassUids;
          Toolbox::JoinStrings(sopClassUids, values, "\\");
          resource.tags_.SetValue(DICOM_TAG_SOP_CLASSES_IN_STUDY, sopClassUids, false);
        }

        resource.missingRequestedTags_.erase(DICOM_TAG_SOP_CLASSES_IN_STUDY);
      }
    }
  }

  static void ComputePatientTags(ExpandedResource& resource,
                                 ServerContext& context,
                                 const std::string& patientPublicId,
                                 const std::set<DicomTag>& requestedTags)
  {
    ServerIndex& index = context.GetIndex();

    std::list<std::string> studies;
    std::list<std::string> series;
    std::list<std::string> instances;

    bool hasNbRelatedStudies = requestedTags.count(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_STUDIES) > 0;
    bool hasNbRelatedSeries = requestedTags.count(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_SERIES) > 0;
    bool hasNbRelatedInstances = requestedTags.count(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_INSTANCES) > 0;

    index.GetChildren(studies, patientPublicId);

    if (hasNbRelatedStudies)
    {
      resource.tags_.SetValue(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_STUDIES,
                              boost::lexical_cast<std::string>(studies.size()), false);
      resource.missingRequestedTags_.erase(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_STUDIES);
    }

    if (hasNbRelatedSeries || hasNbRelatedInstances)
    {
      for (std::list<std::string>::const_iterator
            it = studies.begin(); it != studies.end(); ++it)
      {
        std::list<std::string> thisSeriesIds;
        index.GetChildren(thisSeriesIds, *it);
        series.splice(series.end(), thisSeriesIds);
      }

      if (hasNbRelatedSeries)
      {
        resource.tags_.SetValue(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_SERIES,
                                boost::lexical_cast<std::string>(series.size()), false);
        resource.missingRequestedTags_.erase(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_SERIES);
      }
    }

    if (hasNbRelatedInstances)
    {
      for (std::list<std::string>::const_iterator
            it = series.begin(); it != series.end(); ++it)
      {
        std::list<std::string> thisInstancesIds;
        index.GetChildren(thisInstancesIds, *it);
        instances.splice(instances.end(), thisInstancesIds);
      }

      resource.tags_.SetValue(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_INSTANCES,
                              boost::lexical_cast<std::string>(instances.size()), false);
      resource.missingRequestedTags_.erase(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_INSTANCES);
    }
  }


  static void ComputeTags(ExpandedResource& resource,
                          ServerContext& context,
                          const std::string& resourceId,
                          ResourceType level,
                          const std::set<DicomTag>& requestedTags)
  {
    if (level == ResourceType_Patient 
        && DicomMap::HasComputedTags(resource.missingRequestedTags_, ResourceType_Patient))
    {
      ComputePatientTags(resource, context, resourceId, requestedTags);
    }

    if (level == ResourceType_Study 
        && DicomMap::HasComputedTags(resource.missingRequestedTags_, ResourceType_Study))
    {
      ComputeStudyTags(resource, context, resourceId, requestedTags);
    }

    if (level == ResourceType_Series 
        && DicomMap::HasComputedTags(resource.missingRequestedTags_, ResourceType_Series))
    {
      ComputeSeriesTags(resource, context, resourceId, requestedTags);
    }

    if (level == ResourceType_Instance 
        && DicomMap::HasComputedTags(resource.missingRequestedTags_, ResourceType_Instance))
    {
      ComputeInstanceTags(resource, context, resourceId, requestedTags);
    }
  }

  bool ServerContext::ExpandResource(Json::Value& target,
                                     const std::string& publicId,
                                     ResourceType level,
                                     DicomToJsonFormat format,
                                     const std::set<DicomTag>& requestedTags)
  {
    std::string unusedInstanceId;
    Json::Value* unusedDicomAsJson = NULL;
    DicomMap unusedMainDicomTags;

    return ExpandResource(target, publicId, unusedMainDicomTags, unusedInstanceId, unusedDicomAsJson, level, format, requestedTags);
  }

  bool ServerContext::ExpandResource(Json::Value& target,
                                     const std::string& publicId,
                                     const DicomMap& mainDicomTags,    // optional: the main dicom tags for the resource (if already available)
                                     const std::string& instanceId,    // optional: the id of an instance for the resource (if already available)
                                      const Json::Value* dicomAsJson,  // optional: the dicom-as-json for the resource (if already available)
                                     ResourceType level,
                                     DicomToJsonFormat format,
                                     const std::set<DicomTag>& requestedTags)
  {
    ExpandedResource resource;

    if (ExpandResource(resource, publicId, mainDicomTags, instanceId, dicomAsJson, level, requestedTags, ExpandResourceDbFlags_Default))
    {
      SerializeExpandedResource(target, resource, format, requestedTags);
      return true;
    }

    return false;
  }
  
  bool ServerContext::ExpandResource(ExpandedResource& resource,
                                     const std::string& publicId,
                                     const DicomMap& mainDicomTags,    // optional: the main dicom tags for the resource (if already available)
                                     const std::string& instanceId,    // optional: the id of an instance for the resource (if already available)
                                     const Json::Value* dicomAsJson,   // optional: the dicom-as-json for the resource (if already available)
                                     ResourceType level,
                                     const std::set<DicomTag>& requestedTags,
                                     ExpandResourceDbFlags expandFlags)
  {
    // first try to get the tags from what is already available
    
    if ((expandFlags & ExpandResourceDbFlags_IncludeMainDicomTags)
      && (mainDicomTags.GetSize() > 0)
      && (dicomAsJson != NULL))
    {
      
      resource.tags_.Merge(mainDicomTags);

      if (dicomAsJson->isObject())
      {
        resource.tags_.FromDicomAsJson(*dicomAsJson);
      }

      std::set<DicomTag> retrievedTags;
      std::set<DicomTag> missingTags;
      resource.tags_.GetTags(retrievedTags);

      Toolbox::GetMissingsFromSet(missingTags, requestedTags, retrievedTags);

      // if all possible tags have been read, no need to get them from DB anymore
      if (missingTags.size() == 0 || DicomMap::HasOnlyComputedTags(missingTags))
      {
        expandFlags = static_cast<ExpandResourceDbFlags>(expandFlags & ~ExpandResourceDbFlags_IncludeMainDicomTags);
      }

      if (missingTags.size() == 0 && expandFlags == ExpandResourceDbFlags_None)  // we have already retrieved anything we need
      {
        return true;
      }
    }

    if (expandFlags != ExpandResourceDbFlags_None
        && GetIndex().ExpandResource(resource, publicId, level, requestedTags, static_cast<ExpandResourceDbFlags>(expandFlags | ExpandResourceDbFlags_IncludeMetadata)))  // we always need the metadata to get the mainDicomTagsSignature
    {
      // check the main dicom tags list has not changed since the resource was stored
      if (resource.mainDicomTagsSignature_ != DicomMap::GetMainDicomTagsSignature(resource.type_))
      {
        OrthancConfiguration::ReaderLock lock;
        if (lock.GetConfiguration().IsWarningEnabled(Warnings_002_InconsistentDicomTagsInDb))
        {
          LOG(WARNING) << "W002: " << Orthanc::GetResourceTypeText(resource.type_, false , false) << " has been stored with another version of Main Dicom Tags list, you should POST to /" << Orthanc::GetResourceTypeText(resource.type_, true, false) << "/" << resource.id_ << "/reconstruct to update the list of tags saved in DB.  Some MainDicomTags might be missing from this answer.";
        }
      }

      // possibly merge missing requested tags from dicom-as-json
      if (!resource.missingRequestedTags_.empty() && !DicomMap::HasOnlyComputedTags(resource.missingRequestedTags_))
      {
        std::set<DicomTag> missingSequences;
        DicomMap::ExtractSequences(missingSequences, resource.missingRequestedTags_);

        OrthancConfiguration::ReaderLock lock;
        if (lock.GetConfiguration().IsWarningEnabled(Warnings_001_TagsBeingReadFromStorage))
        {
          std::set<DicomTag> missingTags;
          Toolbox::AppendSets(missingTags, resource.missingRequestedTags_);
          for (std::set<DicomTag>::const_iterator it = resource.missingRequestedTags_.begin(); it != resource.missingRequestedTags_.end(); ++it)
          {
            if (DicomMap::IsComputedTag(*it))
            {
              missingTags.erase(*it);
            }
          }

          std::string missings;
          FromDcmtkBridge::FormatListOfTags(missings, missingTags);

          LOG(WARNING) << "W001: Accessing Dicom tags from storage when accessing " << Orthanc::GetResourceTypeText(resource.type_, false , false) << " : " << missings;
        }


        std::string instanceId_ = instanceId;
        DicomMap tagsFromJson;

        if (dicomAsJson == NULL)
        {
          if (instanceId_.empty())
          {
            if (level == ResourceType_Instance)
            {
              instanceId_ = publicId;
            }
            else
            {
              std::list<std::string> instancesIds;
              GetIndex().GetChildInstances(instancesIds, publicId);
              if (instancesIds.size() < 1)
              {
                throw OrthancException(ErrorCode_InternalError, "ExpandResource: no instances found");
              }
              instanceId_ = instancesIds.front();
            }
          }
  
          Json::Value tmpDicomAsJson;
          ReadDicomAsJson(tmpDicomAsJson, instanceId_, resource.missingRequestedTags_ /* ignoreTagLength */);  // read all tags from DICOM and avoid cropping requested tags
          tagsFromJson.FromDicomAsJson(tmpDicomAsJson);
          resource.sequences_.FromDicomAsJson(tmpDicomAsJson, missingSequences);
        }
        else
        {
          tagsFromJson.FromDicomAsJson(*dicomAsJson);
          resource.sequences_.FromDicomAsJson(*dicomAsJson, missingSequences);
        }

        resource.tags_.Merge(tagsFromJson);
      }

      // compute the requested tags
      ComputeTags(resource, *this, publicId, level, requestedTags);
    }
    else
    {
      return false;
    }

    return true;
  }

}
