/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
#include "../../../OrthancFramework/Sources/DicomNetworking/DicomControlUserConnection.h"
// #include "../../../OrthancFramework/Sources/JobsEngine/SetOfCommandsJob.h"
#include "DicomRetrieveScuBaseJob.h"

#include "../QueryRetrieveHandler.h"

namespace Orthanc
{
  class ServerContext;
  
  class DicomMoveScuJob : public DicomRetrieveScuBaseJob
  {
  private:
    std::string                 targetAet_;
    
    virtual void Retrieve(const DicomMap& findAnswer) ORTHANC_OVERRIDE;
    
  public:
    explicit DicomMoveScuJob(ServerContext& context) :
      DicomRetrieveScuBaseJob(context)
    {
    }

    DicomMoveScuJob(ServerContext& context,
                    const Json::Value& serialized);

    const std::string& GetTargetAet() const
    {
      return targetAet_;
    }
    
    void SetTargetAet(const std::string& aet);

    virtual void GetJobType(std::string& target) const ORTHANC_OVERRIDE
    {
      target = "DicomMoveScu";
    }

    virtual void GetPublicContent(Json::Value& value) const ORTHANC_OVERRIDE;

    virtual bool Serialize(Json::Value& target) const ORTHANC_OVERRIDE;
  };
}
