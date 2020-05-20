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


#include "../../PrecompiledHeaders.h"
#include <dcmtk/dcmnet/diutil.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include "GetScp.h"

#include <memory>

#include "../../DicomParsing/FromDcmtkBridge.h"
#include "../../DicomParsing/ToDcmtkBridge.h"
#include "../../Logging.h"
#include "../../OrthancException.h"

#include <boost/lexical_cast.hpp>


namespace Orthanc
{
  namespace
  {
    struct GetScpData
    {
      //  Handle returns void.
      IGetRequestHandler* handler_;
      DcmDataset* lastRequest_;
      T_ASC_Association * assoc_;

      std::string remoteIp_;
      std::string remoteAet_;
      std::string calledAet_;

      GetScpData()
      {
        handler_ = NULL;
        lastRequest_ = NULL;
        assoc_ = NULL;
      };
    };
      
    static DcmDataset *BuildFailedInstanceList(const std::string& failedUIDs)
    {
      if (failedUIDs.empty())
      {
        return NULL;
      }
      else
      {
        std::unique_ptr<DcmDataset> rspIds(new DcmDataset());
        
        if (!DU_putStringDOElement(rspIds.get(), DCM_FailedSOPInstanceUIDList, failedUIDs.c_str()))
        {
          throw OrthancException(ErrorCode_InternalError,
                                 "getSCP: failed to build DCM_FailedSOPInstanceUIDList");
        }

        return rspIds.release();
      }
    }

    static void GetScpCallback(
      /* in */ 
      void *callbackData,  
      OFBool cancelled, 
      T_DIMSE_C_GetRQ *request, 
      DcmDataset *requestIdentifiers, 
      int responseCount,
      /* out */
      T_DIMSE_C_GetRSP *response,
      DcmDataset **responseIdentifiers,
      DcmDataset **statusDetail)
    {
      bzero(response, sizeof(T_DIMSE_C_GetRSP));
      *statusDetail = NULL;
      *responseIdentifiers = NULL;   

      GetScpData& data = *reinterpret_cast<GetScpData*>(callbackData);
      if (data.lastRequest_ == NULL)
      {
        DicomMap input;
        FromDcmtkBridge::ExtractDicomSummary(input, *requestIdentifiers);

        try
        {
          if(!data.handler_->Handle(input, data.remoteIp_, data.remoteAet_,
                                    data.calledAet_))
          {
            response->DimseStatus = STATUS_GET_Failed_UnableToProcess;
            return;
          }
        }
        catch (OrthancException& e)
        {
          // Internal error!
          LOG(ERROR) << "IGetRequestHandler Failed: " << e.What();
          response->DimseStatus = STATUS_GET_Failed_UnableToProcess;
          return;
        }

        data.lastRequest_ = requestIdentifiers;
      }
      else if (data.lastRequest_ != requestIdentifiers)
      {
        // Internal error!
        LOG(ERROR) << "IGetRequestHandler Failed: Internal error lastRequestIdentifier";
        response->DimseStatus = STATUS_GET_Failed_UnableToProcess;
        return;
      }

      if (data.handler_->GetRemainingCount() == 0)
      {
        response->DimseStatus = STATUS_Success;
      }
      else
      {
        IGetRequestHandler::Status status;

        try
        {
          status = data.handler_->DoNext(data.assoc_);
        }
        catch (OrthancException& e)
        {
          // Internal error!
          LOG(ERROR) << "IGetRequestHandler Failed: " << e.What();
          response->DimseStatus = STATUS_GET_Failed_UnableToProcess;
          return;
        }
        
        if (status == STATUS_Success)
        {
          if (responseCount < static_cast<int>(data.handler_->GetRemainingCount()))
          {
            response->DimseStatus = STATUS_Pending;
          }
          else
          {
            response->DimseStatus = STATUS_Success;
          }
        }
        else
        {
          response->DimseStatus = STATUS_GET_Failed_UnableToProcess;
          
          if (data.handler_->GetFailedCount() > 0 ||
              data.handler_->GetWarningCount() > 0) 
          {
            response->DimseStatus = STATUS_GET_Warning_SubOperationsCompleteOneOrMoreFailures;
          }
          
          /*
           * if all the sub-operations failed then we need to generate
           * a failed or refused status.  cf. DICOM part 4, C.4.3.3.1
           * we choose to generate a "Refused - Out of Resources -
           * Unable to perform suboperations" status.
           */
          if ((data.handler_->GetFailedCount() > 0) &&
              ((data.handler_->GetCompletedCount() +
                data.handler_->GetWarningCount()) == 0)) 
          {
            response->DimseStatus = STATUS_GET_Refused_OutOfResourcesSubOperations;
          }
          
          *responseIdentifiers = BuildFailedInstanceList(data.handler_->GetFailedUids());
        }
      }
      
      response->NumberOfRemainingSubOperations = data.handler_->GetRemainingCount();
      response->NumberOfCompletedSubOperations = data.handler_->GetCompletedCount();
      response->NumberOfFailedSubOperations = data.handler_->GetFailedCount();
      response->NumberOfWarningSubOperations = data.handler_->GetWarningCount();
    }
  }

  OFCondition Internals::getScp(T_ASC_Association * assoc,
                                T_DIMSE_Message * msg, 
                                T_ASC_PresentationContextID presID,
                                IGetRequestHandler& handler,
                                std::string remoteIp,
                                std::string remoteAet,
                                std::string calledAet,
                                int timeout)
  {
    GetScpData data;
    data.lastRequest_ = NULL;
    data.handler_ = &handler;
    data.assoc_ = assoc;
    data.remoteIp_ = remoteIp;
    data.remoteAet_ = remoteAet;
    data.calledAet_ = calledAet;

    OFCondition cond = DIMSE_getProvider(assoc, presID, &msg->msg.CGetRQ, 
                                         GetScpCallback, &data,
                                         /*opt_blockMode*/ (timeout ? DIMSE_NONBLOCKING : DIMSE_BLOCKING),
                                         /*opt_dimse_timeout*/ timeout);
    
    // if some error occured, dump corresponding information and remove the outfile if necessary
    if (cond.bad())
    {
      OFString temp_str;
      LOG(ERROR) << "Get SCP Failed: " << cond.text();
    }

    return cond;
  }
}
