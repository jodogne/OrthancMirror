/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2024 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2024 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "DicomGetScuJob.h"

#include "../../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"
#include "../../../OrthancFramework/Sources/SerializationToolbox.h"
#include "../ServerContext.h"
#include <dcmtk/dcmnet/dimse.h>
#include <algorithm>

static const char* const LOCAL_AET = "LocalAet";
static const char* const QUERY = "Query";
static const char* const QUERY_FORMAT = "QueryFormat";  // New in 1.9.5
static const char* const REMOTE = "Remote";
static const char* const TIMEOUT = "Timeout";

namespace Orthanc
{


  static uint16_t InstanceReceivedHandler(void* callbackContext,
                                          DcmDataset& dataset,
                                          const std::string& remoteAet,
                                          const std::string& remoteIp,
                                          const std::string& calledAet)
  {
    // this code is equivalent to OrthancStoreRequestHandler
    ServerContext* context = reinterpret_cast<ServerContext*>(callbackContext);

    std::unique_ptr<DicomInstanceToStore> toStore(DicomInstanceToStore::CreateFromDcmDataset(dataset));
    
    if (toStore->GetBufferSize() > 0)
    {
      toStore->SetOrigin(DicomInstanceOrigin::FromDicomProtocol
                         (remoteIp.c_str(), remoteAet.c_str(), calledAet.c_str()));

      std::string id;
      ServerContext::StoreResult result = context->Store(id, *toStore, StoreInstanceMode_Default);
      return result.GetCStoreStatusCode();
    }

    return STATUS_STORE_Error_CannotUnderstand;
  }

  void DicomGetScuJob::Retrieve(const DicomMap& findAnswer)
  {
    if (connection_.get() == NULL)
    {
      std::set<std::string> sopClassesToPropose;
      std::set<std::string> sopClassesInStudy;
      std::set<std::string> acceptedSopClasses;
      std::list<DicomTransferSyntax> proposedTransferSyntaxes;

      if (findAnswer.HasTag(DICOM_TAG_SOP_CLASSES_IN_STUDY) &&
          findAnswer.LookupStringValues(sopClassesInStudy, DICOM_TAG_SOP_CLASSES_IN_STUDY, false))
      {
        context_.GetAcceptedSopClasses(acceptedSopClasses, 0); 

        // keep the sop classes from the resources to retrieve only if they are accepted by Orthanc
        Toolbox::GetIntersection(sopClassesToPropose, sopClassesInStudy, acceptedSopClasses);
      }
      else
      {
        // when we don't know what SOP Classes to use, we include the 120 most common SOP Classes because 
        // there are only 128 presentation contexts available
        context_.GetAcceptedSopClasses(sopClassesToPropose, 120); 
      }

      if (sopClassesToPropose.size() == 0)
      {
        throw OrthancException(ErrorCode_NoPresentationContext, "Cannot perform C-Get, no SOPClassUID have been accepted by Orthanc.");        
      }

      context_.GetProposedStorageTransferSyntaxes(proposedTransferSyntaxes);

      connection_.reset(new DicomControlUserConnection(parameters_, 
                                                       ScuOperationFlags_Get, 
                                                       sopClassesToPropose,
                                                       proposedTransferSyntaxes));
    }

    connection_->SetProgressListener(this);
    connection_->Get(findAnswer, InstanceReceivedHandler, &context_);
  }

  void DicomGetScuJob::AddResourceToRetrieve(ResourceType level, const std::string& dicomId)
  {
    // TODO-GET: when retrieving a single series, one must provide the StudyInstanceUID too
    DicomMap item;

    switch (level)
    {
    case ResourceType_Patient:
      item.SetValue(DICOM_TAG_QUERY_RETRIEVE_LEVEL, ResourceTypeToDicomQueryRetrieveLevel(level), false);
      item.SetValue(DICOM_TAG_PATIENT_ID, dicomId, false);
      break;

    case ResourceType_Study:
      item.SetValue(DICOM_TAG_QUERY_RETRIEVE_LEVEL, ResourceTypeToDicomQueryRetrieveLevel(level), false);
      item.SetValue(DICOM_TAG_STUDY_INSTANCE_UID, dicomId, false);
      break;

    case ResourceType_Series:
      item.SetValue(DICOM_TAG_QUERY_RETRIEVE_LEVEL, ResourceTypeToDicomQueryRetrieveLevel(level), false);
      item.SetValue(DICOM_TAG_SERIES_INSTANCE_UID, dicomId, false);
      break;

    case ResourceType_Instance:
      item.SetValue(DICOM_TAG_QUERY_RETRIEVE_LEVEL, ResourceTypeToDicomQueryRetrieveLevel(level), false);
      item.SetValue(DICOM_TAG_SOP_INSTANCE_UID, dicomId, false);
      break;

    default:
      throw OrthancException(ErrorCode_InternalError);
    }
    query_.Add(item);
    
    AddCommand(new Command(*this, item));
  }

}
