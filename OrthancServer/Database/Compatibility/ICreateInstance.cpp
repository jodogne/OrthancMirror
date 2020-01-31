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


#include "../../PrecompiledHeadersServer.h"
#include "ICreateInstance.h"

#include "../../../Core/OrthancException.h"

namespace Orthanc
{
  namespace Compatibility
  {
    bool ICreateInstance::Apply(ICreateInstance& database,
                                IDatabaseWrapper::CreateInstanceResult& result,
                                int64_t& instanceId,
                                const std::string& hashPatient,
                                const std::string& hashStudy,
                                const std::string& hashSeries,
                                const std::string& hashInstance)
    {
      {
        ResourceType type;
        int64_t tmp;
        
        if (database.LookupResource(tmp, type, hashInstance))
        {
          // The instance already exists
          assert(type == ResourceType_Instance);
          instanceId = tmp;
          return false;
        }
      }

      instanceId = database.CreateResource(hashInstance, ResourceType_Instance);

      result.isNewPatient_ = false;
      result.isNewStudy_ = false;
      result.isNewSeries_ = false;
      result.patientId_ = -1;
      result.studyId_ = -1;
      result.seriesId_ = -1;
      
      // Detect up to which level the patient/study/series/instance
      // hierarchy must be created

      {
        ResourceType dummy;

        if (database.LookupResource(result.seriesId_, dummy, hashSeries))
        {
          assert(dummy == ResourceType_Series);
          // The patient, the study and the series already exist

          bool ok = (database.LookupResource(result.patientId_, dummy, hashPatient) &&
                     database.LookupResource(result.studyId_, dummy, hashStudy));
          assert(ok);
        }
        else if (database.LookupResource(result.studyId_, dummy, hashStudy))
        {
          assert(dummy == ResourceType_Study);

          // New series: The patient and the study already exist
          result.isNewSeries_ = true;

          bool ok = database.LookupResource(result.patientId_, dummy, hashPatient);
          assert(ok);
        }
        else if (database.LookupResource(result.patientId_, dummy, hashPatient))
        {
          assert(dummy == ResourceType_Patient);

          // New study and series: The patient already exist
          result.isNewStudy_ = true;
          result.isNewSeries_ = true;
        }
        else
        {
          // New patient, study and series: Nothing exists
          result.isNewPatient_ = true;
          result.isNewStudy_ = true;
          result.isNewSeries_ = true;
        }
      }

      // Create the series if needed
      if (result.isNewSeries_)
      {
        result.seriesId_ = database.CreateResource(hashSeries, ResourceType_Series);
      }

      // Create the study if needed
      if (result.isNewStudy_)
      {
        result.studyId_ = database.CreateResource(hashStudy, ResourceType_Study);
      }

      // Create the patient if needed
      if (result.isNewPatient_)
      {
        result.patientId_ = database.CreateResource(hashPatient, ResourceType_Patient);
      }

      // Create the parent-to-child links
      database.AttachChild(result.seriesId_, instanceId);

      if (result.isNewSeries_)
      {
        database.AttachChild(result.studyId_, result.seriesId_);
      }

      if (result.isNewStudy_)
      {
        database.AttachChild(result.patientId_, result.studyId_);
      }

      database.TagMostRecentPatient(result.patientId_);
      
      // Sanity checks
      assert(result.patientId_ != -1);
      assert(result.studyId_ != -1);
      assert(result.seriesId_ != -1);
      assert(instanceId != -1);

      return true;
    }
  }
}
