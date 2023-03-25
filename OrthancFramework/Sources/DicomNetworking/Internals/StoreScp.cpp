/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
#include "StoreScp.h"

#if !defined(DCMTK_VERSION_NUMBER)
#  error The macro DCMTK_VERSION_NUMBER must be defined
#endif

#include "../../DicomParsing/FromDcmtkBridge.h"
#include "../../DicomParsing/ToDcmtkBridge.h"
#include "../../OrthancException.h"
#include "../../Logging.h"

#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcmetinf.h>
#include <dcmtk/dcmdata/dcostrmb.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmnet/diutil.h>


namespace Orthanc
{
  namespace
  {  
    struct StoreCallbackData
    {
      IStoreRequestHandler* handler;
      const std::string* remoteIp;
      const char* remoteAET;
      const char* calledAET;
      const char* modality;
      const char* affectedSOPInstanceUID;
      uint32_t messageID;
    };

    
    static void
    storeScpCallback(
      void *callbackData,
      T_DIMSE_StoreProgress *progress,
      T_DIMSE_C_StoreRQ *req,
      char * /*imageFileName*/, DcmDataset **imageDataSet,
      T_DIMSE_C_StoreRSP *rsp,
      DcmDataset **statusDetail)
    /*
     * This function.is used to indicate progress when storescp receives instance data over the
     * network. On the final call to this function (identified by progress->state == DIMSE_StoreEnd)
     * this function will store the data set which was received over the network to a file.
     * Earlier calls to this function will simply cause some information to be dumped to stdout.
     *
     * Parameters:
     *   callbackData  - [in] data for this callback function
     *   progress      - [in] The state of progress. (identifies if this is the initial or final call
     *                   to this function, or a call in between these two calls.
     *   req           - [in] The original store request message.
     *   imageFileName - [in] The path to and name of the file the information shall be written to.
     *   imageDataSet  - [in] The data set which shall be stored in the image file
     *   rsp           - [inout] the C-STORE-RSP message (will be sent after the call to this function)
     *   statusDetail  - [inout] This variable can be used to capture detailed information with regard to
     *                   the status information which is captured in the status element (0000,0900). Note
     *                   that this function does specify any such information, the pointer will be set to NULL.
     */
    {
      StoreCallbackData *cbdata = OFstatic_cast(StoreCallbackData *, callbackData);

      DIC_UI sopClass;
      DIC_UI sopInstance;

      // if this is the final call of this function, save the data which was received to a file
      // (note that we could also save the image somewhere else, put it in database, etc.)
      if (progress->state == DIMSE_StoreEnd)
      {
        OFString tmpStr;

        // do not send status detail information
        *statusDetail = NULL;

        // Concerning the following line: an appropriate status code is already set in the resp structure,
        // it need not be success. For example, if the caller has already detected an out of resources problem
        // then the status will reflect this.  The callback function is still called to allow cleanup.
        //rsp->DimseStatus = STATUS_Success;

        // we want to write the received information to a file only if this information
        // is present and the options opt_bitPreserving and opt_ignore are not set.
        if ((imageDataSet != NULL) && (*imageDataSet != NULL))
        {
          // check the image to make sure it is consistent, i.e. that its sopClass and sopInstance correspond
          // to those mentioned in the request. If not, set the status in the response message variable.
          if (rsp->DimseStatus == STATUS_Success)
          {
            // which SOP class and SOP instance ?
	    
#if DCMTK_VERSION_NUMBER >= 364
	            if (!DU_findSOPClassAndInstanceInDataSet(*imageDataSet, sopClass, sizeof(sopClass),
						      sopInstance, sizeof(sopInstance), /*opt_correctUIDPadding*/ OFFalse))
#else
              if (!DU_findSOPClassAndInstanceInDataSet(*imageDataSet, sopClass, sopInstance, /*opt_correctUIDPadding*/ OFFalse))
#endif
              {
		            //LOG4CPP_ERROR(Internals::GetLogger(), "bad DICOM file: " << fileName);
		            rsp->DimseStatus = STATUS_STORE_Error_CannotUnderstand;
              }
              else if (strcmp(sopClass, req->AffectedSOPClassUID) != 0)
              {
                rsp->DimseStatus = STATUS_STORE_Error_DataSetDoesNotMatchSOPClass;
              }
              else if (strcmp(sopInstance, req->AffectedSOPInstanceUID) != 0)
              {
                rsp->DimseStatus = STATUS_STORE_Error_DataSetDoesNotMatchSOPClass;
              }
              else
              {
                try
                {
                  rsp->DimseStatus = cbdata->handler->Handle(**imageDataSet, *cbdata->remoteIp, cbdata->remoteAET, cbdata->calledAET);
                }
                catch (OrthancException& e)
                {
                  rsp->DimseStatus = STATUS_STORE_Refused_OutOfResources;

                  if (e.GetErrorCode() == ErrorCode_InexistentTag)
                  {
                    FromDcmtkBridge::LogMissingTagsForStore(**imageDataSet);
                  }
                  else
                  {
                    CLOG(ERROR, DICOM) << "Exception while storing DICOM: " << e.What();
                  }
                }
              }
          }
        }
      }
    }
  }

/*
 * This function processes a DIMSE C-STORE-RQ commmand that was
 * received over the network connection.
 *
 * Parameters:
 *   assoc  - [in] The association (network connection to another DICOM application).
 *   msg    - [in] The DIMSE C-STORE-RQ message that was received.
 *   presID - [in] The ID of the presentation context which was specified in the PDV which contained
 *                 the DIMSE command.
 */
  OFCondition Internals::storeScp(T_ASC_Association * assoc, 
                                  T_DIMSE_Message * msg, 
                                  T_ASC_PresentationContextID presID,
                                  IStoreRequestHandler& handler,
                                  const std::string& remoteIp,
                                  int timeout)
  {
    OFCondition cond = EC_Normal;
    T_DIMSE_C_StoreRQ *req;

    // assign the actual information of the C-STORE-RQ command to a local variable
    req = &msg->msg.CStoreRQ;

    // intialize some variables
    StoreCallbackData data;
    data.handler = &handler;
    data.remoteIp = &remoteIp;
    data.modality = dcmSOPClassUIDToModality(req->AffectedSOPClassUID/*, "UNKNOWN"*/);
    if (data.modality == NULL)
      data.modality = "UNKNOWN";

    data.affectedSOPInstanceUID = req->AffectedSOPInstanceUID;
    data.messageID = req->MessageID;
    if (assoc && assoc->params)
    {
      data.remoteAET = assoc->params->DULparams.callingAPTitle;
      data.calledAET = assoc->params->DULparams.calledAPTitle;
    }
    else
    {
      data.remoteAET = "";
      data.calledAET = "";
    }

    DcmFileFormat dcmff;

    // store SourceApplicationEntityTitle in metaheader
    if (assoc && assoc->params)
    {
      const char *aet = assoc->params->DULparams.callingAPTitle;
      if (aet) dcmff.getMetaInfo()->putAndInsertString(DCM_SourceApplicationEntityTitle, aet);
    }

    // define an address where the information which will be received over the network will be stored
    DcmDataset *dset = dcmff.getDataset();

    cond = DIMSE_storeProvider(assoc, presID, req, NULL, /*opt_useMetaheader*/OFFalse, &dset,
                               storeScpCallback, &data, 
                               /*opt_blockMode*/ (timeout ? DIMSE_NONBLOCKING : DIMSE_BLOCKING),
                               /*opt_dimse_timeout*/ timeout);

    // if some error occured, dump corresponding information and remove the outfile if necessary
    if (cond.bad())
    {
      OFString temp_str;
      CLOG(ERROR, DICOM) << "Store SCP Failed: " << cond.text();
    }

    // return return value
    return cond;
  }
}
