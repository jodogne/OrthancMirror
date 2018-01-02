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
#include "FindScp.h"

#include "../../DicomParsing/FromDcmtkBridge.h"
#include "../../DicomParsing/ToDcmtkBridge.h"
#include "../../Logging.h"
#include "../../OrthancException.h"

#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcdeftag.h>



/**
 * The function below is extracted from DCMTK 3.6.0, cf. file
 * "dcmtk-3.6.0/dcmwlm/libsrc/wldsfs.cc".
 **/

static void HandleExistentButEmptyReferencedStudyOrPatientSequenceAttributes(DcmDataset *dataset, 
                                                                             const DcmTagKey &sequenceTagKey)
// Date         : May 3, 2005
// Author       : Thomas Wilkens
// Task         : This function performs a check on a sequence attribute in the given dataset. At two different places
//                in the definition of the DICOM worklist management service, a sequence attribute with a return type
//                of 2 is mentioned containing two 1C attributes in its item; the condition of the two 1C attributes
//                specifies that in case a sequence item is present, then these two attributes must be existent and
//                must contain a value. (I am talking about ReferencedStudySequence and ReferencedPatientSequence.)
//                In cases where the sequence attribute contains exactly one item with an empty ReferencedSOPClass
//                and an empty ReferencedSOPInstance, we want to remove the item from the sequence. This is what
//                this function does.
// Parameters   : dataset         - [in] Dataset in which the consistency of the sequence attribute shall be checked.
//                sequenceTagKey  - [in] DcmTagKey of the sequence attribute which shall be checked.
// Return Value : none.
{
  DcmElement *sequenceAttribute = NULL, *referencedSOPClassUIDAttribute = NULL, *referencedSOPInstanceUIDAttribute = NULL;

  // in case the sequence attribute contains exactly one item with an empty
  // ReferencedSOPClassUID and an empty ReferencedSOPInstanceUID, remove the item
  if( dataset->findAndGetElement( sequenceTagKey, sequenceAttribute ).good() &&
      ( (DcmSequenceOfItems*)sequenceAttribute )->card() == 1 &&
      ( (DcmSequenceOfItems*)sequenceAttribute )->getItem(0)->findAndGetElement( DCM_ReferencedSOPClassUID, referencedSOPClassUIDAttribute ).good() &&
      referencedSOPClassUIDAttribute->getLength() == 0 &&
      ( (DcmSequenceOfItems*)sequenceAttribute )->getItem(0)->findAndGetElement( DCM_ReferencedSOPInstanceUID, referencedSOPInstanceUIDAttribute, OFFalse ).good() &&
      referencedSOPInstanceUIDAttribute->getLength() == 0 )
  {
    DcmItem *item = ((DcmSequenceOfItems*)sequenceAttribute)->remove( ((DcmSequenceOfItems*)sequenceAttribute)->getItem(0) );
    delete item;
  }
}



namespace Orthanc
{
  namespace
  {  
    struct FindScpData
    {
      DicomServer::IRemoteModalities* modalities_;
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



    static void FixWorklistQuery(ParsedDicomFile& query)
    {
      // TODO: Check out
      // WlmDataSourceFileSystem::HandleExistentButEmptyDescriptionAndCodeSequenceAttributes()"
      // in DCMTK 3.6.0

      DcmDataset* dataset = query.GetDcmtkObject().getDataset();      
      HandleExistentButEmptyReferencedStudyOrPatientSequenceAttributes(dataset, DCM_ReferencedStudySequence);
      HandleExistentButEmptyReferencedStudyOrPatientSequenceAttributes(dataset, DCM_ReferencedPatientSequence);
    }



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
          RemoteModalityParameters modality;

          /**
           * Ensure that the remote modality is known to Orthanc for C-FIND requests.
           **/

          assert(data.modalities_ != NULL);
          if (!data.modalities_->LookupAETitle(modality, *data.remoteAet_))
          {
            LOG(ERROR) << "Modality with AET \"" << *data.remoteAet_
                       << "\" is not defined in the \"DicomModalities\" configuration option";
            throw OrthancException(ErrorCode_UnknownModality);
          }

          
          if (sopClassUid == UID_FINDModalityWorklistInformationModel)
          {
            data.answers_.SetWorklist(true);

            if (data.worklistHandler_ != NULL)
            {
              ParsedDicomFile query(*requestIdentifiers);
              FixWorklistQuery(query);

              data.worklistHandler_->Handle(data.answers_, query,
                                            *data.remoteIp_, *data.remoteAet_,
                                            *data.calledAet_, modality.GetManufacturer());
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
                                 << ") " << FromDcmtkBridge::GetTagName(*element);
                  }

                  sequencesToReturn.push_back(tag);
                }
              }

              DicomMap input;
              FromDcmtkBridge::ExtractDicomSummary(input, *requestIdentifiers);

              data.findHandler_->Handle(data.answers_, input, sequencesToReturn,
                                        *data.remoteIp_, *data.remoteAet_,
                                        *data.calledAet_, modality.GetManufacturer());
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
                                 DicomServer::IRemoteModalities& modalities,
                                 IFindRequestHandler* findHandler,
                                 IWorklistRequestHandler* worklistHandler,
                                 const std::string& remoteIp,
                                 const std::string& remoteAet,
                                 const std::string& calledAet)
  {
    FindScpData data;
    data.modalities_ = &modalities;
    data.findHandler_ = findHandler;
    data.worklistHandler_ = worklistHandler;
    data.lastRequest_ = NULL;
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
