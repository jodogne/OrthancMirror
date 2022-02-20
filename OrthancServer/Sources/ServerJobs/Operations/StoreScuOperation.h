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

#include "../../../../OrthancFramework/Sources/Compatibility.h"  // For ORTHANC_OVERRIDE
#include "../../../../OrthancFramework/Sources/JobsEngine/Operations/IJobOperation.h"
#include "../../../../OrthancFramework/Sources/DicomNetworking/TimeoutDicomConnectionManager.h"

namespace Orthanc
{
  class ServerContext;
  
  class StoreScuOperation : public IJobOperation
  {
  private:
    ServerContext&                  context_;
    TimeoutDicomConnectionManager&  connectionManager_;
    std::string                     localAet_;
    RemoteModalityParameters        modality_;
    
  public:
    StoreScuOperation(ServerContext& context,
                      TimeoutDicomConnectionManager& connectionManager,
                      const std::string& localAet,
                      const RemoteModalityParameters& modality) :
      context_(context),
      connectionManager_(connectionManager),
      localAet_(localAet),
      modality_(modality)
    {
    }

    StoreScuOperation(ServerContext& context,
                      TimeoutDicomConnectionManager& connectionManager,
                      const Json::Value& serialized);

    const std::string& GetLocalAet() const
    {
      return localAet_;
    }

    const RemoteModalityParameters& GetRemoteModality() const
    {
      return modality_;
    }

    virtual void Apply(JobOperationValues& outputs,
                       const IJobOperationValue& input) ORTHANC_OVERRIDE;

    virtual void Serialize(Json::Value& result) const ORTHANC_OVERRIDE;
  };
}

