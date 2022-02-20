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

#include "../../../OrthancFramework/Sources/Compatibility.h"
#include "../../../OrthancFramework/Sources/JobsEngine/SetOfInstancesJob.h"
#include "../../../OrthancFramework/Sources/DicomNetworking/DicomStoreUserConnection.h"

#include <list>

namespace Orthanc
{
  class ServerContext;
  
  class DicomModalityStoreJob : public SetOfInstancesJob
  {
  private:
    ServerContext&                             context_;
    DicomAssociationParameters                 parameters_;
    std::string                                moveOriginatorAet_;
    uint16_t                                   moveOriginatorId_;
    std::unique_ptr<DicomStoreUserConnection>  connection_;
    bool                                       storageCommitment_;

    // For storage commitment
    std::string             transactionUid_;
    std::list<std::string>  sopInstanceUids_;
    std::list<std::string>  sopClassUids_;

    void OpenConnection();

    void ResetStorageCommitment();

  protected:
    virtual bool HandleInstance(const std::string& instance) ORTHANC_OVERRIDE;
    
    virtual bool HandleTrailingStep() ORTHANC_OVERRIDE;

  public:
    explicit DicomModalityStoreJob(ServerContext& context);

    DicomModalityStoreJob(ServerContext& context,
                          const Json::Value& serialized);

    const DicomAssociationParameters& GetParameters() const
    {
      return parameters_;
    }

    void SetLocalAet(const std::string& aet);

    void SetRemoteModality(const RemoteModalityParameters& remote);

    void SetTimeout(uint32_t seconds);

    bool HasMoveOriginator() const
    {
      return moveOriginatorId_ != 0;
    }
    
    const std::string& GetMoveOriginatorAet() const;
    
    uint16_t GetMoveOriginatorId() const;

    void SetMoveOriginator(const std::string& aet,
                           int id);

    virtual void Stop(JobStopReason reason) ORTHANC_OVERRIDE;

    virtual void GetJobType(std::string& target) ORTHANC_OVERRIDE
    {
      target = "DicomModalityStore";
    }

    virtual void GetPublicContent(Json::Value& value) ORTHANC_OVERRIDE;

    virtual bool Serialize(Json::Value& target) ORTHANC_OVERRIDE;

    virtual void Reset() ORTHANC_OVERRIDE;

    void EnableStorageCommitment(bool enabled);

    bool HasStorageCommitment() const
    {
      return storageCommitment_;
    }
  };
}
