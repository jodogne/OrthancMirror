/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2019 Osimis S.A., Belgium
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

#include "../Core/DicomNetworking/IGetRequestHandler.h"
#include "../Core/DicomNetworking/RemoteModalityParameters.h"

#include <dcmtk/dcmnet/dimse.h>

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
    RemoteModalityParameters remote_;
    std::string originatorAet_;
    
    unsigned int nRemaining_;
    unsigned int nCompleted_;
    unsigned int warningCount_;
    unsigned int nFailed_;
    char *failedUIDs_;
    
    T_DIMSE_Priority priority_;
    DIC_US origMsgId;
    T_ASC_PresentationContextID origPresId;
    
    bool getCancelled_;

    bool LookupIdentifiers(std::vector<std::string>& publicIds,
                           ResourceType level,
                           const DicomMap& input);
    
    OFCondition performGetSubOp(T_ASC_Association *assoc,
                                DIC_UI sopClass,
                                DIC_UI sopInstance,
                                DcmDataset *dataset);
    
    void addFailedUIDInstance(const char *sopInstance);

  public:
    OrthancGetRequestHandler(ServerContext& context) :
      context_(context)
    {
      position_ = 0;

      nRemaining_ = 0;
      nCompleted_  = 0;
      warningCount_ = 0;
      nFailed_ = 0;

      failedUIDs_ = NULL;
    }

    bool Handle(const DicomMap& input,
                const std::string& originatorIp,
                const std::string& originatorAet,
                const std::string& calledAet);
    
    virtual Status DoNext(T_ASC_Association *);
    
    virtual unsigned int GetSubOperationCount() const
    {
      return (unsigned int) instances_.size();
    }
    
    virtual unsigned int nRemaining()
    {
      return nRemaining_;
    }
    
    virtual unsigned int nCompleted()
    {
      return nCompleted_;
    }
    
    virtual unsigned int warningCount()
    {
      return warningCount_;
    }
    
    virtual unsigned int nFailed()
    {
      return nFailed_;
    }
    
    virtual const char * failedUids()
    {
      return failedUIDs_;
    }

    
    
  };
}
