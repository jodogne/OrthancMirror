/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
  void PluginsJob::Setup()
  {
    if (parameters_.job == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
    
    if (parameters_.target == NULL ||
        parameters_.finalize == NULL ||
        parameters_.type == NULL ||
        parameters_.getProgress == NULL ||
        (parameters_.getContent == NULL && deprecatedGetContent_ == NULL) ||
        (parameters_.getSerialized == NULL && deprecatedGetSerialized_ == NULL) ||
        parameters_.step == NULL ||
        parameters_.stop == NULL ||
        parameters_.reset == NULL)
    {
      parameters_.finalize(parameters_.job);
      throw OrthancException(ErrorCode_NullPointer);
    }

    type_.assign(parameters_.type);
  }
  
  PluginsJob::PluginsJob(const _OrthancPluginCreateJob2& parameters) :
    parameters_(parameters),
    deprecatedGetContent_(NULL),
    deprecatedGetSerialized_(NULL)
  {
    Setup();
  }

  PluginsJob::PluginsJob(const _OrthancPluginCreateJob& parameters)
  {
    LOG(WARNING) << "Your plugin is using the deprecated OrthancPluginCreateJob() function";

    memset(&parameters_, 0, sizeof(parameters_));
    parameters_.target = parameters.target;
    parameters_.job = parameters.job;
    parameters_.finalize = parameters.finalize;
    parameters_.type = parameters.type;
    parameters_.getProgress = parameters.getProgress;
    parameters_.getContent = NULL;
    parameters_.getSerialized = NULL;
    parameters_.step = parameters.step;
    parameters_.stop = parameters.stop;
    parameters_.reset = parameters.reset;

    deprecatedGetContent_ = parameters.getContent;
    deprecatedGetSerialized_ = parameters.getSerialized;
    
    Setup();
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


  namespace
  {
    class MemoryBufferRaii : public boost::noncopyable
    {
    private:
      OrthancPluginMemoryBuffer  buffer_;

    public:
      MemoryBufferRaii()
      {
        buffer_.size = 0;
        buffer_.data = NULL;
      }

      ~MemoryBufferRaii()
      {
        if (buffer_.size != 0)
        {
          free(buffer_.data);
        }
      }

      OrthancPluginMemoryBuffer* GetObject()
      {
        return &buffer_;
      }

      void ToJsonObject(Json::Value& target) const
      {
        if ((buffer_.data == NULL && buffer_.size != 0) ||
            (buffer_.data != NULL && buffer_.size == 0) ||
            !Toolbox::ReadJson(target, buffer_.data, buffer_.size) ||
            target.type() != Json::objectValue)
        {
          throw OrthancException(ErrorCode_Plugin,
                                 "A job plugin must provide a JSON object as its public content and as its serialization");
        }
      }
    };
  }
  
  void PluginsJob::GetPublicContent(Json::Value& value)
  {
    if (parameters_.getContent != NULL)
    {
      MemoryBufferRaii target;

      OrthancPluginErrorCode code = parameters_.getContent(target.GetObject(), parameters_.job);

      if (code != OrthancPluginErrorCode_Success)
      {
        throw OrthancException(static_cast<ErrorCode>(code));
      }
      else
      {
        target.ToJsonObject(value);
      }
    }
    else
    {
      // This was the source code in Orthanc <= 1.11.2
      const char* content = deprecatedGetContent_(parameters_.job);

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
  }

  bool PluginsJob::Serialize(Json::Value& value)
  {
    if (parameters_.getSerialized != NULL)
    {
      MemoryBufferRaii target;

      int32_t code = parameters_.getContent(target.GetObject(), parameters_.job);

      if (code < 0)
      {
        throw OrthancException(ErrorCode_Plugin, "Error during the serialization of a job");
      }
      else if (code == 0)
      {
        return false;  // Serialization is not implemented
      }
      else
      {
        target.ToJsonObject(value);

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
    else
    {
      // This was the source code in Orthanc <= 1.11.2
      const char* serialized = deprecatedGetSerialized_(parameters_.job);

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
}
