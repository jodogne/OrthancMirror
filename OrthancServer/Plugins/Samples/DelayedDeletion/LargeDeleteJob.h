#pragma once

#include "../Common/OrthancPluginCppWrapper.h"
#include "../../../../OrthancFramework/Sources/Enumerations.h"
#include "../../../../OrthancFramework/Sources/Compatibility.h"


class LargeDeleteJob : public OrthancPlugins::OrthancJob
{
private:
  std::vector<std::string>            resources_;
  std::vector<Orthanc::ResourceType>  levels_;
  std::vector<std::string>            instances_;
  std::vector<std::string>            series_;
  size_t                              posResources_;
  size_t                              posInstances_;
  size_t                              posSeries_;
  size_t                              posDelete_;

  void UpdateDeleteProgress();

  void ScheduleChildrenResources(std::vector<std::string>& target,
                                 const std::string& uri);
  
  void ScheduleResource(Orthanc::ResourceType level,
                        const std::string& id);

  void DeleteResource(Orthanc::ResourceType level,
                      const std::string& id);
  
public:
  LargeDeleteJob(const std::vector<std::string>& resources,
                 const std::vector<Orthanc::ResourceType>& levels);

  virtual OrthancPluginJobStepStatus Step() ORTHANC_OVERRIDE;

  virtual void Stop(OrthancPluginJobStopReason reason) ORTHANC_OVERRIDE
  {
  }

  virtual void Reset() ORTHANC_OVERRIDE;

  static void RestHandler(OrthancPluginRestOutput* output,
                          const char* url,
                          const OrthancPluginHttpRequest* request);
};
