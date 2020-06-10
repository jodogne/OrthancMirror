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

#include "IJob.h"

#include <set>

namespace Orthanc
{
  class SetOfCommandsJob : public IJob
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

    size_t GetPosition() const
    {
      return position_;
    }

    void SetDescription(const std::string& description)
    {
      description_ = description;
    }

    const std::string& GetDescription() const
    {
      return description_;
    }

    void Reserve(size_t size);

    size_t GetCommandsCount() const
    {
      return commands_.size();
    }

    void AddCommand(ICommand* command);  // Takes ownership

    bool IsPermissive() const
    {
      return permissive_;
    }

    void SetPermissive(bool permissive);

    virtual void Reset() ORTHANC_OVERRIDE;
    
    virtual void Start() ORTHANC_OVERRIDE
    {
      started_ = true;
    }
    
    virtual float GetProgress() ORTHANC_OVERRIDE;

    bool IsStarted() const
    {
      return started_;
    }

    const ICommand& GetCommand(size_t index) const;
      
    virtual JobStepResult Step(const std::string& jobId) ORTHANC_OVERRIDE;
    
    virtual void GetPublicContent(Json::Value& value) ORTHANC_OVERRIDE;
    
    virtual bool Serialize(Json::Value& target) ORTHANC_OVERRIDE;

    virtual bool GetOutput(std::string& output,
                           MimeType& mime,
                           const std::string& key) ORTHANC_OVERRIDE
    {
      return false;
    }
  };
}
