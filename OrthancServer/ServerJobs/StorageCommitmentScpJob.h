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
#include "../../Core/DicomNetworking/RemoteModalityParameters.h"
#include "../../Core/JobsEngine/SetOfCommandsJob.h"
#include "IStorageCommitmentFactory.h"

#include <memory>
#include <vector>

namespace Orthanc
{
  class ServerContext;
  
  class StorageCommitmentScpJob : public SetOfCommandsJob
  {
  private:
    enum CommandType
    {
      CommandType_Setup,
      CommandType_Lookup,
      CommandType_Answer
    };
    
    class StorageCommitmentCommand;
    class SetupCommand;
    class LookupCommand;
    class AnswerCommand;
    class Unserializer;

    ServerContext&            context_;
    bool                      ready_;
    std::string               transactionUid_;
    RemoteModalityParameters  remoteModality_;
    std::string               calledAet_;
    std::vector<std::string>  sopClassUids_;
    std::vector<std::string>  sopInstanceUids_;

    std::unique_ptr<IStorageCommitmentFactory::ILookupHandler>  lookupHandler_;

    void CheckInvariants();
    
    void Setup(const std::string& jobId);
    
    StorageCommitmentFailureReason Lookup(size_t index);
    
    void Answer();
    
  public:
    StorageCommitmentScpJob(ServerContext& context,
                            const std::string& transactionUid,
                            const std::string& remoteAet,
                            const std::string& calledAet);

    StorageCommitmentScpJob(ServerContext& context,
                            const Json::Value& serialized);

    void Reserve(size_t size);
    
    void AddInstance(const std::string& sopClassUid,
                     const std::string& sopInstanceUid);

    void MarkAsReady();

    virtual void Stop(JobStopReason reason) ORTHANC_OVERRIDE
    {
    }

    virtual void GetJobType(std::string& target) ORTHANC_OVERRIDE
    {
      target = "StorageCommitmentScp";
    }

    virtual void GetPublicContent(Json::Value& value) ORTHANC_OVERRIDE;

    virtual bool Serialize(Json::Value& target) ORTHANC_OVERRIDE;
  };
}
