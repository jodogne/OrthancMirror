/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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
#include "../../../OrthancFramework/Sources/JobsEngine/SetOfCommandsJob.h"

#include "../QueryRetrieveHandler.h"

namespace Orthanc
{
  class ServerContext;
  
  class DicomMoveScuJob : public SetOfCommandsJob
  {
  private:
    class Command;
    class Unserializer;
    
    ServerContext&              context_;
    DicomAssociationParameters  parameters_;
    std::string                 targetAet_;
    Json::Value                 query_;

    std::unique_ptr<DicomControlUserConnection>  connection_;
    
    void Retrieve(const DicomMap& findAnswer);
    
  public:
    explicit DicomMoveScuJob(ServerContext& context) :
      context_(context),
      query_(Json::arrayValue)
    {
    }

    DicomMoveScuJob(ServerContext& context,
                    const Json::Value& serialized);

    void AddFindAnswer(const DicomMap& answer);
    
    void AddFindAnswer(QueryRetrieveHandler& query,
                       size_t i);

    const DicomAssociationParameters& GetParameters() const
    {
      return parameters_;
    }
    
    void SetLocalAet(const std::string& aet);

    void SetRemoteModality(const RemoteModalityParameters& remote);

    void SetTimeout(uint32_t timeout);

    const std::string& GetTargetAet() const
    {
      return targetAet_;
    }
    
    void SetTargetAet(const std::string& aet);

    virtual void Stop(JobStopReason reason) ORTHANC_OVERRIDE;

    virtual void GetJobType(std::string& target) ORTHANC_OVERRIDE
    {
      target = "DicomMoveScu";
    }

    virtual void GetPublicContent(Json::Value& value) ORTHANC_OVERRIDE;

    virtual bool Serialize(Json::Value& target) ORTHANC_OVERRIDE;
  };
}
