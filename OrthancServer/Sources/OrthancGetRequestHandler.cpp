/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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

#include "../../OrthancFramework/Sources/DicomFormat/DicomArray.h"
#include "../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"
#include "../../OrthancFramework/Sources/Logging.h"
#include "../../OrthancFramework/Sources/MetricsRegistry.h"
#include "OrthancConfiguration.h"
#include "ServerContext.h"
#include "ServerJobs/DicomModalityStoreJob.h"

#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcistrmb.h>
#include <dcmtk/dcmnet/assoc.h>
#include <dcmtk/dcmnet/dimse.h>
#include <dcmtk/dcmnet/diutil.h>
#include <dcmtk/ofstd/ofstring.h>

#include <sstream>  // For std::stringstream

namespace Orthanc
{
  static void ProgressCallback(void *callbackData,
                               T_DIMSE_StoreProgress *progress,
                               T_DIMSE_C_StoreRQ *req)
  {
    if (req != NULL &&
        progress->state == DIMSE_StoreBegin)
    {
      OFString str;
      CLOG(TRACE, DICOM) << "Sending Store Request following a C-GET:" << std::endl
                         << DIMSE_dumpMessage(str, *req, DIMSE_OUTGOING);
    }
  }


  bool OrthancGetRequestHandler::DoNext(T_ASC_Association* assoc)
  {
    if (position_ >= instances_.size())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    
    const std::string& id = instances_[position_++];

    std::string dicom;
    context_.ReadDicom(dicom, id);
    
    if (dicom.empty())
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    std::unique_ptr<DcmFileFormat> parsed(
      FromDcmtkBridge::LoadFromMemoryBuffer(dicom.c_str(), dicom.size()));

    if (parsed.get() == NULL ||
        parsed->getDataset() == NULL)
    {
      throw OrthancException(ErrorCode_InternalError);
    }
    
    DcmDataset& dataset = *parsed->getDataset();
    
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
    
    return PerformGetSubOp(assoc, sopClassUid, sopInstanceUid, parsed.release());
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


  static bool SelectPresentationContext(T_ASC_PresentationContextID& selectedPresentationId,
                                        DicomTransferSyntax& selectedSyntax,
                                        T_ASC_Association* assoc,
                                        const std::string& sopClassUid,
                                        DicomTransferSyntax sourceSyntax,
                                        bool allowTranscoding)
  {
    typedef std::map<DicomTransferSyntax, T_ASC_PresentationContextID> Accepted;

    Accepted accepted;

    /**
     * 1. Inspect and index all the accepted transfer syntaxes. This
     * is similar to the code from "DicomAssociation::Open()".
     **/

    LST_HEAD **l = &assoc->params->DULparams.acceptedPresentationContext;
    if (*l != NULL)
    {
      DUL_PRESENTATIONCONTEXT* pc = (DUL_PRESENTATIONCONTEXT*) LST_Head(l);
      LST_Position(l, (LST_NODE*)pc);
      while (pc)
      {
        DicomTransferSyntax transferSyntax;
        if (pc->result == ASC_P_ACCEPTANCE &&
            LookupTransferSyntax(transferSyntax, pc->acceptedTransferSyntax))
        {
          /*CLOG(TRACE, DICOM) << "C-GET SCP accepted: SOP class " << pc->abstractSyntax
            << " with transfer syntax " << GetTransferSyntaxUid(transferSyntax);*/
          if (std::string(pc->abstractSyntax) == sopClassUid)
          {
            accepted[transferSyntax] = pc->presentationContextID;
          }
        }
        else
        {
          CLOG(WARNING, DICOM) << "C-GET: Unknown transfer syntax received: "
                               << pc->acceptedTransferSyntax;
        }
            
        pc = (DUL_PRESENTATIONCONTEXT*) LST_Next(l);
      }
    }

    
    /**
     * 2. Select the preferred transfer syntaxes, which corresponds to
     * the source transfer syntax, plus all the uncompressed transfer
     * syntaxes if transcoding is enabled.
     **/
    
    std::list<DicomTransferSyntax> preferred;
    preferred.push_back(sourceSyntax);

    if (allowTranscoding)
    {
      if (sourceSyntax != DicomTransferSyntax_LittleEndianImplicit)
      {
        // Default Transfer Syntax for DICOM
        preferred.push_back(DicomTransferSyntax_LittleEndianImplicit);
      }

      if (sourceSyntax != DicomTransferSyntax_LittleEndianExplicit)
      {
        preferred.push_back(DicomTransferSyntax_LittleEndianExplicit);
      }

      if (sourceSyntax != DicomTransferSyntax_BigEndianExplicit)
      {
        // Retired
        preferred.push_back(DicomTransferSyntax_BigEndianExplicit);
      }
    }


    /**
     * 3. Lookup whether one of the preferred transfer syntaxes was
     * accepted.
     **/
    
    for (std::list<DicomTransferSyntax>::const_iterator
           it = preferred.begin(); it != preferred.end(); ++it)
    {
      Accepted::const_iterator found = accepted.find(*it);
      if (found != accepted.end())
      {
        selectedPresentationId = found->second;
        selectedSyntax = *it;
        return true;
      }
    }

    // No preferred syntax was accepted
    return false;
  }                                                           


  bool OrthancGetRequestHandler::PerformGetSubOp(T_ASC_Association* assoc,
                                                 const std::string& sopClassUid,
                                                 const std::string& sopInstanceUid,
                                                 DcmFileFormat* dicomRaw)
  {
    assert(dicomRaw != NULL);
    std::unique_ptr<DcmFileFormat> dicom(dicomRaw);
    
    DicomTransferSyntax sourceSyntax;
    if (!FromDcmtkBridge::LookupOrthancTransferSyntax(sourceSyntax, *dicom))
    {
      failedCount_++;
      AddFailedUIDInstance(sopInstanceUid);
      throw OrthancException(ErrorCode_NetworkProtocol, 
                             "C-GET SCP: Unknown transfer syntax: (" +
                             std::string(dcmSOPClassUIDToModality(sopClassUid.c_str(), "OT")) +
                             ") " + sopClassUid);
    }

    T_ASC_PresentationContextID presId = 0;  // Unnecessary initialization, makes code clearer
    DicomTransferSyntax selectedSyntax;
    if (!SelectPresentationContext(presId, selectedSyntax, assoc, sopClassUid,
                                   sourceSyntax, allowTranscoding_) ||
        presId == 0)
    {
      failedCount_++;
      AddFailedUIDInstance(sopInstanceUid);
      throw OrthancException(ErrorCode_NetworkProtocol,
                             "C-GET SCP: storeSCU: No presentation context for: (" +
                             std::string(dcmSOPClassUIDToModality(sopClassUid.c_str(), "OT")) +
                             ") " + sopClassUid);
    }
    else
    {
      CLOG(INFO, DICOM) << "C-GET SCP selected transfer syntax " << GetTransferSyntaxUid(selectedSyntax)
                        << ", for source instance with SOP class " << sopClassUid
                        << " and transfer syntax " << GetTransferSyntaxUid(sourceSyntax);

      // make sure that we can send images in this presentation context
      T_ASC_PresentationContext pc;
      ASC_findAcceptedPresentationContext(assoc->params, presId, &pc);
      // the acceptedRole is the association requestor role

      if (pc.acceptedRole != ASC_SC_ROLE_DEFAULT &&  // "DEFAULT" is necessary for GinkgoCADx
          pc.acceptedRole != ASC_SC_ROLE_SCP &&
          pc.acceptedRole != ASC_SC_ROLE_SCUSCP)
      {
        // the role is not appropriate
        failedCount_++;
        AddFailedUIDInstance(sopInstanceUid);
        throw OrthancException(ErrorCode_NetworkProtocol,
                               "C-GET SCP: storeSCU: [No presentation context with requestor SCP role for: (" +
                               std::string(dcmSOPClassUIDToModality(sopClassUid.c_str(), "OT")) +
                               ") " + sopClassUid);
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

    CLOG(INFO, DICOM) << "Store SCU RQ: MsgID " << msgId << ", ("
                      << dcmSOPClassUIDToModality(sopClassUid.c_str(), "OT") << ")";
    
    T_DIMSE_DetectedCancelParameters cancelParameters;
    memset(&cancelParameters, 0, sizeof(cancelParameters));

    std::unique_ptr<DcmDataset> stDetail;

    OFCondition cond;

    if (sourceSyntax == selectedSyntax)
    {
      // No transcoding is required
      DcmDataset *stDetailTmp = NULL;
      cond = DIMSE_storeUser(
        assoc, presId, &req, NULL /* imageFileName */, dicom->getDataset(),
        ProgressCallback, NULL /* callbackData */,
        (timeout_ > 0 ? DIMSE_NONBLOCKING : DIMSE_BLOCKING), timeout_,
        &rsp, &stDetailTmp, &cancelParameters);
      stDetail.reset(stDetailTmp);
    }
    else
    {
      // Transcoding to the selected uncompressed transfer syntax
      IDicomTranscoder::DicomImage source, transcoded;
      source.AcquireParsed(dicom.release());

      std::set<DicomTransferSyntax> ts;
      ts.insert(selectedSyntax);
      
      if (context_.Transcode(transcoded, source, ts, true))
      {
        // Transcoding has succeeded
        DcmDataset *stDetailTmp = NULL;
        cond = DIMSE_storeUser(
          assoc, presId, &req, NULL /* imageFileName */,
          transcoded.GetParsed().getDataset(),
          ProgressCallback, NULL /* callbackData */,
          (timeout_ > 0 ? DIMSE_NONBLOCKING : DIMSE_BLOCKING), timeout_,
          &rsp, &stDetailTmp, &cancelParameters);
        stDetail.reset(stDetailTmp);
      }
      else
      {
        // Cannot transcode
        failedCount_++;
        AddFailedUIDInstance(sopInstanceUid);
        throw OrthancException(ErrorCode_NotImplemented,
                               "C-GET SCP: Cannot transcode " + sopClassUid +
                               " from transfer syntax " + GetTransferSyntaxUid(sourceSyntax) +
                               " to " + GetTransferSyntaxUid(selectedSyntax));
      }      
    }

    bool isContinue;
    
    if (cond.good())
    {
      {
        OFString str;
        CLOG(TRACE, DICOM) << "Received Store Response following a C-GET:" << std::endl
                           << DIMSE_dumpMessage(str, rsp, DIMSE_INCOMING);
      }
      
      if (cancelParameters.cancelEncountered)
      {
        CLOG(INFO, DICOM) << "C-GET SCP: Received C-Cancel RQ";
        isContinue = false;
      }
      else if (rsp.DimseStatus == STATUS_Success)
      {
        // everything ok
        completedCount_++;
        isContinue = true;
      }
      else if ((rsp.DimseStatus & 0xf000) == 0xb000)
      {
        // a warning status message
        warningCount_++;
        CLOG(ERROR, DICOM) << "C-GET SCP: Store Warning: Response Status: "
                           << DU_cstoreStatusString(rsp.DimseStatus);
        isContinue = true;
      }
      else
      {
        failedCount_++;
        AddFailedUIDInstance(sopInstanceUid);
        // print a status message
        CLOG(ERROR, DICOM) << "C-GET SCP: Store Failed: Response Status: "
                           << DU_cstoreStatusString(rsp.DimseStatus);
        isContinue = true;
      }
    }
    else
    {
      failedCount_++;
      AddFailedUIDInstance(sopInstanceUid);
      OFString temp_str;
      CLOG(ERROR, DICOM) << "C-GET SCP: storeSCU: Store Request Failed: "
                         << DimseCondition::dump(temp_str, cond);
      isContinue = true;
    }
    
    if (stDetail.get() != NULL)
    {
      std::stringstream s;  // DcmObject::PrintHelper cannot be used with VS2008
      stDetail->print(s);
      CLOG(INFO, DICOM) << "  Status Detail: " << s.str();
    }
    
    return isContinue;
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
          CLOG(ERROR, DICOM) << "C-GET: Cannot locate resource \"" << tokens[i]
                             << "\" at the " << EnumerationToString(level) << " level";
          return false;
        }
        else
        {
          for (size_t j = 0; j < tmp.size(); j++)
          {
            publicIds.push_back(tmp[j]);
          }
        }
      }

      return true;      
    }
  }


  OrthancGetRequestHandler::OrthancGetRequestHandler(ServerContext& context) :
    context_(context),
    position_(0),
    completedCount_ (0),
    warningCount_(0),
    failedCount_(0),
    timeout_(0),
    allowTranscoding_(false)
  {
  }


  bool OrthancGetRequestHandler::Handle(const DicomMap& input,
                                        const std::string& originatorIp,
                                        const std::string& originatorAet,
                                        const std::string& calledAet,
                                        uint32_t timeout)
  {
    MetricsRegistry::Timer timer(context_.GetMetricsRegistry(), "orthanc_get_scp_duration_ms");

    CLOG(WARNING, DICOM) << "C-GET-SCU request received from AET \"" << originatorAet << "\"";

    {
      DicomArray query(input);
      for (size_t i = 0; i < query.GetSize(); i++)
      {
        if (!query.GetElement(i).GetValue().IsNull())
        {
          CLOG(INFO, DICOM) << "  (" << query.GetElement(i).GetTag().Format()
                            << ")  " << FromDcmtkBridge::GetTagName(query.GetElement(i))
                            << " = " << context_.GetDeidentifiedContent(query.GetElement(i));
        }
      }
    }

    /**
     * Retrieve the query level.
     **/

    const DicomValue* levelTmp = input.TestAndGetValue(DICOM_TAG_QUERY_RETRIEVE_LEVEL);
    if (levelTmp == NULL ||
        levelTmp->IsNull() ||
        levelTmp->IsBinary())
    {
      throw OrthancException(ErrorCode_BadRequest,
                             "C-GET request without the tag 0008,0052 (QueryRetrieveLevel)");
    }

    ResourceType level = StringToResourceType(levelTmp->GetContent().c_str());      


    /**
     * Lookup for the resource to be sent.
     **/

    std::list<std::string> publicIds;

    if (!LookupIdentifiers(publicIds, level, input))
    {
      CLOG(ERROR, DICOM) << "Cannot determine what resources are requested by C-GET";
      return false; 
    }

    localAet_ = context_.GetDefaultLocalApplicationEntityTitle();
    position_ = 0;
    originatorAet_ = originatorAet;
    
    {
      OrthancConfiguration::ReaderLock lock;

      RemoteModalityParameters remote;

      if (lock.GetConfiguration().LookupDicomModalityUsingAETitle(remote, originatorAet))
      {
        allowTranscoding_ = (context_.IsTranscodeDicomProtocol() &&
                             remote.IsTranscodingAllowed());
      }
      else if (lock.GetConfiguration().GetBooleanParameter("DicomAlwaysAllowGet", false))
      {
        CLOG(INFO, DICOM) << "C-GET: Allowing SCU request from unknown modality with AET: " << originatorAet;
        allowTranscoding_ = context_.IsTranscodeDicomProtocol();
      }
      else
      {
        // This should never happen, given the test at bottom of
        // "OrthancApplicationEntityFilter::IsAllowedRequest()"
        throw OrthancException(ErrorCode_InexistentItem,
                               "C-GET: Rejecting SCU request from unknown modality with AET: " + originatorAet);
      }
    }

    for (std::list<std::string>::const_iterator
           resource = publicIds.begin(); resource != publicIds.end(); ++resource)
    {
      CLOG(INFO, DICOM) << "C-GET: Sending resource " << *resource
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

    completedCount_ = 0;
    failedCount_ = 0;
    warningCount_ = 0;
    timeout_ = timeout;

    return true;
  }
};
