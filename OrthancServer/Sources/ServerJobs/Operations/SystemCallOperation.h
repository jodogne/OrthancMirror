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


#pragma once

#include "../../../../OrthancFramework/Sources/Compatibility.h"  // For ORTHANC_OVERRIDE
#include "../../../../OrthancFramework/Sources/JobsEngine/Operations/IJobOperation.h"

#include <string>

namespace Orthanc
{
  class SystemCallOperation : public IJobOperation
  {
  private:
    std::string               command_;
    std::vector<std::string>  preArguments_;
    std::vector<std::string>  postArguments_;
    
  public:
    explicit SystemCallOperation(const std::string& command) :
      command_(command)
    {
    }

    explicit SystemCallOperation(const Json::Value& serialized);

    SystemCallOperation(const std::string& command,
                        const std::vector<std::string>& preArguments,
                        const std::vector<std::string>& postArguments) :
      command_(command),
      preArguments_(preArguments),
      postArguments_(postArguments)
    {
    }

    void AddPreArgument(const std::string& argument)
    {
      preArguments_.push_back(argument);
    }

    void AddPostArgument(const std::string& argument)
    {
      postArguments_.push_back(argument);
    }

    const std::string& GetCommand() const
    {
      return command_;
    }

    size_t GetPreArgumentsCount() const
    {
      return preArguments_.size();
    }

    size_t GetPostArgumentsCount() const
    {
      return postArguments_.size();
    }

    const std::string& GetPreArgument(size_t i) const;

    const std::string& GetPostArgument(size_t i) const;

    virtual void Apply(JobOperationValues& outputs,
                       const IJobOperationValue& input) ORTHANC_OVERRIDE;

    virtual void Serialize(Json::Value& result) const ORTHANC_OVERRIDE;
  };
}

