/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "../PrecompiledHeaders.h"
#include "DicomControlUserConnection.h"

#include "../Compatibility.h"
#include "../DicomFormat/DicomArray.h"
#include "../DicomParsing/FromDcmtkBridge.h"
#include "../Logging.h"
#include "../OrthancException.h"
#include "DicomAssociation.h"

#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmnet/diutil.h>

namespace Orthanc
{
  static void TestAndCopyTag(DicomMap& result,
                             const DicomMap& source,
                             const DicomTag& tag)
  {
    if (!source.HasTag(tag))
    {
      throw OrthancException(ErrorCode_BadRequest, "Missing tag " + tag.Format());
    }
    else
    {
      result.SetValue(tag, source.GetValue(tag));
    }
  }


  namespace
  {
    struct FindPayload
    {
      DicomFindAnswers* answers;
      const char*       level;
      bool              isWorklist;
    };
  }


  static void FindCallback(
    /* in */
    void *callbackData,
    T_DIMSE_C_FindRQ *request,      /* original find request */
    int responseCount,
    T_DIMSE_C_FindRSP *response,    /* pending response received */
    DcmDataset *responseIdentifiers /* pending response identifiers */
    )
  {
    if (response != NULL)
    {
      OFString str;
      CLOG(TRACE, DICOM) << "Received Find Response " << responseCount << ":" << std::endl
                         << DIMSE_dumpMessage(str, *response, DIMSE_INCOMING);
    }
      
    if (responseIdentifiers != NULL)
    {
      std::stringstream s;  // DcmObject::PrintHelper cannot be used with VS2008
      responseIdentifiers->print(s);
      CLOG(TRACE, DICOM) << "Response Identifiers "  << responseCount << ":" << std::endl << s.str();
    }
    
    if (responseIdentifiers != NULL)
    {
      FindPayload& payload = *reinterpret_cast<FindPayload*>(callbackData);

      if (payload.isWorklist)
      {
        const ParsedDicomFile answer(*responseIdentifiers);
        payload.answers->Add(answer);
      }
      else
      {
        DicomMap m;
        std::set<DicomTag> ignoreTagLength;
        FromDcmtkBridge::ExtractDicomSummary(m, *responseIdentifiers, 0 /* don't truncate tags */, ignoreTagLength);
        
        if (!m.HasTag(DICOM_TAG_QUERY_RETRIEVE_LEVEL))
        {
          m.SetValue(DICOM_TAG_QUERY_RETRIEVE_LEVEL, payload.level, false);
        }

        payload.answers->Add(m);
      }
    }
  }


  static void NormalizeFindQuery(DicomMap& fixedQuery,
                                 ResourceType level,
                                 const DicomMap& fields)
  {
    std::set<DicomTag> allowedTags;

    // WARNING: Do not add "break" or reorder items in this switch-case!
    switch (level)
    {
      case ResourceType_Instance:
        DicomTag::AddTagsForModule(allowedTags, DicomModule_Instance);

      case ResourceType_Series:
        DicomTag::AddTagsForModule(allowedTags, DicomModule_Series);

      case ResourceType_Study:
        DicomTag::AddTagsForModule(allowedTags, DicomModule_Study);

      case ResourceType_Patient:
        DicomTag::AddTagsForModule(allowedTags, DicomModule_Patient);
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    switch (level)
    {
      case ResourceType_Patient:
        allowedTags.insert(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_STUDIES);
        allowedTags.insert(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_SERIES);
        allowedTags.insert(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_INSTANCES);
        break;

      case ResourceType_Study:
        allowedTags.insert(DICOM_TAG_MODALITIES_IN_STUDY);
        allowedTags.insert(DICOM_TAG_NUMBER_OF_STUDY_RELATED_SERIES);
        allowedTags.insert(DICOM_TAG_NUMBER_OF_STUDY_RELATED_INSTANCES);
        allowedTags.insert(DICOM_TAG_SOP_CLASSES_IN_STUDY);
        break;

      case ResourceType_Series:
        allowedTags.insert(DICOM_TAG_NUMBER_OF_SERIES_RELATED_INSTANCES);
        break;

      default:
        break;
    }

    allowedTags.insert(DICOM_TAG_SPECIFIC_CHARACTER_SET);

    DicomArray query(fields);
    for (size_t i = 0; i < query.GetSize(); i++)
    {
      const DicomTag& tag = query.GetElement(i).GetTag();
      if (allowedTags.find(tag) == allowedTags.end())
      {
        CLOG(WARNING, DICOM) << "Tag not allowed for this C-Find level, will be ignored: ("
                             << tag.Format() << ")";
      }
      else
      {
        fixedQuery.SetValue(tag, query.GetElement(i).GetValue());
      }
    }
  }



  static ParsedDicomFile* ConvertQueryFields(const DicomMap& fields,
                                             ModalityManufacturer manufacturer)
  {
    // Fix outgoing C-Find requests issue for Syngo.Via and its
    // solution was reported by Emsy Chan by private mail on
    // 2015-06-17. According to Robert van Ommen (2015-11-30), the
    // same fix is required for Agfa Impax. This was generalized for
    // generic manufacturer since it seems to affect PhilipsADW,
    // GEWAServer as well:
    // https://orthanc.uclouvain.be/bugs/show_bug.cgi?id=31

    switch (manufacturer)
    {
      case ModalityManufacturer_GenericNoWildcardInDates:
      case ModalityManufacturer_GenericNoUniversalWildcard:
      {
        std::unique_ptr<DicomMap> fix(fields.Clone());

        std::set<DicomTag> tags;
        fix->GetTags(tags);

        for (std::set<DicomTag>::const_iterator it = tags.begin(); it != tags.end(); ++it)
        {
          // Replace a "*" wildcard query by an empty query ("") for
          // "date" or "all" value representations depending on the
          // type of manufacturer.
          if (manufacturer == ModalityManufacturer_GenericNoUniversalWildcard ||
              (manufacturer == ModalityManufacturer_GenericNoWildcardInDates &&
               FromDcmtkBridge::LookupValueRepresentation(*it) == ValueRepresentation_Date))
          {
            const DicomValue* value = fix->TestAndGetValue(*it);

            if (value != NULL && 
                !value->IsNull() &&
                value->GetContent() == "*")
            {
              fix->SetValue(*it, "", false);
            }
          }
        }

        return new ParsedDicomFile(*fix, GetDefaultDicomEncoding(),
                                   false /* be strict */);
      }

      default:
        return new ParsedDicomFile(fields, GetDefaultDicomEncoding(),
                                   false /* be strict */);
    }
  }



  void DicomControlUserConnection::SetupPresentationContexts(
    ScuOperationFlags scuOperation,
    const std::set<std::string>& acceptedStorageSopClasses,
    const std::list<DicomTransferSyntax>& proposedStorageTransferSyntaxes)
  {
    assert(association_.get() != NULL);

    if ((scuOperation & ScuOperationFlags_Echo) != 0)
    {
      association_->ProposeGenericPresentationContext(UID_VerificationSOPClass);
    }

    if ((scuOperation & ScuOperationFlags_FindPatient) != 0)
    {
      association_->ProposeGenericPresentationContext(UID_FINDPatientRootQueryRetrieveInformationModel);
    }

    if ((scuOperation & ScuOperationFlags_FindStudy) != 0)
    {
      association_->ProposeGenericPresentationContext(UID_FINDStudyRootQueryRetrieveInformationModel);
    }

    if ((scuOperation & ScuOperationFlags_FindWorklist) != 0)
    {
      association_->ProposeGenericPresentationContext(UID_FINDModalityWorklistInformationModel);
    }

    if ((scuOperation & ScuOperationFlags_MovePatient) != 0)
    {
      association_->ProposeGenericPresentationContext(UID_MOVEPatientRootQueryRetrieveInformationModel);
    }

    if ((scuOperation & ScuOperationFlags_MoveStudy) != 0)
    {
      association_->ProposeGenericPresentationContext(UID_MOVEStudyRootQueryRetrieveInformationModel);
    }

    if ((scuOperation & ScuOperationFlags_Get) != 0)
    {
      association_->ProposeGenericPresentationContext(UID_GETStudyRootQueryRetrieveInformationModel);
      association_->ProposeGenericPresentationContext(UID_GETPatientRootQueryRetrieveInformationModel);

      if (acceptedStorageSopClasses.size() == 0)
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls); // the acceptedStorageSopClassUids should always be defined for a C-Get
      }

      for (std::set<std::string>::const_iterator it = acceptedStorageSopClasses.begin(); it != acceptedStorageSopClasses.end(); ++it)
      {
        association_->ProposePresentationContext(*it, proposedStorageTransferSyntaxes, DicomAssociationRole_Scp);
      }
    }
  }
    

  void DicomControlUserConnection::FindInternal(DicomFindAnswers& answers,
                                                DcmDataset* dataset,
                                                const char* sopClass,
                                                bool isWorklist,
                                                const char* level)
  {
    assert(dataset != NULL);
    assert(isWorklist ^ (level != NULL));
    assert(association_.get() != NULL);

    association_->Open(parameters_);

    FindPayload payload;
    payload.answers = &answers;
    payload.level = level;
    payload.isWorklist = isWorklist;

    // Figure out which of the accepted presentation contexts should be used
    int presID = ASC_findAcceptedPresentationContextID(
      &association_->GetDcmtkAssociation(), sopClass);
    if (presID == 0)
    {
      throw OrthancException(ErrorCode_DicomFindUnavailable,
                             "Remote AET is " + parameters_.GetRemoteModality().GetApplicationEntityTitle());
    }

    T_DIMSE_C_FindRQ request;
    memset(&request, 0, sizeof(request));
    request.MessageID = association_->GetDcmtkAssociation().nextMsgID++;
    strncpy(request.AffectedSOPClassUID, sopClass, DIC_UI_LEN);
    request.Priority = DIMSE_PRIORITY_MEDIUM;
    request.DataSetType = DIMSE_DATASET_PRESENT;

    T_DIMSE_C_FindRSP response;
    DcmDataset* statusDetail = NULL;

#if DCMTK_VERSION_NUMBER >= 364
    int responseCount;
#endif

    {
      std::stringstream s;  // DcmObject::PrintHelper cannot be used with VS2008
      dataset->print(s);

      OFString str;
      CLOG(TRACE, DICOM) << "Sending Find Request:" << std::endl
                         << DIMSE_dumpMessage(str, request, DIMSE_OUTGOING, NULL, presID) << std::endl
                         << s.str();
    }

    OFCondition cond = DIMSE_findUser(
      &association_->GetDcmtkAssociation(), presID, &request, dataset,
#if DCMTK_VERSION_NUMBER >= 364
      responseCount,
#endif
      FindCallback, &payload,
      /*opt_blockMode*/ (parameters_.HasTimeout() ? DIMSE_NONBLOCKING : DIMSE_BLOCKING),
      /*opt_dimse_timeout*/ parameters_.GetTimeout(),
      &response, &statusDetail);
    
    if (statusDetail)
    {
      delete statusDetail;
    }

    DicomAssociation::CheckCondition(cond, parameters_, "C-FIND");

    {
      OFString str;
      CLOG(TRACE, DICOM) << "Received Final Find Response:" << std::endl
                         << DIMSE_dumpMessage(str, response, DIMSE_INCOMING);
    }

    
    /**
     * New in Orthanc 1.6.0: Deal with failures during C-FIND.
     * http://dicom.nema.org/medical/dicom/current/output/chtml/part04/sect_C.4.html#table_C.4-1
     **/
    
    if (response.DimseStatus != 0x0000 &&  // Success
        response.DimseStatus != 0xFF00 &&  // Pending - Matches are continuing 
        response.DimseStatus != 0xFF01)    // Pending - Matches are continuing 
    {
      char buf[16];
      sprintf(buf, "%04X", response.DimseStatus);

      if (response.DimseStatus == STATUS_FIND_Failed_UnableToProcess)
      {
        throw OrthancException(ErrorCode_NetworkProtocol,
                               HttpStatus_422_UnprocessableEntity,
                               "C-FIND SCU to AET \"" +
                               parameters_.GetRemoteModality().GetApplicationEntityTitle() +
                               "\" has failed with DIMSE status 0x" + buf +
                               " (unable to process - invalid query ?)");
      }
      else
      {
        throw OrthancException(ErrorCode_NetworkProtocol, "C-FIND SCU to AET \"" +
                               parameters_.GetRemoteModality().GetApplicationEntityTitle() +
                               "\" has failed with DIMSE status 0x" + buf);
      }
    }
  }

  void MoveProgressCallback(void *callbackData,
                            T_DIMSE_C_MoveRQ *request,
                            int responseCount, 
                            T_DIMSE_C_MoveRSP *response)
  {
    DicomControlUserConnection::IProgressListener* listener = reinterpret_cast<DicomControlUserConnection::IProgressListener*>(callbackData);
    if (listener)
    {
      listener->OnProgressUpdated(response->NumberOfRemainingSubOperations,
                                  response->NumberOfCompletedSubOperations,
                                  response->NumberOfFailedSubOperations,
                                  response->NumberOfWarningSubOperations);
    }
  }

    
  void DicomControlUserConnection::MoveInternal(const std::string& targetAet,
                                                ResourceType level,
                                                const DicomMap& fields)
  {
    assert(association_.get() != NULL);
    association_->Open(parameters_);

    std::unique_ptr<ParsedDicomFile> query(
      ConvertQueryFields(fields, parameters_.GetRemoteModality().GetManufacturer()));
    DcmDataset* dataset = query->GetDcmtkObject().getDataset();

    const char* sopClass = UID_MOVEStudyRootQueryRetrieveInformationModel;
    DU_putStringDOElement(dataset, DCM_QueryRetrieveLevel, ResourceTypeToDicomQueryRetrieveLevel(level));

    // Figure out which of the accepted presentation contexts should be used
    int presID = ASC_findAcceptedPresentationContextID(&association_->GetDcmtkAssociation(), sopClass);
    if (presID == 0)
    {
      throw OrthancException(ErrorCode_DicomMoveUnavailable,
                             "Remote AET is " + parameters_.GetRemoteModality().GetApplicationEntityTitle());
    }

    T_DIMSE_C_MoveRQ request;
    memset(&request, 0, sizeof(request));
    request.MessageID = association_->GetDcmtkAssociation().nextMsgID++;
    strncpy(request.AffectedSOPClassUID, sopClass, DIC_UI_LEN);
    request.Priority = DIMSE_PRIORITY_MEDIUM;
    request.DataSetType = DIMSE_DATASET_PRESENT;
    strncpy(request.MoveDestination, targetAet.c_str(), DIC_AE_LEN);

    {
      OFString str;
      CLOG(TRACE, DICOM) << "Sending Move Request:" << std::endl
                         << DIMSE_dumpMessage(str, request, DIMSE_OUTGOING, NULL, presID);
    }
    
    T_DIMSE_C_MoveRSP response;
    DcmDataset* statusDetail = NULL;
    DcmDataset* responseIdentifiers = NULL;
    OFCondition cond = DIMSE_moveUser(
      &association_->GetDcmtkAssociation(), presID, &request, dataset, 
      (progressListener_ != NULL ? MoveProgressCallback : NULL), progressListener_,
      /*opt_blockMode*/ (parameters_.HasTimeout() ? DIMSE_NONBLOCKING : DIMSE_BLOCKING),
      /*opt_dimse_timeout*/ parameters_.GetTimeout(),
      &association_->GetDcmtkNetwork(), /*subOpCallback*/ NULL, NULL,
      &response, &statusDetail, &responseIdentifiers);

    if (statusDetail)
    {
      delete statusDetail;
    }

    if (responseIdentifiers)
    {
      delete responseIdentifiers;
    }

    DicomAssociation::CheckCondition(cond, parameters_, "C-MOVE");

    {
      OFString str;
      CLOG(TRACE, DICOM) << "Received Final Move Response:" << std::endl
                         << DIMSE_dumpMessage(str, response, DIMSE_INCOMING);

      if (progressListener_ != NULL)
      {
        progressListener_->OnProgressUpdated(response.NumberOfRemainingSubOperations,
                                             response.NumberOfCompletedSubOperations,
                                             response.NumberOfFailedSubOperations,
                                             response.NumberOfWarningSubOperations);
      }
    }
    
    /**
     * New in Orthanc 1.6.0: Deal with failures during C-MOVE.
     * http://dicom.nema.org/medical/dicom/current/output/chtml/part04/sect_C.4.2.html#table_C.4-2
     **/
    
    if (response.DimseStatus != 0x0000 &&  // Success
        response.DimseStatus != 0xFF00)    // Pending - Sub-operations are continuing
    {
      char buf[16];
      sprintf(buf, "%04X", response.DimseStatus);

      if (response.DimseStatus == STATUS_MOVE_Failed_UnableToProcess)
      {
        throw OrthancException(ErrorCode_NetworkProtocol,
                               HttpStatus_422_UnprocessableEntity,
                               "C-MOVE SCU to AET \"" +
                               parameters_.GetRemoteModality().GetApplicationEntityTitle() +
                               "\" has failed with DIMSE status 0x" + buf +
                               " (unable to process - resource not found ?)");
      }
      else
      {
        throw OrthancException(ErrorCode_NetworkProtocol, "C-MOVE SCU to AET \"" +
                               parameters_.GetRemoteModality().GetApplicationEntityTitle() +
                               "\" has failed with DIMSE status 0x" + buf);
      }
    }
  }
    

  void DicomControlUserConnection::Get(const DicomMap& findResult,
                                       CGetInstanceReceivedCallback instanceReceivedCallback,
                                       void* callbackContext)
  {
    assert(association_.get() != NULL);
    association_->Open(parameters_);

    std::unique_ptr<ParsedDicomFile> query(
      ConvertQueryFields(findResult, parameters_.GetRemoteModality().GetManufacturer()));
    DcmDataset* queryDataset = query->GetDcmtkObject().getDataset();

    std::string remoteAet;
    std::string remoteIp;
    std::string calledAet;

    association_->GetAssociationParameters(remoteAet, remoteIp, calledAet);

    const char* sopClass = NULL;
    const std::string tmp = findResult.GetValue(DICOM_TAG_QUERY_RETRIEVE_LEVEL).GetContent();
    ResourceType level = StringToResourceType(tmp.c_str());
    switch (level)
    {
      case ResourceType_Patient:
        sopClass = UID_GETPatientRootQueryRetrieveInformationModel;
        break;
      case ResourceType_Study:
      case ResourceType_Series:
      case ResourceType_Instance:
        sopClass = UID_GETStudyRootQueryRetrieveInformationModel;
        break;
      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    // Figure out which of the accepted presentation contexts should be used
    int cgetPresID = ASC_findAcceptedPresentationContextID(&association_->GetDcmtkAssociation(), sopClass);
    if (cgetPresID == 0)
    {
      throw OrthancException(ErrorCode_DicomGetUnavailable,
                             "Remote AET is " + parameters_.GetRemoteModality().GetApplicationEntityTitle());
    }

    T_DIMSE_Message msgGetRequest;
    memset((char*)&msgGetRequest, 0, sizeof(msgGetRequest));
    msgGetRequest.CommandField = DIMSE_C_GET_RQ;

    T_DIMSE_C_GetRQ* request = &(msgGetRequest.msg.CGetRQ);
    request->MessageID = association_->GetDcmtkAssociation().nextMsgID++;
    strncpy(request->AffectedSOPClassUID, sopClass, DIC_UI_LEN);
    request->Priority = DIMSE_PRIORITY_MEDIUM;
    request->DataSetType = DIMSE_DATASET_PRESENT;

    {
      OFString str;
      CLOG(TRACE, DICOM) << "Sending Get Request:" << std::endl
                         << DIMSE_dumpMessage(str, *request, DIMSE_OUTGOING, NULL, cgetPresID);
    }
    
    OFCondition cond = DIMSE_sendMessageUsingMemoryData(
          &(association_->GetDcmtkAssociation()), cgetPresID, &msgGetRequest, NULL /* statusDetail */, queryDataset,
          NULL, NULL, NULL /* commandSet */);
      
    if (cond.bad())
    {
        OFString tempStr;
        CLOG(TRACE, DICOM) << "Failed sending C-GET request: " << DimseCondition::dump(tempStr, cond);
        // return cond;
    }

    // equivalent to handleCGETSession in DCMTK
    bool continueSession = true;

    // As long we want to continue (usually, as long as we receive more objects,
    // i.e. the final C-GET response has not arrived yet)
    while (continueSession)
    {
        T_DIMSE_Message rsp;
        // Make sure everything is zeroed (especially options)
        memset((char*)&rsp, 0, sizeof(rsp));

        // DcmDataset* statusDetail = NULL;
        T_ASC_PresentationContextID cmdPresId = 0;

        OFCondition result = DIMSE_receiveCommand(&(association_->GetDcmtkAssociation()),
                                                  (parameters_.HasTimeout() ? DIMSE_NONBLOCKING : DIMSE_BLOCKING),
                                                  parameters_.GetTimeout(),
                                                  &cmdPresId,
                                                  &rsp,
                                                  NULL /* statusDetail */,
                                                  NULL /* not interested in the command set */);

        if (result.bad())
        {
          OFString tempStr;
          CLOG(TRACE, DICOM) << "Failed receiving DIMSE command: " << DimseCondition::dump(tempStr, result);
          // delete statusDetail;
          break;  // TODO: return value
        }
        // Handle C-GET Response
        if (rsp.CommandField == DIMSE_C_GET_RSP)
        {
          {
            OFString tempStr;
            CLOG(TRACE, DICOM) << "Received C-GET Response: " << std::endl
              << DIMSE_dumpMessage(tempStr, rsp, DIMSE_INCOMING, NULL, cmdPresId);
          }

          if (progressListener_ != NULL)
          {
            progressListener_->OnProgressUpdated(rsp.msg.CGetRSP.NumberOfRemainingSubOperations,
                                                  rsp.msg.CGetRSP.NumberOfCompletedSubOperations,
                                                  rsp.msg.CGetRSP.NumberOfFailedSubOperations,
                                                  rsp.msg.CGetRSP.NumberOfWarningSubOperations);
          }

          if (rsp.msg.CGetRSP.DimseStatus == 0x0000)  // final success message
          {
            continueSession = false;
          }
        }
        // Handle C-STORE Request
        else if (rsp.CommandField == DIMSE_C_STORE_RQ)
        {
          {
            OFString tempStr;
            CLOG(TRACE, DICOM) << "Received C-STORE Request: " << std::endl
              << DIMSE_dumpMessage(tempStr, rsp, DIMSE_INCOMING, NULL, cmdPresId);
          }

          T_DIMSE_C_StoreRQ* storeRequest = &(rsp.msg.CStoreRQ);

          // Check if dataset is announced correctly
          if (rsp.msg.CStoreRQ.DataSetType == DIMSE_DATASET_NULL)
          {
            CLOG(WARNING, DICOM) << "C-GET SCU handler: Incoming C-STORE with no dataset";
          }

          Uint16 desiredCStoreReturnStatus = 0;
          DcmDataset* dataObject = NULL;

          // Receive dataset
          result = DIMSE_receiveDataSetInMemory(&(association_->GetDcmtkAssociation()),
                                                  (parameters_.HasTimeout() ? DIMSE_NONBLOCKING : DIMSE_BLOCKING),
                                                  parameters_.GetTimeout(),
                                                  &cmdPresId,
                                                  &dataObject,
                                                  NULL, NULL);

          if (result.bad())
          {
            LOG(WARNING) << "C-GET SCU handler: Failed to receive dataset: " << result.text();
            desiredCStoreReturnStatus = STATUS_STORE_Error_CannotUnderstand;
          }
          else
          {
            // callback the OrthancServer with the received data
            if (instanceReceivedCallback != NULL)
            {
              desiredCStoreReturnStatus = instanceReceivedCallback(callbackContext, *dataObject, remoteAet, remoteIp, calledAet);
            }

            // send the Store response
            T_DIMSE_Message storeResponse;
            memset((char*)&storeResponse, 0, sizeof(storeResponse));
            storeResponse.CommandField         = DIMSE_C_STORE_RSP;

            T_DIMSE_C_StoreRSP& storeRsp       = storeResponse.msg.CStoreRSP;
            storeRsp.MessageIDBeingRespondedTo = storeRequest->MessageID;
            storeRsp.DimseStatus               = desiredCStoreReturnStatus;
            storeRsp.DataSetType               = DIMSE_DATASET_NULL;

            OFStandard::strlcpy(
                storeRsp.AffectedSOPClassUID, storeRequest->AffectedSOPClassUID, sizeof(storeRsp.AffectedSOPClassUID));
            OFStandard::strlcpy(
                storeRsp.AffectedSOPInstanceUID, storeRequest->AffectedSOPInstanceUID, sizeof(storeRsp.AffectedSOPInstanceUID));
            storeRsp.opts = O_STORE_AFFECTEDSOPCLASSUID | O_STORE_AFFECTEDSOPINSTANCEUID;

            result = DIMSE_sendMessageUsingMemoryData(&(association_->GetDcmtkAssociation()), 
                                                      cmdPresId, 
                                                      &storeResponse, NULL /* statusDetail */, NULL /* dataObject */,
                                                      NULL, NULL, NULL /* commandSet */);
            if (result.bad())
            {
              continueSession = false;
            }
            else
            {
              OFString tempStr;
              CLOG(TRACE, DICOM) << "Sent C-STORE Response: " << std::endl
                << DIMSE_dumpMessage(tempStr, storeResponse, DIMSE_OUTGOING, NULL, cmdPresId);
            }
          }
        }
        // Handle other DIMSE command (error since other command than GET/STORE not expected)
        else
        {
          CLOG(WARNING, DICOM) << "Expected C-GET response or C-STORE request but received DIMSE command 0x"
                               << std::hex << std::setfill('0') << std::setw(4)
                               << static_cast<unsigned int>(rsp.CommandField);
          
          result          = DIMSE_BADCOMMANDTYPE;
          continueSession = false;
        }

        // delete statusDetail; // should be NULL if not existing or added to response list
        // statusDetail = NULL;
    }
    /* All responses received or break signal occurred */

    // return result;
}


  DicomControlUserConnection::DicomControlUserConnection(const DicomAssociationParameters& params, ScuOperationFlags scuOperation) :
    parameters_(params),
    association_(new DicomAssociation),
    progressListener_(NULL)
  {
    assert((scuOperation & ScuOperationFlags_Get) == 0);  // you must provide acceptedStorageSopClassUids for Get SCU
    std::set<std::string> emptyStorageSopClasses;
    std::list<DicomTransferSyntax> emptyStorageTransferSyntaxes;

    SetupPresentationContexts(scuOperation, emptyStorageSopClasses, emptyStorageTransferSyntaxes);
  }
    
  DicomControlUserConnection::DicomControlUserConnection(const DicomAssociationParameters& params, 
                                                         ScuOperationFlags scuOperation,
                                                         const std::set<std::string>& acceptedStorageSopClasses,
                                                         const std::list<DicomTransferSyntax>& proposedStorageTransferSyntaxes) :
    parameters_(params),
    association_(new DicomAssociation),
    progressListener_(NULL)
  {
    SetupPresentationContexts(scuOperation, acceptedStorageSopClasses, proposedStorageTransferSyntaxes);
  }
    

  void DicomControlUserConnection::Close()
  {
    assert(association_.get() != NULL);
    association_->Close();
  }


  bool DicomControlUserConnection::Echo()
  {
    assert(association_.get() != NULL);
    association_->Open(parameters_);

    DIC_US status;
    DicomAssociation::CheckCondition(
      DIMSE_echoUser(&association_->GetDcmtkAssociation(),
                     association_->GetDcmtkAssociation().nextMsgID++, 
                     /*opt_blockMode*/ (parameters_.HasTimeout() ? DIMSE_NONBLOCKING : DIMSE_BLOCKING),
                     /*opt_dimse_timeout*/ parameters_.GetTimeout(),
                     &status, NULL),
      parameters_, "C-ECHO");
      
    return status == STATUS_Success;
  }


  void DicomControlUserConnection::Find(DicomFindAnswers& result,
                                        ResourceType level,
                                        const DicomMap& originalFields,
                                        bool normalize)
  {
    std::unique_ptr<ParsedDicomFile> query;

    if (normalize)
    {
      DicomMap fields;
      NormalizeFindQuery(fields, level, originalFields);
      query.reset(ConvertQueryFields(fields, parameters_.GetRemoteModality().GetManufacturer()));
    }
    else
    {
      query.reset(new ParsedDicomFile(originalFields, GetDefaultDicomEncoding(),
                                      false /* be strict */));
    }
    
    DcmDataset* dataset = query->GetDcmtkObject().getDataset();
    assert(dataset != NULL);

    const char* clevel = ResourceTypeToDicomQueryRetrieveLevel(level);
    const char* sopClass = NULL;

    DU_putStringDOElement(dataset, DCM_QueryRetrieveLevel, clevel);

    switch (level)
    {
      case ResourceType_Patient:
        sopClass = UID_FINDPatientRootQueryRetrieveInformationModel;
        break;

      case ResourceType_Study:
        sopClass = UID_FINDStudyRootQueryRetrieveInformationModel;
        break;

      case ResourceType_Series:
        sopClass = UID_FINDStudyRootQueryRetrieveInformationModel;
        break;

      case ResourceType_Instance:
        sopClass = UID_FINDStudyRootQueryRetrieveInformationModel;
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }


    const char* universal;
    if (parameters_.GetRemoteModality().GetManufacturer() == ModalityManufacturer_GE)
    {
      universal = "*";
    }
    else
    {
      universal = "";
    }      
    

    // Add the expected tags for this query level.
    // WARNING: Do not reorder or add "break" in this switch-case!
    switch (level)
    {
      case ResourceType_Instance:
        if (!dataset->tagExists(DCM_SOPInstanceUID))
        {
          DU_putStringDOElement(dataset, DCM_SOPInstanceUID, universal);
        }

      case ResourceType_Series:
        if (!dataset->tagExists(DCM_SeriesInstanceUID))
        {
          DU_putStringDOElement(dataset, DCM_SeriesInstanceUID, universal);
        }

      case ResourceType_Study:
        if (!dataset->tagExists(DCM_AccessionNumber))
        {
          DU_putStringDOElement(dataset, DCM_AccessionNumber, universal);
        }

        if (!dataset->tagExists(DCM_StudyInstanceUID))
        {
          DU_putStringDOElement(dataset, DCM_StudyInstanceUID, universal);
        }

      case ResourceType_Patient:
        if (!dataset->tagExists(DCM_PatientID))
        {
          DU_putStringDOElement(dataset, DCM_PatientID, universal);
        }
        
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    assert(clevel != NULL && sopClass != NULL);
    FindInternal(result, dataset, sopClass, false, clevel);
  }
    

  void DicomControlUserConnection::Move(const std::string& targetAet,
                                        ResourceType level,
                                        const DicomMap& findResult)
  {
    DicomMap move;
    switch (level)
    {
      case ResourceType_Patient:
        TestAndCopyTag(move, findResult, DICOM_TAG_PATIENT_ID);
        break;

      case ResourceType_Study:
        TestAndCopyTag(move, findResult, DICOM_TAG_STUDY_INSTANCE_UID);
        break;

      case ResourceType_Series:
        TestAndCopyTag(move, findResult, DICOM_TAG_STUDY_INSTANCE_UID);
        TestAndCopyTag(move, findResult, DICOM_TAG_SERIES_INSTANCE_UID);
        break;

      case ResourceType_Instance:
        TestAndCopyTag(move, findResult, DICOM_TAG_STUDY_INSTANCE_UID);
        TestAndCopyTag(move, findResult, DICOM_TAG_SERIES_INSTANCE_UID);
        TestAndCopyTag(move, findResult, DICOM_TAG_SOP_INSTANCE_UID);
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    MoveInternal(targetAet, level, move);
  }


  void DicomControlUserConnection::Move(const std::string& targetAet,
                                        const DicomMap& findResult)
  {
    if (!findResult.HasTag(DICOM_TAG_QUERY_RETRIEVE_LEVEL))
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    const std::string tmp = findResult.GetValue(DICOM_TAG_QUERY_RETRIEVE_LEVEL).GetContent();
    ResourceType level = StringToResourceType(tmp.c_str());

    Move(targetAet, level, findResult);
  }


  void DicomControlUserConnection::MovePatient(const std::string& targetAet,
                                               const std::string& patientId)
  {
    DicomMap query;
    query.SetValue(DICOM_TAG_PATIENT_ID, patientId, false);
    MoveInternal(targetAet, ResourceType_Patient, query);
  }
    

  void DicomControlUserConnection::MoveStudy(const std::string& targetAet,
                                             const std::string& studyUid)
  {
    DicomMap query;
    query.SetValue(DICOM_TAG_STUDY_INSTANCE_UID, studyUid, false);
    MoveInternal(targetAet, ResourceType_Study, query);
  }

    
  void DicomControlUserConnection::MoveSeries(const std::string& targetAet,
                                              const std::string& studyUid,
                                              const std::string& seriesUid)
  {
    DicomMap query;
    query.SetValue(DICOM_TAG_STUDY_INSTANCE_UID, studyUid, false);
    query.SetValue(DICOM_TAG_SERIES_INSTANCE_UID, seriesUid, false);
    MoveInternal(targetAet, ResourceType_Series, query);
  }


  void DicomControlUserConnection::MoveInstance(const std::string& targetAet,
                                                const std::string& studyUid,
                                                const std::string& seriesUid,
                                                const std::string& instanceUid)
  {
    DicomMap query;
    query.SetValue(DICOM_TAG_STUDY_INSTANCE_UID, studyUid, false);
    query.SetValue(DICOM_TAG_SERIES_INSTANCE_UID, seriesUid, false);
    query.SetValue(DICOM_TAG_SOP_INSTANCE_UID, instanceUid, false);
    MoveInternal(targetAet, ResourceType_Instance, query);
  }


  void DicomControlUserConnection::FindWorklist(DicomFindAnswers& result,
                                                ParsedDicomFile& query)
  {
    DcmDataset* dataset = query.GetDcmtkObject().getDataset();
    const char* sopClass = UID_FINDModalityWorklistInformationModel;

    FindInternal(result, dataset, sopClass, true, NULL);
  }
}
