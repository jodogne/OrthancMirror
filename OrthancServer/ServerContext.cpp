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
#include "ServerContext.h"

#include "../Core/FileStorage/StorageAccessor.h"
#include "../Core/HttpServer/FilesystemHttpSender.h"
#include "../Core/HttpServer/HttpStreamTranscoder.h"
#include "../Core/Logging.h"
#include "../Core/DicomParsing/FromDcmtkBridge.h"
#include "ServerToolbox.h"
#include "OrthancInitialization.h"

#include <EmbeddedResources.h>
#include <dcmtk/dcmdata/dcfilefo.h>


#include "Scheduler/CallSystemCommand.h"
#include "Scheduler/DeleteInstanceCommand.h"
#include "Scheduler/ModifyInstanceCommand.h"
#include "Scheduler/StoreScuCommand.h"
#include "Scheduler/StorePeerCommand.h"
#include "OrthancRestApi/OrthancRestApi.h"
#include "../Plugins/Engine/OrthancPlugins.h"
#include "Search/LookupResource.h"


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
  void ServerContext::ChangeThread(ServerContext* that)
  {
    while (!that->done_)
    {
      std::auto_ptr<IDynamicObject> obj(that->pendingChanges_.Dequeue(100));
        
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


  ServerContext::ServerContext(IDatabaseWrapper& database,
                               IStorageArea& area) :
    index_(*this, database),
    area_(area),
    compressionEnabled_(false),
    storeMD5_(true),
    provider_(*this),
    dicomCache_(provider_, DICOM_CACHE_SIZE),
    scheduler_(Configuration::GetGlobalUnsignedIntegerParameter("LimitJobs", 10)),
    lua_(*this),
#if ORTHANC_ENABLE_PLUGINS == 1
    plugins_(NULL),
#endif
    done_(false),
    queryRetrieveArchive_(Configuration::GetGlobalUnsignedIntegerParameter("QueryRetrieveSize", 10)),
    defaultLocalAet_(Configuration::GetGlobalStringParameter("DicomAet", "ORTHANC"))
  {
    uint64_t s = Configuration::GetGlobalUnsignedIntegerParameter("DicomAssociationCloseDelay", 5);  // In seconds
    scu_.SetMillisecondsBeforeClose(s * 1000);  // Milliseconds are expected here

    listeners_.push_back(ServerListener(lua_, "Lua"));

    changeThread_ = boost::thread(ChangeThread, this);
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

      scu_.Finalize();

      // Do not change the order below!
      scheduler_.Stop();
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
    area_.Remove(fileUuid, type);
  }


  StoreStatus ServerContext::Store(std::string& resultPublicId,
                                   DicomInstanceToStore& dicom)
  {
    try
    {
      StorageAccessor accessor(area_);

      DicomInstanceHasher hasher(dicom.GetSummary());
      resultPublicId = hasher.HashInstance();

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

    StorageAccessor accessor(area_);
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

    StorageAccessor accessor(area_);
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
        LOG(WARNING) << "Cannot associate the DICOM-as-JSON summary to instance: " << instancePublicId;
        throw OrthancException(ErrorCode_InternalError);
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
      LOG(WARNING) << "Unable to read attachment " << EnumerationToString(content) << " of instance " << instancePublicId;
      throw OrthancException(ErrorCode_InternalError);
    }

    if (uncompressIfNeeded)
    {
      ReadAttachment(result, attachment);
    }
    else
    {
      // Do not interpret the content of the storage area, return the
      // raw data
      area_.Read(result, attachment.GetUuid(), content);
    }
  }


  void ServerContext::ReadAttachment(std::string& result,
                                     const FileInfo& attachment)
  {
    // This will decompress the attachment
    StorageAccessor accessor(area_);
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
    static std::auto_ptr<IDynamicObject> p;
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

    StorageAccessor accessor(area_);
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
    listeners_.push_back(ServerListener(lua_, "Lua"));
    listeners_.push_back(ServerListener(plugins, "plugin"));
  }


  void ServerContext::ResetPlugins()
  {
    boost::recursive_mutex::scoped_lock lock(listenersMutex_);

    plugins_ = NULL;

    // TODO REFACTOR THIS
    listeners_.clear();
    listeners_.push_back(ServerListener(lua_, "Lua"));
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


  void ServerContext::Apply(std::list<std::string>& result,
                            const ::Orthanc::LookupResource& lookup,
                            size_t since,
                            size_t limit)
  {
    result.clear();

    std::vector<std::string> resources, instances;
    GetIndex().FindCandidates(resources, instances, lookup);

    assert(resources.size() == instances.size());

    size_t skipped = 0;
    for (size_t i = 0; i < instances.size(); i++)
    {
      Json::Value dicom;
      ReadDicomAsJson(dicom, instances[i]);
      
      if (lookup.IsMatch(dicom))
      {
        if (skipped < since)
        {
          skipped++;
        }
        else if (limit != 0 &&
                 result.size() >= limit)
        {
          return;  // too many results
        }
        else
        {
          result.push_back(resources[i]);
        }
      }
    }
  }

}
