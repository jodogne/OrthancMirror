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


#include "../../PrecompiledHeadersServer.h"
#include "SystemCallOperation.h"

#include "DicomInstanceOperationValue.h"

#include "../../../../OrthancFramework/Sources/JobsEngine/Operations/StringOperationValue.h"
#include "../../../../OrthancFramework/Sources/Logging.h"
#include "../../../../OrthancFramework/Sources/OrthancException.h"
#include "../../../../OrthancFramework/Sources/SerializationToolbox.h"
#include "../../../../OrthancFramework/Sources/TemporaryFile.h"
#include "../../../../OrthancFramework/Sources/Toolbox.h"
#include "../../../../OrthancFramework/Sources/SystemToolbox.h"
#include "../../OrthancConfiguration.h"

namespace Orthanc
{
  const std::string& SystemCallOperation::GetPreArgument(size_t i) const
  {
    if (i >= preArguments_.size())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    else
    {
      return preArguments_[i];
    }
  }

  
  const std::string& SystemCallOperation::GetPostArgument(size_t i) const
  {
    if (i >= postArguments_.size())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    else
    {
      return postArguments_[i];
    }
  }


  void SystemCallOperation::Apply(JobOperationValues& outputs,
                                  const IJobOperationValue& input)
  {
    std::vector<std::string> arguments = preArguments_;

    arguments.reserve(arguments.size() + postArguments_.size() + 1);

    std::unique_ptr<TemporaryFile> tmp;
    
    switch (input.GetType())
    {
      case IJobOperationValue::Type_DicomInstance:
      {
        const DicomInstanceOperationValue& instance =
          dynamic_cast<const DicomInstanceOperationValue&>(input);

        std::string dicom;
        instance.ReadDicom(dicom);

        {
          OrthancConfiguration::ReaderLock lock;
          tmp.reset(lock.GetConfiguration().CreateTemporaryFile());
        }

        tmp->Write(dicom);
        
        arguments.push_back(tmp->GetPath());
        break;
      }

      case IJobOperationValue::Type_String:
      {
        const StringOperationValue& value =
          dynamic_cast<const StringOperationValue&>(input);

        arguments.push_back(value.GetContent());
        break;
      }

      case IJobOperationValue::Type_Null:
        break;

      default:
        throw OrthancException(ErrorCode_BadParameterType);
    }

    for (size_t i = 0; i < postArguments_.size(); i++)
    {
      arguments.push_back(postArguments_[i]);
    }

    std::string info = command_;
    for (size_t i = 0; i < arguments.size(); i++)
    {
      info += " " + arguments[i];
    }
    
    LOG(INFO) << "Lua: System call: \"" << info << "\"";

    try
    {
      SystemToolbox::ExecuteSystemCommand(command_, arguments);

      // Only chain with other commands if this operation succeeds
      outputs.Append(input.Clone());
    }
    catch (OrthancException& e)
    {
      LOG(ERROR) << "Lua: Failed system call - \"" << info << "\": " << e.What();
    }
  }


  void SystemCallOperation::Serialize(Json::Value& result) const
  {
    result = Json::objectValue;
    result["Type"] = "SystemCall";
    result["Command"] = command_;
    SerializationToolbox::WriteArrayOfStrings(result, preArguments_, "PreArguments");
    SerializationToolbox::WriteArrayOfStrings(result, postArguments_, "PostArguments");
  }


  SystemCallOperation::SystemCallOperation(const Json::Value& serialized)
  {
    if (SerializationToolbox::ReadString(serialized, "Type") != "SystemCall")
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    command_ = SerializationToolbox::ReadString(serialized, "Command");
    SerializationToolbox::ReadArrayOfStrings(preArguments_, serialized, "PreArguments");
    SerializationToolbox::ReadArrayOfStrings(postArguments_, serialized, "PostArguments");
  }
}
