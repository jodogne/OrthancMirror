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


#include "../../OrthancServer/PrecompiledHeadersServer.h"
#include "PluginsJob.h"

#if ORTHANC_ENABLE_PLUGINS != 1
#error The plugin support is disabled
#endif


#include "../../Core/OrthancException.h"

#include <json/reader.h>
#include <cassert>

namespace Orthanc
{
  PluginsJob::PluginsJob(const _OrthancPluginSubmitJob& parameters) :
    job_(parameters.job_),
    free_(parameters.free_),
    getProgress_(parameters.getProgress_),
    step_(parameters.step_),
    releaseResources_(parameters.releaseResources_),
    reset_(parameters.reset_)
  {
    if (job_ == NULL ||
        parameters.type_ == NULL ||
        free_ == NULL ||
        getProgress_ == NULL ||
        step_ == NULL ||
        releaseResources_ == NULL ||
        reset_ == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }

    type_.assign(parameters.type_);

    if (parameters.content_ == NULL)
    {
      publicContent_ = Json::objectValue;
    }
    else
    {
      Json::Reader reader;
      if (!reader.parse(parameters.content_, publicContent_) ||
          publicContent_.type() != Json::objectValue)
      {
        free_(job_);
        throw OrthancException(ErrorCode_BadFileFormat);
      }
    }

    if (parameters.serialized_ == NULL)
    {
      hasSerialized_ = false;
    }
    else
    {
      hasSerialized_ = true;

      Json::Reader reader;
      if (!reader.parse(parameters.serialized_, serialized_) ||
          serialized_.type() != Json::objectValue ||
          !serialized_.isMember("Type") ||
          serialized_["Type"].type() != Json::stringValue)
      {
        free_(job_);
        throw OrthancException(ErrorCode_BadFileFormat);
      }
    }
  }

  PluginsJob::~PluginsJob()
  {
    assert(job_ != NULL);
    free_(job_);
  }

  JobStepResult PluginsJob::ExecuteStep()
  {
    OrthancPluginJobStepStatus status = step_(job_);

    switch (status)
    {
      case OrthancPluginJobStepStatus_Success:
        return JobStepResult::Success();

      case OrthancPluginJobStepStatus_Failure:
        return JobStepResult::Failure(ErrorCode_Plugin);

      case OrthancPluginJobStepStatus_Continue:
        return JobStepResult::Continue();

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }

  void PluginsJob::SignalResubmit()
  {
    reset_(job_);
  }

  void PluginsJob::ReleaseResources(JobReleaseReason reason)
  {
    switch (reason)
    {
      case JobReleaseReason_Success:
        releaseResources_(job_, OrthancPluginJobReleaseReason_Success);
        break;

      case JobReleaseReason_Failure:
        releaseResources_(job_, OrthancPluginJobReleaseReason_Failure);
        break;

      case JobReleaseReason_Canceled:
        releaseResources_(job_, OrthancPluginJobReleaseReason_Canceled);
        break;

      case JobReleaseReason_Paused:
        releaseResources_(job_, OrthancPluginJobReleaseReason_Paused);
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }

  float PluginsJob::GetProgress()
  {
    return getProgress_(job_);
  }

  bool PluginsJob::Serialize(Json::Value& value)
  {
    if (hasSerialized_)
    {
      value = serialized_;
      return true;
    }
    else
    {
      return false;
    }
  }
}
