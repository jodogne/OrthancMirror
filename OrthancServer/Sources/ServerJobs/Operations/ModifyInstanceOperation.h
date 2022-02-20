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

#include "../../../../OrthancFramework/Sources/Compatibility.h"
#include "../../../../OrthancFramework/Sources/DicomParsing/DicomModification.h"
#include "../../../../OrthancFramework/Sources/JobsEngine/Operations/IJobOperation.h"

namespace Orthanc
{
  class ServerContext;
  
  class ModifyInstanceOperation : public IJobOperation
  {
  private:
    ServerContext&                      context_;
    RequestOrigin                       origin_;
    std::unique_ptr<DicomModification>  modification_;
    
  public:
    ModifyInstanceOperation(ServerContext& context,
                            RequestOrigin origin,
                            DicomModification* modification);  // Takes ownership

    ModifyInstanceOperation(ServerContext& context,
                            const Json::Value& serialized);

    const RequestOrigin& GetRequestOrigin() const
    {
      return origin_;
    }
    
    const DicomModification& GetModification() const
    {
      return *modification_;
    }

    virtual void Apply(JobOperationValues& outputs,
                       const IJobOperationValue& input) ORTHANC_OVERRIDE;

    virtual void Serialize(Json::Value& target) const ORTHANC_OVERRIDE;
  };
}

