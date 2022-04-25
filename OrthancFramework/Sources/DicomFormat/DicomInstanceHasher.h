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

#include "DicomMap.h"

namespace Orthanc
{
  /**
   * This class implements the hashing mechanism that is used to
   * convert DICOM unique identifiers to Orthanc identifiers. Any
   * Orthanc identifier for a DICOM resource corresponds to the SHA-1
   * hash of the DICOM identifiers. 

   * \note SHA-1 hash is used because it is less sensitive to
   * collision attacks than MD5. <a
   * href="http://en.wikipedia.org/wiki/SHA-256#Comparison_of_SHA_functions">[Reference]</a>
   **/
  class ORTHANC_PUBLIC DicomInstanceHasher
  {
  private:
    std::string patientId_;
    std::string studyUid_;
    std::string seriesUid_;
    std::string instanceUid_;

    mutable std::string patientHash_;
    mutable std::string studyHash_;
    mutable std::string seriesHash_;
    mutable std::string instanceHash_;

    void Setup(const std::string& patientId,
               const std::string& studyUid,
               const std::string& seriesUid,
               const std::string& instanceUid);

  public:
    explicit DicomInstanceHasher(const DicomMap& instance);

    DicomInstanceHasher(const std::string& patientId,
                        const std::string& studyUid,
                        const std::string& seriesUid,
                        const std::string& instanceUid);

    const std::string& GetPatientId() const;

    const std::string& GetStudyUid() const;

    const std::string& GetSeriesUid() const;

    const std::string& GetInstanceUid() const;

    const std::string& HashPatient() const;

    const std::string& HashStudy() const;

    const std::string& HashSeries() const;

    const std::string& HashInstance() const;
  };
}
