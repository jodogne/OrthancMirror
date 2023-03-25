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
#include "CommandDispatcher.h"

#if !defined(DCMTK_VERSION_NUMBER)
#  error The macro DCMTK_VERSION_NUMBER must be defined
#endif

#include "../../Compatibility.h"
#include "../../DicomParsing/FromDcmtkBridge.h"
#include "../../Logging.h"
#include "../../OrthancException.h"
#include "../../Toolbox.h"
#include "FindScp.h"
#include "GetScp.h"
#include "MoveScp.h"
#include "StoreScp.h"

#include <dcmtk/dcmdata/dcdeftag.h>     /* for storage commitment */
#include <dcmtk/dcmdata/dcsequen.h>     /* for class DcmSequenceOfItems */
#include <dcmtk/dcmdata/dcuid.h>        /* for variable dcmAllStorageSOPClassUIDs */
#include <dcmtk/dcmnet/dcasccfg.h>      /* for class DcmAssociationConfiguration */

#include <boost/lexical_cast.hpp>

static OFBool    opt_rejectWithoutImplementationUID = OFFalse;



static DUL_PRESENTATIONCONTEXT *
findPresentationContextID(LST_HEAD * head,
                          T_ASC_PresentationContextID presentationContextID)
{
  DUL_PRESENTATIONCONTEXT *pc;
  LST_HEAD **l;
  OFBool found = OFFalse;

  if (head == NULL)
    return NULL;

  l = &head;
  if (*l == NULL)
    return NULL;

  pc = OFstatic_cast(DUL_PRESENTATIONCONTEXT *, LST_Head(l));
  (void)LST_Position(l, OFstatic_cast(LST_NODE *, pc));

  while (pc && !found) {
    if (pc->presentationContextID == presentationContextID) {
      found = OFTrue;
    } else {
      pc = OFstatic_cast(DUL_PRESENTATIONCONTEXT *, LST_Next(l));
    }
  }
  return pc;
}


/** accept all presenstation contexts for unknown SOP classes,
 *  i.e. UIDs appearing in the list of abstract syntaxes
 *  where no corresponding name is defined in the UID dictionary.
 *  @param params pointer to association parameters structure
 *  @param transferSyntax transfer syntax to accept
 *  @param acceptedRole SCU/SCP role to accept
 */
static OFCondition acceptUnknownContextsWithTransferSyntax(
  T_ASC_Parameters * params,
  const char* transferSyntax,
  T_ASC_SC_ROLE acceptedRole)
{
  int n, i, k;
  DUL_PRESENTATIONCONTEXT *dpc;
  T_ASC_PresentationContext pc;

  n = ASC_countPresentationContexts(params);
  for (i = 0; i < n; i++)
  {
    OFCondition cond = ASC_getPresentationContext(params, i, &pc);
    if (cond.bad()) return cond;
    OFBool abstractOK = OFFalse;
    OFBool accepted = OFFalse;

    if (dcmFindNameOfUID(pc.abstractSyntax) == NULL)
    {
      abstractOK = OFTrue;

      /* check the transfer syntax */
      for (k = 0; (k < OFstatic_cast(int, pc.transferSyntaxCount)) && !accepted; k++)
      {
        if (strcmp(pc.proposedTransferSyntaxes[k], transferSyntax) == 0)
        {
          accepted = OFTrue;
        }
      }
    }
    
    if (accepted)
    {
      cond = ASC_acceptPresentationContext(
        params, pc.presentationContextID,
        transferSyntax, acceptedRole);
      if (cond.bad()) return cond;
    } else {
      T_ASC_P_ResultReason reason;

      /* do not refuse if already accepted */
      dpc = findPresentationContextID(params->DULparams.acceptedPresentationContext,
                                      pc.presentationContextID);
      if (dpc == NULL || dpc->result != ASC_P_ACCEPTANCE)
      {

        if (abstractOK) {
          reason = ASC_P_TRANSFERSYNTAXESNOTSUPPORTED;
        } else {
          reason = ASC_P_ABSTRACTSYNTAXNOTSUPPORTED;
        }
        /*
         * If previously this presentation context was refused
         * because of bad transfer syntax let it stay that way.
         */
        if ((dpc != NULL) && (dpc->result == ASC_P_TRANSFERSYNTAXESNOTSUPPORTED))
          reason = ASC_P_TRANSFERSYNTAXESNOTSUPPORTED;

        cond = ASC_refusePresentationContext(params, pc.presentationContextID, reason);
        if (cond.bad()) return cond;
      }
    }
  }
  return EC_Normal;
}


/** accept all presenstation contexts for unknown SOP classes,
 *  i.e. UIDs appearing in the list of abstract syntaxes
 *  where no corresponding name is defined in the UID dictionary.
 *  This method is passed a list of "preferred" transfer syntaxes.
 *  @param params pointer to association parameters structure
 *  @param transferSyntax transfer syntax to accept
 *  @param acceptedRole SCU/SCP role to accept
 */
static OFCondition acceptUnknownContextsWithPreferredTransferSyntaxes(
  T_ASC_Parameters * params,
  const char* transferSyntaxes[], int transferSyntaxCount,
  T_ASC_SC_ROLE acceptedRole)
{
  OFCondition cond = EC_Normal;
  /*
  ** Accept in the order "least wanted" to "most wanted" transfer
  ** syntax.  Accepting a transfer syntax will override previously
  ** accepted transfer syntaxes.
  */
  for (int i = transferSyntaxCount - 1; i >= 0; i--)
  {
    cond = acceptUnknownContextsWithTransferSyntax(params, transferSyntaxes[i], acceptedRole);
    if (cond.bad()) return cond;
  }
  return cond;
}



namespace Orthanc
{
  namespace Internals
  {
    OFCondition AssociationCleanup(T_ASC_Association *assoc)
    {
      OFCondition cond = ASC_dropSCPAssociation(assoc);
      if (cond.bad())
      {
        CLOG(ERROR, DICOM) << cond.text();
        return cond;
      }

      cond = ASC_destroyAssociation(&assoc);
      if (cond.bad())
      {
        CLOG(ERROR, DICOM) << cond.text();
        return cond;
      }

      return cond;
    }



    CommandDispatcher* AcceptAssociation(const DicomServer& server,
                                         T_ASC_Network *net,
                                         unsigned int maximumPduLength,
                                         bool useDicomTls)
    {
      DcmAssociationConfiguration asccfg;
      char buf[BUFSIZ];
      T_ASC_Association *assoc;
      OFCondition cond;
      OFString sprofile;
      OFString temp_str;

      cond = ASC_receiveAssociation(net, &assoc, maximumPduLength, NULL, NULL,
                                    useDicomTls /*opt_secureConnection*/,
                                    DUL_NOBLOCK, 1);

      if (cond == DUL_NOASSOCIATIONREQUEST)
      {
        // Timeout
        AssociationCleanup(assoc);
        return NULL;
      }

      // if some kind of error occured, take care of it
      if (cond.bad())
      {
        CLOG(ERROR, DICOM) << "Receiving Association failed: " << cond.text();
        // no matter what kind of error occurred, we need to do a cleanup
        AssociationCleanup(assoc);
        return NULL;
      }

      {
        OFString str;
        CLOG(TRACE, DICOM) << "Received Association Parameters:" << std::endl
                           << ASC_dumpParameters(str, assoc->params, ASC_ASSOC_RQ);
      }

      // Retrieve the AET and the IP address of the remote modality
      std::string remoteAet;
      std::string remoteIp;
      std::string calledAet;
  
      {
        DIC_AE remoteAet_C;
        DIC_AE calledAet_C;
        DIC_AE remoteIp_C;
        DIC_AE calledIP_C;

        if (
#if DCMTK_VERSION_NUMBER >= 364
          ASC_getAPTitles(assoc->params, remoteAet_C, sizeof(remoteAet_C), calledAet_C, sizeof(calledAet_C), NULL, 0).bad() ||
          ASC_getPresentationAddresses(assoc->params, remoteIp_C, sizeof(remoteIp_C), calledIP_C, sizeof(calledIP_C)).bad()
#else
          ASC_getAPTitles(assoc->params, remoteAet_C, calledAet_C, NULL).bad() ||
          ASC_getPresentationAddresses(assoc->params, remoteIp_C, calledIP_C).bad()
#endif
          )
        {
          T_ASC_RejectParameters rej =
            {
              ASC_RESULT_REJECTEDPERMANENT,
              ASC_SOURCE_SERVICEUSER,
              ASC_REASON_SU_NOREASON
            };
          ASC_rejectAssociation(assoc, &rej);
          AssociationCleanup(assoc);
          return NULL;
        }

        remoteIp = std::string(/*OFSTRING_GUARD*/(remoteIp_C));
        remoteAet = std::string(/*OFSTRING_GUARD*/(remoteAet_C));
        calledAet = (/*OFSTRING_GUARD*/(calledAet_C));
      }

      CLOG(INFO, DICOM) << "Association Received from AET " << remoteAet 
                        << " on IP " << remoteIp;


      {
        /* accept the abstract syntaxes for C-ECHO, C-FIND, C-MOVE,
           and storage commitment, if presented */

        std::vector<const char*> genericTransferSyntaxes;
        genericTransferSyntaxes.push_back(UID_LittleEndianExplicitTransferSyntax);
        genericTransferSyntaxes.push_back(UID_BigEndianExplicitTransferSyntax);
        genericTransferSyntaxes.push_back(UID_LittleEndianImplicitTransferSyntax);

        std::vector<const char*> knownAbstractSyntaxes;

        // For C-ECHO (always enabled since Orthanc 1.6.0; in earlier
        // versions, only enabled if C-STORE was also enabled)
        knownAbstractSyntaxes.push_back(UID_VerificationSOPClass);

        // For C-FIND
        if (server.HasFindRequestHandlerFactory())
        {
          knownAbstractSyntaxes.push_back(UID_FINDPatientRootQueryRetrieveInformationModel);
          knownAbstractSyntaxes.push_back(UID_FINDStudyRootQueryRetrieveInformationModel);
        }

        if (server.HasWorklistRequestHandlerFactory())
        {
          knownAbstractSyntaxes.push_back(UID_FINDModalityWorklistInformationModel);
        }

        // For C-MOVE
        if (server.HasMoveRequestHandlerFactory())
        {
          knownAbstractSyntaxes.push_back(UID_MOVEStudyRootQueryRetrieveInformationModel);
          knownAbstractSyntaxes.push_back(UID_MOVEPatientRootQueryRetrieveInformationModel);
        }

        // For C-GET
        if (server.HasGetRequestHandlerFactory())
        {
          knownAbstractSyntaxes.push_back(UID_GETStudyRootQueryRetrieveInformationModel);
          knownAbstractSyntaxes.push_back(UID_GETPatientRootQueryRetrieveInformationModel);
        }

        cond = ASC_acceptContextsWithPreferredTransferSyntaxes(
          assoc->params,
          &knownAbstractSyntaxes[0], knownAbstractSyntaxes.size(),
          &genericTransferSyntaxes[0], genericTransferSyntaxes.size());
        if (cond.bad())
        {
          CLOG(INFO, DICOM) << cond.text();
          AssociationCleanup(assoc);
          return NULL;
        }

      
        /* storage commitment support, new in Orthanc 1.6.0 */
        if (server.HasStorageCommitmentRequestHandlerFactory())
        {
          /**
           * "ASC_SC_ROLE_SCUSCP": The "SCU" role is needed to accept
           * remote storage commitment requests, and the "SCP" role is
           * needed to receive storage commitments answers.
           **/        
          const char* as[1] = { UID_StorageCommitmentPushModelSOPClass }; 
          cond = ASC_acceptContextsWithPreferredTransferSyntaxes(
            assoc->params, as, 1,
            &genericTransferSyntaxes[0], genericTransferSyntaxes.size(), ASC_SC_ROLE_SCUSCP);
          if (cond.bad())
          {
            CLOG(INFO, DICOM) << cond.text();
            AssociationCleanup(assoc);
            return NULL;
          }
        }
      }
      

      {
        /* accept the abstract syntaxes for C-STORE, if presented */
        
        std::set<DicomTransferSyntax> storageTransferSyntaxes;

        if (server.HasApplicationEntityFilter())
        {
          server.GetApplicationEntityFilter().GetAcceptedTransferSyntaxes(
            storageTransferSyntaxes, remoteIp, remoteAet, calledAet);
        }
        else
        {
          /**
           * In the absence of filter, accept all the known transfer
           * syntaxes. Note that this is different from Orthanc
           * framework <= 1.8.2, where only the uncompressed transfer
           * syntaxes are accepted by default.
           **/
          GetAllDicomTransferSyntaxes(storageTransferSyntaxes);
        }

        if (storageTransferSyntaxes.empty())
        {
          LOG(WARNING) << "The DICOM server accepts no transfer syntax, thus C-STORE SCP is disabled";
        }
        else
        {
          /**
           * If accepted, put "Little Endian Explicit" at the first
           * place in the accepted transfer syntaxes. This first place
           * has an impact on the result of "getscu" (cf. integration
           * test "test_getscu"). We choose "Little Endian Explicit",
           * as this preserves the VR of the private tags, even if the
           * remote modality doesn't have the dictionary of private tags.
           *
           * TODO - Should "PREFERRED_TRANSFER_SYNTAX" be an option of
           * class "DicomServer"?
           **/
          const DicomTransferSyntax PREFERRED_TRANSFER_SYNTAX = DicomTransferSyntax_LittleEndianExplicit;

          E_TransferSyntax dummy;
          assert(FromDcmtkBridge::LookupDcmtkTransferSyntax(dummy, PREFERRED_TRANSFER_SYNTAX));

          std::vector<const char*> storageTransferSyntaxesC;
          storageTransferSyntaxesC.reserve(storageTransferSyntaxes.size());

          if (storageTransferSyntaxes.find(PREFERRED_TRANSFER_SYNTAX) != storageTransferSyntaxes.end())
          {
            storageTransferSyntaxesC.push_back(GetTransferSyntaxUid(PREFERRED_TRANSFER_SYNTAX));
          }
          
          for (std::set<DicomTransferSyntax>::const_iterator
                 syntax = storageTransferSyntaxes.begin(); syntax != storageTransferSyntaxes.end(); ++syntax)
          {
            if (*syntax != PREFERRED_TRANSFER_SYNTAX &&  // Don't add the preferred transfer syntax twice
                // Make sure that the current version of DCMTK knows this transfer syntax
                FromDcmtkBridge::LookupDcmtkTransferSyntax(dummy, *syntax))
            {
              storageTransferSyntaxesC.push_back(GetTransferSyntaxUid(*syntax));
            }
          }

          /* the array of Storage SOP Class UIDs that is defined within "dcmdata/libsrc/dcuid.cc" */
          size_t count = 0;
          while (dcmAllStorageSOPClassUIDs[count] != NULL)
          {
            count++;
          }
        
#if DCMTK_VERSION_NUMBER >= 362
          // The global variable "numberOfDcmAllStorageSOPClassUIDs" is
          // only published if DCMTK >= 3.6.2:
          // https://bugs.orthanc-server.com/show_bug.cgi?id=137
          assert(static_cast<int>(count) == numberOfDcmAllStorageSOPClassUIDs);
#endif
      
          if (!server.HasGetRequestHandlerFactory())    // dcmqrsrv.cc line 828
          {
            // This branch exactly corresponds to Orthanc <= 1.6.1 (in
            // which C-GET SCP was not supported)
            cond = ASC_acceptContextsWithPreferredTransferSyntaxes(
              assoc->params, dcmAllStorageSOPClassUIDs, count,
              &storageTransferSyntaxesC[0], storageTransferSyntaxesC.size());
            if (cond.bad())
            {
              CLOG(INFO, DICOM) << cond.text();
              AssociationCleanup(assoc);
              return NULL;
            }
          }
          else                                         // see dcmqrsrv.cc lines 839 - 876
          {
            /* accept storage syntaxes with proposed role */
            int npc = ASC_countPresentationContexts(assoc->params);
            for (int i = 0; i < npc; i++)
            {
              T_ASC_PresentationContext pc;
              ASC_getPresentationContext(assoc->params, i, &pc);
              if (dcmIsaStorageSOPClassUID(pc.abstractSyntax))
              {
                /**
                 * We are prepared to accept whatever role the caller
                 * proposes.  Normally we can be the SCP of the Storage
                 * Service Class.  When processing the C-GET operation
                 * we can be the SCU of the Storage Service Class.
                 **/
                const T_ASC_SC_ROLE role = pc.proposedRole;
            
                /**
                 * Accept in the order "least wanted" to "most wanted"
                 * transfer syntax.  Accepting a transfer syntax will
                 * override previously accepted transfer syntaxes.
                 * Since Orthanc 1.11.2+, we give priority to the transfer
                 * syntaxes proposed in the presentation context.
                 **/
                for (int j = static_cast<int>(pc.transferSyntaxCount)-1; j >=0; j--)
                {
                  for (int k = static_cast<int>(storageTransferSyntaxesC.size()) - 1; k >= 0; k--)
                  {
                    /**
                     * If the transfer syntax was proposed then we can accept it
                     * appears in our supported list of transfer syntaxes
                     **/
                    if (strcmp(pc.proposedTransferSyntaxes[j], storageTransferSyntaxesC[k]) == 0)
                    {
                      cond = ASC_acceptPresentationContext(
                        assoc->params, pc.presentationContextID, storageTransferSyntaxesC[k], role);
                      if (cond.bad())
                      {
                        CLOG(INFO, DICOM) << cond.text();
                        AssociationCleanup(assoc);
                        return NULL;
                      }
                    }
                  }
                }
              }
            } /* for */
          }

          if (!server.HasApplicationEntityFilter() ||
              server.GetApplicationEntityFilter().IsUnknownSopClassAccepted(remoteIp, remoteAet, calledAet))
          {
            /*
             * Promiscous mode is enabled: Accept everything not known not
             * to be a storage SOP class.
             **/
            cond = acceptUnknownContextsWithPreferredTransferSyntaxes(
              assoc->params, &storageTransferSyntaxesC[0], storageTransferSyntaxesC.size(), ASC_SC_ROLE_DEFAULT);
            if (cond.bad())
            {
              CLOG(INFO, DICOM) << cond.text();
              AssociationCleanup(assoc);
              return NULL;
            }
          }
        }
      }

      /* set our app title */
      ASC_setAPTitles(assoc->params, NULL, NULL, server.GetApplicationEntityTitle().c_str());

      /* acknowledge or reject this association */
#if DCMTK_VERSION_NUMBER >= 364
      cond = ASC_getApplicationContextName(assoc->params, buf, sizeof(buf));
#else
      cond = ASC_getApplicationContextName(assoc->params, buf);
#endif

      if ((cond.bad()) || strcmp(buf, UID_StandardApplicationContext) != 0)
      {
        /* reject: the application context name is not supported */
        T_ASC_RejectParameters rej =
          {
            ASC_RESULT_REJECTEDPERMANENT,
            ASC_SOURCE_SERVICEUSER,
            ASC_REASON_SU_APPCONTEXTNAMENOTSUPPORTED
          };

        CLOG(INFO, DICOM) << "Association Rejected: Bad Application Context Name: " << buf;
        cond = ASC_rejectAssociation(assoc, &rej);
        if (cond.bad())
        {
          CLOG(INFO, DICOM) << cond.text();
        }
        AssociationCleanup(assoc);
        return NULL;
      }

      /* check the AETs */
      if (!server.IsMyAETitle(calledAet))
      {
        CLOG(WARNING, DICOM) << "Rejected association, because of a bad called AET in the request (" << calledAet << ")";
        T_ASC_RejectParameters rej =
          {
            ASC_RESULT_REJECTEDPERMANENT,
            ASC_SOURCE_SERVICEUSER,
            ASC_REASON_SU_CALLEDAETITLENOTRECOGNIZED
          };
        ASC_rejectAssociation(assoc, &rej);
        AssociationCleanup(assoc);
        return NULL;
      }

      if (server.HasApplicationEntityFilter() &&
          !server.GetApplicationEntityFilter().IsAllowedConnection(remoteIp, remoteAet, calledAet))
      {
        CLOG(WARNING, DICOM) << "Rejected association for remote AET " << remoteAet << " on IP " << remoteIp;
        T_ASC_RejectParameters rej =
          {
            ASC_RESULT_REJECTEDPERMANENT,
            ASC_SOURCE_SERVICEUSER,
            ASC_REASON_SU_CALLINGAETITLENOTRECOGNIZED
          };
        ASC_rejectAssociation(assoc, &rej);
        AssociationCleanup(assoc);
        return NULL;
      }

      if (opt_rejectWithoutImplementationUID && 
          strlen(assoc->params->theirImplementationClassUID) == 0)
      {
        /* reject: the no implementation Class UID provided */
        T_ASC_RejectParameters rej =
          {
            ASC_RESULT_REJECTEDPERMANENT,
            ASC_SOURCE_SERVICEUSER,
            ASC_REASON_SU_NOREASON
          };

        CLOG(INFO, DICOM) << "Association Rejected: No Implementation Class UID provided";
        cond = ASC_rejectAssociation(assoc, &rej);
        if (cond.bad())
        {
          CLOG(INFO, DICOM) << cond.text();
        }
        AssociationCleanup(assoc);
        return NULL;
      }

      {
        cond = ASC_acknowledgeAssociation(assoc);
        if (cond.bad())
        {
          CLOG(ERROR, DICOM) << cond.text();
          AssociationCleanup(assoc);
          return NULL;
        }

        {
          std::string suffix;
          if (ASC_countAcceptedPresentationContexts(assoc->params) == 0)
            suffix = " (but no valid presentation contexts)";
          
          CLOG(INFO, DICOM) << "Association Acknowledged (Max Send PDV: " << assoc->sendPDVLength
                            << ") to AET " << remoteAet << " on IP " << remoteIp << suffix;
        }

        {
          OFString str;
          CLOG(TRACE, DICOM) << "Association Acknowledged Details:" << std::endl
                             << ASC_dumpParameters(str, assoc->params, ASC_ASSOC_AC);
        }
      }

      IApplicationEntityFilter* filter = server.HasApplicationEntityFilter() ? &server.GetApplicationEntityFilter() : NULL;
      return new CommandDispatcher(server, assoc, remoteIp, remoteAet, calledAet, maximumPduLength, filter);
    }


    CommandDispatcher::CommandDispatcher(const DicomServer& server,
                                         T_ASC_Association* assoc,
                                         const std::string& remoteIp,
                                         const std::string& remoteAet,
                                         const std::string& calledAet,
                                         unsigned int maximumPduLength,
                                         IApplicationEntityFilter* filter) :
      server_(server),
      assoc_(assoc),
      remoteIp_(remoteIp),
      remoteAet_(remoteAet),
      calledAet_(calledAet),
      filter_(filter)
    {
      associationTimeout_ = server.GetAssociationTimeout();
      elapsedTimeSinceLastCommand_ = 0;
    }


    CommandDispatcher::~CommandDispatcher()
    {
      try
      {
        AssociationCleanup(assoc_);
      }
      catch (...)
      {
        CLOG(ERROR, DICOM) << "Some association was not cleanly aborted";
      }
    }


    bool CommandDispatcher::Step()
    /*
     * This function receives DIMSE commmands over the network connection
     * and handles these commands correspondingly. Note that in case of
     * storscp only C-ECHO-RQ and C-STORE-RQ commands can be processed.
     */
    {
      bool finished = false;

      // receive a DIMSE command over the network, with a timeout of 1 second
      DcmDataset *statusDetail = NULL;
      T_ASC_PresentationContextID presID = 0;
      T_DIMSE_Message msg;

      OFCondition cond = DIMSE_receiveCommand(assoc_, DIMSE_NONBLOCKING, 1, &presID, &msg, &statusDetail);
      elapsedTimeSinceLastCommand_++;
    
      // if the command which was received has extra status
      // detail information, dump this information
      if (statusDetail != NULL)
      {
        std::stringstream s;  // DcmObject::PrintHelper cannot be used with VS2008
        statusDetail->print(s);
        CLOG(TRACE, DICOM) << "Status Detail:" << std::endl << s.str();

        delete statusDetail;
      }

      if (cond == DIMSE_OUTOFRESOURCES)
      {
        finished = true;
      }
      else if (cond == DIMSE_NODATAAVAILABLE)
      {
        // Timeout due to DIMSE_NONBLOCKING
        if (associationTimeout_ != 0 && 
            elapsedTimeSinceLastCommand_ >= associationTimeout_)
        {
          // This timeout is actually a association timeout
          finished = true;
        }
      }
      else if (cond == EC_Normal)
      {
        {
          OFString str;
          CLOG(TRACE, DICOM) << "Received Command:" << std::endl
                             << DIMSE_dumpMessage(str, msg, DIMSE_INCOMING, NULL, presID);
        }
        
        // Reset the association timeout counter
        elapsedTimeSinceLastCommand_ = 0;

        // Convert the type of request to Orthanc's internal type
        bool supported = false;
        DicomRequestType request;
        switch (msg.CommandField)
        {
          case DIMSE_C_ECHO_RQ:
            request = DicomRequestType_Echo;
            supported = true;
            break;

          case DIMSE_C_STORE_RQ:
            request = DicomRequestType_Store;
            supported = true;
            break;

          case DIMSE_C_MOVE_RQ:
            request = DicomRequestType_Move;
            supported = true;
            break;
            
          case DIMSE_C_GET_RQ:
            request = DicomRequestType_Get;
            supported = true;
            break;

          case DIMSE_C_FIND_RQ:
          {
            std::string sopClassUid(msg.msg.CFindRQ.AffectedSOPClassUID);
            if (sopClassUid == UID_FINDModalityWorklistInformationModel)
            {
              request = DicomRequestType_FindWorklist;
            }
            else
            {
              request = DicomRequestType_Find;
            }
            supported = true;
            break;
          }
          
          case DIMSE_N_ACTION_RQ:
            request = DicomRequestType_NAction;
            supported = true;
            break;

          case DIMSE_N_EVENT_REPORT_RQ:
            request = DicomRequestType_NEventReport;
            supported = true;
            break;

          default:
            // we cannot handle this kind of message
            cond = DIMSE_BADCOMMANDTYPE;
            CLOG(ERROR, DICOM) << "cannot handle command: 0x" << std::hex << msg.CommandField;
            break;
        }


        // Check whether this request is allowed by the security filter
        if (supported && 
            filter_ != NULL &&
            !filter_->IsAllowedRequest(remoteIp_, remoteAet_, calledAet_, request))
        {
          CLOG(WARNING, DICOM) << "Rejected " << EnumerationToString(request)
                               << " request from remote DICOM modality with AET \""
                               << remoteAet_ << "\" and hostname \"" << remoteIp_ << "\"";
          cond = DIMSE_ILLEGALASSOCIATION;
          supported = false;
          finished = true;
        }

        // in case we received a supported message, process this command
        if (supported)
        {
          // If anything goes wrong, there will be a "BADCOMMANDTYPE" answer
          cond = DIMSE_BADCOMMANDTYPE;

          switch (request)
          {
            case DicomRequestType_Echo:
              cond = EchoScp(assoc_, &msg, presID);
              break;

            case DicomRequestType_Store:
              if (server_.HasStoreRequestHandlerFactory()) // Should always be true
              {
                std::unique_ptr<IStoreRequestHandler> handler
                  (server_.GetStoreRequestHandlerFactory().ConstructStoreRequestHandler());

                if (handler.get() != NULL)
                {
                  cond = Internals::storeScp(assoc_, &msg, presID, *handler, remoteIp_, associationTimeout_);
                }
              }
              break;

            case DicomRequestType_Move:
              if (server_.HasMoveRequestHandlerFactory()) // Should always be true
              {
                std::unique_ptr<IMoveRequestHandler> handler
                  (server_.GetMoveRequestHandlerFactory().ConstructMoveRequestHandler());

                if (handler.get() != NULL)
                {
                  cond = Internals::moveScp(assoc_, &msg, presID, *handler, remoteIp_, remoteAet_, calledAet_, associationTimeout_);
                }
              }
              break;
              
            case DicomRequestType_Get:
              if (server_.HasGetRequestHandlerFactory()) // Should always be true
              {
                std::unique_ptr<IGetRequestHandler> handler
                  (server_.GetGetRequestHandlerFactory().ConstructGetRequestHandler());
                
                if (handler.get() != NULL)
                {
                  cond = Internals::getScp(assoc_, &msg, presID, *handler, remoteIp_, remoteAet_, calledAet_, associationTimeout_);
                }
              }
              break;

            case DicomRequestType_Find:
            case DicomRequestType_FindWorklist:
              if (server_.HasFindRequestHandlerFactory() || // Should always be true
                  server_.HasWorklistRequestHandlerFactory())
              {
                std::unique_ptr<IFindRequestHandler> findHandler;
                if (server_.HasFindRequestHandlerFactory())
                {
                  findHandler.reset(server_.GetFindRequestHandlerFactory().ConstructFindRequestHandler());
                }

                std::unique_ptr<IWorklistRequestHandler> worklistHandler;
                if (server_.HasWorklistRequestHandlerFactory())
                {
                  worklistHandler.reset(server_.GetWorklistRequestHandlerFactory().ConstructWorklistRequestHandler());
                }

                cond = Internals::findScp(assoc_, &msg, presID, findHandler.get(), worklistHandler.get(),
                                          remoteIp_, remoteAet_, calledAet_, associationTimeout_);
              }
              break;

            case DicomRequestType_NAction:
              cond = NActionScp(&msg, presID);
              break;              

            case DicomRequestType_NEventReport:
              cond = NEventReportScp(&msg, presID);
              break;              

            default:
              // Should never happen
              break;
          }
        }
      }
      else
      {
        // Bad status, which indicates the closing of the connection by
        // the peer or a network error
        finished = true;

        CLOG(INFO, DICOM) << "Finishing association with AET " << remoteAet_
                          << " on IP " << remoteIp_ << ": " << cond.text();
      }
    
      if (finished)
      {
        if (cond == DUL_PEERREQUESTEDRELEASE)
        {
          CLOG(INFO, DICOM) << "Association Release with AET " << remoteAet_ << " on IP " << remoteIp_;
          ASC_acknowledgeRelease(assoc_);
        }
        else if (cond == DUL_PEERABORTEDASSOCIATION)
        {
          CLOG(INFO, DICOM) << "Association Aborted with AET " << remoteAet_ << " on IP " << remoteIp_;
        }
        else
        {
          OFString temp_str;
          CLOG(INFO, DICOM) << "DIMSE failure (aborting association with AET " << remoteAet_
                            << " on IP " << remoteIp_ << "): " << cond.text();
          /* some kind of error so abort the association */
          ASC_abortAssociation(assoc_);
        }
      }

      return !finished;
    }


    OFCondition EchoScp(T_ASC_Association * assoc, T_DIMSE_Message * msg, T_ASC_PresentationContextID presID)
    {
      OFString temp_str;
      CLOG(INFO, DICOM) << "Received Echo Request";

      /* the echo succeeded !! */
      OFCondition cond = DIMSE_sendEchoResponse(assoc, presID, &msg->msg.CEchoRQ, STATUS_Success, NULL);
      if (cond.bad())
      {
        CLOG(ERROR, DICOM) << "Echo SCP Failed: " << cond.text();
      }
      return cond;
    }


    static DcmDataset* ReadDataset(T_ASC_Association* assoc,
                                   const char* errorMessage,
                                   int timeout)
    {
      DcmDataset *tmp = NULL;
      T_ASC_PresentationContextID presIdData;
    
      OFCondition cond = DIMSE_receiveDataSetInMemory(
        assoc, (timeout ? DIMSE_NONBLOCKING : DIMSE_BLOCKING), timeout,
        &presIdData, &tmp, NULL, NULL);
      if (!cond.good() ||
          tmp == NULL)
      {
        throw OrthancException(ErrorCode_NetworkProtocol, errorMessage);
      }

      return tmp;
    }


    static std::string ReadString(DcmDataset& dataset,
                                  const DcmTagKey& tag)
    {
      const char* s = NULL;
      if (!dataset.findAndGetString(tag, s).good() ||
          s == NULL)
      {
        char buf[64];
        sprintf(buf, "Missing mandatory tag in dataset: (%04X,%04X)",
                tag.getGroup(), tag.getElement());
        throw OrthancException(ErrorCode_NetworkProtocol, buf);
      }

      return std::string(s);
    }


    static void ReadSopSequence(
      std::vector<std::string>& sopClassUids,
      std::vector<std::string>& sopInstanceUids,
      std::vector<StorageCommitmentFailureReason>* failureReasons, // Can be NULL
      DcmDataset& dataset,
      const DcmTagKey& tag,
      bool mandatory)
    {
      sopClassUids.clear();
      sopInstanceUids.clear();

      if (failureReasons)
      {
        failureReasons->clear();
      }

      DcmSequenceOfItems* sequence = NULL;
      if (!dataset.findAndGetSequence(tag, sequence).good() ||
          sequence == NULL)
      {
        if (mandatory)
        {        
          char buf[64];
          sprintf(buf, "Missing mandatory sequence in dataset: (%04X,%04X)",
                  tag.getGroup(), tag.getElement());
          throw OrthancException(ErrorCode_NetworkProtocol, buf);
        }
        else
        {
          return;
        }
      }

      sopClassUids.reserve(sequence->card());
      sopInstanceUids.reserve(sequence->card());

      if (failureReasons)
      {
        failureReasons->reserve(sequence->card());
      }

      for (unsigned long i = 0; i < sequence->card(); i++)
      {
        const char* a = NULL;
        const char* b = NULL;
        if (!sequence->getItem(i)->findAndGetString(DCM_ReferencedSOPClassUID, a).good() ||
            !sequence->getItem(i)->findAndGetString(DCM_ReferencedSOPInstanceUID, b).good() ||
            a == NULL ||
            b == NULL)
        {
          throw OrthancException(ErrorCode_NetworkProtocol,
                                 "Missing Referenced SOP Class/Instance UID "
                                 "in storage commitment dataset");
        }

        sopClassUids.push_back(a);
        sopInstanceUids.push_back(b);

        if (failureReasons != NULL)
        {
          Uint16 reason;
          if (!sequence->getItem(i)->findAndGetUint16(DCM_FailureReason, reason).good())
          {
            throw OrthancException(ErrorCode_NetworkProtocol,
                                   "Missing Failure Reason (0008,1197) "
                                   "in storage commitment dataset");
          }

          failureReasons->push_back(static_cast<StorageCommitmentFailureReason>(reason));
        }
      }
    }

    
    OFCondition CommandDispatcher::NActionScp(T_DIMSE_Message* msg,
                                              T_ASC_PresentationContextID presID)
    {
      /**
       * Starting with Orthanc 1.6.0, only storage commitment is
       * supported with DICOM N-ACTION. This corresponds to the case
       * where "Action Type ID" equals "1".
       * http://dicom.nema.org/medical/dicom/2019a/output/chtml/part04/sect_J.3.2.html
       * http://dicom.nema.org/medical/dicom/2019a/output/chtml/part07/chapter_10.html#table_10.1-4
       **/
      
      if (msg->CommandField != DIMSE_N_ACTION_RQ /* value == 304 == 0x0130 */ ||
          !server_.HasStorageCommitmentRequestHandlerFactory())
      {
        throw OrthancException(ErrorCode_InternalError);
      }


      /**
       * Check that the storage commitment request is correctly formatted.
       **/
      
      const T_DIMSE_N_ActionRQ& request = msg->msg.NActionRQ;

      if (request.ActionTypeID != 1)
      {
        throw OrthancException(ErrorCode_NotImplemented,
                               "Only storage commitment is implemented for DICOM N-ACTION SCP");
      }

      if (std::string(request.RequestedSOPClassUID) != UID_StorageCommitmentPushModelSOPClass ||
          std::string(request.RequestedSOPInstanceUID) != UID_StorageCommitmentPushModelSOPInstance)
      {
        throw OrthancException(ErrorCode_NetworkProtocol,
                               "Unexpected incoming SOP class or instance UID for storage commitment");
      }

      if (request.DataSetType != DIMSE_DATASET_PRESENT)
      {
        throw OrthancException(ErrorCode_NetworkProtocol,
                               "Incoming storage commitment request without a dataset");
      }


      /**
       * Extract the DICOM dataset that is associated with the DIMSE
       * message. The content of this dataset is documented in "Table
       * J.3-1. Storage Commitment Request - Action Information":
       * http://dicom.nema.org/medical/dicom/2019a/output/chtml/part04/sect_J.3.2.html#table_J.3-1
       **/
      
      std::unique_ptr<DcmDataset> dataset(
        ReadDataset(assoc_, "Cannot read the dataset in N-ACTION SCP", associationTimeout_));
      assert(dataset.get() != NULL);

      {
        std::stringstream s;  // DcmObject::PrintHelper cannot be used with VS2008
        dataset->print(s);
        CLOG(TRACE, DICOM) << "Received Storage Commitment Request:" << std::endl << s.str();
      }
      
      std::string transactionUid = ReadString(*dataset, DCM_TransactionUID);

      std::vector<std::string> sopClassUid, sopInstanceUid;
      ReadSopSequence(sopClassUid, sopInstanceUid, NULL,
                      *dataset, DCM_ReferencedSOPSequence, true /* mandatory */);

      CLOG(INFO, DICOM) << "Incoming storage commitment request, with transaction UID: " << transactionUid;

      for (size_t i = 0; i < sopClassUid.size(); i++)
      {
        CLOG(INFO, DICOM) << "  (" << (i + 1) << "/" << sopClassUid.size()
                          << ") queried SOP Class/Instance UID: "
                          << sopClassUid[i] << " / " << sopInstanceUid[i];
      }


      /**
       * Call the Orthanc handler. The list of available DIMSE status
       * codes can be found at:
       * http://dicom.nema.org/medical/dicom/2019a/output/chtml/part07/chapter_10.html#sect_10.1.4.1.10
       **/

      DIC_US dimseStatus;
  
      try
      {
        std::unique_ptr<IStorageCommitmentRequestHandler> handler
          (server_.GetStorageCommitmentRequestHandlerFactory().
           ConstructStorageCommitmentRequestHandler());

        handler->HandleRequest(transactionUid, sopClassUid, sopInstanceUid,
                               remoteIp_, remoteAet_, calledAet_);
        
        dimseStatus = 0;  // Success
      }
      catch (OrthancException& e)
      {
        CLOG(ERROR, DICOM) << "Error while processing an incoming storage commitment request: " << e.What();

        // Code 0x0110 - "General failure in processing the operation was encountered"
        dimseStatus = STATUS_N_ProcessingFailure;
      }


      /**
       * Send the DIMSE status back to the SCU.
       **/

      {
        T_DIMSE_Message response;
        memset(&response, 0, sizeof(response));
        response.CommandField = DIMSE_N_ACTION_RSP;

        T_DIMSE_N_ActionRSP& content = response.msg.NActionRSP;
        content.MessageIDBeingRespondedTo = request.MessageID;
        strncpy(content.AffectedSOPClassUID, UID_StorageCommitmentPushModelSOPClass, DIC_UI_LEN);
        content.DimseStatus = dimseStatus;
        strncpy(content.AffectedSOPInstanceUID, UID_StorageCommitmentPushModelSOPInstance, DIC_UI_LEN);
        content.ActionTypeID = 0; // Not present, as "O_NACTION_ACTIONTYPEID" not set in "opts"
        content.DataSetType = DIMSE_DATASET_NULL;  // Dataset is absent in storage commitment response
        content.opts = O_NACTION_AFFECTEDSOPCLASSUID | O_NACTION_AFFECTEDSOPINSTANCEUID;

        {
          OFString str;
          CLOG(TRACE, DICOM) << "Sending Storage Commitment Request Response:" << std::endl
                             << DIMSE_dumpMessage(str, response, DIMSE_OUTGOING);
        }
        
        return DIMSE_sendMessageUsingMemoryData(
          assoc_, presID, &response, NULL /* no dataset */, NULL /* dataObject */,
          NULL /* callback */, NULL /* callback context */, NULL /* commandSet */);
      }
    }


    OFCondition CommandDispatcher::NEventReportScp(T_DIMSE_Message* msg,
                                                   T_ASC_PresentationContextID presID)
    {
      /**
       * Starting with Orthanc 1.6.0, handling N-EVENT-REPORT for
       * storage commitment.
       * http://dicom.nema.org/medical/dicom/2019a/output/chtml/part04/sect_J.3.3.html
       * http://dicom.nema.org/medical/dicom/2019a/output/chtml/part07/chapter_10.html#table_10.1-1
       **/

      if (msg->CommandField != DIMSE_N_EVENT_REPORT_RQ /* value == 256 == 0x0100 */ ||
          !server_.HasStorageCommitmentRequestHandlerFactory())
      {
        throw OrthancException(ErrorCode_InternalError);
      }


      /**
       * Check that the storage commitment report is correctly formatted.
       **/
      
      const T_DIMSE_N_EventReportRQ& report = msg->msg.NEventReportRQ;

      if (report.EventTypeID != 1 /* successful */ &&
          report.EventTypeID != 2 /* failures exist */)
      {
        throw OrthancException(ErrorCode_NotImplemented,
                               "Unknown event for DICOM N-EVENT-REPORT SCP");
      }

      if (std::string(report.AffectedSOPClassUID) != UID_StorageCommitmentPushModelSOPClass ||
          std::string(report.AffectedSOPInstanceUID) != UID_StorageCommitmentPushModelSOPInstance)
      {
        throw OrthancException(ErrorCode_NetworkProtocol,
                               "Unexpected incoming SOP class or instance UID for storage commitment");
      }

      if (report.DataSetType != DIMSE_DATASET_PRESENT)
      {
        throw OrthancException(ErrorCode_NetworkProtocol,
                               "Incoming storage commitment report without a dataset");
      }


      /**
       * Extract the DICOM dataset that is associated with the DIMSE
       * message. The content of this dataset is documented in "Table
       * J.3-2. Storage Commitment Result - Event Information":
       * http://dicom.nema.org/medical/dicom/2019a/output/chtml/part04/sect_J.3.3.html#table_J.3-2
       **/
      
      std::unique_ptr<DcmDataset> dataset(
        ReadDataset(assoc_, "Cannot read the dataset in N-EVENT-REPORT SCP", associationTimeout_));
      assert(dataset.get() != NULL);

      {
        std::stringstream s;  // DcmObject::PrintHelper cannot be used with VS2008
        dataset->print(s);
        CLOG(TRACE, DICOM) << "Received Storage Commitment Report:" << std::endl << s.str();
      }
      
      std::string transactionUid = ReadString(*dataset, DCM_TransactionUID);

      std::vector<std::string> successSopClassUid, successSopInstanceUid;
      ReadSopSequence(successSopClassUid, successSopInstanceUid, NULL,
                      *dataset, DCM_ReferencedSOPSequence,
                      (report.EventTypeID == 1) /* mandatory in the case of success */);

      std::vector<std::string> failedSopClassUid, failedSopInstanceUid;
      std::vector<StorageCommitmentFailureReason> failureReasons;

      if (report.EventTypeID == 2 /* failures exist */)
      {
        ReadSopSequence(failedSopClassUid, failedSopInstanceUid, &failureReasons,
                        *dataset, DCM_FailedSOPSequence, true);
      }

      CLOG(INFO, DICOM) << "Incoming storage commitment report, with transaction UID: " << transactionUid;

      for (size_t i = 0; i < successSopClassUid.size(); i++)
      {
        CLOG(INFO, DICOM) << "  (success " << (i + 1) << "/" << successSopClassUid.size()
                          << ") SOP Class/Instance UID: "
                          << successSopClassUid[i] << " / " << successSopInstanceUid[i];
      }

      for (size_t i = 0; i < failedSopClassUid.size(); i++)
      {
        CLOG(INFO, DICOM) << "  (failure " << (i + 1) << "/" << failedSopClassUid.size()
                          << ") SOP Class/Instance UID: "
                          << failedSopClassUid[i] << " / " << failedSopInstanceUid[i];
      }

      /**
       * Call the Orthanc handler. The list of available DIMSE status
       * codes can be found at:
       * http://dicom.nema.org/medical/dicom/2019a/output/chtml/part07/chapter_10.html#sect_10.1.4.1.10
       **/

      DIC_US dimseStatus;

      try
      {
        std::unique_ptr<IStorageCommitmentRequestHandler> handler
          (server_.GetStorageCommitmentRequestHandlerFactory().
           ConstructStorageCommitmentRequestHandler());

        handler->HandleReport(transactionUid, successSopClassUid, successSopInstanceUid,
                              failedSopClassUid, failedSopInstanceUid, failureReasons,
                              remoteIp_, remoteAet_, calledAet_);
        
        dimseStatus = 0;  // Success
      }
      catch (OrthancException& e)
      {
        CLOG(ERROR, DICOM) << "Error while processing an incoming storage commitment report: " << e.What();

        // Code 0x0110 - "General failure in processing the operation was encountered"
        dimseStatus = STATUS_N_ProcessingFailure;
      }

      
      /**
       * Send the DIMSE status back to the SCU.
       **/

      {
        T_DIMSE_Message response;
        memset(&response, 0, sizeof(response));
        response.CommandField = DIMSE_N_EVENT_REPORT_RSP;

        T_DIMSE_N_EventReportRSP& content = response.msg.NEventReportRSP;
        content.MessageIDBeingRespondedTo = report.MessageID;
        strncpy(content.AffectedSOPClassUID, UID_StorageCommitmentPushModelSOPClass, DIC_UI_LEN);
        content.DimseStatus = dimseStatus;
        strncpy(content.AffectedSOPInstanceUID, UID_StorageCommitmentPushModelSOPInstance, DIC_UI_LEN);
        content.EventTypeID = 0; // Not present, as "O_NEVENTREPORT_EVENTTYPEID" not set in "opts"
        content.DataSetType = DIMSE_DATASET_NULL;  // Dataset is absent in storage commitment response
        content.opts = O_NEVENTREPORT_AFFECTEDSOPCLASSUID | O_NEVENTREPORT_AFFECTEDSOPINSTANCEUID;

        {
          OFString str;
          CLOG(TRACE, DICOM) << "Sending Storage Commitment Report Response:" << std::endl
                             << DIMSE_dumpMessage(str, response, DIMSE_OUTGOING);
        }

        return DIMSE_sendMessageUsingMemoryData(
          assoc_, presID, &response, NULL /* no dataset */, NULL /* dataObject */,
          NULL /* callback */, NULL /* callback context */, NULL /* commandSet */);
      }
    }
  }
}
