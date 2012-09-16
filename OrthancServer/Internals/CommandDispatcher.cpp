/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012 Medical Physics Department, CHU of Liege,
 * Belgium
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


#include "CommandDispatcher.h"

#include "FindScp.h"
#include "StoreScp.h"
#include "MoveScp.h"
#include "../../Core/Toolbox.h"

#include <dcmtk/dcmnet/dcasccfg.h>      /* for class DcmAssociationConfiguration */

static OFBool    opt_rejectWithoutImplementationUID = OFFalse;


namespace Orthanc
{
  namespace Internals
  {
    extern OFLogger Logger;



    OFCondition AssociationCleanup(T_ASC_Association *assoc)
    {
      OFString temp_str;
      OFCondition cond = ASC_dropSCPAssociation(assoc);
      if (cond.bad())
      {
        OFLOG_FATAL(Internals::Logger, DimseCondition::dump(temp_str, cond));
        return cond;
      }

      cond = ASC_destroyAssociation(&assoc);
      if (cond.bad())
      {
        OFLOG_FATAL(Internals::Logger, DimseCondition::dump(temp_str, cond));
        return cond;
      }

      return cond;
    }



    CommandDispatcher* AcceptAssociation(const DicomServer& server, T_ASC_Network *net)
    {
      DcmAssociationConfiguration asccfg;
      char buf[BUFSIZ];
      T_ASC_Association *assoc;
      OFCondition cond;
      OFString sprofile;
      OFString temp_str;

      std::vector<const char*> knownAbstractSyntaxes;

      // For C-STORE
      if (server.HasStoreRequestHandlerFactory())
      {
        knownAbstractSyntaxes.push_back(UID_VerificationSOPClass);
      }

      // For C-FIND
      if (server.HasFindRequestHandlerFactory())
      {
        knownAbstractSyntaxes.push_back(UID_FINDPatientRootQueryRetrieveInformationModel);
        knownAbstractSyntaxes.push_back(UID_FINDStudyRootQueryRetrieveInformationModel);
      }

      // For C-MOVE
      if (server.HasMoveRequestHandlerFactory())
      {
        knownAbstractSyntaxes.push_back(UID_MOVEStudyRootQueryRetrieveInformationModel);
      }

      const char* transferSyntaxes[] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
      int numTransferSyntaxes = 0;

      cond = ASC_receiveAssociation(net, &assoc, 
                                    /*opt_maxPDU*/ ASC_DEFAULTMAXPDU, 
                                    NULL, NULL,
                                    /*opt_secureConnection*/ OFFalse,
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
        OFLOG_ERROR(Internals::Logger, "Receiving Association failed: " << DimseCondition::dump(temp_str, cond));
        // no matter what kind of error occurred, we need to do a cleanup
        AssociationCleanup(assoc);
        return NULL;
      }

      OFLOG_INFO(Internals::Logger, "Association Received");

      transferSyntaxes[0] = UID_LittleEndianExplicitTransferSyntax;
      transferSyntaxes[1] = UID_BigEndianExplicitTransferSyntax;
      transferSyntaxes[2] = UID_LittleEndianImplicitTransferSyntax;
      numTransferSyntaxes = 3;

      /* accept the Verification SOP Class if presented */
      cond = ASC_acceptContextsWithPreferredTransferSyntaxes( assoc->params, &knownAbstractSyntaxes[0], knownAbstractSyntaxes.size(), transferSyntaxes, numTransferSyntaxes);
      if (cond.bad())
      {
        OFLOG_DEBUG(Internals::Logger, DimseCondition::dump(temp_str, cond));
        AssociationCleanup(assoc);
        return NULL;
      }

      /* the array of Storage SOP Class UIDs comes from dcuid.h */
      cond = ASC_acceptContextsWithPreferredTransferSyntaxes( assoc->params, dcmAllStorageSOPClassUIDs, numberOfAllDcmStorageSOPClassUIDs, transferSyntaxes, numTransferSyntaxes);
      if (cond.bad())
      {
        OFLOG_DEBUG(Internals::Logger, DimseCondition::dump(temp_str, cond));
        AssociationCleanup(assoc);
        return NULL;
      }

      /* set our app title */
      ASC_setAPTitles(assoc->params, NULL, NULL, server.GetApplicationEntityTitle().c_str());

      /* acknowledge or reject this association */
      cond = ASC_getApplicationContextName(assoc->params, buf);
      if ((cond.bad()) || strcmp(buf, UID_StandardApplicationContext) != 0)
      {
        /* reject: the application context name is not supported */
        T_ASC_RejectParameters rej =
          {
            ASC_RESULT_REJECTEDPERMANENT,
            ASC_SOURCE_SERVICEUSER,
            ASC_REASON_SU_APPCONTEXTNAMENOTSUPPORTED
          };

        OFLOG_INFO(Internals::Logger, "Association Rejected: Bad Application Context Name: " << buf);
        cond = ASC_rejectAssociation(assoc, &rej);
        if (cond.bad())
        {
          OFLOG_DEBUG(Internals::Logger, DimseCondition::dump(temp_str, cond));
        }
        AssociationCleanup(assoc);
        return NULL;
      }
  
      /* check the AETs */
      {
        DIC_AE callingTitle_C;
        DIC_AE calledTitle_C;
        DIC_AE callingIP_C;
        DIC_AE calledIP_C;
        if (ASC_getAPTitles(assoc->params, callingTitle_C, calledTitle_C, NULL).bad() ||
            ASC_getPresentationAddresses(assoc->params, callingIP_C, calledIP_C).bad())
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

        std::string callingIP(OFSTRING_GUARD(callingIP_C));
        std::string callingTitle(OFSTRING_GUARD(callingTitle_C));
        std::string calledTitle(OFSTRING_GUARD(calledTitle_C));
        Toolbox::ToUpperCase(callingIP);
        Toolbox::ToUpperCase(callingTitle);
        Toolbox::ToUpperCase(calledTitle);

        if (server.HasCalledApplicationEntityTitleCheck() &&
            calledTitle != server.GetApplicationEntityTitle())
        {
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
            !server.GetApplicationEntityFilter().IsAllowed(callingIP, callingTitle))
        {
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
      }

      if (opt_rejectWithoutImplementationUID && strlen(assoc->params->theirImplementationClassUID) == 0)
      {
        /* reject: the no implementation Class UID provided */
        T_ASC_RejectParameters rej =
          {
            ASC_RESULT_REJECTEDPERMANENT,
            ASC_SOURCE_SERVICEUSER,
            ASC_REASON_SU_NOREASON
          };

        OFLOG_INFO(Internals::Logger, "Association Rejected: No Implementation Class UID provided");
        cond = ASC_rejectAssociation(assoc, &rej);
        if (cond.bad())
        {
          OFLOG_DEBUG(Internals::Logger, DimseCondition::dump(temp_str, cond));
        }
        AssociationCleanup(assoc);
        return NULL;
      }

      {
        cond = ASC_acknowledgeAssociation(assoc);
        if (cond.bad())
        {
          OFLOG_ERROR(Internals::Logger, DimseCondition::dump(temp_str, cond));
          AssociationCleanup(assoc);
          return NULL;
        }
        OFLOG_INFO(Internals::Logger, "Association Acknowledged (Max Send PDV: " << assoc->sendPDVLength << ")");
        if (ASC_countAcceptedPresentationContexts(assoc->params) == 0)
          OFLOG_INFO(Internals::Logger, "    (but no valid presentation contexts)");
      }

      return new CommandDispatcher(server, assoc);
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
        OFLOG_WARN(Internals::Logger, "Status Detail:" << OFendl << DcmObject::PrintHelper(*statusDetail));
        delete statusDetail;
      }

      if (cond == DIMSE_OUTOFRESOURCES)
      {
        finished = true;
      }
      else if (cond == DIMSE_NODATAAVAILABLE)
      {
        // Timeout due to DIMSE_NONBLOCKING
        if (clientTimeout_ != 0 && 
            elapsedTimeSinceLastCommand_ >= clientTimeout_)
        {
          // This timeout is actually a client timeout
          finished = true;
        }
      }
      else if (cond == EC_Normal)
      {
        // Reset the client timeout counter
        elapsedTimeSinceLastCommand_ = 0;

        // in case we received a valid message, process this command
        // note that storescp can only process a C-ECHO-RQ and a C-STORE-RQ
        switch (msg.CommandField)
        {
        case DIMSE_C_ECHO_RQ:
          // process C-ECHO-Request
          cond = EchoScp(assoc_, &msg, presID);
          break;

        case DIMSE_C_STORE_RQ:
          // process C-STORE-Request
          if (server_.HasStoreRequestHandlerFactory())
          {
            std::auto_ptr<IStoreRequestHandler> handler
              (server_.GetStoreRequestHandlerFactory().ConstructStoreRequestHandler());
            cond = Internals::storeScp(assoc_, &msg, presID, *handler);
          }
          else
            cond = DIMSE_BADCOMMANDTYPE;  // Should never happen
          break;

        case DIMSE_C_MOVE_RQ:
          // process C-MOVE-Request
          if (server_.HasMoveRequestHandlerFactory())
          {
            std::auto_ptr<IMoveRequestHandler> handler
              (server_.GetMoveRequestHandlerFactory().ConstructMoveRequestHandler());
            cond = Internals::moveScp(assoc_, &msg, presID, *handler);
          }
          else
            cond = DIMSE_BADCOMMANDTYPE;  // Should never happen
          break;

        case DIMSE_C_FIND_RQ:
          // process C-FIND-Request
          if (server_.HasFindRequestHandlerFactory())
          {
            std::auto_ptr<IFindRequestHandler> handler
              (server_.GetFindRequestHandlerFactory().ConstructFindRequestHandler());
            cond = Internals::findScp(assoc_, &msg, presID, *handler);
          }
          else
            cond = DIMSE_BADCOMMANDTYPE;  // Should never happen
          break;

        default:
          // we cannot handle this kind of message
          cond = DIMSE_BADCOMMANDTYPE;
          OFLOG_ERROR(Internals::Logger, "cannot handle command: 0x"
                      << STD_NAMESPACE hex << OFstatic_cast(unsigned, msg.CommandField));
          break;
        }
      }
      else
      {
        // Bad status, which indicates the closing of the connection by
        // the peer or a network error
        finished = true;
      }
    
      if (finished)
      {
        if (cond == DUL_PEERREQUESTEDRELEASE)
        {
          OFLOG_INFO(Internals::Logger, "Association Release");
          ASC_acknowledgeRelease(assoc_);
        }
        else if (cond == DUL_PEERABORTEDASSOCIATION)
        {
          OFLOG_INFO(Internals::Logger, "Association Aborted");
        }
        else
        {
          OFString temp_str;
          OFLOG_ERROR(Internals::Logger, "DIMSE failure (aborting association): " << DimseCondition::dump(temp_str, cond));
          /* some kind of error so abort the association */
          ASC_abortAssociation(assoc_);
        }
      }

      return !finished;
    }


    OFCondition EchoScp( T_ASC_Association * assoc, T_DIMSE_Message * msg, T_ASC_PresentationContextID presID)
    {
      OFString temp_str;
      OFLOG_INFO(Internals::Logger, "Received Echo Request");
      OFLOG_DEBUG(Internals::Logger, DIMSE_dumpMessage(temp_str, msg->msg.CEchoRQ, DIMSE_INCOMING, NULL, presID));

      /* the echo succeeded !! */
      OFCondition cond = DIMSE_sendEchoResponse(assoc, presID, &msg->msg.CEchoRQ, STATUS_Success, NULL);
      if (cond.bad())
      {
        OFLOG_ERROR(Internals::Logger, "Echo SCP Failed: " << DimseCondition::dump(temp_str, cond));
      }
      return cond;
    }
  }
}
