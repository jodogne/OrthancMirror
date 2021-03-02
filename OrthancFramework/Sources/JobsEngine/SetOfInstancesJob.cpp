/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 **/


#include "../PrecompiledHeaders.h"
#include "SetOfInstancesJob.h"

#include "../OrthancException.h"
#include "../SerializationToolbox.h"

#include <cassert>

namespace Orthanc
{
  class SetOfInstancesJob::InstanceCommand : public SetOfInstancesJob::ICommand
  {
  private:
    SetOfInstancesJob& that_;
    std::string        instance_;

  public:
    InstanceCommand(SetOfInstancesJob& that,
                    const std::string& instance) :
      that_(that),
      instance_(instance)
    {
    }

    const std::string& GetInstance() const
    {
      return instance_;
    }
      
    virtual bool Execute(const std::string& jobId) ORTHANC_OVERRIDE
    {
      if (!that_.HandleInstance(instance_))
      {
        that_.failedInstances_.insert(instance_);
        return false;
      }
      else
      {
        return true;
      }
    }

    virtual void Serialize(Json::Value& target) const ORTHANC_OVERRIDE
    {
      target = instance_;
    }
  };


  class SetOfInstancesJob::TrailingStepCommand : public SetOfInstancesJob::ICommand
  {
  private:
    SetOfInstancesJob& that_;

  public:
    explicit TrailingStepCommand(SetOfInstancesJob& that) :
      that_(that)
    {
    }       
      
    virtual bool Execute(const std::string& jobId) ORTHANC_OVERRIDE
    {
      return that_.HandleTrailingStep();
    }

    virtual void Serialize(Json::Value& target) const ORTHANC_OVERRIDE
    {
      target = Json::nullValue;
    }
  };


  class SetOfInstancesJob::InstanceUnserializer :
    public SetOfInstancesJob::ICommandUnserializer
  {
  private:
    SetOfInstancesJob& that_;

  public:
    explicit InstanceUnserializer(SetOfInstancesJob& that) :
      that_(that)
    {
    }

    virtual ICommand* Unserialize(const Json::Value& source) const
    {
      if (source.type() == Json::nullValue)
      {
        return new TrailingStepCommand(that_);
      }
      else if (source.type() == Json::stringValue)
      {
        return new InstanceCommand(that_, source.asString());
      }
      else
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }
    }
  };
    

  SetOfInstancesJob::SetOfInstancesJob() :
    hasTrailingStep_(false)
  {
  }


  void SetOfInstancesJob::AddParentResource(const std::string &resource)
  {
    parentResources_.insert(resource);
  }


  void SetOfInstancesJob::AddInstance(const std::string& instance)
  {
    AddCommand(new InstanceCommand(*this, instance));
  }


  void SetOfInstancesJob::AddTrailingStep()
  {
    AddCommand(new TrailingStepCommand(*this));
    hasTrailingStep_ = true;
  }
  
  
  size_t SetOfInstancesJob::GetInstancesCount() const
  {
    if (hasTrailingStep_)
    {
      assert(GetCommandsCount() > 0);
      return GetCommandsCount() - 1;
    }
    else
    {
      return GetCommandsCount();
    }
  }

  
  const std::string& SetOfInstancesJob::GetInstance(size_t index) const
  {
    if (index >= GetInstancesCount())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    else
    {
      return dynamic_cast<const InstanceCommand&>(GetCommand(index)).GetInstance();
    }
  }

  bool SetOfInstancesJob::HasTrailingStep() const
  {
    return hasTrailingStep_;
  }

  const std::set<std::string> &SetOfInstancesJob::GetFailedInstances() const
  {
    return failedInstances_;
  }

  bool SetOfInstancesJob::IsFailedInstance(const std::string &instance) const
  {
    return failedInstances_.find(instance) != failedInstances_.end();
  }


  void SetOfInstancesJob::Start()
  {
    SetOfCommandsJob::Start();
  }


  void SetOfInstancesJob::Reset()
  {
    SetOfCommandsJob::Reset();

    failedInstances_.clear();
  }


  static const char* KEY_TRAILING_STEP = "TrailingStep";
  static const char* KEY_FAILED_INSTANCES = "FailedInstances";
  static const char* KEY_PARENT_RESOURCES = "ParentResources";

  void SetOfInstancesJob::GetPublicContent(Json::Value& target)
  {
    SetOfCommandsJob::GetPublicContent(target);
    target["InstancesCount"] = static_cast<uint32_t>(GetInstancesCount());
    target["FailedInstancesCount"] = static_cast<uint32_t>(failedInstances_.size());

    if (!parentResources_.empty())
    {
      SerializationToolbox::WriteSetOfStrings(target, parentResources_, KEY_PARENT_RESOURCES);
    }
  }


  bool SetOfInstancesJob::Serialize(Json::Value& target) 
  {
    if (SetOfCommandsJob::Serialize(target))
    {
      target[KEY_TRAILING_STEP] = hasTrailingStep_;
      SerializationToolbox::WriteSetOfStrings(target, failedInstances_, KEY_FAILED_INSTANCES);
      SerializationToolbox::WriteSetOfStrings(target, parentResources_, KEY_PARENT_RESOURCES);
      return true;
    }
    else
    {
      return false;
    }
  }
  

  SetOfInstancesJob::SetOfInstancesJob(const Json::Value& source) :
    SetOfCommandsJob(new InstanceUnserializer(*this), source)
  {
    SerializationToolbox::ReadSetOfStrings(failedInstances_, source, KEY_FAILED_INSTANCES);

    if (source.isMember(KEY_PARENT_RESOURCES))
    {
      // Backward compatibility with Orthanc <= 1.5.6
      SerializationToolbox::ReadSetOfStrings(parentResources_, source, KEY_PARENT_RESOURCES);
    }
    
    if (source.isMember(KEY_TRAILING_STEP))
    {
      hasTrailingStep_ = SerializationToolbox::ReadBoolean(source, KEY_TRAILING_STEP);
    }
    else
    {
      // Backward compatibility with Orthanc <= 1.4.2
      hasTrailingStep_ = false;
    }
  }
}
