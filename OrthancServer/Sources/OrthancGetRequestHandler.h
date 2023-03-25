/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include "../../OrthancFramework/Sources/Compatibility.h"  // For ORTHANC_OVERRIDE
#include "../../OrthancFramework/Sources/DicomNetworking/IGetRequestHandler.h"
#include "../../OrthancFramework/Sources/DicomNetworking/RemoteModalityParameters.h"

#include <dcmtk/dcmnet/dimse.h>

#include <list>

class DcmFileFormat;

namespace Orthanc
{
  class ServerContext;
  
  class OrthancGetRequestHandler : public IGetRequestHandler
  {
  private:
    ServerContext& context_;
    std::string localAet_;
    std::vector<std::string> instances_;
    size_t position_;
    std::string originatorAet_;
    
    unsigned int completedCount_;
    unsigned int warningCount_;
    unsigned int failedCount_;
    std::string failedUIDs_;
    
    uint32_t timeout_;
    bool allowTranscoding_;

    bool LookupIdentifiers(std::list<std::string>& publicIds,
                           ResourceType level,
                           const DicomMap& input) const;
    
    // Returns "false" iff cancel
    bool PerformGetSubOp(T_ASC_Association *assoc,
                         const std::string& sopClassUid,
                         const std::string& sopInstanceUid,
                         DcmFileFormat* datasetRaw);
    
    void AddFailedUIDInstance(const std::string& sopInstance);

  public:
    explicit OrthancGetRequestHandler(ServerContext& context);
    
    virtual bool Handle(const DicomMap& input,
                        const std::string& originatorIp,
                        const std::string& originatorAet,
                        const std::string& calledAet,
                        uint32_t timeout) ORTHANC_OVERRIDE;

    virtual bool DoNext(T_ASC_Association *assoc) ORTHANC_OVERRIDE;
    
    virtual unsigned int GetSubOperationCount() const ORTHANC_OVERRIDE
    {
      return static_cast<unsigned int>(instances_.size());
    }
    
    virtual unsigned int GetCompletedCount() const ORTHANC_OVERRIDE
    {
      return completedCount_;
    }
    
    virtual unsigned int GetWarningCount() const ORTHANC_OVERRIDE
    {
      return warningCount_;
    }
    
    virtual unsigned int GetFailedCount() const ORTHANC_OVERRIDE
    {
      return failedCount_;
    }
    
    virtual const std::string& GetFailedUids() const ORTHANC_OVERRIDE
    {
      return failedUIDs_;
    }    
  };
}
