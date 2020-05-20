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

#include "PrecompiledHeadersServer.h"
#include "OrthancGetRequestHandler.h"

#include <dcmtk/dcmnet/assoc.h>
#include <dcmtk/dcmnet/dimse.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcistrmb.h>
#include <dcmtk/dcmnet/diutil.h>

#include "../../Core/DicomParsing/FromDcmtkBridge.h"
#include "../Core/DicomFormat/DicomArray.h"
#include "../Core/Logging.h"
#include "../Core/MetricsRegistry.h"
#include "OrthancConfiguration.h"
#include "ServerContext.h"
#include "ServerJobs/DicomModalityStoreJob.h"



namespace Orthanc
{
  namespace
  {
    // Anonymous namespace to avoid clashes between compilation modules
    
    static void GetSubOpProgressCallback(
      void * /* callbackData == pointer to the "OrthancGetRequestHandler" object */,
      T_DIMSE_StoreProgress *progress,
      T_DIMSE_C_StoreRQ * /*req*/)
    {
      // SBL - no logging to be done here.
    }
  }

  OrthancGetRequestHandler::Status OrthancGetRequestHandler::DoNext(T_ASC_Association* assoc)
  {
    if (position_ >= instances_.size())
    {
      return Status_Failure;
    }
    
    const std::string& id = instances_[position_++];

    std::string dicom;
    context_.ReadDicom(dicom, id);
    
    if (dicom.size() <= 0)
    {
      return Status_Failure;
    }

    ParsedDicomFile parsed(dicom);

    if (parsed.GetDcmtkObject().getDataset() == NULL)
    {
      throw OrthancException(ErrorCode_InternalError);
    }
    
    DcmDataset& dataset = *parsed.GetDcmtkObject().getDataset();
    
    OFString a, b;
    if (!dataset.findAndGetOFString(DCM_SOPClassUID, a).good() ||
        !dataset.findAndGetOFString(DCM_SOPInstanceUID, b).good())
    {
      throw OrthancException(ErrorCode_NoSopClassOrInstance,
                             "Unable to determine the SOP class/instance for C-STORE with AET " +
                             originatorAet_);
    }

    std::string sopClassUid(a.c_str());
    std::string sopInstanceUid(b.c_str());
    
    OFCondition cond = PerformGetSubOp(assoc, sopClassUid, sopInstanceUid, dataset);
    
    if (getCancelled_)
    {
      LOG(INFO) << "Get SCP: Received C-Cancel RQ";
    }
    
    if (cond.bad() || getCancelled_)
    {
      return Status_Failure;
    }
    
    return Status_Success;
  }

  
  void OrthancGetRequestHandler::AddFailedUIDInstance(const std::string& sopInstance)
  {
    if (failedUIDs_.empty())
    {
      failedUIDs_ = sopInstance;
    }
    else
    {
      failedUIDs_ += "\\" + sopInstance;
    }
  }


  OFCondition OrthancGetRequestHandler::PerformGetSubOp(T_ASC_Association* assoc,
                                                        const std::string& sopClassUid,
                                                        const std::string& sopInstanceUid,
                                                        DcmDataset& dataset)
  {
    T_ASC_PresentationContextID presId;
    
    // which presentation context should be used
    presId = ASC_findAcceptedPresentationContextID(assoc, sopClassUid.c_str());
    
    if (presId == 0)
    {
      nFailed_++;
      AddFailedUIDInstance(sopInstanceUid);
      LOG(ERROR) << "Get SCP: storeSCU: No presentation context for: ("
                 << dcmSOPClassUIDToModality(sopClassUid.c_str(), "OT") << ") " << sopClassUid;
      return DIMSE_NOVALIDPRESENTATIONCONTEXTID;
    }
    else
    {
      // make sure that we can send images in this presentation context
      T_ASC_PresentationContext pc;
      ASC_findAcceptedPresentationContext(assoc->params, presId, &pc);
      // the acceptedRole is the association requestor role
      if ((pc.acceptedRole != ASC_SC_ROLE_SCP) &&
          (pc.acceptedRole != ASC_SC_ROLE_SCUSCP))
      {
        // the role is not appropriate
        nFailed_++;
        AddFailedUIDInstance(sopInstanceUid);
        LOG(ERROR) <<"Get SCP: storeSCU: [No presentation context with requestor SCP role for: ("
                   << dcmSOPClassUIDToModality(sopClassUid.c_str(), "OT") << ") " << sopClassUid;
        return DIMSE_NOVALIDPRESENTATIONCONTEXTID;
      }
    }

    const DIC_US msgId = assoc->nextMsgID++;
    
    T_DIMSE_C_StoreRQ req;
    memset(&req, 0, sizeof(req));
    req.MessageID = msgId;
    strncpy(req.AffectedSOPClassUID, sopClassUid.c_str(), DIC_UI_LEN);
    strncpy(req.AffectedSOPInstanceUID, sopInstanceUid.c_str(), DIC_UI_LEN);
    req.DataSetType = DIMSE_DATASET_PRESENT;
    req.Priority = DIMSE_PRIORITY_MEDIUM;
    req.opts = 0;
    
    T_DIMSE_C_StoreRSP rsp;
    memset(&rsp, 0, sizeof(rsp));

    LOG(INFO) << "Store SCU RQ: MsgID " << msgId << ", ("
              << dcmSOPClassUIDToModality(sopClassUid.c_str(), "OT") << ")";
    
    T_DIMSE_DetectedCancelParameters cancelParameters;
    memset(&cancelParameters, 0, sizeof(cancelParameters));

    std::unique_ptr<DcmDataset> stDetail;

    OFCondition cond;

    {
      DcmDataset *stDetailTmp = NULL;
      cond = DIMSE_storeUser(assoc, presId, &req, NULL /* imageFileName */, &dataset,
                             GetSubOpProgressCallback, this /* callbackData */,
                             (timeout_ > 0 ? DIMSE_NONBLOCKING : DIMSE_BLOCKING), timeout_,
                             &rsp, &stDetailTmp, &cancelParameters);
      stDetail.reset(stDetailTmp);
    }
    
    if (cond.good())
    {
      if (cancelParameters.cancelEncountered)
      {
        if (origPresId_ == cancelParameters.presId &&
            origMsgId_ == cancelParameters.req.MessageIDBeingRespondedTo)
        {
          getCancelled_ = OFTrue;
        }
        else
        {
          LOG(ERROR) << "Get SCP: Unexpected C-Cancel-RQ encountered: pid=" << (int)cancelParameters.presId
                     << ", mid=" << (int)cancelParameters.req.MessageIDBeingRespondedTo;
        }
      }
      
      if (rsp.DimseStatus == STATUS_Success)
      {
        // everything ok
        nCompleted_++;
      }
      else if ((rsp.DimseStatus & 0xf000) == 0xb000)
      {
        // a warning status message
        warningCount_++;
        LOG(ERROR) << "Get SCP: Store Warning: Response Status: "
                   << DU_cstoreStatusString(rsp.DimseStatus);
      }
      else
      {
        nFailed_++;
        AddFailedUIDInstance(sopInstanceUid);
        // print a status message
        LOG(ERROR) << "Get SCP: Store Failed: Response Status: "
                   << DU_cstoreStatusString(rsp.DimseStatus);
      }
    }
    else
    {
      nFailed_++;
      AddFailedUIDInstance(sopInstanceUid);
      OFString temp_str;
      LOG(ERROR) << "Get SCP: storeSCU: Store Request Failed: " << DimseCondition::dump(temp_str, cond);
    }
    
    if (stDetail.get() != NULL)
    {
      LOG(INFO) << "  Status Detail:" << OFendl << DcmObject::PrintHelper(*stDetail);
    }
    
    return cond;
  }

  bool OrthancGetRequestHandler::LookupIdentifiers(std::list<std::string>& publicIds,
                                                   ResourceType level,
                                                   const DicomMap& input) const
  {
    DicomTag tag(0, 0);   // Dummy initialization

    switch (level)
    {
      case ResourceType_Patient:
        tag = DICOM_TAG_PATIENT_ID;
        break;

      case ResourceType_Study:
        tag = (input.HasTag(DICOM_TAG_ACCESSION_NUMBER) ?
               DICOM_TAG_ACCESSION_NUMBER : DICOM_TAG_STUDY_INSTANCE_UID);
        break;
        
      case ResourceType_Series:
        tag = DICOM_TAG_SERIES_INSTANCE_UID;
        break;
        
      case ResourceType_Instance:
        tag = DICOM_TAG_SOP_INSTANCE_UID;
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    if (!input.HasTag(tag))
    {
      return false;
    }

    const DicomValue& value = input.GetValue(tag);
    if (value.IsNull() ||
        value.IsBinary())
    {
      return false;
    }
    else
    {
      std::vector<std::string> tokens;
      Toolbox::TokenizeString(tokens, value.GetContent(), '\\');

      for (size_t i = 0; i < tokens.size(); i++)
      {
        std::vector<std::string> tmp;
        context_.GetIndex().LookupIdentifierExact(tmp, level, tag, tokens[i]);

        if (tmp.empty())
        {
          LOG(ERROR) << "C-GET: Cannot locate resource \"" << tokens[i]
                     << "\" at the " << EnumerationToString(level) << " level";
          return false;
        }
        else
        {
          for (size_t i = 0; i < tmp.size(); i++)
          {
            publicIds.push_back(tmp[i]);
          }
        }
      }

      return true;      
    }
  }


    OrthancGetRequestHandler::OrthancGetRequestHandler(ServerContext& context) :
      context_(context)
    {
      position_ = 0;
      nRemaining_ = 0;
      nCompleted_  = 0;
      warningCount_ = 0;
      nFailed_ = 0;
      timeout_ = 0;
    }


  bool OrthancGetRequestHandler::Handle(const DicomMap& input,
                                        const std::string& originatorIp,
                                        const std::string& originatorAet,
                                        const std::string& calledAet,
                                        uint32_t timeout)
  {
    MetricsRegistry::Timer timer(context_.GetMetricsRegistry(), "orthanc_get_scp_duration_ms");

    LOG(WARNING) << "Get-SCU request received from AET \"" << originatorAet << "\"";

    {
      DicomArray query(input);
      for (size_t i = 0; i < query.GetSize(); i++)
      {
        if (!query.GetElement(i).GetValue().IsNull())
        {
          LOG(INFO) << "  " << query.GetElement(i).GetTag()
                    << "  " << FromDcmtkBridge::GetTagName(query.GetElement(i))
                    << " = " << query.GetElement(i).GetValue().GetContent();
        }
      }
    }

    /**
     * Retrieve the query level.
     **/

    const DicomValue* levelTmp = input.TestAndGetValue(DICOM_TAG_QUERY_RETRIEVE_LEVEL);

    assert(levelTmp != NULL);
    ResourceType level = StringToResourceType(levelTmp->GetContent().c_str());      


    /**
     * Lookup for the resource to be sent.
     **/

    std::list<std::string> publicIds;

    if (!LookupIdentifiers(publicIds, level, input))
    {
      LOG(ERROR) << "Cannot determine what resources are requested by C-GET";
      return false; 
    }

    localAet_ = context_.GetDefaultLocalApplicationEntityTitle();
    position_ = 0;
    originatorAet_ = originatorAet;
    
    {
      OrthancConfiguration::ReaderLock lock;
      remote_ = lock.GetConfiguration().GetModalityUsingAet(originatorAet);
    }

    for (std::list<std::string>::const_iterator
           resource = publicIds.begin(); resource != publicIds.end(); ++resource)
    {
      LOG(INFO) << "C-GET: Sending resource " << *resource
                << " to modality \"" << originatorAet << "\"";
      
      std::list<std::string> tmp;
      context_.GetIndex().GetChildInstances(tmp, *resource);
      
      instances_.reserve(tmp.size());
      for (std::list<std::string>::iterator it = tmp.begin(); it != tmp.end(); ++it)
      {
        instances_.push_back(*it);
      }
    }

    failedUIDs_.clear();
    getCancelled_ = OFFalse;

    nRemaining_ = GetSubOperationCount();
    nCompleted_ = 0;
    nFailed_ = 0;
    warningCount_ = 0;
    timeout_ = timeout;

    return true;
  }
};
