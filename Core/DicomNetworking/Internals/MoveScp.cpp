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
#include "MoveScp.h"

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
    struct MoveScpData
    {
      std::string target_;
      IMoveRequestHandler* handler_;
      DcmDataset* lastRequest_;
      unsigned int subOperationCount_;
      unsigned int failureCount_;
      unsigned int warningCount_;
      std::auto_ptr<IMoveRequestIterator> iterator_;
      const std::string* remoteIp_;
      const std::string* remoteAet_;
      const std::string* calledAet_;
    };



    static uint16_t GetMessageId(const DicomMap& message)
    {
      /**
       * Retrieve the Message ID (0000,0110) for this C-MOVE request, if
       * any. If present, this Message ID will be stored in the Move
       * Originator Message ID (0000,1031) field of the C-MOVE response.
       * http://dicom.nema.org/dicom/2013/output/chtml/part07/chapter_E.html
       **/

      const DicomValue* value = message.TestAndGetValue(DICOM_TAG_MESSAGE_ID);

      if (value != NULL &&
          !value->IsNull() &&
          !value->IsBinary())
      {
        try
        {
          int tmp = boost::lexical_cast<int>(value->GetContent());
          if (tmp >= 0 && tmp <= 0xffff)
          {
            return static_cast<uint16_t>(tmp);
          }
        }
        catch (boost::bad_lexical_cast&)
        {
          LOG(WARNING) << "Cannot convert the Message ID (\"" << value->GetContent()
                       << "\") of an incoming C-MOVE request to an integer, assuming zero";
        }
      }

      return 0;
    }



    void MoveScpCallback(
      /* in */ 
      void *callbackData,  
      OFBool cancelled, 
      T_DIMSE_C_MoveRQ *request, 
      DcmDataset *requestIdentifiers, 
      int responseCount,
      /* out */
      T_DIMSE_C_MoveRSP *response,
      DcmDataset **responseIdentifiers,
      DcmDataset **statusDetail)
    {
      bzero(response, sizeof(T_DIMSE_C_MoveRSP));
      *statusDetail = NULL;
      *responseIdentifiers = NULL;   

      MoveScpData& data = *reinterpret_cast<MoveScpData*>(callbackData);
      if (data.lastRequest_ == NULL)
      {
        DicomMap input;
        FromDcmtkBridge::ExtractDicomSummary(input, *requestIdentifiers);

        try
        {
          data.iterator_.reset(data.handler_->Handle(data.target_, input, *data.remoteIp_, *data.remoteAet_,
                                                     *data.calledAet_, GetMessageId(input)));

          if (data.iterator_.get() == NULL)
          {
            // Internal error!
            response->DimseStatus = STATUS_MOVE_Failed_UnableToProcess;
            return;
          }

          data.subOperationCount_ = data.iterator_->GetSubOperationCount();
          data.failureCount_ = 0;
          data.warningCount_ = 0;
        }
        catch (OrthancException& e)
        {
          // Internal error!
          LOG(ERROR) << "IMoveRequestHandler Failed: " << e.What();
          response->DimseStatus = STATUS_MOVE_Failed_UnableToProcess;
          return;
        }

        data.lastRequest_ = requestIdentifiers;
      }
      else if (data.lastRequest_ != requestIdentifiers)
      {
        // Internal error!
        response->DimseStatus = STATUS_MOVE_Failed_UnableToProcess;
        return;
      }
  
      if (data.subOperationCount_ == 0)
      {
        response->DimseStatus = STATUS_Success;
      }
      else
      {
        IMoveRequestIterator::Status status;

        try
        {
          status = data.iterator_->DoNext();
        }
        catch (OrthancException& e)
        {
          // Internal error!
          LOG(ERROR) << "IMoveRequestHandler Failed: " << e.What();
          response->DimseStatus = STATUS_MOVE_Failed_UnableToProcess;
          return;
        }

        if (status == IMoveRequestIterator::Status_Failure)
        {
          data.failureCount_++;
        }
        else if (status == IMoveRequestIterator::Status_Warning)
        {
          data.warningCount_++;
        }

        if (responseCount < static_cast<int>(data.subOperationCount_))
        {
          response->DimseStatus = STATUS_Pending;
        }
        else
        {
          response->DimseStatus = STATUS_Success;
        }
      }

      response->NumberOfRemainingSubOperations = data.subOperationCount_ - responseCount;
      response->NumberOfCompletedSubOperations = responseCount;
      response->NumberOfFailedSubOperations = data.failureCount_;
      response->NumberOfWarningSubOperations = data.warningCount_;
    }
  }


  OFCondition Internals::moveScp(T_ASC_Association * assoc, 
                                 T_DIMSE_Message * msg, 
                                 T_ASC_PresentationContextID presID,
                                 IMoveRequestHandler& handler,
                                 const std::string& remoteIp,
                                 const std::string& remoteAet,
                                 const std::string& calledAet)
  {
    MoveScpData data;
    data.target_ = std::string(msg->msg.CMoveRQ.MoveDestination);
    data.lastRequest_ = NULL;
    data.handler_ = &handler;
    data.remoteIp_ = &remoteIp;
    data.remoteAet_ = &remoteAet;
    data.calledAet_ = &calledAet;

    OFCondition cond = DIMSE_moveProvider(assoc, presID, &msg->msg.CMoveRQ, 
                                          MoveScpCallback, &data,
                                          /*opt_blockMode*/ DIMSE_BLOCKING, 
                                          /*opt_dimse_timeout*/ 0);

    // if some error occured, dump corresponding information and remove the outfile if necessary
    if (cond.bad())
    {
      OFString temp_str;
      LOG(ERROR) << "Move SCP Failed: " << cond.text();
    }

    return cond;
  }
}
