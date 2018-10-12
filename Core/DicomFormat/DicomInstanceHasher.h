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
  class DicomInstanceHasher
  {
  private:
    std::string patientId_;
    std::string studyUid_;
    std::string seriesUid_;
    std::string instanceUid_;

    std::string patientHash_;
    std::string studyHash_;
    std::string seriesHash_;
    std::string instanceHash_;

    void Setup(const std::string& patientId,
               const std::string& studyUid,
               const std::string& seriesUid,
               const std::string& instanceUid);

  public:
    DicomInstanceHasher(const DicomMap& instance);

    DicomInstanceHasher(const std::string& patientId,
                        const std::string& studyUid,
                        const std::string& seriesUid,
                        const std::string& instanceUid)
    {
      Setup(patientId, studyUid, seriesUid, instanceUid);
    }

    const std::string& GetPatientId() const
    {
      return patientId_;
    }

    const std::string& GetStudyUid() const
    {
      return studyUid_;
    }

    const std::string& GetSeriesUid() const
    {
      return seriesUid_;
    }

    const std::string& GetInstanceUid() const
    {
      return instanceUid_;
    }

    const std::string& HashPatient();

    const std::string& HashStudy();

    const std::string& HashSeries();

    const std::string& HashInstance();
  };
}
