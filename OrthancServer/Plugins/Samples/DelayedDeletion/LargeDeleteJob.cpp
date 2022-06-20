#include "LargeDeleteJob.h"

#include "../../../../OrthancFramework/Sources/Logging.h"
#include "../../../../OrthancFramework/Sources/OrthancException.h"

#include <json/reader.h>

void LargeDeleteJob::UpdateDeleteProgress()
{
  size_t total = 2 * resources_.size() + instances_.size() + series_.size();

  float progress;
  if (total == 0)
  {
    progress = 1;
  }
  else
  {
    progress = (static_cast<float>(posResources_ + posInstances_ + posSeries_ + posDelete_) /
                static_cast<float>(total));
  }

  UpdateProgress(progress);
}


void LargeDeleteJob::ScheduleChildrenResources(std::vector<std::string>& target,
                                               const std::string& uri)
{
  Json::Value items;
  if (OrthancPlugins::RestApiGet(items, uri, false))
  {
    if (items.type() != Json::arrayValue)
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_InternalError);
    }

    for (Json::Value::ArrayIndex i = 0; i < items.size(); i++)
    {
      if (items[i].type() != Json::objectValue ||
          !items[i].isMember("ID") ||
          items[i]["ID"].type() != Json::stringValue)
      {
        throw Orthanc::OrthancException(Orthanc::ErrorCode_InternalError);
      }
      else
      {
        target.push_back(items[i]["ID"].asString());
      }
    }
  }
}
  

void LargeDeleteJob::ScheduleResource(Orthanc::ResourceType level,
                                      const std::string& id)
{
#if 0
  // Instance-level granularity => very slow!
  switch (level)
  {
    case Orthanc::ResourceType_Patient:
      ScheduleChildrenResources(instances_, "/patients/" + id + "/instances");
      break;
            
    case Orthanc::ResourceType_Study:
      ScheduleChildrenResources(instances_, "/studies/" + id + "/instances");
      break;
            
    case Orthanc::ResourceType_Series:
      ScheduleChildrenResources(instances_, "/series/" + id + "/instances");
      break;

    case Orthanc::ResourceType_Instance:
      instances_.push_back(id);
      break;

    default:
      throw Orthanc::OrthancException(Orthanc::ErrorCode_InternalError);
  }
#else
  /**
   * Series-level granularity => looks like a good compromise between
   * having the Orthanc mutex locked during all the study, and very
   * slow instance-level granularity.
   **/
  switch (level)
  {
    case Orthanc::ResourceType_Patient:
      ScheduleChildrenResources(series_, "/patients/" + id + "/series");
      break;
            
    case Orthanc::ResourceType_Study:
      ScheduleChildrenResources(series_, "/studies/" + id + "/series");
      break;
            
    case Orthanc::ResourceType_Series:
      series_.push_back(id);
      break;

    case Orthanc::ResourceType_Instance:
      instances_.push_back(id);
      break;

    default:
      throw Orthanc::OrthancException(Orthanc::ErrorCode_InternalError);
  }
#endif
}


void LargeDeleteJob::DeleteResource(Orthanc::ResourceType level,
                                    const std::string& id)
{
  std::string uri;      
  switch (level)
  {
    case Orthanc::ResourceType_Patient:
      uri = "/patients/" + id;
      break;
          
    case Orthanc::ResourceType_Study:
      uri = "/studies/" + id;
      break;
          
    case Orthanc::ResourceType_Series:
      uri = "/series/" + id;
      break;
          
    case Orthanc::ResourceType_Instance:
      uri = "/instances/" + id;
      break;

    default:
      throw Orthanc::OrthancException(Orthanc::ErrorCode_InternalError);
  }

  OrthancPlugins::RestApiDelete(uri, false);
}

  
LargeDeleteJob::LargeDeleteJob(const std::vector<std::string>& resources,
                               const std::vector<Orthanc::ResourceType>& levels) :
  OrthancJob("LargeDelete"),
  resources_(resources),
  levels_(levels),
  posResources_(0),
  posInstances_(0),
  posDelete_(0)
{
  if (resources.size() != levels.size())
  {
    throw Orthanc::OrthancException(Orthanc::ErrorCode_ParameterOutOfRange);
  }
}

  
OrthancPluginJobStepStatus LargeDeleteJob::Step()
{
  if (posResources_ == 0)
  {
    if (resources_.size() == 1)
    {
      // LOG(WARNING) << "LargeDeleteJob has started on resource: " << resources_[0];
    }
    else
    {
      // LOG(WARNING) << "LargeDeleteJob has started";
    }
  }
  
  if (posResources_ < resources_.size())
  {
    // First step: Discovering all the instances of the resources

    ScheduleResource(levels_[posResources_], resources_[posResources_]);
      
    posResources_ += 1;
    UpdateDeleteProgress();
    return OrthancPluginJobStepStatus_Continue;
  }    
  else if (posInstances_ < instances_.size())
  {
    // Second step: Deleting the instances one by one

    DeleteResource(Orthanc::ResourceType_Instance, instances_[posInstances_]);

    posInstances_ += 1;
    UpdateDeleteProgress();
    return OrthancPluginJobStepStatus_Continue;
  }
  else if (posSeries_ < series_.size())
  {
    // Third step: Deleting the series one by one

    DeleteResource(Orthanc::ResourceType_Series, series_[posSeries_]);

    posSeries_ += 1;
    UpdateDeleteProgress();
    return OrthancPluginJobStepStatus_Continue;
  }
  else if (posDelete_ < resources_.size())
  {
    // Fourth step: Make sure the resources where fully deleted
    // (instances might have been received since the beginning of
    // the job)

    DeleteResource(levels_[posDelete_], resources_[posDelete_]);

    posDelete_ += 1;
    UpdateDeleteProgress();
    return OrthancPluginJobStepStatus_Continue;
  }
  else
  {
    if (resources_.size() == 1)
    {
      // LOG(WARNING) << "LargeDeleteJob has completed on resource: " << resources_[0];
    }
    else
    {
      // LOG(WARNING) << "LargeDeleteJob has completed";
    }

    UpdateProgress(1);
    return OrthancPluginJobStepStatus_Success;
  }                   
}


void LargeDeleteJob::Reset()
{
  posResources_ = 0;
  posInstances_ = 0;
  posDelete_ = 0;
  instances_.clear();
}


void LargeDeleteJob::RestHandler(OrthancPluginRestOutput* output,
                                 const char* url,
                                 const OrthancPluginHttpRequest* request)
{
  static const char* KEY_RESOURCES = "Resources";
  
  if (request->method != OrthancPluginHttpMethod_Post)
  {
    OrthancPluginSendMethodNotAllowed(OrthancPlugins::GetGlobalContext(), output, "POST");
    return;
  }

  Json::Value body;
  Json::Reader reader;
  if (!reader.parse(reinterpret_cast<const char*>(request->body),
                    reinterpret_cast<const char*>(request->body) + request->bodySize, body))
  {
    throw Orthanc::OrthancException(Orthanc::ErrorCode_BadFileFormat, "JSON body is expected");
  }

  if (body.type() != Json::objectValue)
  {
    throw Orthanc::OrthancException(Orthanc::ErrorCode_BadFileFormat,
                                    "Expected a JSON object in the body");
  }

  if (!body.isMember(KEY_RESOURCES) ||
      body[KEY_RESOURCES].type() != Json::arrayValue)
  {
    throw Orthanc::OrthancException(Orthanc::ErrorCode_BadFileFormat,
                                    "The JSON object must contain an array in \"" +
                                    std::string(KEY_RESOURCES) + "\"");
  }

  std::vector<std::string> resources;
  std::vector<Orthanc::ResourceType>  levels;

  resources.reserve(body.size());
  levels.reserve(body.size());

  const Json::Value& arr = body[KEY_RESOURCES];
  for (Json::Value::ArrayIndex i = 0; i < arr.size(); i++)
  {
    if (arr[i].type() != Json::arrayValue ||
        arr[i].size() != 2u ||
        arr[i][0].type() != Json::stringValue ||
        arr[i][1].type() != Json::stringValue)
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_BadFileFormat,
                                      "Each entry must be an array containing 2 strings, "
                                      "the resource level and its ID");
    }
    else
    {
      levels.push_back(Orthanc::StringToResourceType(arr[i][0].asCString()));
      resources.push_back(arr[i][1].asString());
    }
  }
  
  OrthancPlugins::OrthancJob::SubmitFromRestApiPost(
    output, body, new LargeDeleteJob(resources, levels));
}
