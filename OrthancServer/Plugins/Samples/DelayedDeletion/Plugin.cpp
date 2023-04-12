/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "PendingDeletionsDatabase.h"

#include "../../../../OrthancFramework/Sources/FileStorage/FilesystemStorage.h"
#include "../../../../OrthancFramework/Sources/Logging.h"
#include "../../../../OrthancFramework/Sources/MultiThreading/SharedMessageQueue.h"
#include "../Common/OrthancPluginCppWrapper.h"

#include <boost/thread.hpp>


class PendingDeletion : public Orthanc::IDynamicObject
{
private:
  Orthanc::FileContentType  type_;
  std::string               uuid_;

public:
  PendingDeletion(Orthanc::FileContentType type,
                  const std::string& uuid) :
    type_(type),
    uuid_(uuid)
  {
  }

  Orthanc::FileContentType GetType() const
  {
    return type_;
  }

  const std::string& GetUuid() const
  {
    return uuid_;
  }
};


static const char* DELAYED_DELETION = "DelayedDeletion";
static bool                                         continue_ = false;
static Orthanc::SharedMessageQueue                  queue_;
static std::unique_ptr<Orthanc::FilesystemStorage>  storage_;
static std::unique_ptr<PendingDeletionsDatabase>    db_;
static std::unique_ptr<boost::thread>               deletionThread_;
static const char*                                  databaseServerIdentifier_ = NULL;
static unsigned int                                 throttleDelayMs_ = 0;


static Orthanc::FileContentType Convert(OrthancPluginContentType type)
{
  switch (type)
  {
    case OrthancPluginContentType_Dicom:
      return Orthanc::FileContentType_Dicom;

    case OrthancPluginContentType_DicomAsJson:
      return Orthanc::FileContentType_DicomAsJson;

    case OrthancPluginContentType_DicomUntilPixelData:
      return Orthanc::FileContentType_DicomUntilPixelData;

    default:
      return Orthanc::FileContentType_Unknown;
  }
}


static OrthancPluginErrorCode StorageCreate(const char* uuid,
                                            const void* content,
                                            int64_t size,
                                            OrthancPluginContentType type)
{
  try
  {
    storage_->Create(uuid, content, size, Convert(type));
    return OrthancPluginErrorCode_Success;
  }
  catch (Orthanc::OrthancException& e)
  {
    return static_cast<OrthancPluginErrorCode>(e.GetErrorCode());
  }
  catch (...)
  {
    return OrthancPluginErrorCode_StorageAreaPlugin;
  }
}


static OrthancPluginErrorCode StorageReadWhole(OrthancPluginMemoryBuffer64* target, // Memory buffer where to store the content of the file. It must be allocated by the plugin using OrthancPluginCreateMemoryBuffer64(). The core of Orthanc will free it.
                                               const char* uuid,
                                               OrthancPluginContentType type)
{
  try
  {
    std::unique_ptr<Orthanc::IMemoryBuffer> buffer(storage_->Read(uuid, Convert(type)));

    // copy from a buffer allocated on plugin's heap into a buffer allocated on core's heap
    if (OrthancPluginCreateMemoryBuffer64(OrthancPlugins::GetGlobalContext(), target, buffer->GetSize()) != OrthancPluginErrorCode_Success)
    {
      OrthancPlugins::LogError("Delayed deletion plugin: error while reading object " + std::string(uuid) + ", cannot allocate memory of size " + boost::lexical_cast<std::string>(buffer->GetSize()) + " bytes");
      return OrthancPluginErrorCode_StorageAreaPlugin;
    }

    memcpy(target->data, buffer->GetData(), buffer->GetSize());
    
    return OrthancPluginErrorCode_Success;
  }
  catch (Orthanc::OrthancException& e)
  {
    return static_cast<OrthancPluginErrorCode>(e.GetErrorCode());
  }
  catch (...)
  {
    return OrthancPluginErrorCode_StorageAreaPlugin;
  }
}


static OrthancPluginErrorCode StorageReadRange(OrthancPluginMemoryBuffer64* target, // Memory buffer where to store the content of the range.  The memory buffer is allocated and freed by Orthanc. The length of the range of interest corresponds to the size of this buffer.
                                               const char* uuid,
                                               OrthancPluginContentType type,
                                               uint64_t rangeStart)
{
  try
  {
    std::unique_ptr<Orthanc::IMemoryBuffer> buffer(storage_->ReadRange(uuid, Convert(type), rangeStart, rangeStart + target->size));

    assert(buffer->GetSize() == target->size);

    memcpy(target->data, buffer->GetData(), buffer->GetSize());
    
    return OrthancPluginErrorCode_Success;
  }
  catch (Orthanc::OrthancException& e)
  {
    return static_cast<OrthancPluginErrorCode>(e.GetErrorCode());
  }
  catch (...)
  {
    return OrthancPluginErrorCode_StorageAreaPlugin;
  }

  return OrthancPluginErrorCode_Success;
}


static OrthancPluginErrorCode StorageRemove(const char* uuid,
                                            OrthancPluginContentType type)
{
  try
  {
    LOG(INFO) << "DelayedDeletion - Scheduling delayed deletion of " << uuid;
    db_->Enqueue(uuid, Convert(type));
    
    return OrthancPluginErrorCode_Success;
  }
  catch (Orthanc::OrthancException& e)
  {
    return static_cast<OrthancPluginErrorCode>(e.GetErrorCode());
  }
  catch (...)
  {
    return OrthancPluginErrorCode_StorageAreaPlugin;
  }
}

static void DeletionWorker()
{
  static const unsigned int GRANULARITY = 100;  // In milliseconds

  while (continue_)
  {
    std::string uuid;
    Orthanc::FileContentType type = Orthanc::FileContentType_Dicom;  // Dummy initialization

    bool hasDeleted = false;
    
    while (continue_ && db_->Dequeue(uuid, type))
    {
      if (!hasDeleted)
      {
        LOG(INFO) << "DelayedDeletion - Starting to process the pending deletions";        
      }
      
      hasDeleted = true;
      
      try
      {
        LOG(INFO) << "DelayedDeletion - Asynchronous removal of file: " << uuid;
        storage_->Remove(uuid, type);

        if (throttleDelayMs_ > 0)
        {
          boost::this_thread::sleep(boost::posix_time::milliseconds(throttleDelayMs_));
        }
      }
      catch (Orthanc::OrthancException& ex)
      {
        LOG(ERROR) << "DelayedDeletion - Cannot remove file: " << uuid << " " << ex.What();
      }
    }

    if (hasDeleted)
    {
      LOG(INFO) << "DelayedDeletion - All the pending deletions have been completed";
    }      

    boost::this_thread::sleep(boost::posix_time::milliseconds(GRANULARITY));
  }
}


OrthancPluginErrorCode OnChangeCallback(OrthancPluginChangeType changeType,
                                        OrthancPluginResourceType resourceType,
                                        const char* resourceId)
{
  switch (changeType)
  {
    case OrthancPluginChangeType_OrthancStarted:
      assert(deletionThread_.get() == NULL);
      
      LOG(WARNING) << "DelayedDeletion - Starting the deletion thread";
      continue_ = true;
      deletionThread_.reset(new boost::thread(DeletionWorker));
      break;

    case OrthancPluginChangeType_OrthancStopped:

      if (deletionThread_.get() != NULL)
      {
        LOG(WARNING) << "DelayedDeletion - Stopping the deletion thread";
        continue_ = false;
        if (deletionThread_->joinable())
        {
          deletionThread_->join();
        }
      }

      break;

    default:
      break;
  }

  return OrthancPluginErrorCode_Success;
}

  

void GetPluginStatus(OrthancPluginRestOutput* output,
                const char* url,
                const OrthancPluginHttpRequest* request)
{

  Json::Value status;
  status["FilesPendingDeletion"] = db_->GetSize();
  status["DatabaseServerIdentifier"] = databaseServerIdentifier_;

  std::string s = status.toStyledString();
  OrthancPluginAnswerBuffer(OrthancPlugins::GetGlobalContext(), output, s.c_str(),
                            s.size(), "application/json");
}



extern "C"
{
  ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* context)
  {
    OrthancPlugins::SetGlobalContext(context);
    Orthanc::Logging::InitializePluginContext(context);
    

    /* Check the version of the Orthanc core */
    if (OrthancPluginCheckVersion(context) == 0)
    {
      char info[1024];
      sprintf(info, "Your version of Orthanc (%s) must be above %d.%d.%d to run this plugin",
              context->orthancVersion,
              ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_REVISION_NUMBER);
      OrthancPluginLogError(context, info);
      return -1;
    }

    OrthancPluginSetDescription(context, "Plugin removing files from storage asynchronously.");

    OrthancPlugins::OrthancConfiguration orthancConfig;

    if (!orthancConfig.IsSection(DELAYED_DELETION))
    {
      LOG(WARNING) << "DelayedDeletion - plugin is loaded but not enabled (no \"DelayedDeletion\" section found in configuration)";
      return 0;
    }

    OrthancPlugins::OrthancConfiguration delayedDeletionConfig;
    orthancConfig.GetSection(delayedDeletionConfig, DELAYED_DELETION);

    if (delayedDeletionConfig.GetBooleanValue("Enable", true))
    {
      databaseServerIdentifier_ = OrthancPluginGetDatabaseServerIdentifier(context);
      throttleDelayMs_ = delayedDeletionConfig.GetUnsignedIntegerValue("ThrottleDelayMs", 0);   // delay in ms    


      std::string pathStorage = orthancConfig.GetStringValue("StorageDirectory", "OrthancStorage");
      LOG(WARNING) << "DelayedDeletion - Path to the storage area: " << pathStorage;

      storage_.reset(new Orthanc::FilesystemStorage(pathStorage));

      boost::filesystem::path defaultDbPath = boost::filesystem::path(pathStorage) / (std::string("pending-deletions.") + databaseServerIdentifier_ + ".db");
      std::string dbPath = delayedDeletionConfig.GetStringValue("Path", defaultDbPath.string());

      LOG(WARNING) << "DelayedDeletion - Path to the SQLite database: " << dbPath;
      
      // This must run after the allocation of "storage_", to make sure
      // that the folder actually exists
      db_.reset(new PendingDeletionsDatabase(dbPath));

      OrthancPluginRegisterStorageArea2(context, StorageCreate, StorageReadWhole, StorageReadRange, StorageRemove);

      OrthancPluginRegisterOnChangeCallback(context, OnChangeCallback);

      OrthancPlugins::RegisterRestCallback<GetPluginStatus>(std::string("/plugins/") + ORTHANC_PLUGIN_NAME + "/status", true);
    }
    else
    {
      LOG(WARNING) << "DelayedDeletion - plugin is loaded but disabled (check your \"DelayedDeletion.Enable\" configuration)";
    }

    return 0;
  }

  ORTHANC_PLUGINS_API void OrthancPluginFinalize()
  {
    db_.reset();
    storage_.reset();
  }

  ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
  {
    return ORTHANC_PLUGIN_NAME;
  }

  ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion()
  {
    return ORTHANC_PLUGIN_VERSION;
  }
}
