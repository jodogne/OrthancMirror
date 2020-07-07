/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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

#include "JobStatus.h"

#include <boost/date_time/posix_time/posix_time.hpp>

namespace Orthanc
{
  class ORTHANC_PUBLIC JobInfo
  {
  private:
    std::string                       id_;
    int                               priority_;
    JobState                          state_;
    boost::posix_time::ptime          timestamp_;
    boost::posix_time::ptime          creationTime_;
    boost::posix_time::ptime          lastStateChangeTime_;
    boost::posix_time::time_duration  runtime_;
    bool                              hasEta_;
    boost::posix_time::ptime          eta_;
    JobStatus                         status_;

  public:
    JobInfo(const std::string& id,
            int priority,
            JobState state,
            const JobStatus& status,
            const boost::posix_time::ptime& creationTime,
            const boost::posix_time::ptime& lastStateChangeTime,
            const boost::posix_time::time_duration& runtime);

    JobInfo();

    const std::string& GetIdentifier() const
    {
      return id_;
    }

    int GetPriority() const
    {
      return priority_;
    }

    JobState GetState() const
    {
      return state_;
    }

    const boost::posix_time::ptime& GetInfoTime() const
    {
      return timestamp_;
    }

    const boost::posix_time::ptime& GetCreationTime() const
    {
      return creationTime_;
    }

    const boost::posix_time::time_duration& GetRuntime() const
    {
      return runtime_;
    }

    bool HasEstimatedTimeOfArrival() const
    {
      return hasEta_;
    }

    bool HasCompletionTime() const;

    const boost::posix_time::ptime& GetEstimatedTimeOfArrival() const;

    const boost::posix_time::ptime& GetCompletionTime() const;

    const JobStatus& GetStatus() const
    {
      return status_;
    }

    JobStatus& GetStatus()
    {
      return status_;
    }

    void Format(Json::Value& target) const;
  };
}
