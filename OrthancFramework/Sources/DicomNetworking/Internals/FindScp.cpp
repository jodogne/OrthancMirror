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

#include "../../DicomFormat/DicomArray.h"
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
      IFindRequestHandler* findHandler_;
      IWorklistRequestHandler* worklistHandler_;
      DicomFindAnswers answers_;
      DcmDataset* lastRequest_;
      const std::string* remoteIp_;
      const std::string* remoteAet_;
      const std::string* calledAet_;

      FindScpData() :
        findHandler_(NULL),
        worklistHandler_(NULL),
        answers_(false),
        lastRequest_(NULL),
        remoteIp_(NULL),
        remoteAet_(NULL),
        calledAet_(NULL)
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


    static void FixFindQuery(DicomMap& target,
                             const DicomMap& source)
    {
      // "The definition of a Data Set in PS3.5 specifically excludes
      // the range of groups below group 0008, and this includes in
      // particular Meta Information Header elements such as Transfer
      // Syntax UID (0002,0010)."
      // http://dicom.nema.org/medical/dicom/current/output/chtml/part04/sect_C.4.html#sect_C.4.1.1.3
      // https://groups.google.com/d/msg/orthanc-users/D3kpPuX8yV0/_zgHOzkMEQAJ

      DicomArray a(source);

      for (size_t i = 0; i < a.GetSize(); i++)
      {
        if (a.GetElement(i).GetTag().GetGroup() >= 0x0008)
        {
          target.SetValue(a.GetElement(i).GetTag(), a.GetElement(i).GetValue());
        }
      }
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
      assert(response != NULL);
      assert(requestIdentifiers != NULL);
      
      bzero(response, sizeof(T_DIMSE_C_FindRSP));
      *statusDetail = NULL;

      std::string sopClassUid(request->AffectedSOPClassUID);

      FindScpData& data = *reinterpret_cast<FindScpData*>(callbackData);
      if (data.lastRequest_ == NULL)
      {
        {
          std::stringstream s;  // DcmObject::PrintHelper cannot be used with VS2008
          requestIdentifiers->print(s);
          CLOG(TRACE, DICOM) << "Received C-FIND Request:" << std::endl << s.str();
        }
      
        bool ok = false;

        try
        {
          RemoteModalityParameters modality;

          /**
           * Ensure that the remote modality is known to Orthanc for C-FIND requests.
           **/

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
              CLOG(ERROR, DICOM) << "No worklist handler is installed, cannot handle this C-FIND request";
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
                    CLOG(WARNING, DICOM) << "Orthanc only supports sequence matching on worklists, "
                                         << "ignoring C-FIND SCU constraint on tag (" << tag.Format() 
                                         << ") " << FromDcmtkBridge::GetTagName(*element);
                  }

                  sequencesToReturn.push_back(tag);
                }
              }

              DicomMap input;
              std::set<DicomTag> ignoreTagLength;
              FromDcmtkBridge::ExtractDicomSummary(input, *requestIdentifiers, 0 /* don't truncate tags */, ignoreTagLength);
              input.RemoveSequences();

              DicomMap filtered;
              FixFindQuery(filtered, input);

              data.findHandler_->Handle(data.answers_, filtered, sequencesToReturn,
                                        *data.remoteIp_, *data.remoteAet_,
                                        *data.calledAet_, modality.GetManufacturer());
              ok = true;
            }
            else
            {
              CLOG(ERROR, DICOM) << "No C-Find handler is installed, cannot handle this request";
            }
          }
        }
        catch (OrthancException& e)
        {
          // Internal error!
          CLOG(ERROR, DICOM) <<  "C-FIND request handler has failed: " << e.What();
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

        if (*responseIdentifiers)
        {
          std::stringstream s;  // DcmObject::PrintHelper cannot be used with VS2008
          (*responseIdentifiers)->print(s);
          OFString str;
          CLOG(TRACE, DICOM) << "Sending C-FIND Response "
                             << responseCount << "/" << data.answers_.GetSize() << ":" << std::endl
                             << s.str();
        }
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
        CLOG(WARNING, DICOM) <<  "Too many results for an incoming C-FIND query";
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
                                 const std::string& calledAet,
                                 int timeout)
  {
    FindScpData data;
    data.findHandler_ = findHandler;
    data.worklistHandler_ = worklistHandler;
    data.lastRequest_ = NULL;
    data.remoteIp_ = &remoteIp;
    data.remoteAet_ = &remoteAet;
    data.calledAet_ = &calledAet;

    OFCondition cond = DIMSE_findProvider(assoc, presID, &msg->msg.CFindRQ, 
                                          FindScpCallback, &data,
                                          /*opt_blockMode*/ (timeout ? DIMSE_NONBLOCKING : DIMSE_BLOCKING),
                                          /*opt_dimse_timeout*/ timeout);

    // if some error occured, dump corresponding information and remove the outfile if necessary
    if (cond.bad())
    {
      OFString temp_str;
      CLOG(ERROR, DICOM) << "Find SCP Failed: " << cond.text();
    }

    return cond;
  }
}
