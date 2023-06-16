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


#pragma once

#if ORTHANC_ENABLE_PLUGINS == 1

#include "../../../OrthancFramework/Sources/Compatibility.h"  // For ORTHANC_OVERRIDE
#include "../../../OrthancFramework/Sources/JobsEngine/IJob.h"
#include "../Include/orthanc/OrthancCPlugin.h"

namespace Orthanc
{
  class PluginsJob : public IJob
  {
  private:
    _OrthancPluginCreateJob2       parameters_;
    std::string                    type_;
    OrthancPluginJobGetContent     deprecatedGetContent_;
    OrthancPluginJobGetSerialized  deprecatedGetSerialized_;

    void Setup();

  public:
    explicit PluginsJob(const _OrthancPluginCreateJob& parameters);

    explicit PluginsJob(const _OrthancPluginCreateJob2& parameters);

    virtual ~PluginsJob();

    virtual void Start() ORTHANC_OVERRIDE
    {
    }
    
    virtual JobStepResult Step(const std::string& jobId) ORTHANC_OVERRIDE;

    virtual void Reset() ORTHANC_OVERRIDE;

    virtual void Stop(JobStopReason reason) ORTHANC_OVERRIDE;

    virtual float GetProgress() ORTHANC_OVERRIDE;

    virtual void GetJobType(std::string& target) ORTHANC_OVERRIDE
    {
      target = type_;
    }
    
    virtual void GetPublicContent(Json::Value& value) ORTHANC_OVERRIDE;

    virtual bool Serialize(Json::Value& value) ORTHANC_OVERRIDE;

    virtual bool GetOutput(std::string& output,
                           MimeType& mime,
                           std::string& filename,
                           const std::string& key) ORTHANC_OVERRIDE
    {
      // TODO
      return false;
    }

    virtual bool DeleteOutput(const std::string& key) ORTHANC_OVERRIDE
    {
      // TODO
      return false;
    }
  };
}

#endif
