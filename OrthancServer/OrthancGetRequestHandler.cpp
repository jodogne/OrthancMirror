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
    
    static void getSubOpProgressCallback(void * /* callbackData */,
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

    std::unique_ptr<DcmFileFormat> parsed(
      FromDcmtkBridge::LoadFromMemoryBuffer(dicom.c_str(), dicom.size()));
    
    // Determine the storage SOP class UID for this instance
    DIC_UI sopClass;
    DIC_UI sopInstance;
    
#if DCMTK_VERSION_NUMBER >= 364
    if (!DU_findSOPClassAndInstanceInDataSet(static_cast<DcmItem *> (parsed->getDataset()),
                                             sopClass, sizeof(sopClass),
                                             sopInstance, sizeof(sopInstance)))
#else
      if (!DU_findSOPClassAndInstanceInDataSet(parsed->getDataset(), sopClass, sopInstance))
#endif
      {
        throw OrthancException(ErrorCode_NoSopClassOrInstance,
                               "Unable to determine the SOP class/instance for C-STORE with AET " +
                               originatorAet_);
      }
    
    OFCondition cond = performGetSubOp(assoc, sopClass, sopInstance, parsed->getDataset());
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

  
  void OrthancGetRequestHandler::addFailedUIDInstance(const char *sopInstance)
  {
    if (failedUIDs_.empty())
    {
      failedUIDs_ = sopInstance;
    }
    else
    {
      failedUIDs_ += "\\" + std::string(sopInstance);
    }
  }


  OFCondition OrthancGetRequestHandler::performGetSubOp(T_ASC_Association* assoc, 
                                                        DIC_UI sopClass, 
                                                        DIC_UI sopInstance, 
                                                        DcmDataset *dataset)
  {
    OFCondition cond = EC_Normal;
    T_DIMSE_C_StoreRQ req;
    T_DIMSE_C_StoreRSP rsp;
    DIC_US msgId;
    T_ASC_PresentationContextID presId;
    DcmDataset *stDetail = NULL;
    
    msgId = assoc->nextMsgID++;
    
    // which presentation context should be used
    presId = ASC_findAcceptedPresentationContextID(assoc, sopClass);
    
    if (presId == 0)
    {
      nFailed_++;
      addFailedUIDInstance(sopInstance);
      LOG(ERROR) << "Get SCP: storeSCU: No presentation context for: ("
                 << dcmSOPClassUIDToModality(sopClass, "OT") << ") " << sopClass;
      return DIMSE_NOVALIDPRESENTATIONCONTEXTID;
    }
    else
    {
      // make sure that we can send images in this presentation context
      T_ASC_PresentationContext pc;
      ASC_findAcceptedPresentationContext(assoc->params, presId, &pc);
      // the acceptedRole is the association requestor role
      if ((pc.acceptedRole != ASC_SC_ROLE_SCP) && (pc.acceptedRole != ASC_SC_ROLE_SCUSCP))
      {
        // the role is not appropriate
        nFailed_++;
        addFailedUIDInstance(sopInstance);
        LOG(ERROR) <<"Get SCP: storeSCU: [No presentation context with requestor SCP role for: ("
                   << dcmSOPClassUIDToModality(sopClass, "OT") << ") " << sopClass;
        return DIMSE_NOVALIDPRESENTATIONCONTEXTID;
      }
    }
    
    req.MessageID = msgId;
    strcpy(req.AffectedSOPClassUID, sopClass);
    strcpy(req.AffectedSOPInstanceUID, sopInstance);
    req.DataSetType = DIMSE_DATASET_PRESENT;
    req.Priority = priority_;
    req.opts = 0;
    
    LOG(INFO) << "Store SCU RQ: MsgID " << msgId << ", ("
              << dcmSOPClassUIDToModality(sopClass, "OT") << ")";
    
    T_DIMSE_DetectedCancelParameters cancelParameters;
    
    cond = DIMSE_storeUser(assoc, presId, &req,
                           NULL, dataset, getSubOpProgressCallback, this, DIMSE_BLOCKING, 0,
                           &rsp, &stDetail, &cancelParameters);
    
    if (cond.good())
    {
      if (cancelParameters.cancelEncountered)
      {
        if (origPresId == cancelParameters.presId &&
            origMsgId == cancelParameters.req.MessageIDBeingRespondedTo)
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
        addFailedUIDInstance(sopInstance);
        // print a status message
        LOG(ERROR) << "Get SCP: Store Failed: Response Status: "
                   << DU_cstoreStatusString(rsp.DimseStatus);
      }
    }
    else
    {
      nFailed_++;
      addFailedUIDInstance(sopInstance);
      OFString temp_str;
      LOG(ERROR) << "Get SCP: storeSCU: Store Request Failed: " << DimseCondition::dump(temp_str, cond);
    }
    
    if (stDetail)
    {
      LOG(INFO) << "  Status Detail:" << OFendl << DcmObject::PrintHelper(*stDetail);
      delete stDetail;
    }
    
    return cond;
  }

  bool OrthancGetRequestHandler::LookupIdentifiers(std::vector<std::string>& publicIds,
                                                   ResourceType level,
                                                   const DicomMap& input)
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
      const std::string& content = value.GetContent();
      context_.GetIndex().LookupIdentifierExact(publicIds, level, tag, content);
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
  }


  bool OrthancGetRequestHandler::Handle(const DicomMap& input,
                                        const std::string& originatorIp,
                                        const std::string& originatorAet,
                                        const std::string& calledAet)
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

    std::vector<std::string> publicIds;

    bool retVal = LookupIdentifiers(publicIds, level, input);
    localAet_ = context_.GetDefaultLocalApplicationEntityTitle();
    position_ = 0;
    originatorAet_ = originatorAet;
    
    {
      OrthancConfiguration::ReaderLock lock;
      remote_ = lock.GetConfiguration().GetModalityUsingAet(originatorAet);
    }
    
    for (size_t i = 0; i < publicIds.size(); i++)
    {
      LOG(INFO) << "Sending resource " << publicIds[i] << " to modality \""
                << originatorAet << "\" in synchronous mode";
      
      std::list<std::string> tmp;
      context_.GetIndex().GetChildInstances(tmp, publicIds[i]);
      
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

    return retVal;    
  }
};
