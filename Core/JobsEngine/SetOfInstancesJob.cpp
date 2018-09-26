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
#include "../SerializationToolbox.h"

namespace Orthanc
{
  SetOfInstancesJob::SetOfInstancesJob(bool hasTrailingStep) :
    hasTrailingStep_(hasTrailingStep),
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

    
  size_t SetOfInstancesJob::GetStepsCount() const
  {
    if (HasTrailingStep())
    {
      return instances_.size() + 1;
    }
    else
    {
      return instances_.size();
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
    if (started_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      permissive_ = permissive;
    }
  }


  void SetOfInstancesJob::Reset()
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
    const size_t steps = GetStepsCount();
    
    if (steps == 0)
    {
      return 0;
    }
    else
    {
      return (static_cast<float>(position_) /
              static_cast<float>(steps));
    }
  }


  const std::string& SetOfInstancesJob::GetInstance(size_t index) const
  {
    if (index >= instances_.size())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    else
    {
      return instances_[index];
    }
  }
      

  JobStepResult SetOfInstancesJob::Step()
  {
    if (!started_)
    {
      throw OrthancException(ErrorCode_InternalError);
    }
    
    const size_t steps = GetStepsCount();

    if (steps == 0 &&
        position_ == 0)
    {
      // Nothing to handle (no instance, nor trailing step): We're done
      position_ = 1;
      return JobStepResult::Success();
    }
    
    if (position_ >= steps)
    {
      // Already done
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    bool isTrailingStep = (hasTrailingStep_ &&
                           position_ + 1 == steps);
    
    bool ok;
      
    try
    {
      if (isTrailingStep)
      {
        ok = HandleTrailingStep();
      }
      else
      {
        // Not at the trailing step: Handle the current instance
        ok = HandleInstance(instances_[position_]);
      }

      if (!ok && !permissive_)
      {
        return JobStepResult::Failure(ErrorCode_InternalError);
      }
    }
    catch (OrthancException&)
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

    if (!ok &&
        !isTrailingStep)
    {
      failedInstances_.insert(instances_[position_]);
    }

    position_ += 1;

    if (position_ == steps)
    {
      // We're done
      return JobStepResult::Success();
    }
    else
    {
      return JobStepResult::Continue();
    }
  }



  static const char* KEY_DESCRIPTION = "Description";
  static const char* KEY_PERMISSIVE = "Permissive";
  static const char* KEY_POSITION = "Position";
  static const char* KEY_TYPE = "Type";
  static const char* KEY_INSTANCES = "Instances";
  static const char* KEY_FAILED_INSTANCES = "FailedInstances";
  static const char* KEY_TRAILING_STEP = "TrailingStep";

  
  void SetOfInstancesJob::GetPublicContent(Json::Value& value)
  {
    value[KEY_DESCRIPTION] = GetDescription();
    value["InstancesCount"] = static_cast<uint32_t>(instances_.size());
    value["FailedInstancesCount"] = static_cast<uint32_t>(failedInstances_.size());
  }    


  bool SetOfInstancesJob::Serialize(Json::Value& value)
  {
    value = Json::objectValue;

    std::string type;
    GetJobType(type);
    value[KEY_TYPE] = type;
    
    value[KEY_PERMISSIVE] = permissive_;
    value[KEY_POSITION] = static_cast<unsigned int>(position_);
    value[KEY_DESCRIPTION] = description_;
    value[KEY_TRAILING_STEP] = hasTrailingStep_;

    SerializationToolbox::WriteArrayOfStrings(value, instances_, KEY_INSTANCES);
    SerializationToolbox::WriteSetOfStrings(value, failedInstances_, KEY_FAILED_INSTANCES);

    return true;
  }


  SetOfInstancesJob::SetOfInstancesJob(const Json::Value& value) :
    started_(false),
    permissive_(SerializationToolbox::ReadBoolean(value, KEY_PERMISSIVE)),
    position_(SerializationToolbox::ReadUnsignedInteger(value, KEY_POSITION)),
    description_(SerializationToolbox::ReadString(value, KEY_DESCRIPTION))
  {
    SerializationToolbox::ReadArrayOfStrings(instances_, value, KEY_INSTANCES);
    SerializationToolbox::ReadSetOfStrings(failedInstances_, value, KEY_FAILED_INSTANCES);

    if (value.isMember(KEY_TRAILING_STEP))
    {
      hasTrailingStep_ = SerializationToolbox::ReadBoolean(value, KEY_TRAILING_STEP);
    }
    else
    {
      // Backward compatibility with Orthanc <= 1.4.2
      hasTrailingStep_ = false;
    }

    if (position_ > GetStepsCount() + 1)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
  }
}
