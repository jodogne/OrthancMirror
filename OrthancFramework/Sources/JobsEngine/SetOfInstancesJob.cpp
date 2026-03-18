/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
#include "../Logging.h"

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

    virtual ICommand* Unserialize(const Json::Value& source) const ORTHANC_OVERRIDE
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


  void SetOfInstancesJob::AddParentResource(const std::string &resource, ResourceType level)
  {
    parentResources_[resource] = level;
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


  void SetOfInstancesJob::Reset()
  {
    SetOfCommandsJob::Reset();

    failedInstances_.clear();
  }


  static const char* KEY_TRAILING_STEP = "TrailingStep";
  static const char* KEY_FAILED_INSTANCES = "FailedInstances";
  static const char* KEY_PARENT_RESOURCES = "ParentResources"; // old style but we keep it for backward compatibility
  static const char* KEY_RESOURCES = "Resources"; // new style with the Resource type


  static void SerializeResources(Json::Value& target, const std::map<std::string, ResourceType>& parentResources, bool includeParentResourcesField)
  {
    if (!parentResources.empty())
    {
      target[KEY_RESOURCES] = Json::arrayValue;
      SerializationToolbox::WriteMapOfResourcesAndTypes(target[KEY_RESOURCES], parentResources);

      if (includeParentResourcesField)
      {
        std::set<std::string> keys;
        for (std::map<std::string, ResourceType>::const_iterator it = parentResources.begin(); it != parentResources.end(); ++it) 
        {
          keys.insert(it->first);
        }

        SerializationToolbox::WriteSetOfStrings(target, keys, KEY_PARENT_RESOURCES);
      }
    }
  }

  void SetOfInstancesJob::GetPublicContent(Json::Value& target) const
  {
    SetOfCommandsJob::GetPublicContent(target);
    target["InstancesCount"] = static_cast<uint32_t>(GetInstancesCount());
    target["FailedInstancesCount"] = static_cast<uint32_t>(failedInstances_.size());
    
    SerializeResources(target, parentResources_, true);
  }

  bool SetOfInstancesJob::Serialize(Json::Value& target) const 
  {
    if (SetOfCommandsJob::Serialize(target))
    {
      target[KEY_TRAILING_STEP] = hasTrailingStep_;
      SerializationToolbox::WriteSetOfStrings(target, failedInstances_, KEY_FAILED_INSTANCES);
      SerializeResources(target, parentResources_, false);
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

    if (source.isMember(KEY_PARENT_RESOURCES) && !source.isMember(KEY_RESOURCES))
    {
      LOG(ERROR) << "Unable to read the " << KEY_PARENT_RESOURCES << " of a job that has been saved with the previous version of Orthanc";
    }
    else if (source.isMember(KEY_RESOURCES) && source[KEY_RESOURCES].isArray())
    {
      SerializationToolbox::ReadMapOfResourcesAndTypes(parentResources_, source, KEY_RESOURCES);
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
