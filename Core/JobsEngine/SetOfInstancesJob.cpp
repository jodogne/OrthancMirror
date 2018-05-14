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


#include "../PrecompiledHeaders.h"
#include "SetOfInstancesJob.h"

#include "../OrthancException.h"

namespace Orthanc
{
  SetOfInstancesJob::SetOfInstancesJob() :
    started_(false),
    permissive_(false),
    position_(0)
  {
  }

    
  void SetOfInstancesJob::Reserve(size_t size)
  {
    if (started_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      instances_.reserve(size);
    }
  }

    
  void SetOfInstancesJob::AddInstance(const std::string& instance)
  {
    if (started_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      instances_.push_back(instance);
    }
  }


  void SetOfInstancesJob::SetPermissive(bool permissive)
  {
    if (IsStarted())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      permissive_ = permissive;
    }
  }


  void SetOfInstancesJob::SignalResubmit()
  {
    if (started_)
    {
      position_ = 0;
      failedInstances_.clear();
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }

    
  float SetOfInstancesJob::GetProgress()
  {
    if (instances_.size() == 0)
    {
      return 0;
    }
    else
    {
      return (static_cast<float>(position_) /
              static_cast<float>(instances_.size()));
    }
  }


  void SetOfInstancesJob::Next()
  {
    if (IsDone())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      position_ += 1;
    }
  }


  const std::string& SetOfInstancesJob::GetCurrentInstance() const
  {
    if (IsDone())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      return instances_[position_];
    }      
  }


  JobStepResult* SetOfInstancesJob::ExecuteStep()
  {
    if (IsDone())
    {
      return new JobStepResult(JobStepCode_Failure);
    }

    bool ok;
      
    try
    {
      ok = HandleInstance(GetCurrentInstance());

      if (!ok && !permissive_)
      {
        throw OrthancException(ErrorCode_InternalError);
      }
    }
    catch (OrthancException& e)
    {
      if (permissive_)
      {
        ok = false;
      }
      else
      {
        throw;
      }
    }

    if (!ok)
    {
      failedInstances_.insert(GetCurrentInstance());
    }

    Next();

    if (IsDone())
    {
      return new JobStepResult(JobStepCode_Success);
    }
    else
    {
      return new JobStepResult(JobStepCode_Continue);
    }
  }

    
  void SetOfInstancesJob::GetInternalContent(Json::Value& value)
  {
    Json::Value v = Json::arrayValue;
      
    for (size_t i = 0; i < instances_.size(); i++)
    {
      v.append(instances_[i]);
    }

    value["Instances"] = v;

      
    v = Json::arrayValue;

    for (std::set<std::string>::const_iterator it = failedInstances_.begin();
         it != failedInstances_.end(); ++it)
    {
      v.append(*it);
    }
      
    value["FailedInstances"] = v;
  }
}
