/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

namespace Orthanc
{
  class JobStatus
  {
  private:
    ErrorCode      errorCode_;
    float          progress_;
    std::string    jobType_;
    Json::Value    publicContent_;
    Json::Value    serialized_;
    bool           hasSerialized_;
    std::string    details_;

  public:
    JobStatus();

    JobStatus(ErrorCode code,
              const std::string& details,
              IJob& job);

    ErrorCode GetErrorCode() const
    {
      return errorCode_;
    }

    void SetErrorCode(ErrorCode error)
    {
      errorCode_ = error;
    }

    float GetProgress() const
    {
      return progress_;
    }

    const std::string& GetJobType() const
    {
      return jobType_;
    }

    const Json::Value& GetPublicContent() const
    {
      return publicContent_;
    }

    const Json::Value& GetSerialized() const;

    bool HasSerialized() const
    {
      return hasSerialized_;
    }

    const std::string& GetDetails() const
    {
      return details_;
    }
  };
}
