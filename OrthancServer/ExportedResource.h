/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
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

#include "ServerEnumerations.h"
#include "../Core/Toolbox.h"

#include <string>
#include <json/value.h>

namespace Orthanc
{
  class ExportedResource
  {
  private:
    int64_t      seq_;
    ResourceType resourceType_;
    std::string  publicId_;
    std::string  modality_;
    std::string  date_;
    std::string  patientId_;
    std::string  studyInstanceUid_;
    std::string  seriesInstanceUid_;
    std::string  sopInstanceUid_;

  public:
    ExportedResource(int64_t seq,
                     ResourceType resourceType,
                     const std::string& publicId,
                     const std::string& modality,
                     const std::string& date,
                     const std::string& patientId,
                     const std::string& studyInstanceUid,
                     const std::string& seriesInstanceUid,
                     const std::string& sopInstanceUid) :
      seq_(seq),
      resourceType_(resourceType),
      publicId_(publicId),
      modality_(modality),
      date_(date),
      patientId_(patientId),
      studyInstanceUid_(studyInstanceUid),
      seriesInstanceUid_(seriesInstanceUid),
      sopInstanceUid_(sopInstanceUid)
    {
    }

    int64_t  GetSeq() const
    {
      return seq_;
    }

    ResourceType  GetResourceType() const
    {
      return resourceType_;
    }

    const std::string&  GetPublicId() const
    {
      return publicId_;
    }

    const std::string& GetModality() const
    {
      return modality_;
    }

    const std::string& GetDate() const
    {
      return date_;
    }

    const std::string& GetPatientId() const
    {
      return patientId_;
    }

    const std::string& GetStudyInstanceUid() const
    {
      return studyInstanceUid_;
    }

    const std::string& GetSeriesInstanceUid() const
    {
      return seriesInstanceUid_;
    }

    const std::string& GetSopInstanceUid() const
    {
      return sopInstanceUid_;
    }

    void Format(Json::Value& item) const;
  };
}
