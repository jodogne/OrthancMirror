/**
 * Palanthir - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012 Medical Physics Department, CHU of Liege,
 * Belgium
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

#include "../DicomProtocol/DicomServer.h"
#include "../../Core/MultiThreading/IRunnableBySteps.h"

#include <dcmtk/dcmnet/assoc.h>
#include <dcmtk/dcmnet/dimse.h>

namespace Palanthir
{
  namespace Internals
  {
    OFCondition AssociationCleanup(T_ASC_Association *assoc);

    class CommandDispatcher : public IRunnableBySteps
    {
    private:
      uint32_t clientTimeout_;
      uint32_t elapsedTimeSinceLastCommand_;
      const DicomServer& server_;
      T_ASC_Association* assoc_;

    public:
      CommandDispatcher(const DicomServer& server,
                        T_ASC_Association* assoc) : 
        server_(server),
        assoc_(assoc)
      {
        clientTimeout_ = server.GetClientTimeout();
        elapsedTimeSinceLastCommand_ = 0;
      }

      virtual ~CommandDispatcher()
      {
        AssociationCleanup(assoc_);
      }

      virtual bool Step();
    };

    OFCondition EchoScp(T_ASC_Association * assoc, 
                        T_DIMSE_Message * msg, 
                        T_ASC_PresentationContextID presID);

    CommandDispatcher* AcceptAssociation(const DicomServer& server, 
                                         T_ASC_Network *net);
  }
}
