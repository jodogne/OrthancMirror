/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
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



/*=========================================================================

  This file is based on portions of the following project:

  Program: DCMTK 3.6.0
  Module:  http://dicom.offis.de/dcmtk.php.en

Copyright (C) 1994-2011, OFFIS e.V.
All rights reserved.

This software and supporting documentation were developed by

  OFFIS e.V.
  R&D Division Health
  Escherweg 2
  26121 Oldenburg, Germany

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

- Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

- Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

- Neither the name of OFFIS nor the names of its contributors may be
  used to endorse or promote products derived from this software
  without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=========================================================================*/



#include "../PrecompiledHeadersServer.h"
#include "FindScp.h"

#include "../FromDcmtkBridge.h"
#include "../ToDcmtkBridge.h"
#include "../../Core/Logging.h"
#include "../../Core/OrthancException.h"
#include "../OrthancInitialization.h"

#include <dcmtk/dcmdata/dcfilefo.h>

namespace Orthanc
{
  namespace
  {  
    struct FindScpData
    {
      IFindRequestHandler* findHandler_;
      IWorklistRequestHandler* worklistHandler_;
      DicomFindAnswers answers_;
      DcmDataset* lastRequest_;
      const std::string* remoteIp_;
      const std::string* remoteAet_;
      const std::string* calledAet_;

      FindScpData() : answers_(false)
      {
      }
    };


    void FindScpCallback(
      /* in */ 
      void *callbackData,  
      OFBool cancelled, 
      T_DIMSE_C_FindRQ *request, 
      DcmDataset *requestIdentifiers, 
      int responseCount,
      /* out */
      T_DIMSE_C_FindRSP *response,
      DcmDataset **responseIdentifiers,
      DcmDataset **statusDetail)
    {
      bzero(response, sizeof(T_DIMSE_C_FindRSP));
      *statusDetail = NULL;

      std::string sopClassUid(request->AffectedSOPClassUID);

      FindScpData& data = *reinterpret_cast<FindScpData*>(callbackData);
      if (data.lastRequest_ == NULL)
      {
        bool ok = false;

        try
        {
          if (sopClassUid == UID_FINDModalityWorklistInformationModel)
          {
            data.answers_.SetWorklist(true);

            if (data.worklistHandler_ != NULL)
            {
              ParsedDicomFile query(*requestIdentifiers);
              data.worklistHandler_->Handle(data.answers_, query,
                                            *data.remoteIp_, *data.remoteAet_,
                                            *data.calledAet_);
              ok = true;
            }
            else
            {
              LOG(ERROR) << "No worklist handler is installed, cannot handle this C-FIND request";
            }
          }
          else
          {
            data.answers_.SetWorklist(false);

            if (data.findHandler_ != NULL)
            {
              std::list<DicomTag> sequencesToReturn;

              for (unsigned long i = 0; i < requestIdentifiers->card(); i++)
              {
                DcmElement* element = requestIdentifiers->getElement(i);
                if (element && !element->isLeaf())
                {
                  const DicomTag tag(FromDcmtkBridge::Convert(element->getTag()));

                  DcmSequenceOfItems& sequence = dynamic_cast<DcmSequenceOfItems&>(*element);
                  if (sequence.card() != 0)
                  {
                    LOG(WARNING) << "Orthanc only supports sequence matching on worklists, "
                                 << "ignoring C-FIND SCU constraint on tag (" << tag.Format() 
                                 << ") " << FromDcmtkBridge::GetName(tag);
                  }

                  sequencesToReturn.push_back(tag);
                }
              }

              DicomMap input;
              FromDcmtkBridge::Convert(input, *requestIdentifiers, ORTHANC_MAXIMUM_TAG_LENGTH,
                                       Configuration::GetDefaultEncoding());
              data.findHandler_->Handle(data.answers_, input, sequencesToReturn,
                                        *data.remoteIp_, *data.remoteAet_,
                                        *data.calledAet_);
              ok = true;
            }
            else
            {
              LOG(ERROR) << "No C-Find handler is installed, cannot handle this request";
            }
          }
        }
        catch (OrthancException& e)
        {
          // Internal error!
          LOG(ERROR) <<  "C-FIND request handler has failed: " << e.What();
        }

        if (!ok)
        {
          response->DimseStatus = STATUS_FIND_Failed_UnableToProcess;
          *responseIdentifiers = NULL;   
          return;
        }

        data.lastRequest_ = requestIdentifiers;
      }
      else if (data.lastRequest_ != requestIdentifiers)
      {
        // Internal error!
        response->DimseStatus = STATUS_FIND_Failed_UnableToProcess;
        *responseIdentifiers = NULL;   
        return;
      }

      if (responseCount <= static_cast<int>(data.answers_.GetSize()))
      {
        // There are pending results that are still to be sent
        response->DimseStatus = STATUS_Pending;
        *responseIdentifiers = data.answers_.ExtractDcmDataset(responseCount - 1);
      }
      else if (data.answers_.IsComplete())
      {
        // Success: All the results have been sent
        response->DimseStatus = STATUS_Success;
        *responseIdentifiers = NULL;
      }
      else
      {
        // Success, but the results were too numerous and had to be cropped
        LOG(WARNING) <<  "Too many results for an incoming C-FIND query";
        response->DimseStatus = STATUS_FIND_Cancel_MatchingTerminatedDueToCancelRequest;
        *responseIdentifiers = NULL;
      }
    }
  }


  OFCondition Internals::findScp(T_ASC_Association * assoc, 
                                 T_DIMSE_Message * msg, 
                                 T_ASC_PresentationContextID presID,
                                 IFindRequestHandler* findHandler,
                                 IWorklistRequestHandler* worklistHandler,
                                 const std::string& remoteIp,
                                 const std::string& remoteAet,
                                 const std::string& calledAet)
  {
    FindScpData data;
    data.lastRequest_ = NULL;
    data.findHandler_ = findHandler;
    data.worklistHandler_ = worklistHandler;
    data.remoteIp_ = &remoteIp;
    data.remoteAet_ = &remoteAet;
    data.calledAet_ = &calledAet;

    OFCondition cond = DIMSE_findProvider(assoc, presID, &msg->msg.CFindRQ, 
                                          FindScpCallback, &data,
                                          /*opt_blockMode*/ DIMSE_BLOCKING, 
                                          /*opt_dimse_timeout*/ 0);

    // if some error occured, dump corresponding information and remove the outfile if necessary
    if (cond.bad())
    {
      OFString temp_str;
      LOG(ERROR) << "Find SCP Failed: " << cond.text();
    }

    return cond;
  }
}
