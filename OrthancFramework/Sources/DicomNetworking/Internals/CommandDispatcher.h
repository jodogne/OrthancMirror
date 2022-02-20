/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 **/


#pragma once

#include "../DicomServer.h"
#include "../../MultiThreading/IRunnableBySteps.h"

#include <dcmtk/dcmnet/dimse.h>

namespace Orthanc
{
  namespace Internals
  {
    OFCondition AssociationCleanup(T_ASC_Association *assoc);

    class CommandDispatcher : public IRunnableBySteps
    {
    private:
      uint32_t associationTimeout_;
      uint32_t elapsedTimeSinceLastCommand_;
      const DicomServer& server_;
      T_ASC_Association* assoc_;
      std::string remoteIp_;
      std::string remoteAet_;
      std::string calledAet_;
      IApplicationEntityFilter* filter_;

      OFCondition NActionScp(T_DIMSE_Message* msg, 
                             T_ASC_PresentationContextID presID);

      OFCondition NEventReportScp(T_DIMSE_Message* msg, 
                                  T_ASC_PresentationContextID presID);
      
    public:
      CommandDispatcher(const DicomServer& server,
                        T_ASC_Association* assoc,
                        const std::string& remoteIp,
                        const std::string& remoteAet,
                        const std::string& calledAet,
                        unsigned int maximumPduLength,
                        IApplicationEntityFilter* filter);

      virtual ~CommandDispatcher();

      virtual bool Step();
    };

    CommandDispatcher* AcceptAssociation(const DicomServer& server, 
                                         T_ASC_Network *net,
                                         unsigned int maximumPduLength,
                                         bool useDicomTls);

    OFCondition EchoScp(T_ASC_Association* assoc, 
                        T_DIMSE_Message* msg, 
                        T_ASC_PresentationContextID presID);
  }
}
