/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2024 Osimis S.A., Belgium
 * Copyright (C) 2021-2024 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
