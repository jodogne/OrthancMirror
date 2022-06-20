#include "PendingDeletionsDatabase.h"
#include "LargeDeleteJob.h"

#include "../../../../OrthancFramework/Sources/FileStorage/FilesystemStorage.h"
#include "../../../../OrthancFramework/Sources/Logging.h"
#include "../../../../OrthancFramework/Sources/MultiThreading/SharedMessageQueue.h"
#include "../../../../OrthancServer/Plugins/Engine/PluginsEnumerations.h"

#include <boost/thread.hpp>


#define ASYNCHRONOUS_SQLITE  0


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





static bool continue_;
static Orthanc::SharedMessageQueue                  queue_;
static std::unique_ptr<Orthanc::FilesystemStorage>  storage_;
static std::unique_ptr<PendingDeletionsDatabase>    db_;
static std::unique_ptr<boost::thread>               databaseThread_;
static std::unique_ptr<boost::thread>               deletionThread_;



static OrthancPluginErrorCode StorageCreate(const char* uuid,
                                            const void* content,
                                            int64_t size,
                                            OrthancPluginContentType type)
{
  try
  {
    storage_->Create(uuid, content, size, Orthanc::Plugins::Convert(type));
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
    std::unique_ptr<Orthanc::IMemoryBuffer> buffer(storage_->Read(uuid, Orthanc::Plugins::Convert(type)));

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
    std::unique_ptr<Orthanc::IMemoryBuffer> buffer(storage_->ReadRange(uuid, Orthanc::Plugins::Convert(type), rangeStart, rangeStart + target->size));

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
#if ASYNCHRONOUS_SQLITE == 1
    queue_.Enqueue(new PendingDeletion(Orthanc::Plugins::Convert(type), uuid));
#else
    db_->Enqueue(uuid, Orthanc::Plugins::Convert(type));
#endif
    
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


static void DatabaseWorker()
{
#if ASYNCHRONOUS_SQLITE == 1
  while (continue_)
  {
    for (;;)
    {
      std::auto_ptr<Orthanc::IDynamicObject> obj(queue_.Dequeue(100));
      if (obj.get() == NULL)
      {
        break;
      }
      else
      {
        const PendingDeletion& deletion = dynamic_cast<const PendingDeletion&>(*obj);
        db_->Enqueue(deletion.GetUuid(), deletion.GetType());
      }
    }
  }
#endif
}


static void DeletionWorker()
{
  while (continue_)
  {
    std::string uuid;
    Orthanc::FileContentType type = Orthanc::FileContentType_Dicom;  // Dummy initialization

    bool hasDeleted = false;
    
    while (db_->Dequeue(uuid, type))
    {
      if (!hasDeleted)
      {
        LOG(INFO) << "TEST DELETION - Starting to process the pending deletions";        
      }
      
      hasDeleted = true;
      
      try
      {
        LOG(INFO) << "TEST DELETION - Asynchronous removal of file: " << uuid;
        storage_->Remove(uuid, type);
      }
      catch (Orthanc::OrthancException&)
      {
        LOG(ERROR) << "Cannot remove file: " << uuid;
      }
    }

    if (hasDeleted)
    {
      LOG(INFO) << "TEST DELETION - All the pending deletions have been completed";
    }      

    boost::this_thread::sleep(boost::posix_time::milliseconds(100));
  }
}


OrthancPluginErrorCode OnChangeCallback(OrthancPluginChangeType changeType,
                                        OrthancPluginResourceType resourceType,
                                        const char* resourceId)
{
  switch (changeType)
  {
    case OrthancPluginChangeType_OrthancStarted:
      assert(deletionThread_.get() == NULL &&
             databaseThread_.get() == NULL);
      
      LOG(WARNING) << "TEST DELETION - Starting the threads";
      continue_ = true;
      deletionThread_.reset(new boost::thread(DeletionWorker));
      databaseThread_.reset(new boost::thread(DatabaseWorker));
      break;

    case OrthancPluginChangeType_OrthancStopped:

      if (deletionThread_.get() != NULL)
      {
        LOG(WARNING) << "TEST DELETION - Stopping the deletion thread";
        continue_ = false;
        if (deletionThread_->joinable())
        {
          deletionThread_->join();
        }
      }

      if (databaseThread_.get() != NULL)
      {
        LOG(WARNING) << "TEST DELETION - Stopping the database thread";
        continue_ = false;
        if (databaseThread_->joinable())
        {
          databaseThread_->join();
        }
      }
      
      break;

    default:
      break;
  }

  return OrthancPluginErrorCode_Success;
}

  

void Statistics(OrthancPluginRestOutput* output,
                const char* url,
                const OrthancPluginHttpRequest* request)
{
  Json::Value stats;
  OrthancPlugins::RestApiGet(stats, "/statistics", false);

  stats["PendingDeletions"] = db_->GetSize();

  std::string s = stats.toStyledString();
  OrthancPluginAnswerBuffer(OrthancPlugins::GetGlobalContext(), output, s.c_str(),
                            s.size(), "application/json");
}



extern "C"
{
  ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* context)
  {
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

    Orthanc::Logging::InitializePluginContext(context);
    
    OrthancPlugins::SetGlobalContext(context);
    OrthancPluginSetDescription(context, "Plugin removing files from storage asynchronously.");

    OrthancPlugins::OrthancConfiguration config;

    if (config.GetBooleanValue("DelayedDeletionEnabled", false))
    {
      // Json::Value system;
      // OrthancPlugins::RestApiGet(system, "/system", false);
      // const std::string& databaseIdentifier = system["DatabaseIdentifier"].asString();
      std::string databaseServerIdentifier = config.GetDatabaseServerIdentifier();

      std::string pathStorage = config.GetStringValue("StorageDirectory", "OrthancStorage");
      LOG(WARNING) << "DelayedDeletion - Path to the storage area: " << pathStorage;

      storage_.reset(new Orthanc::FilesystemStorage(pathStorage));

      boost::filesystem::path p = boost::filesystem::path(pathStorage) / ("pending-deletions." + databaseServerIdentifier + ".db");
      LOG(WARNING) << "DelayedDeletion - Path to the SQLite database: " << p.string();
      
      // This must run after the allocation of "storage_", to make sure
      // that the folder actually exists
      db_.reset(new PendingDeletionsDatabase(p.string()));

      OrthancPluginRegisterStorageArea2(context, StorageCreate, StorageReadWhole, StorageReadRange, StorageRemove);

      OrthancPluginRegisterOnChangeCallback(context, OnChangeCallback);
    }
    else
    {
      LOG(WARNING) << "DelayedDeletion - plugin is loaded but not enabled (check your \"DelayedDeletionEnabled\" configuration)";
    }

    OrthancPlugins::RegisterRestCallback<LargeDeleteJob::RestHandler>("/tools/large-delete", true);
    OrthancPlugins::RegisterRestCallback<Statistics>("/statistics", true);

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
