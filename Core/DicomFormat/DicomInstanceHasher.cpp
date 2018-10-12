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


#include "../PrecompiledHeaders.h"
#include "DicomInstanceHasher.h"

#include "../OrthancException.h"
#include "../Toolbox.h"

namespace Orthanc
{
  void DicomInstanceHasher::Setup(const std::string& patientId,
                                  const std::string& studyUid,
                                  const std::string& seriesUid,
                                  const std::string& instanceUid)
  {
    patientId_ = patientId;
    studyUid_ = studyUid;
    seriesUid_ = seriesUid;
    instanceUid_ = instanceUid;

    if (studyUid_.size() == 0 ||
        seriesUid_.size() == 0 ||
        instanceUid_.size() == 0)
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
  }

  DicomInstanceHasher::DicomInstanceHasher(const DicomMap& instance)
  {
    const DicomValue* patientId = instance.TestAndGetValue(DICOM_TAG_PATIENT_ID);

    Setup(patientId == NULL ? "" : patientId->GetContent(),
          instance.GetValue(DICOM_TAG_STUDY_INSTANCE_UID).GetContent(),
          instance.GetValue(DICOM_TAG_SERIES_INSTANCE_UID).GetContent(),
          instance.GetValue(DICOM_TAG_SOP_INSTANCE_UID).GetContent());
  }

  const std::string& DicomInstanceHasher::HashPatient()
  {
    if (patientHash_.size() == 0)
    {
      Toolbox::ComputeSHA1(patientHash_, patientId_);
    }

    return patientHash_;
  }

  const std::string& DicomInstanceHasher::HashStudy()
  {
    if (studyHash_.size() == 0)
    {
      Toolbox::ComputeSHA1(studyHash_, patientId_ + "|" + studyUid_);
    }

    return studyHash_;
  }

  const std::string& DicomInstanceHasher::HashSeries()
  {
    if (seriesHash_.size() == 0)
    {
      Toolbox::ComputeSHA1(seriesHash_, patientId_ + "|" + studyUid_ + "|" + seriesUid_);
    }

    return seriesHash_;
  }

  const std::string& DicomInstanceHasher::HashInstance()
  {
    if (instanceHash_.size() == 0)
    {
      Toolbox::ComputeSHA1(instanceHash_, patientId_ + "|" + studyUid_ + "|" + seriesUid_ + "|" + instanceUid_);
    }

    return instanceHash_;
  }
}
