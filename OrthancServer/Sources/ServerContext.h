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


#pragma once

#include "IServerListener.h"
#include "LuaScripting.h"
#include "OrthancHttpHandler.h"
#include "ServerIndex.h"
#include "ServerJobs/IStorageCommitmentFactory.h"

#include "../../OrthancFramework/Sources/DataSource/DicomDataSource.h"
#include "../../OrthancFramework/Sources/DataSource/DicomSequentialReader.h"
#include "../../OrthancFramework/Sources/DataSource/StorageAreaDataSource.h"
#include "../../OrthancFramework/Sources/DataSource/TranscoderDataSource.h"
#include "../../OrthancFramework/Sources/DicomParsing/DicomModification.h"
#include "../../OrthancFramework/Sources/JobsEngine/JobsEngine.h"
#include "../../OrthancFramework/Sources/MetricsRegistry.h"
#include "../../OrthancFramework/Sources/MultiThreading/Future.h"
#include "../../OrthancFramework/Sources/MultiThreading/Semaphore.h"


namespace Orthanc
{
  class DicomElement;
  class DicomStoreUserConnection;
  class IExecutorService;
  class OrthancPlugins;
  class ServerTranscoder;
  class SharedArchive;
  class StorageCommitmentReports;
  class ThreadPool;
  

  /**
   * This class is responsible for maintaining the storage area on the
   * filesystem (including compression), as well as the index of the
   * DICOM store. It implements the required locking mechanisms.
   **/
  class ServerContext :
    public IStorageCommitmentFactory,
    private JobsRegistry::IObserver
  {
    friend class ServerIndex;  // To access "RemoveFile()"
    
  public:
    struct StoreResult
    {
    private:
      StoreStatus  status_;
      uint16_t     cstoreStatusCode_;
      // uint16_t     httpStatusCode_; // for future use

    public:
      StoreResult();

      void SetStatus(StoreStatus status)
      {
        status_ = status;
      }

      StoreStatus GetStatus()
      {
        return status_;
      }

      void SetCStoreStatusCode(uint16_t statusCode)
      {
        cstoreStatusCode_ = statusCode;
      }

      uint16_t GetCStoreStatusCode()
      {
        return cstoreStatusCode_;
      }
    };
    
  private:
    class LuaServerListener : public IServerListener
    {
    private:
      ServerContext& context_;

    public:
      explicit LuaServerListener(ServerContext& context) :
        context_(context)
      {
      }

      virtual void SignalStoredInstance(const std::string& publicId,
                                        const DicomInstanceToStore& instance,
                                        const Json::Value& simplifiedTags) ORTHANC_OVERRIDE
      {
        context_.mainLua_.SignalStoredInstance(publicId, instance, simplifiedTags);
      }
    
      virtual void SignalChange(const ServerIndexChange& change) ORTHANC_OVERRIDE
      {
        context_.mainLua_.SignalChange(change);
      }

      virtual void SignalJobEvent(const JobEvent& event) ORTHANC_OVERRIDE
      {
        context_.mainLua_.SignalJobEvent(event);
      }

      virtual bool FilterIncomingInstance(const DicomInstanceToStore& instance,
                                          const Json::Value& simplified) ORTHANC_OVERRIDE
      {
        return context_.filterLua_.FilterIncomingInstance(instance, simplified);
      }

      virtual bool FilterIncomingCStoreInstance(uint16_t& dimseStatus,
                                                const DicomInstanceToStore& instance,
                                                const Json::Value& simplified) ORTHANC_OVERRIDE
      {
        return context_.filterLua_.FilterIncomingCStoreInstance(dimseStatus, instance, simplified);
      }

      virtual bool FilterOutgoingCStoreInstance(const OutgoingDicomInstance& instance,
                                                const Json::Value& simplified) ORTHANC_OVERRIDE
      {
        return context_.filterLua_.FilterOutgoingCStoreInstance(instance, simplified);
      }

    };
    
    class ServerListener
    {
    private:
      IServerListener *listener_;
      std::string      description_;

    public:
      ServerListener(IServerListener& listener,
                     const std::string& description) :
        listener_(&listener),
        description_(description)
      {
      }

      IServerListener& GetListener()
      {
        return *listener_;
      }

      const std::string& GetDescription()
      {
        return description_;
      }
    };

    typedef std::list<ServerListener>  ServerListeners;


    static void ChangeThread(ServerContext* that,
                             unsigned int sleepDelay);

    static void JobEventsThread(ServerContext* that,
                                unsigned int sleepDelay);

    static void SaveJobsThread(ServerContext* that,
                               unsigned int sleepDelay);

#if HAVE_MALLOC_TRIM == 1
    static void MemoryTrimmingThread(ServerContext* that,
                                     unsigned int intervalInSeconds);
#endif

    void SaveJobsEngine();

    virtual void SignalJobSubmitted(const std::string& jobId) ORTHANC_OVERRIDE;

    virtual void SignalJobSuccess(const std::string& jobId) ORTHANC_OVERRIDE;

    virtual void SignalJobFailure(const std::string& jobId) ORTHANC_OVERRIDE;

    ServerIndex index_;
    IPluginStorageArea& area_;

    bool compressionEnabled_;
    bool storeMD5_;

    Semaphore largeDicomThrottler_;  // New in Orthanc 1.9.0 (notably for very large DICOM files in WSI)

    LuaScripting mainLua_;
    LuaScripting filterLua_;
    LuaServerListener  luaListener_;
    std::unique_ptr<SharedArchive>  mediaArchive_;
    
    // The "JobsEngine" must be *after* "LuaScripting", as
    // "LuaScripting" embeds "LuaJobManager" that registers as an
    // observer to "SequenceOfOperationsJob", whose lifetime
    // corresponds to that of "JobsEngine". It must also be after
    // "mediaArchive_", as jobs might access this archive.
    JobsEngine jobsEngine_;
    
#if ORTHANC_ENABLE_PLUGINS == 1
    OrthancPlugins* plugins_;
#endif

    ServerListeners listeners_;
    boost::shared_mutex listenersMutex_;

    bool done_;
    bool haveJobsChanged_;
    bool isJobsEngineUnserialized_;
    SharedMessageQueue  pendingChanges_;
    SharedMessageQueue  pendingJobEvents_;
    boost::thread  changeThread_;
    boost::thread  jobEventsThread_;
    boost::thread  saveJobsThread_;
    boost::thread  memoryTrimmingThread_;
        
    std::unique_ptr<SharedArchive>  queryRetrieveArchive_;
    std::string defaultLocalAet_;
    RetrieveMethod defaultDicomRetrieveMethod_;
    OrthancHttpHandler  httpHandler_;
    bool saveJobs_;
    FindStorageAccessMode findStorageAccessMode_;
    unsigned int limitFindInstances_;
    unsigned int limitFindResults_;

    std::unique_ptr<MetricsRegistry>  metricsRegistry_;
    bool isHttpServerSecure_;
    bool isExecuteLuaEnabled_;
    bool isRestApiWriteToFileSystemEnabled_;
    OverwriteInstancesMode overwriteInstances_;

    std::unique_ptr<StorageCommitmentReports>  storageCommitmentReports_;

    boost::shared_ptr<ServerTranscoder> transcoder_;
    bool transcodeDicomProtocol_;
    bool isIngestTranscoding_;
    DicomTransferSyntax ingestTransferSyntax_;
    bool ingestTranscodingOfUncompressed_;
    bool ingestTranscodingOfCompressed_;

    // New in Orthanc 1.9.0
    DicomTransferSyntax preferredTransferSyntax_;
    mutable boost::mutex dynamicOptionsMutex_;
    bool isUnknownSopClassAccepted_;
    std::set<DicomTransferSyntax>  acceptedTransferSyntaxes_;
    std::list<std::string>         acceptedSopClasses_;  // ordered; the most 120 common ones first
    bool readOnly_;
    bool patientLevelEnabled_;
    
    StoreResult StoreAfterTranscoding(std::string& resultPublicId,
                                      DicomInstanceToStore& dicom,
                                      bool isReconstruct);

    StoreResult StoreAfterTranscoding(std::string& resultPublicId,
                                      DicomInstanceToStore& dicom,
                                      bool isReconstruct,
                                      bool isAdoption,
                                      const FileInfo& adoptedFile);

    // This method must only be called from "ServerIndex"!
    void RemoveFile(const std::string& fileUuid,
                    FileContentType type,
                    const std::string& customData);

    // This DicomModification object is intended to be used as a
    // "rules engine" when de-identifying logs for C-Find, C-Get, and
    // C-Move queries (new in Orthanc 1.8.2)
    DicomModification logsDeidentifierRules_;
    bool              deidentifyLogs_;

    boost::posix_time::ptime serverStartTimeUtc_;

    // For streaming
    boost::shared_ptr<DataSourceReader>                storageAreaReader_;
    boost::shared_ptr<DataSourceReader>                dicomReader_;
    boost::shared_ptr<ThreadPool>                      transcoderThreadPool_;
    boost::shared_ptr<DataSourceReader>                transcoderReader_;
    boost::shared_ptr<DicomSequentialReader::Factory>  dicomSequentialReaderFactory_;

  public:
    ServerContext(IDatabaseWrapper& database,
                  IPluginStorageArea& area,
                  ServerTranscoder* transcoder /* takes ownership */,
                  bool unitTesting,
                  size_t maxCompletedJobs,
                  bool readOnly);

    ~ServerContext() ORTHANC_OVERRIDE;

    void SetupJobsEngine(bool unitTesting,
                         bool loadJobsFromDatabase);

    ServerIndex& GetIndex()
    {
      return index_;
    }

    void SetMaximumStorageCacheSize(size_t size)
    {
      // TODO-Streaming
    }

    void SetPatientLevelEnabled(bool enabled);

    bool IsPatientLevelEnabled() const
    {
      return patientLevelEnabled_;
    }

    void SetCompressionEnabled(bool enabled);

    bool IsCompressionEnabled() const
    {
      return compressionEnabled_;
    }
    bool IsReadOnly() const
    {
      return readOnly_;
    }

    bool IsSaveJobs() const
    {
      return saveJobs_;
    }

    bool AddAttachment(int64_t& newRevision,
                       const std::string& resourceId,
                       ResourceType resourceType,
                       FileContentType attachmentType,
                       const void* data,
                       size_t size,
                       bool hasOldRevision,
                       int64_t oldRevision,
                       const std::string& oldMD5);

    StoreResult Store(std::string& resultPublicId,
                      DicomInstanceToStore& dicom);

    StoreResult AdoptDicomInstance(std::string& resultPublicId,
                                   DicomInstanceToStore& dicom,
                                   const FileInfo& adoptedFile);

    StoreResult TranscodeAndStore(std::string& resultPublicId,
                                  DicomInstanceToStore* dicom,
                                  bool isReconstruct = false);  // TODO-Streaming: To check

    void AnswerAttachment(RestApiOutput& output,
                          const FileInfo& fileInfo,
                          const std::string& filename);

    void ChangeAttachmentCompression(ResourceType level,
                                     const std::string& resourceId,
                                     FileContentType attachmentType,
                                     CompressionType compression);

    void ReadDicomAsJson(Json::Value& result,
                         const std::string& instancePublicId,
                         const std::map<MetadataType, std::string>& instanceMetadata,
                         const std::map<FileContentType, FileInfo>& instanceAttachments,
                         const std::set<DicomTag>& ignoreTagLength);

    void ReadDicomAsJson(Json::Value& result,
                         const std::string& instancePublicId,
                         const std::set<DicomTag>& ignoreTagLength);  // TODO-FIND: Can this be removed?

    void ReadDicomAsJson(Json::Value& result,
                         const std::string& instancePublicId);  // TODO-FIND: Can this be removed?

    FileInfo LookupDicomForInstance(const std::string& instancePublicId);

    StorageAreaDataSource::Range* ReadAttachment(const FileInfo& attachment,
                                                 bool uncompress);

    StorageAreaDataSource::Range* ReadAttachment(const FileInfo& attachment,
                                                 const StorageRange& range,
                                                 bool uncompress);

    StorageAreaDataSource::Range* ReadRawDicom(const std::string& instancePublicId);

    DicomDataSource::Dicom* ReadParsedDicom(const std::string& instancePublicId);

    DicomDataSource::Dicom* ReadDicomUntilPixelData(const std::string& instancePublicId);

    TranscoderDataSource::Transcoded* ReadTranscodedDicom(const std::string& instancePublicId,
                                                          DicomTransferSyntax targetSyntax,
                                                          TranscodingSopInstanceUidMode mode,
                                                          bool hasLossyQuality,
                                                          unsigned int lossyQuality);

    void SetStoreMD5ForAttachments(bool storeMD5);

    bool IsStoreMD5ForAttachments() const
    {
      return storeMD5_;
    }

    JobsEngine& GetJobsEngine()
    {
      return jobsEngine_;
    }

    bool DeleteResource(Json::Value& remainingAncestor,
                        const std::string& uuid,
                        ResourceType expectedType);

    void SignalChange(const ServerIndexChange& change);

    SharedArchive& GetQueryRetrieveArchive()
    {
      return *queryRetrieveArchive_;
    }

    SharedArchive& GetMediaArchive()
    {
      return *mediaArchive_;
    }

    const std::string& GetDefaultLocalApplicationEntityTitle() const
    {
      return defaultLocalAet_;
    }

    RetrieveMethod GetDefaultDicomRetrieveMethod() const
    {
      return defaultDicomRetrieveMethod_;
    }

    LuaScripting& GetLuaScripting()
    {
      return mainLua_;
    }

    OrthancHttpHandler& GetHttpHandler()
    {
      return httpHandler_;
    }

    void Stop();

    uint64_t GetDatabaseLimits(ResourceType level) const
    {
      return (level == ResourceType_Instance ? limitFindInstances_ : limitFindResults_);
    }

    bool LookupOrReconstructMetadata(std::string& target,
                                     const std::string& publicId,
                                     ResourceType level,
                                     MetadataType type);


    /**
     * Management of the plugins
     **/

#if ORTHANC_ENABLE_PLUGINS == 1
    void SetPlugins(OrthancPlugins& plugins);

    void ResetPlugins();

    const OrthancPlugins& GetPlugins() const;

    OrthancPlugins& GetPlugins();
#endif

    bool HasPlugins() const;

    void GetOrderedChildInstances(std::vector<std::string>& instancesIds,
                                  std::vector<FileInfo>& filesInfo,
                                  const std::string& publicId,
                                  ResourceType level);

    void SignalUpdatedModalities();

    void SignalUpdatedPeers();

    MetricsRegistry& GetMetricsRegistry()
    {
      return *metricsRegistry_;
    }

    void SetHttpServerSecure(bool isSecure)
    {
      isHttpServerSecure_ = isSecure;
    }

    bool IsHttpServerSecure() const
    {
      return isHttpServerSecure_;
    }

    void SetExecuteLuaEnabled(bool enabled)
    {
      isExecuteLuaEnabled_ = enabled;
    }

    bool IsExecuteLuaEnabled() const
    {
      return isExecuteLuaEnabled_;
    }

    void SetRestApiWriteToFileSystemEnabled(bool enabled)
    {
      isRestApiWriteToFileSystemEnabled_ = enabled;
    }

    bool IsRestApiWriteToFileSystemEnabled() const
    {
      return isRestApiWriteToFileSystemEnabled_;
    }

    void SetOverwriteInstances(OverwriteInstancesMode overwrite)
    {
      overwriteInstances_ = overwrite;
    }
    
    OverwriteInstancesMode GetOverwriteInstances() const
    {
      return overwriteInstances_;
    }

    bool IsOverwriteInstances() const // for backward compatibility
    {
      return overwriteInstances_ != OverwriteInstancesMode_Never;
    }

    virtual IStorageCommitmentFactory::ILookupHandler*
    CreateStorageCommitment(const std::string& jobId,
                            const std::string& transactionUid,
                            const std::vector<std::string>& sopClassUids,
                            const std::vector<std::string>& sopInstanceUids,
                            const DicomConnectionInfo& connection) ORTHANC_OVERRIDE;

    StorageCommitmentReports& GetStorageCommitmentReports()
    {
      return *storageCommitmentReports_;
    }

    ImageAccessor* DecodeDicomFrame(const std::string& publicId,
                                    unsigned int frameIndex);

    void PerformCStoreWithTranscoding(std::string& sopClassUid,
                                      std::string& sopInstanceUid,
                                      DicomStoreUserConnection& connection,
                                      const void* dicomData,
                                      size_t dicomSize,
                                      bool hasMoveOriginator,
                                      const std::string& moveOriginatorAet,
                                      uint16_t moveOriginatorId);

    bool IsTranscodeDicomProtocol() const
    {
      return transcodeDicomProtocol_;
    }

    const std::string& GetDeidentifiedContent(const DicomElement& element) const;

    void GetAcceptedTransferSyntaxes(std::set<DicomTransferSyntax>& syntaxes) const;

    void SetAcceptedTransferSyntaxes(const std::set<DicomTransferSyntax>& syntaxes);

    void GetProposedStorageTransferSyntaxes(std::list<DicomTransferSyntax>& syntaxes) const;

    void SetAcceptedSopClasses(const std::list<std::string>& acceptedSopClasses,
                               const std::set<std::string>& rejectedSopClasses);

    void GetAcceptedSopClasses(std::set<std::string>& sopClasses, size_t maxCount) const;

    bool IsUnknownSopClassAccepted() const;

    void SetUnknownSopClassAccepted(bool accepted);

    FindStorageAccessMode GetFindStorageAccessMode() const
    {
      return findStorageAccessMode_;
    }

    int64_t GetServerUpTime() const;

    void PublishCacheMetrics();

    const boost::shared_ptr<ServerTranscoder>& GetTranscoder() const;

    DicomSequentialReader::Factory& GetDicomSequentialReaderFactory();
  };
}
