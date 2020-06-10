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
#include "SetOfCommandsJob.h"

#include <set>

namespace Orthanc
{
  class SetOfInstancesJob : public SetOfCommandsJob
  {
  private:
    class InstanceCommand;
    class TrailingStepCommand;
    class InstanceUnserializer;
    
    bool                   hasTrailingStep_;
    std::set<std::string>  failedInstances_;
    std::set<std::string>  parentResources_;

  protected:
    virtual bool HandleInstance(const std::string& instance) = 0;

    virtual bool HandleTrailingStep() = 0;

    // Hiding this method, use AddInstance() instead
    using SetOfCommandsJob::AddCommand;

  public:
    SetOfInstancesJob();

    SetOfInstancesJob(const Json::Value& source);  // Unserialization

    // Only used for reporting in the public content
    // https://groups.google.com/d/msg/orthanc-users/9GCV88GLEzw/6wAgP_PRAgAJ
    void AddParentResource(const std::string& resource)
    {
      parentResources_.insert(resource);
    }
    
    void AddInstance(const std::string& instance);

    void AddTrailingStep(); 

    size_t GetInstancesCount() const;
    
    const std::string& GetInstance(size_t index) const;

    bool HasTrailingStep() const
    {
      return hasTrailingStep_;
    }

    const std::set<std::string>& GetFailedInstances() const
    {
      return failedInstances_;
    }

    bool IsFailedInstance(const std::string& instance) const
    {
      return failedInstances_.find(instance) != failedInstances_.end();
    }

    virtual void Start();

    virtual void Reset();

    virtual void GetPublicContent(Json::Value& target);

    virtual bool Serialize(Json::Value& target);
  };
}
