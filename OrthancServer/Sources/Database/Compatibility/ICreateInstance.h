/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include "../IDatabaseWrapper.h"

namespace Orthanc
{
  namespace Compatibility
  {
    class ICreateInstance : public boost::noncopyable
    {
    public:
      virtual bool LookupResource(int64_t& id,
                                  ResourceType& type,
                                  const std::string& publicId) = 0;

      virtual int64_t CreateResource(const std::string& publicId,
                                     ResourceType type) = 0;

      virtual void AttachChild(int64_t parent,
                               int64_t child) = 0;

      virtual void TagMostRecentPatient(int64_t patientId) = 0;
      
      static bool Apply(ICreateInstance& database,
                        IDatabaseWrapper::CreateInstanceResult& result,
                        int64_t& instanceId,
                        const std::string& patient,
                        const std::string& study,
                        const std::string& series,
                        const std::string& instance);
    };
  }
}
