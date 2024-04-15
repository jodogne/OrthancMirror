/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2024 Osimis S.A., Belgium
 * Copyright (C) 2021-2024 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include "../../../OrthancFramework/Sources/Compatibility.h"
#include "../../../OrthancFramework/Sources/Enumerations.h"

#include <boost/noncopyable.hpp>
#include <string>


namespace Orthanc
{
  class OrthancIdentifiers : public boost::noncopyable
  {
  private:
    std::unique_ptr<std::string>  patientId_;
    std::unique_ptr<std::string>  studyId_;
    std::unique_ptr<std::string>  seriesId_;
    std::unique_ptr<std::string>  instanceId_;

  public:
    OrthancIdentifiers()
    {
    }

    OrthancIdentifiers(const OrthancIdentifiers& other);

    void SetPatientId(const std::string& id);

    bool HasPatientId() const
    {
      return patientId_.get() != NULL;
    }

    const std::string& GetPatientId() const;

    void SetStudyId(const std::string& id);

    bool HasStudyId() const
    {
      return studyId_.get() != NULL;
    }

    const std::string& GetStudyId() const;

    void SetSeriesId(const std::string& id);

    bool HasSeriesId() const
    {
      return seriesId_.get() != NULL;
    }

    const std::string& GetSeriesId() const;

    void SetInstanceId(const std::string& id);

    bool HasInstanceId() const
    {
      return instanceId_.get() != NULL;
    }

    const std::string& GetInstanceId() const;

    ResourceType DetectLevel() const;

    void SetLevel(ResourceType level,
                  const std::string id);

    std::string GetLevel(ResourceType level) const;
  };
}
