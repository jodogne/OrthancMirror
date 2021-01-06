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


#pragma once

#include "IJob.h"

#include "../Compatibility.h"  // For ORTHANC_OVERRIDE

#include <set>

namespace Orthanc
{
  class ORTHANC_PUBLIC SetOfCommandsJob : public IJob
  {
  public:
    class ICommand : public boost::noncopyable
    {
    public:
      virtual ~ICommand()
      {
      }

      virtual bool Execute(const std::string& jobId) = 0;

      virtual void Serialize(Json::Value& target) const = 0;
    };

    class ICommandUnserializer : public boost::noncopyable
    {
    public:
      virtual ~ICommandUnserializer()
      {
      }
      
      virtual ICommand* Unserialize(const Json::Value& source) const = 0;
    };
    
  private:
    bool                    started_;
    std::vector<ICommand*>  commands_;
    bool                    permissive_;
    size_t                  position_;
    std::string             description_;

  public:
    SetOfCommandsJob();

    SetOfCommandsJob(ICommandUnserializer* unserializer  /* takes ownership */,
                     const Json::Value& source);

    virtual ~SetOfCommandsJob();

    size_t GetPosition() const;

    void SetDescription(const std::string& description);

    const std::string& GetDescription() const;

    void Reserve(size_t size);

    size_t GetCommandsCount() const;

    void AddCommand(ICommand* command);  // Takes ownership

    bool IsPermissive() const;

    void SetPermissive(bool permissive);

    virtual void Reset() ORTHANC_OVERRIDE;
    
    virtual void Start() ORTHANC_OVERRIDE;
    
    virtual float GetProgress() ORTHANC_OVERRIDE;

    bool IsStarted() const;

    const ICommand& GetCommand(size_t index) const;
      
    virtual JobStepResult Step(const std::string& jobId) ORTHANC_OVERRIDE;
    
    virtual void GetPublicContent(Json::Value& value) ORTHANC_OVERRIDE;
    
    virtual bool Serialize(Json::Value& target) ORTHANC_OVERRIDE;

    virtual bool GetOutput(std::string& output,
                           MimeType& mime,
                           const std::string& key) ORTHANC_OVERRIDE;
  };
}
