/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2013 Medical Physics Department, CHU of Liege,
 * Belgium
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


#include "MoveScp.h"

#include <memory>

#include "../FromDcmtkBridge.h"
#include "../ToDcmtkBridge.h"
#include "../../Core/OrthancException.h"

#include <glog/logging.h>


namespace Orthanc
{
  namespace
  {  
    struct MoveScpData
    {
      std::string target_;
      IMoveRequestHandler* handler_;
      DicomMap input_;
      DcmDataset* lastRequest_;
      unsigned int subOperationCount_;
      unsigned int failureCount_;
      unsigned int warningCount_;
      std::auto_ptr<IMoveRequestIterator> iterator_;
    };


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

      MoveScpData& data = *(MoveScpData*) callbackData;
      if (data.lastRequest_ == NULL)
      {
        FromDcmtkBridge::Convert(data.input_, *requestIdentifiers);

        try
        {
          data.iterator_.reset(data.handler_->Handle(data.target_, data.input_));
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
                                 IMoveRequestHandler& handler)
  {
    MoveScpData data;
    data.target_ = std::string(msg->msg.CMoveRQ.MoveDestination);
    data.lastRequest_ = NULL;
    data.handler_ = &handler;

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
