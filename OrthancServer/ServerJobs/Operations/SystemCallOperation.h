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


#pragma once

#include "../../../Core/JobsEngine/Operations/IJobOperation.h"

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
    SystemCallOperation(const std::string& command) :
      command_(command)
    {
    }

    SystemCallOperation(const Json::Value& serialized);

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
                       const JobOperationValue& input,
                       IDicomConnectionManager& connectionManager);

    virtual void Serialize(Json::Value& result) const;
  };
}

