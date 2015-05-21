/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
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

#include "Study.h"

namespace OrthancClient
{
  /**
   * {summary}{Connection to a patient stored in %Orthanc.}
   * {description}{This class encapsulates a connection to a patient
   * from a remote instance of %Orthanc.}
   **/
  class LAAW_API Patient : 
    public Orthanc::IDynamicObject, 
    private Orthanc::ArrayFilledByThreads::IFiller
  {
  private:
    const OrthancConnection& connection_;
    std::string id_;
    Json::Value patient_;
    Orthanc::ArrayFilledByThreads  studies_;

    void ReadPatient();

    virtual size_t GetFillerSize()
    {
      return patient_["Studies"].size();
    }

    virtual Orthanc::IDynamicObject* GetFillerItem(size_t index);

  public:
    /**
     * {summary}{Create a connection to some patient.}
     * {param}{connection The remote instance of %Orthanc.}
     * {param}{id The %Orthanc identifier of the patient.}
     **/
    Patient(const OrthancConnection& connection,
            const char* id);

    /**
     * {summary}{Reload the studies of this patient.}
     * {description}{This method will reload the list of the studies of this patient. Pay attention to the fact that the studies that have been previously returned by GetStudy() will be invalidated.}
     **/
    void Reload()
    {
      studies_.Reload();
    }

    /**
     * {summary}{Return the number of studies for this patient.}
     * {returns}{The number of studies.}
     **/
    uint32_t GetStudyCount()
    {
      return studies_.GetSize();
    }

    /**
     * {summary}{Get some study of this patient.}
     * {description}{This method will return an object that contains information about some study. The studies are indexed by a number between 0 (inclusive) and the result of GetStudyCount() (exclusive).}
     * {param}{index The index of the study of interest.}
     * {returns}{The study.}
     **/
    Study& GetStudy(uint32_t index)
    {
      return dynamic_cast<Study&>(studies_.GetItem(index));
    }

    /**
     * {summary}{Get the %Orthanc identifier of this patient.}
     * {returns}{The identifier.}
     **/
    const char* GetId() const
    {
      return id_.c_str();
    }

    /**
     * {summary}{Get the value of one of the main DICOM tags for this patient.}
     * {param}{tag The name of the tag of interest ("PatientName", "PatientID", "PatientSex" or "PatientBirthDate").}
     * {param}{defaultValue The default value to be returned if this tag does not exist.}
     * {returns}{The value of the tag.}
     **/
    const char* GetMainDicomTag(const char* tag, 
                                const char* defaultValue) const;

    LAAW_API_INTERNAL void Delete();
  };
}
