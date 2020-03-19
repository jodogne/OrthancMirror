/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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
#include "SetOfCommandsJob.h"

#include "../Logging.h"
#include "../OrthancException.h"
#include "../SerializationToolbox.h"

#include <cassert>
#include <memory>

namespace Orthanc
{
  SetOfCommandsJob::SetOfCommandsJob() :
    started_(false),
    permissive_(false),
    position_(0)
  {
  }


  SetOfCommandsJob::~SetOfCommandsJob()
  {
    for (size_t i = 0; i < commands_.size(); i++)
    {
      assert(commands_[i] != NULL);
      delete commands_[i];
    }
  }

    
  void SetOfCommandsJob::Reserve(size_t size)
  {
    if (started_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      commands_.reserve(size);
    }
  }

    
  void SetOfCommandsJob::AddCommand(ICommand* command)
  {
    if (command == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
    else if (started_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      commands_.push_back(command);
    }
  }


  void SetOfCommandsJob::SetPermissive(bool permissive)
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


  void SetOfCommandsJob::Reset()
  {
    if (started_)
    {
      position_ = 0;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }

    
  float SetOfCommandsJob::GetProgress()
  {
    if (commands_.empty())
    {
      return 1;
    }
    else
    {
      return (static_cast<float>(position_) /
              static_cast<float>(commands_.size()));
    }
  }


  const SetOfCommandsJob::ICommand& SetOfCommandsJob::GetCommand(size_t index) const
  {
    if (index >= commands_.size())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    else
    {
      assert(commands_[index] != NULL);
      return *commands_[index];
    }
  }
      

  JobStepResult SetOfCommandsJob::Step(const std::string& jobId)
  {
    if (!started_)
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    if (commands_.empty() &&
        position_ == 0)
    {
      // No command to handle: We're done
      position_ = 1;
      return JobStepResult::Success();
    }
    
    if (position_ >= commands_.size())
    {
      // Already done
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    try
    {
      // Not at the trailing step: Handle the current command
      if (!commands_[position_]->Execute(jobId))
      {
        // Error
        if (!permissive_)
        {
          return JobStepResult::Failure(ErrorCode_InternalError, NULL);
        }
      }
    }
    catch (OrthancException& e)
    {
      if (permissive_)
      {
        LOG(WARNING) << "Ignoring an error in a permissive job: " << e.What();
      }
      else
      {
        return JobStepResult::Failure(e);
      }
    }

    position_ += 1;

    if (position_ == commands_.size())
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
  static const char* KEY_COMMANDS = "Commands";

  
  void SetOfCommandsJob::GetPublicContent(Json::Value& value)
  {
    value[KEY_DESCRIPTION] = GetDescription();
  }    


  bool SetOfCommandsJob::Serialize(Json::Value& target)
  {
    target = Json::objectValue;

    std::string type;
    GetJobType(type);
    target[KEY_TYPE] = type;
    
    target[KEY_PERMISSIVE] = permissive_;
    target[KEY_POSITION] = static_cast<unsigned int>(position_);
    target[KEY_DESCRIPTION] = description_;

    target[KEY_COMMANDS] = Json::arrayValue;
    Json::Value& tmp = target[KEY_COMMANDS];

    for (size_t i = 0; i < commands_.size(); i++)
    {
      assert(commands_[i] != NULL);
      
      Json::Value command;
      commands_[i]->Serialize(command);
      tmp.append(command);
    }

    return true;
  }


  SetOfCommandsJob::SetOfCommandsJob(ICommandUnserializer* unserializer,
                                     const Json::Value& source) :
    started_(false)
  {
    std::unique_ptr<ICommandUnserializer> raii(unserializer);

    permissive_ = SerializationToolbox::ReadBoolean(source, KEY_PERMISSIVE);
    position_ = SerializationToolbox::ReadUnsignedInteger(source, KEY_POSITION);
    description_ = SerializationToolbox::ReadString(source, KEY_DESCRIPTION);
    
    if (!source.isMember(KEY_COMMANDS) ||
        source[KEY_COMMANDS].type() != Json::arrayValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
    else
    {
      const Json::Value& tmp = source[KEY_COMMANDS];
      commands_.resize(tmp.size());

      for (Json::Value::ArrayIndex i = 0; i < tmp.size(); i++)
      {
        try
        {
          commands_[i] = unserializer->Unserialize(tmp[i]);
        }
        catch (OrthancException&)
        {
        }

        if (commands_[i] == NULL)
        {
          for (size_t j = 0; j < i; j++)
          {
            delete commands_[j];
          }

          throw OrthancException(ErrorCode_BadFileFormat);
        }
      }
    }

    if (commands_.empty())
    {
      if (position_ > 1)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }
    }
    else if (position_ > commands_.size())
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
  }
}
