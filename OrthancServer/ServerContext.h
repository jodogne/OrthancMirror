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


#pragma once

#include "IServerListener.h"
#include "LuaScripting.h"
#include "OrthancHttpHandler.h"
#include "ServerIndex.h"
#include "ServerJobs/IStorageCommitmentFactory.h"

#include "../Core/Cache/MemoryCache.h"


namespace Orthanc
{
  class DicomInstanceToStore;
  class IStorageArea;
  class JobsEngine;
  class MetricsRegistry;
  class OrthancPlugins;
  class ParsedDicomFile;
  class RestApiOutput;
  class SetOfInstancesJob;
  class SharedArchive;
  class SharedMessageQueue;
  class StorageCommitmentReports;
  
  
  /**
   * This class is responsible for maintaining the storage area on the
   * filesystem (including compression), as well as the index of the
   * DICOM store. It implements the required locking mechanisms.
   **/
  class ServerContext :
    public IStorageCommitmentFactory,
    private JobsRegistry::IObserver
  {
  public:
    class ILookupVisitor : public boost::noncopyable
    {
    public:
      virtual ~ILookupVisitor()
      {
      }

      virtual bool IsDicomAsJsonNeeded() const = 0;
      
      virtual void MarkAsComplete() = 0;

      virtual void Visit(const std::string& publicId,
                         const std::string& instanceId,
                         const DicomMap& mainDicomTags,
                         const Json::Value* dicomAsJson) = 0;
    };
    
    
  private:
    class LuaServerListener : public IServerListener
    {
    private:
      ServerContext& context_;

    public:
      LuaServerListener(ServerContext& context) :
        context_(context)
      {
      }

      virtual void SignalStoredInstance(const std::string& publicId,
                                        DicomInstanceToStore& instance,
                                        const Json::Value& simplifiedTags)
      {
        context_.mainLua_.SignalStoredInstance(publicId, instance, simplifiedTags);
      }
    
      virtual void SignalChange(const ServerIndexChange& change)
      {
        context_.mainLua_.SignalChange(change);
      }

      virtual bool FilterIncomingInstance(const DicomInstanceToStore& instance,
                                          const Json::Value& simplified)
      {
        return context_.filterLua_.FilterIncomingInstance(instance, simplified);
      }
    };
    
    class DicomCacheProvider : public Deprecated::ICachePageProvider  // TODO
    {
    private:
      ServerContext& context_;

    public:
      DicomCacheProvider(ServerContext& context) : context_(context)
      {
      }
      
      virtual IDynamicObject* Provide(const std::string& id);
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

    static void SaveJobsThread(ServerContext* that,
                               unsigned int sleepDelay);

    void ReadDicomAsJsonInternal(std::string& result,
                                 const std::string& instancePublicId);

    void SaveJobsEngine();

    virtual void SignalJobSubmitted(const std::string& jobId) ORTHANC_OVERRIDE;

    virtual void SignalJobSuccess(const std::string& jobId) ORTHANC_OVERRIDE;

    virtual void SignalJobFailure(const std::string& jobId) ORTHANC_OVERRIDE;

    ServerIndex index_;
    IStorageArea& area_;

    bool compressionEnabled_;
    bool storeMD5_;
    
    DicomCacheProvider provider_;
    boost::mutex dicomCacheMutex_;
    Deprecated::MemoryCache dicomCache_;  // TODO

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
    boost::recursive_mutex listenersMutex_;

    bool done_;
    bool haveJobsChanged_;
    bool isJobsEngineUnserialized_;
    SharedMessageQueue  pendingChanges_;
    boost::thread  changeThread_;
    boost::thread  saveJobsThread_;
        
    std::unique_ptr<SharedArchive>  queryRetrieveArchive_;
    std::string defaultLocalAet_;
    OrthancHttpHandler  httpHandler_;
    bool saveJobs_;
    FindStorageAccessMode findStorageAccessMode_;
    unsigned int limitFindInstances_;
    unsigned int limitFindResults_;

    std::unique_ptr<MetricsRegistry>  metricsRegistry_;
    bool isHttpServerSecure_;
    bool isExecuteLuaEnabled_;

    std::unique_ptr<StorageCommitmentReports>  storageCommitmentReports_;

  public:
    class DicomCacheLocker : public boost::noncopyable
    {
    private:
      ServerContext& that_;
      ParsedDicomFile *dicom_;
      boost::mutex::scoped_lock lock_;

    public:
      DicomCacheLocker(ServerContext& that,
                       const std::string& instancePublicId);

      ~DicomCacheLocker();

      ParsedDicomFile& GetDicom()
      {
        return *dicom_;
      }
    };

    ServerContext(IDatabaseWrapper& database,
                  IStorageArea& area,
                  bool unitTesting,
                  size_t maxCompletedJobs);

    ~ServerContext();

    void SetupJobsEngine(bool unitTesting,
                         bool loadJobsFromDatabase);

    ServerIndex& GetIndex()
    {
      return index_;
    }

    void SetCompressionEnabled(bool enabled);

    bool IsCompressionEnabled() const
    {
      return compressionEnabled_;
    }

    void RemoveFile(const std::string& fileUuid,
                    FileContentType type);

    bool AddAttachment(const std::string& resourceId,
                       FileContentType attachmentType,
                       const void* data,
                       size_t size);

    StoreStatus Store(std::string& resultPublicId,
                      DicomInstanceToStore& dicom);

    void AnswerAttachment(RestApiOutput& output,
                          const std::string& resourceId,
                          FileContentType content);

    void ChangeAttachmentCompression(const std::string& resourceId,
                                     FileContentType attachmentType,
                                     CompressionType compression);

    void ReadDicomAsJson(std::string& result,
                         const std::string& instancePublicId,
                         const std::set<DicomTag>& ignoreTagLength);

    void ReadDicomAsJson(Json::Value& result,
                         const std::string& instancePublicId,
                         const std::set<DicomTag>& ignoreTagLength);

    void ReadDicomAsJson(std::string& result,
                         const std::string& instancePublicId)
    {
      std::set<DicomTag> ignoreTagLength;
      ReadDicomAsJson(result, instancePublicId, ignoreTagLength);
    }

    void ReadDicomAsJson(Json::Value& result,
                         const std::string& instancePublicId)
    {
      std::set<DicomTag> ignoreTagLength;
      ReadDicomAsJson(result, instancePublicId, ignoreTagLength);
    }

    void ReadDicom(std::string& dicom,
                   const std::string& instancePublicId)
    {
      ReadAttachment(dicom, instancePublicId, FileContentType_Dicom, true);
    }
    
    // TODO CACHING MECHANISM AT THIS POINT
    void ReadAttachment(std::string& result,
                        const std::string& instancePublicId,
                        FileContentType content,
                        bool uncompressIfNeeded);
    
    void ReadAttachment(std::string& result,
                        const FileInfo& attachment);

    void SetStoreMD5ForAttachments(bool storeMD5);

    bool IsStoreMD5ForAttachments() const
    {
      return storeMD5_;
    }

    JobsEngine& GetJobsEngine()
    {
      return jobsEngine_;
    }

    bool DeleteResource(Json::Value& target,
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

    LuaScripting& GetLuaScripting()
    {
      return mainLua_;
    }

    OrthancHttpHandler& GetHttpHandler()
    {
      return httpHandler_;
    }

    void Stop();

    void Apply(ILookupVisitor& visitor,
               const DatabaseLookup& lookup,
               ResourceType queryLevel,
               size_t since,
               size_t limit);

    bool LookupOrReconstructMetadata(std::string& target,
                                     const std::string& publicId,
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

    void AddChildInstances(SetOfInstancesJob& job,
                           const std::string& publicId);

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

    virtual IStorageCommitmentFactory::ILookupHandler*
    CreateStorageCommitment(const std::string& jobId,
                            const std::string& transactionUid,
                            const std::vector<std::string>& sopClassUids,
                            const std::vector<std::string>& sopInstanceUids,
                            const std::string& remoteAet,
                            const std::string& calledAet) ORTHANC_OVERRIDE;

    StorageCommitmentReports& GetStorageCommitmentReports()
    {
      return *storageCommitmentReports_;
    }
  };
}
