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
#include "../../../OrthancFramework/Sources/HttpClient.h"

#include <stdint.h>


namespace Orthanc
{
  class ServerContext;
  
  class OrthancPeerStoreJob : public SetOfInstancesJob
  {
  private:
    ServerContext&               context_;
    WebServiceParameters         peer_;
    std::unique_ptr<HttpClient>  client_;
    bool                         transcode_;
    DicomTransferSyntax          transferSyntax_;
    bool                         compress_;
    uint64_t                     size_;

  protected:
    virtual bool HandleInstance(const std::string& instance) ORTHANC_OVERRIDE;
    
    virtual bool HandleTrailingStep() ORTHANC_OVERRIDE;

  public:
    explicit OrthancPeerStoreJob(ServerContext& context) :
      context_(context),
      transcode_(false),
      transferSyntax_(DicomTransferSyntax_LittleEndianExplicit),  // Dummy value
      compress_(false),
      size_(0)
    {
    }

    OrthancPeerStoreJob(ServerContext& context,
                        const Json::Value& serialize);

    void SetPeer(const WebServiceParameters& peer);

    const WebServiceParameters& GetPeer() const
    {
      return peer_;
    }

    bool IsTranscode() const
    {
      return transcode_;
    }

    bool IsCompress() const
    {
      return compress_;
    }

    DicomTransferSyntax GetTransferSyntax() const;

    void SetTranscode(DicomTransferSyntax syntax);

    void SetTranscode(const std::string& transferSyntaxUid);

    void ClearTranscode();

    void SetCompress(bool compress);

    virtual void Stop(JobStopReason reason) ORTHANC_OVERRIDE;   // For pausing jobs

    virtual void GetJobType(std::string& target) ORTHANC_OVERRIDE
    {
      target = "OrthancPeerStore";
    }

    virtual void GetPublicContent(Json::Value& value) ORTHANC_OVERRIDE;

    virtual bool Serialize(Json::Value& target) ORTHANC_OVERRIDE;
  };
}
