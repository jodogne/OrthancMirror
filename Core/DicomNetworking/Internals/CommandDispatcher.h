/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
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

    public:
      CommandDispatcher(const DicomServer& server,
                        T_ASC_Association* assoc,
                        const std::string& remoteIp,
                        const std::string& remoteAet,
                        const std::string& calledAet,
                        IApplicationEntityFilter* filter);

      virtual ~CommandDispatcher();

      virtual bool Step();
    };

    OFCondition EchoScp(T_ASC_Association * assoc, 
                        T_DIMSE_Message * msg, 
                        T_ASC_PresentationContextID presID);

    CommandDispatcher* AcceptAssociation(const DicomServer& server, 
                                         T_ASC_Network *net);
  }
}
