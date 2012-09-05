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


#include "MoveScp.h"

#include <memory>

#include "../FromDcmtkBridge.h"
#include "../ToDcmtkBridge.h"
#include "../../Core/PalanthirException.h"

#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcmetinf.h>
#include <dcmtk/dcmdata/dcostrmb.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmnet/diutil.h>


namespace Palanthir
{
  namespace Internals
  {
    extern OFLogger Logger;
  }


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
          data.subOperationCount_ = data.iterator_->GetSubOperationCount();
          data.failureCount_ = 0;
          data.warningCount_ = 0;
        }
        catch (PalanthirException& e)
        {
          // Internal error!
          OFLOG_ERROR(Internals::Logger, "IMoveRequestHandler Failed: " << e.What());
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
        catch (PalanthirException& e)
        {
          // Internal error!
          OFLOG_ERROR(Internals::Logger, "IMoveRequestHandler Failed: " << e.What());
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
      OFLOG_ERROR(Internals::Logger, "Move SCP Failed: " << DimseCondition::dump(temp_str, cond));
    }

    return cond;
  }
}
