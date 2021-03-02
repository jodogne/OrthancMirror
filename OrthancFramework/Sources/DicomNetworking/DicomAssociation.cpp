/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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
#include "DicomAssociation.h"

#if !defined(DCMTK_VERSION_NUMBER)
#  error The macro DCMTK_VERSION_NUMBER must be defined
#endif

#include "../Compatibility.h"
#include "../Logging.h"
#include "../OrthancException.h"
#include "NetworkingCompatibility.h"

#include <dcmtk/dcmnet/diutil.h>  // For dcmConnectionTimeout()
#include <dcmtk/dcmdata/dcdeftag.h>

namespace Orthanc
{
  static void FillSopSequence(DcmDataset& dataset,
                              const DcmTagKey& tag,
                              const std::vector<std::string>& sopClassUids,
                              const std::vector<std::string>& sopInstanceUids,
                              const std::vector<StorageCommitmentFailureReason>& failureReasons,
                              bool hasFailureReasons)
  {
    assert(sopClassUids.size() == sopInstanceUids.size() &&
           (hasFailureReasons ?
            failureReasons.size() == sopClassUids.size() :
            failureReasons.empty()));

    if (sopInstanceUids.empty())
    {
      // Add an empty sequence
      if (!dataset.insertEmptyElement(tag).good())
      {
        throw OrthancException(ErrorCode_InternalError);
      }
    }
    else
    {
      for (size_t i = 0; i < sopClassUids.size(); i++)
      {
        std::unique_ptr<DcmItem> item(new DcmItem);
        if (!item->putAndInsertString(DCM_ReferencedSOPClassUID, sopClassUids[i].c_str()).good() ||
            !item->putAndInsertString(DCM_ReferencedSOPInstanceUID, sopInstanceUids[i].c_str()).good() ||
            (hasFailureReasons &&
             !item->putAndInsertUint16(DCM_FailureReason, failureReasons[i]).good()) ||
            !dataset.insertSequenceItem(tag, item.release()).good())
        {
          throw OrthancException(ErrorCode_InternalError);
        }
      }
    }
  }                              


  void DicomAssociation::Initialize()
  {
    role_ = DicomAssociationRole_Default;
    isOpen_ = false;
    net_ = NULL; 
    params_ = NULL;
    assoc_ = NULL;      

    // Must be after "isOpen_ = false"
    ClearPresentationContexts();
  }

    
  void DicomAssociation::CheckConnecting(const DicomAssociationParameters& parameters,
                                         const OFCondition& cond)
  {
    try
    {
      if (cond.bad() &&
          cond == DUL_ASSOCIATIONREJECTED)
      {
        T_ASC_RejectParameters rej;
        ASC_getRejectParameters(params_, &rej);

        OFString str;
        CLOG(TRACE, DICOM) << "Association Rejected:" << std::endl
                           << ASC_printRejectParameters(str, &rej);
      }
      
      CheckCondition(cond, parameters, "connecting");
    }
    catch (OrthancException&)
    {
      CloseInternal();
      throw;
    }
  }

    
  void DicomAssociation::CloseInternal()
  {
#if ORTHANC_ENABLE_SSL == 1
    tls_.reset(NULL);  // Transport layer must be destroyed before the association itself
#endif
    
    if (assoc_ != NULL)
    {
      ASC_releaseAssociation(assoc_);
      ASC_destroyAssociation(&assoc_);
      assoc_ = NULL;
      params_ = NULL;
    }
    else
    {
      if (params_ != NULL)
      {
        ASC_destroyAssociationParameters(&params_);
        params_ = NULL;
      }
    }

    if (net_ != NULL)
    {
      ASC_dropNetwork(&net_);
      net_ = NULL;
    }

    accepted_.clear();
    isOpen_ = false;
  }

    
  void DicomAssociation::AddAccepted(const std::string& abstractSyntax,
                                     DicomTransferSyntax syntax,
                                     uint8_t presentationContextId)
  {
    AcceptedPresentationContexts::iterator found = accepted_.find(abstractSyntax);

    if (found == accepted_.end())
    {
      std::map<DicomTransferSyntax, uint8_t> syntaxes;
      syntaxes[syntax] = presentationContextId;
      accepted_[abstractSyntax] = syntaxes;
    }      
    else
    {
      if (found->second.find(syntax) != found->second.end())
      {
        CLOG(WARNING, DICOM) << "The same transfer syntax ("
                             << GetTransferSyntaxUid(syntax)
                             << ") was accepted twice for the same abstract syntax UID ("
                             << abstractSyntax << ")";
      }
      else
      {
        found->second[syntax] = presentationContextId;
      }
    }
  }


  DicomAssociation::~DicomAssociation()
  {
    try
    {
      Close();
    }
    catch (OrthancException& e)
    {
      // Don't throw exception in destructors
      CLOG(ERROR, DICOM) << "Error while destroying a DICOM association: " << e.What();
    }
  }


  void DicomAssociation::SetRole(DicomAssociationRole role)
  {
    if (role_ != role)
    {
      Close();
      role_ = role;
    }
  }

  
  void DicomAssociation::ClearPresentationContexts()
  {
    Close();
    proposed_.clear();
    proposed_.reserve(MAX_PROPOSED_PRESENTATIONS);
  }

  
  void DicomAssociation::Open(const DicomAssociationParameters& parameters)
  {
    if (isOpen_)
    {
      return;  // Already open
    }

    // Timeout used during association negociation and ASC_releaseAssociation()
    uint32_t acseTimeout = parameters.GetTimeout();
    if (acseTimeout == 0)
    {
      /**
       * Timeout is disabled. Global timeout (seconds) for
       * connecting to remote hosts.  Default value is -1 which
       * selects infinite timeout, i.e. blocking connect().
       **/
      dcmConnectionTimeout.set(-1);
      acseTimeout = 10;
    }
    else
    {
      dcmConnectionTimeout.set(acseTimeout);
    }
      
    T_ASC_SC_ROLE dcmtkRole;
    switch (role_)
    {
      case DicomAssociationRole_Default:
        dcmtkRole = ASC_SC_ROLE_DEFAULT;
        break;

      case DicomAssociationRole_Scu:
        dcmtkRole = ASC_SC_ROLE_SCU;
        break;

      case DicomAssociationRole_Scp:
        dcmtkRole = ASC_SC_ROLE_SCP;
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    assert(net_ == NULL &&
           params_ == NULL &&
           assoc_ == NULL);

#if ORTHANC_ENABLE_SSL == 1
    assert(tls_.get() == NULL);
#endif

    if (proposed_.empty())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls,
                             "No presentation context was proposed");
    }

    std::string localAet = parameters.GetLocalApplicationEntityTitle();
    if (parameters.GetRemoteModality().HasLocalAet())
    {
      localAet = parameters.GetRemoteModality().GetLocalAet();
    }

    CLOG(INFO, DICOM) << "Opening a DICOM SCU connection "
                      << (parameters.GetRemoteModality().IsDicomTlsEnabled() ? "using DICOM TLS" : "without DICOM TLS")
                      << " from AET \"" << localAet
                      << "\" to AET \"" << parameters.GetRemoteModality().GetApplicationEntityTitle()
                      << "\" on host " << parameters.GetRemoteModality().GetHost()
                      << ":" << parameters.GetRemoteModality().GetPortNumber() 
                      << " (manufacturer: " << EnumerationToString(parameters.GetRemoteModality().GetManufacturer())
                      << ", " << (parameters.HasTimeout() ?
                                  "timeout: " + boost::lexical_cast<std::string>(parameters.GetTimeout()) + "s" :
                                  "no timeout") << ")";

    CheckConnecting(parameters, ASC_initializeNetwork(NET_REQUESTOR, 0, /*opt_acse_timeout*/ acseTimeout, &net_));
    CheckConnecting(parameters, ASC_createAssociationParameters(&params_, parameters.GetMaximumPduLength()));

#if ORTHANC_ENABLE_SSL == 1
    if (parameters.GetRemoteModality().IsDicomTlsEnabled())
    {
      try
      {
        assert(net_ != NULL &&
               params_ != NULL);
        
        tls_.reset(Internals::InitializeDicomTls(net_, NET_REQUESTOR, parameters.GetOwnPrivateKeyPath(),
                                                 parameters.GetOwnCertificatePath(),
                                                 parameters.GetTrustedCertificatesPath()));
      }
      catch (OrthancException&)
      {
        CloseInternal();
        throw;
      }
    }
#endif

    // Set this application's title and the called application's title in the params
    CheckConnecting(parameters, ASC_setAPTitles(
                      params_, localAet.c_str(),
                      parameters.GetRemoteModality().GetApplicationEntityTitle().c_str(), NULL));

    // Set the network addresses of the local and remote entities
    char localHost[HOST_NAME_MAX];
    gethostname(localHost, HOST_NAME_MAX - 1);

    char remoteHostAndPort[HOST_NAME_MAX];

#ifdef _MSC_VER
    _snprintf
#else
      snprintf
#endif
      (remoteHostAndPort, HOST_NAME_MAX - 1, "%s:%d",
       parameters.GetRemoteModality().GetHost().c_str(),
       parameters.GetRemoteModality().GetPortNumber());

    CheckConnecting(parameters, ASC_setPresentationAddresses(params_, localHost, remoteHostAndPort));

    // Set various options
#if ORTHANC_ENABLE_SSL == 1
    CheckConnecting(parameters, ASC_setTransportLayerType(params_, (tls_.get() != NULL) /*opt_secureConnection*/));
#else
    CheckConnecting(parameters, ASC_setTransportLayerType(params_, false /*opt_secureConnection*/));
#endif

    // Setup the list of proposed presentation contexts
    unsigned int presentationContextId = 1;
    for (size_t i = 0; i < proposed_.size(); i++)
    {
      assert(presentationContextId <= 255);
      const char* abstractSyntax = proposed_[i].abstractSyntax_.c_str();

      const std::set<DicomTransferSyntax>& source = proposed_[i].transferSyntaxes_;
          
      std::vector<const char*> transferSyntaxes;
      transferSyntaxes.reserve(source.size());
          
      for (std::set<DicomTransferSyntax>::const_iterator
             it = source.begin(); it != source.end(); ++it)
      {
        transferSyntaxes.push_back(GetTransferSyntaxUid(*it));
      }

      assert(!transferSyntaxes.empty());
      CheckConnecting(parameters, ASC_addPresentationContext(
                        params_, presentationContextId, abstractSyntax,
                        &transferSyntaxes[0], transferSyntaxes.size(), dcmtkRole));

      presentationContextId += 2;
    }

    {
      OFString str;
      CLOG(TRACE, DICOM) << "Request Parameters:" << std::endl
                         << ASC_dumpParameters(str, params_, ASC_ASSOC_RQ);
    }

    // Do the association
    CheckConnecting(parameters, ASC_requestAssociation(net_, params_, &assoc_));
    isOpen_ = true;

    {
      OFString str;
      CLOG(TRACE, DICOM) << "Connection Parameters: "
                         << ASC_dumpConnectionParameters(str, assoc_);
      CLOG(TRACE, DICOM) << "Association Parameters Negotiated:" << std::endl
                         << ASC_dumpParameters(str, params_, ASC_ASSOC_AC);
    }


    // Inspect the accepted transfer syntaxes
    LST_HEAD **l = &params_->DULparams.acceptedPresentationContext;
    if (*l != NULL)
    {
      DUL_PRESENTATIONCONTEXT* pc = (DUL_PRESENTATIONCONTEXT*) LST_Head(l);
      LST_Position(l, (LST_NODE*)pc);
      while (pc)
      {
        if (pc->result == ASC_P_ACCEPTANCE)
        {
          DicomTransferSyntax transferSyntax;
          if (LookupTransferSyntax(transferSyntax, pc->acceptedTransferSyntax))
          {
            AddAccepted(pc->abstractSyntax, transferSyntax, pc->presentationContextID);
          }
          else
          {
            CLOG(WARNING, DICOM) << "Unknown transfer syntax received from AET \""
                                 << parameters.GetRemoteModality().GetApplicationEntityTitle()
                                 << "\": " << pc->acceptedTransferSyntax;
          }
        }
            
        pc = (DUL_PRESENTATIONCONTEXT*) LST_Next(l);
      }
    }

    if (accepted_.empty())
    {
      throw OrthancException(ErrorCode_NoPresentationContext,
                             "Unable to negotiate a presentation context with AET \"" +
                             parameters.GetRemoteModality().GetApplicationEntityTitle() + "\"");
    }
  }

  void DicomAssociation::Close()
  {
    if (isOpen_)
    {
      CloseInternal();
    }
  }

    
  bool DicomAssociation::LookupAcceptedPresentationContext(std::map<DicomTransferSyntax, uint8_t>& target,
                                                           const std::string& abstractSyntax) const
  {
    if (!IsOpen())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls, "Connection not opened");
    }
      
    AcceptedPresentationContexts::const_iterator found = accepted_.find(abstractSyntax);

    if (found == accepted_.end())
    {
      return false;
    }
    else
    {
      target = found->second;
      return true;
    }
  }

    
  void DicomAssociation::ProposeGenericPresentationContext(const std::string& abstractSyntax)
  {
    std::set<DicomTransferSyntax> ts;
    ts.insert(DicomTransferSyntax_LittleEndianImplicit);
    ts.insert(DicomTransferSyntax_LittleEndianExplicit);
    ts.insert(DicomTransferSyntax_BigEndianExplicit);  // Retired
    ProposePresentationContext(abstractSyntax, ts);
  }

    
  void DicomAssociation::ProposePresentationContext(const std::string& abstractSyntax,
                                                    DicomTransferSyntax transferSyntax)
  {
    std::set<DicomTransferSyntax> ts;
    ts.insert(transferSyntax);
    ProposePresentationContext(abstractSyntax, ts);
  }

    
  size_t DicomAssociation::GetRemainingPropositions() const
  {
    assert(proposed_.size() <= MAX_PROPOSED_PRESENTATIONS);
    return MAX_PROPOSED_PRESENTATIONS - proposed_.size();
  }
    

  void DicomAssociation::ProposePresentationContext(
    const std::string& abstractSyntax,
    const std::set<DicomTransferSyntax>& transferSyntaxes)
  {
    if (transferSyntaxes.empty())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange,
                             "No transfer syntax provided");
    }
      
    if (proposed_.size() >= MAX_PROPOSED_PRESENTATIONS)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange,
                             "Too many proposed presentation contexts");
    }
      
    if (IsOpen())
    {
      Close();
    }

    ProposedPresentationContext context;
    context.abstractSyntax_ = abstractSyntax;
    context.transferSyntaxes_ = transferSyntaxes;

    proposed_.push_back(context);
  }

    
  T_ASC_Association& DicomAssociation::GetDcmtkAssociation() const
  {
    if (isOpen_)
    {
      assert(assoc_ != NULL);
      return *assoc_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls,
                             "The connection is not open");
    }
  }

    
  T_ASC_Network& DicomAssociation::GetDcmtkNetwork() const
  {
    if (isOpen_)
    {
      assert(net_ != NULL);
      return *net_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls,
                             "The connection is not open");
    }
  }

    
  void DicomAssociation::CheckCondition(const OFCondition& cond,
                                        const DicomAssociationParameters& parameters,
                                        const std::string& command)
  {
    if (cond.bad())
    {
      // Reformat the error message from DCMTK by turning multiline
      // errors into a single line
      
      std::string s(cond.text());
      std::string info;
      info.reserve(s.size());

      bool isMultiline = false;
      for (size_t i = 0; i < s.size(); i++)
      {
        if (s[i] == '\r')
        {
          // Ignore
        }
        else if (s[i] == '\n')
        {
          if (isMultiline)
          {
            info += "; ";
          }
          else
          {
            info += " (";
            isMultiline = true;
          }
        }
        else
        {
          info.push_back(s[i]);
        }
      }

      if (isMultiline)
      {
        info += ")";
      }

      throw OrthancException(ErrorCode_NetworkProtocol,
                             "DicomAssociation - " + command + " to AET \"" +
                             parameters.GetRemoteModality().GetApplicationEntityTitle() +
                             "\": " + info);
    }
  }
    

  void DicomAssociation::ReportStorageCommitment(
    const DicomAssociationParameters& parameters,
    const std::string& transactionUid,
    const std::vector<std::string>& sopClassUids,
    const std::vector<std::string>& sopInstanceUids,
    const std::vector<StorageCommitmentFailureReason>& failureReasons)
  {
    if (sopClassUids.size() != sopInstanceUids.size() ||
        sopClassUids.size() != failureReasons.size())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    

    std::vector<std::string> successSopClassUids, successSopInstanceUids, failedSopClassUids, failedSopInstanceUids;
    std::vector<StorageCommitmentFailureReason> failedReasons;

    successSopClassUids.reserve(sopClassUids.size());
    successSopInstanceUids.reserve(sopClassUids.size());
    failedSopClassUids.reserve(sopClassUids.size());
    failedSopInstanceUids.reserve(sopClassUids.size());
    failedReasons.reserve(sopClassUids.size());

    for (size_t i = 0; i < sopClassUids.size(); i++)
    {
      switch (failureReasons[i])
      {
        case StorageCommitmentFailureReason_Success:
          successSopClassUids.push_back(sopClassUids[i]);
          successSopInstanceUids.push_back(sopInstanceUids[i]);
          break;

        case StorageCommitmentFailureReason_ProcessingFailure:
        case StorageCommitmentFailureReason_NoSuchObjectInstance:
        case StorageCommitmentFailureReason_ResourceLimitation:
        case StorageCommitmentFailureReason_ReferencedSOPClassNotSupported:
        case StorageCommitmentFailureReason_ClassInstanceConflict:
        case StorageCommitmentFailureReason_DuplicateTransactionUID:
          failedSopClassUids.push_back(sopClassUids[i]);
          failedSopInstanceUids.push_back(sopInstanceUids[i]);
          failedReasons.push_back(failureReasons[i]);
          break;

        default:
        {
          char buf[16];
          sprintf(buf, "%04xH", failureReasons[i]);
          throw OrthancException(ErrorCode_ParameterOutOfRange,
                                 "Unsupported failure reason for storage commitment: " + std::string(buf));
        }
      }
    }
    
    DicomAssociation association;

    {
      std::set<DicomTransferSyntax> transferSyntaxes;
      transferSyntaxes.insert(DicomTransferSyntax_LittleEndianExplicit);
      transferSyntaxes.insert(DicomTransferSyntax_LittleEndianImplicit);

      association.SetRole(DicomAssociationRole_Scp);
      association.ProposePresentationContext(UID_StorageCommitmentPushModelSOPClass,
                                             transferSyntaxes);
    }
      
    association.Open(parameters);

    /**
     * N-EVENT-REPORT
     * http://dicom.nema.org/medical/dicom/2019a/output/chtml/part04/sect_J.3.3.html
     * http://dicom.nema.org/medical/dicom/2019a/output/chtml/part07/chapter_10.html#table_10.1-1
     *
     * Status code:
     * http://dicom.nema.org/medical/dicom/2019a/output/chtml/part07/chapter_10.html#sect_10.1.1.1.8
     **/

    /**
     * Send the "EVENT_REPORT_RQ" request
     **/

    CLOG(INFO, DICOM) << "Reporting modality \""
                      << parameters.GetRemoteModality().GetApplicationEntityTitle()
                      << "\" about storage commitment transaction: " << transactionUid
                      << " (" << successSopClassUids.size() << " successes, " 
                      << failedSopClassUids.size() << " failures)";
    const DIC_US messageId = association.GetDcmtkAssociation().nextMsgID++;
      
    {
      T_DIMSE_Message message;
      memset(&message, 0, sizeof(message));
      message.CommandField = DIMSE_N_EVENT_REPORT_RQ;

      T_DIMSE_N_EventReportRQ& content = message.msg.NEventReportRQ;
      content.MessageID = messageId;
      strncpy(content.AffectedSOPClassUID, UID_StorageCommitmentPushModelSOPClass, DIC_UI_LEN);
      strncpy(content.AffectedSOPInstanceUID, UID_StorageCommitmentPushModelSOPInstance, DIC_UI_LEN);
      content.DataSetType = DIMSE_DATASET_PRESENT;

      DcmDataset dataset;
      if (!dataset.putAndInsertString(DCM_TransactionUID, transactionUid.c_str()).good())
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      {
        std::vector<StorageCommitmentFailureReason> empty;
        FillSopSequence(dataset, DCM_ReferencedSOPSequence, successSopClassUids,
                        successSopInstanceUids, empty, false);
      }

      // http://dicom.nema.org/medical/dicom/2019a/output/chtml/part04/sect_J.3.3.html
      if (failedSopClassUids.empty())
      {
        content.EventTypeID = 1;  // "Storage Commitment Request Successful"
      }
      else
      {
        content.EventTypeID = 2;  // "Storage Commitment Request Complete - Failures Exist"

        // Failure reason
        // http://dicom.nema.org/medical/dicom/2019a/output/chtml/part03/sect_C.14.html#sect_C.14.1.1
        FillSopSequence(dataset, DCM_FailedSOPSequence, failedSopClassUids,
                        failedSopInstanceUids, failedReasons, true);
      }

      int presID = ASC_findAcceptedPresentationContextID(
        &association.GetDcmtkAssociation(), UID_StorageCommitmentPushModelSOPClass);
      if (presID == 0)
      {
        throw OrthancException(ErrorCode_NetworkProtocol, "Storage commitment - "
                               "Unable to send N-EVENT-REPORT request to AET: " +
                               parameters.GetRemoteModality().GetApplicationEntityTitle());
      }

      {
        std::stringstream s;  // DcmObject::PrintHelper cannot be used with VS2008
        dataset.print(s);

        OFString str;
        CLOG(TRACE, DICOM) << "Sending Storage Commitment Report:" << std::endl
                           << DIMSE_dumpMessage(str, message, DIMSE_OUTGOING) << std::endl
                           << s.str();
      }

      if (!DIMSE_sendMessageUsingMemoryData(
            &association.GetDcmtkAssociation(), presID, &message, NULL /* status detail */,
            &dataset, NULL /* callback */, NULL /* callback context */,
            NULL /* commandSet */).good())
      {
        throw OrthancException(ErrorCode_NetworkProtocol);
      }
    }

    /**
     * Read the "EVENT_REPORT_RSP" response
     **/

    {
      T_ASC_PresentationContextID presID = 0;
      T_DIMSE_Message message;

      if (!DIMSE_receiveCommand(&association.GetDcmtkAssociation(),
                                (parameters.HasTimeout() ? DIMSE_NONBLOCKING : DIMSE_BLOCKING),
                                parameters.GetTimeout(), &presID, &message,
                                NULL /* no statusDetail */).good() ||
          message.CommandField != DIMSE_N_EVENT_REPORT_RSP)
      {
        throw OrthancException(ErrorCode_NetworkProtocol, "Storage commitment - "
                               "Unable to read N-EVENT-REPORT response from AET: " +
                               parameters.GetRemoteModality().GetApplicationEntityTitle());
      }

      {
        OFString str;
        CLOG(TRACE, DICOM) << "Received Storage Commitment Report Response:" << std::endl
                           << DIMSE_dumpMessage(str, message, DIMSE_INCOMING, NULL, presID);
      }
      
      const T_DIMSE_N_EventReportRSP& content = message.msg.NEventReportRSP;
      if (content.MessageIDBeingRespondedTo != messageId ||
          !(content.opts & O_NEVENTREPORT_AFFECTEDSOPCLASSUID) ||
          !(content.opts & O_NEVENTREPORT_AFFECTEDSOPINSTANCEUID) ||
          //(content.opts & O_NEVENTREPORT_EVENTTYPEID) ||  // Pedantic test - The "content.EventTypeID" is not used by Orthanc
          std::string(content.AffectedSOPClassUID) != UID_StorageCommitmentPushModelSOPClass ||
          std::string(content.AffectedSOPInstanceUID) != UID_StorageCommitmentPushModelSOPInstance ||
          content.DataSetType != DIMSE_DATASET_NULL)
      {
        throw OrthancException(ErrorCode_NetworkProtocol, "Storage commitment - "
                               "Badly formatted N-EVENT-REPORT response from AET: " +
                               parameters.GetRemoteModality().GetApplicationEntityTitle());
      }

      if (content.DimseStatus != 0 /* success */)
      {
        throw OrthancException(ErrorCode_NetworkProtocol, "Storage commitment - "
                               "The request cannot be handled by remote AET: " +
                               parameters.GetRemoteModality().GetApplicationEntityTitle());
      }
    }

    association.Close();
  }

    
  void DicomAssociation::RequestStorageCommitment(
    const DicomAssociationParameters& parameters,
    const std::string& transactionUid,
    const std::vector<std::string>& sopClassUids,
    const std::vector<std::string>& sopInstanceUids)
  {
    if (sopClassUids.size() != sopInstanceUids.size())
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    for (size_t i = 0; i < sopClassUids.size(); i++)
    {
      if (sopClassUids[i].empty() ||
          sopInstanceUids[i].empty())
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange,
                               "The SOP class/instance UIDs cannot be empty, found: \"" +
                               sopClassUids[i] + "\" / \"" + sopInstanceUids[i] + "\"");
      }
    }

    if (transactionUid.size() < 5 ||
        transactionUid.substr(0, 5) != "2.25.")
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    DicomAssociation association;

    {
      std::set<DicomTransferSyntax> transferSyntaxes;
      transferSyntaxes.insert(DicomTransferSyntax_LittleEndianExplicit);
      transferSyntaxes.insert(DicomTransferSyntax_LittleEndianImplicit);
      
      association.SetRole(DicomAssociationRole_Default);
      association.ProposePresentationContext(UID_StorageCommitmentPushModelSOPClass,
                                             transferSyntaxes);
    }
      
    association.Open(parameters);
      
    /**
     * N-ACTION
     * http://dicom.nema.org/medical/dicom/2019a/output/chtml/part04/sect_J.3.2.html
     * http://dicom.nema.org/medical/dicom/2019a/output/chtml/part07/chapter_10.html#table_10.1-4
     *
     * Status code:
     * http://dicom.nema.org/medical/dicom/2019a/output/chtml/part07/chapter_10.html#sect_10.1.1.1.8
     **/

    /**
     * Send the "N_ACTION_RQ" request
     **/

    CLOG(INFO, DICOM) << "Request to modality \""
                      << parameters.GetRemoteModality().GetApplicationEntityTitle()
                      << "\" about storage commitment for " << sopClassUids.size()
                      << " instances, with transaction UID: " << transactionUid;
    const DIC_US messageId = association.GetDcmtkAssociation().nextMsgID++;
      
    {
      T_DIMSE_Message message;
      memset(&message, 0, sizeof(message));
      message.CommandField = DIMSE_N_ACTION_RQ;

      T_DIMSE_N_ActionRQ& content = message.msg.NActionRQ;
      content.MessageID = messageId;
      strncpy(content.RequestedSOPClassUID, UID_StorageCommitmentPushModelSOPClass, DIC_UI_LEN);
      strncpy(content.RequestedSOPInstanceUID, UID_StorageCommitmentPushModelSOPInstance, DIC_UI_LEN);
      content.ActionTypeID = 1;  // "Request Storage Commitment"
      content.DataSetType = DIMSE_DATASET_PRESENT;

      DcmDataset dataset;
      if (!dataset.putAndInsertString(DCM_TransactionUID, transactionUid.c_str()).good())
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      {
        std::vector<StorageCommitmentFailureReason> empty;
        FillSopSequence(dataset, DCM_ReferencedSOPSequence, sopClassUids, sopInstanceUids, empty, false);
      }
          
      int presID = ASC_findAcceptedPresentationContextID(
        &association.GetDcmtkAssociation(), UID_StorageCommitmentPushModelSOPClass);
      if (presID == 0)
      {
        throw OrthancException(ErrorCode_NetworkProtocol, "Storage commitment - "
                               "Unable to send N-ACTION request to AET: " +
                               parameters.GetRemoteModality().GetApplicationEntityTitle());
      }

      {
        std::stringstream s;  // DcmObject::PrintHelper cannot be used with VS2008
        dataset.print(s);

        OFString str;
        CLOG(TRACE, DICOM) << "Sending Storage Commitment Request:" << std::endl
                           << DIMSE_dumpMessage(str, message, DIMSE_OUTGOING) << std::endl
                           << s.str();
      }

      if (!DIMSE_sendMessageUsingMemoryData(
            &association.GetDcmtkAssociation(), presID, &message, NULL /* status detail */,
            &dataset, NULL /* callback */, NULL /* callback context */,
            NULL /* commandSet */).good())
      {
        throw OrthancException(ErrorCode_NetworkProtocol);
      }
    }

    /**
     * Read the "N_ACTION_RSP" response
     **/

    {
      T_ASC_PresentationContextID presID = 0;
      T_DIMSE_Message message;
        
      if (!DIMSE_receiveCommand(&association.GetDcmtkAssociation(),
                                (parameters.HasTimeout() ? DIMSE_NONBLOCKING : DIMSE_BLOCKING),
                                parameters.GetTimeout(), &presID, &message,
                                NULL /* no statusDetail */).good() ||
          message.CommandField != DIMSE_N_ACTION_RSP)
      {
        throw OrthancException(ErrorCode_NetworkProtocol, "Storage commitment - "
                               "Unable to read N-ACTION response from AET: " +
                               parameters.GetRemoteModality().GetApplicationEntityTitle());
      }

      const T_DIMSE_N_ActionRSP& content = message.msg.NActionRSP;
      if (content.MessageIDBeingRespondedTo != messageId ||
          !(content.opts & O_NACTION_AFFECTEDSOPCLASSUID) ||
          !(content.opts & O_NACTION_AFFECTEDSOPINSTANCEUID) ||
          //(content.opts & O_NACTION_ACTIONTYPEID) ||  // Pedantic test - The "content.ActionTypeID" is not used by Orthanc
          std::string(content.AffectedSOPClassUID) != UID_StorageCommitmentPushModelSOPClass ||
          std::string(content.AffectedSOPInstanceUID) != UID_StorageCommitmentPushModelSOPInstance ||
          content.DataSetType != DIMSE_DATASET_NULL)
      {
        throw OrthancException(ErrorCode_NetworkProtocol, "Storage commitment - "
                               "Badly formatted N-ACTION response from AET: " +
                               parameters.GetRemoteModality().GetApplicationEntityTitle());
      }

      {
        OFString str;
        CLOG(TRACE, DICOM) << "Received Storage Commitment Request Response:" << std::endl
                           << DIMSE_dumpMessage(str, message, DIMSE_INCOMING, NULL, presID);
      }

      if (content.DimseStatus != 0 /* success */)
      {
        throw OrthancException(ErrorCode_NetworkProtocol, "Storage commitment - "
                               "The request cannot be handled by remote AET: " +
                               parameters.GetRemoteModality().GetApplicationEntityTitle());
      }
    }

    association.Close();
  }
}
