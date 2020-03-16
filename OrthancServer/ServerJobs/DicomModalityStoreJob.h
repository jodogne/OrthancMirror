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


#pragma once

#include "../../Core/Compatibility.h"
#include "../../Core/JobsEngine/SetOfInstancesJob.h"
#include "../../Core/DicomNetworking/DicomUserConnection.h"

namespace Orthanc
{
  class ServerContext;
  
  class DicomModalityStoreJob : public SetOfInstancesJob
  {
  private:
    ServerContext&                        context_;
    std::string                           localAet_;
    RemoteModalityParameters              remote_;
    std::string                           moveOriginatorAet_;
    uint16_t                              moveOriginatorId_;
    std::unique_ptr<DicomUserConnection>  connection_;
    bool                                  storageCommitment_;

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
    DicomModalityStoreJob(ServerContext& context);

    DicomModalityStoreJob(ServerContext& context,
                          const Json::Value& serialized);

    const std::string& GetLocalAet() const
    {
      return localAet_;
    }

    void SetLocalAet(const std::string& aet);

    const RemoteModalityParameters& GetRemoteModality() const
    {
      return remote_;
    }

    void SetRemoteModality(const RemoteModalityParameters& remote);

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
  };
}
