/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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


#include "../../Sources/PrecompiledHeadersServer.h"
#include "PluginsJob.h"

#if ORTHANC_ENABLE_PLUGINS != 1
#error The plugin support is disabled
#endif


#include "../../../OrthancFramework/Sources/Logging.h"
#include "../../../OrthancFramework/Sources/OrthancException.h"
#include "../../../OrthancFramework/Sources/Toolbox.h"

#include <cassert>

namespace Orthanc
{
  PluginsJob::PluginsJob(const _OrthancPluginCreateJob& parameters) :
    parameters_(parameters)
  {
    if (parameters_.job == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
    
    if (parameters_.target == NULL ||
        parameters_.finalize == NULL ||
        parameters_.type == NULL ||
        parameters_.getProgress == NULL ||
        parameters_.getContent == NULL ||
        parameters_.getSerialized == NULL ||
        parameters_.step == NULL ||
        parameters_.stop == NULL ||
        parameters_.reset == NULL)
    {
      parameters_.finalize(parameters.job);
      throw OrthancException(ErrorCode_NullPointer);
    }

    type_.assign(parameters.type);
  }

  PluginsJob::~PluginsJob()
  {
    assert(parameters_.job != NULL);
    parameters_.finalize(parameters_.job);
  }

  JobStepResult PluginsJob::Step(const std::string& jobId)
  {
    OrthancPluginJobStepStatus status = parameters_.step(parameters_.job);

    switch (status)
    {
      case OrthancPluginJobStepStatus_Success:
        return JobStepResult::Success();

      case OrthancPluginJobStepStatus_Failure:
        return JobStepResult::Failure(ErrorCode_Plugin, NULL);

      case OrthancPluginJobStepStatus_Continue:
        return JobStepResult::Continue();

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }

  void PluginsJob::Reset()
  {
    parameters_.reset(parameters_.job);
  }

  void PluginsJob::Stop(JobStopReason reason)
  {
    switch (reason)
    {
      case JobStopReason_Success:
        parameters_.stop(parameters_.job, OrthancPluginJobStopReason_Success);
        break;

      case JobStopReason_Failure:
        parameters_.stop(parameters_.job, OrthancPluginJobStopReason_Failure);
        break;

      case JobStopReason_Canceled:
        parameters_.stop(parameters_.job, OrthancPluginJobStopReason_Canceled);
        break;

      case JobStopReason_Paused:
        parameters_.stop(parameters_.job, OrthancPluginJobStopReason_Paused);
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }

  float PluginsJob::GetProgress()
  {
    return parameters_.getProgress(parameters_.job);
  }

  void PluginsJob::GetPublicContent(Json::Value& value)
  {
    const char* content = parameters_.getContent(parameters_.job);

    if (content == NULL)
    {
      value = Json::objectValue;
    }
    else
    {
      if (!Toolbox::ReadJson(value, content) ||
          value.type() != Json::objectValue)
      {
        throw OrthancException(ErrorCode_Plugin,
                               "A job plugin must provide a JSON object as its public content");
      }
    }
  }

  bool PluginsJob::Serialize(Json::Value& value)
  {
    const char* serialized = parameters_.getSerialized(parameters_.job);

    if (serialized == NULL)
    {
      return false;
    }
    else
    {
      if (!Toolbox::ReadJson(value, serialized) ||
          value.type() != Json::objectValue)
      {
        throw OrthancException(ErrorCode_Plugin,
                               "A job plugin must provide a JSON object as its serialized content");
      }


      static const char* KEY_TYPE = "Type";
      
      if (value.isMember(KEY_TYPE))
      {
        throw OrthancException(ErrorCode_Plugin,
                               "The \"Type\" field is for reserved use for serialized job");
      }

      value[KEY_TYPE] = type_;
      return true;
    }
  }
}
