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
#include "CommandDispatcher.h"

#include "FindScp.h"
#include "StoreScp.h"
#include "MoveScp.h"
#include "../../Toolbox.h"
#include "../../Logging.h"

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
  OFCondition cond = EC_Normal;
  int n, i, k;
  DUL_PRESENTATIONCONTEXT *dpc;
  T_ASC_PresentationContext pc;
  OFBool accepted = OFFalse;
  OFBool abstractOK = OFFalse;

  n = ASC_countPresentationContexts(params);
  for (i = 0; i < n; i++)
  {
    cond = ASC_getPresentationContext(params, i, &pc);
    if (cond.bad()) return cond;
    abstractOK = OFFalse;
    accepted = OFFalse;

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
      if ((dpc == NULL) || ((dpc != NULL) && (dpc->result != ASC_P_ACCEPTANCE)))
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
  T_ASC_SC_ROLE acceptedRole = ASC_SC_ROLE_DEFAULT)
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
    /**
     * EXTRACT OF FILE "dcmdata/libsrc/dcuid.cc" FROM DCMTK 3.6.0
     * (dcmAllStorageSOPClassUIDs).
     *
     * an array of const strings containing all known Storage SOP
     * Classes that fit into the conventional
     * PATIENT-STUDY-SERIES-INSTANCE information model,
     * i.e. everything a Storage SCP might want to store in a PACS.
     * Special cases such as hanging protocol storage or the Storage
     * SOP Class are not included in this list.
     *
     * THIS LIST CONTAINS ALL STORAGE SOP CLASSES INCLUDING RETIRED
     * ONES AND IS LARGER THAN 64 ENTRIES.
     */

    const char* orthancStorageSOPClassUIDs[] =
    {
      UID_AmbulatoryECGWaveformStorage,
      UID_ArterialPulseWaveformStorage,
      UID_AutorefractionMeasurementsStorage,
      UID_BasicStructuredDisplayStorage,
      UID_BasicTextSRStorage,
      UID_BasicVoiceAudioWaveformStorage,
      UID_BlendingSoftcopyPresentationStateStorage,
      UID_BreastTomosynthesisImageStorage,
      UID_CardiacElectrophysiologyWaveformStorage,
      UID_ChestCADSRStorage,
      UID_ColonCADSRStorage,
      UID_ColorSoftcopyPresentationStateStorage,
      UID_ComprehensiveSRStorage,
      UID_ComputedRadiographyImageStorage,
      UID_CTImageStorage,
      UID_DeformableSpatialRegistrationStorage,
      UID_DigitalIntraOralXRayImageStorageForPresentation,
      UID_DigitalIntraOralXRayImageStorageForProcessing,
      UID_DigitalMammographyXRayImageStorageForPresentation,
      UID_DigitalMammographyXRayImageStorageForProcessing,
      UID_DigitalXRayImageStorageForPresentation,
      UID_DigitalXRayImageStorageForProcessing,
      UID_EncapsulatedCDAStorage,
      UID_EncapsulatedPDFStorage,
      UID_EnhancedCTImageStorage,
      UID_EnhancedMRColorImageStorage,
      UID_EnhancedMRImageStorage,
      UID_EnhancedPETImageStorage,
      UID_EnhancedSRStorage,
      UID_EnhancedUSVolumeStorage,
      UID_EnhancedXAImageStorage,
      UID_EnhancedXRFImageStorage,
      UID_GeneralAudioWaveformStorage,
      UID_GeneralECGWaveformStorage,
      UID_GenericImplantTemplateStorage,
      UID_GrayscaleSoftcopyPresentationStateStorage,
      UID_HemodynamicWaveformStorage,
      UID_ImplantAssemblyTemplateStorage,
      UID_ImplantationPlanSRDocumentStorage,
      UID_ImplantTemplateGroupStorage,
      UID_IntraocularLensCalculationsStorage,
      UID_KeratometryMeasurementsStorage,
      UID_KeyObjectSelectionDocumentStorage,
      UID_LensometryMeasurementsStorage,
      UID_MacularGridThicknessAndVolumeReportStorage,
      UID_MammographyCADSRStorage,
      UID_MRImageStorage,
      UID_MRSpectroscopyStorage,
      UID_MultiframeGrayscaleByteSecondaryCaptureImageStorage,
      UID_MultiframeGrayscaleWordSecondaryCaptureImageStorage,
      UID_MultiframeSingleBitSecondaryCaptureImageStorage,
      UID_MultiframeTrueColorSecondaryCaptureImageStorage,
      UID_NuclearMedicineImageStorage,
      UID_OphthalmicAxialMeasurementsStorage,
      UID_OphthalmicPhotography16BitImageStorage,
      UID_OphthalmicPhotography8BitImageStorage,
      UID_OphthalmicTomographyImageStorage,
      UID_OphthalmicVisualFieldStaticPerimetryMeasurementsStorage,
      UID_PositronEmissionTomographyImageStorage,
      UID_ProcedureLogStorage,
      UID_PseudoColorSoftcopyPresentationStateStorage,
      UID_RawDataStorage,
      UID_RealWorldValueMappingStorage,
      UID_RespiratoryWaveformStorage,
      UID_RTBeamsTreatmentRecordStorage,
      UID_RTBrachyTreatmentRecordStorage,
      UID_RTDoseStorage,
      UID_RTImageStorage,
      UID_RTIonBeamsTreatmentRecordStorage,
      UID_RTIonPlanStorage,
      UID_RTPlanStorage,
      UID_RTStructureSetStorage,
      UID_RTTreatmentSummaryRecordStorage,
      UID_SecondaryCaptureImageStorage,
      UID_SegmentationStorage,
      UID_SpatialFiducialsStorage,
      UID_SpatialRegistrationStorage,
      UID_SpectaclePrescriptionReportStorage,
      UID_StereometricRelationshipStorage,
      UID_SubjectiveRefractionMeasurementsStorage,
      UID_SurfaceSegmentationStorage,
      UID_TwelveLeadECGWaveformStorage,
      UID_UltrasoundImageStorage,
      UID_UltrasoundMultiframeImageStorage,
      UID_VideoEndoscopicImageStorage,
      UID_VideoMicroscopicImageStorage,
      UID_VideoPhotographicImageStorage,
      UID_VisualAcuityMeasurementsStorage,
      UID_VLEndoscopicImageStorage,
      UID_VLMicroscopicImageStorage,
      UID_VLPhotographicImageStorage,
      UID_VLSlideCoordinatesMicroscopicImageStorage,
      UID_VLWholeSlideMicroscopyImageStorage,
      UID_XAXRFGrayscaleSoftcopyPresentationStateStorage,
      UID_XRay3DAngiographicImageStorage,
      UID_XRay3DCraniofacialImageStorage,
      UID_XRayAngiographicImageStorage,
      UID_XRayRadiationDoseSRStorage,
      UID_XRayRadiofluoroscopicImageStorage,
      // retired
      UID_RETIRED_HardcopyColorImageStorage,
      UID_RETIRED_HardcopyGrayscaleImageStorage,
      UID_RETIRED_NuclearMedicineImageStorage,
      UID_RETIRED_StandaloneCurveStorage,
      UID_RETIRED_StandaloneModalityLUTStorage,
      UID_RETIRED_StandaloneOverlayStorage,
      UID_RETIRED_StandalonePETCurveStorage,
      UID_RETIRED_StandaloneVOILUTStorage,
      UID_RETIRED_StoredPrintStorage,
      UID_RETIRED_UltrasoundImageStorage,
      UID_RETIRED_UltrasoundMultiframeImageStorage,
      UID_RETIRED_VLImageStorage,
      UID_RETIRED_VLMultiFrameImageStorage,
      UID_RETIRED_XRayAngiographicBiPlaneImageStorage,
      // draft
      UID_DRAFT_SRAudioStorage,
      UID_DRAFT_SRComprehensiveStorage,
      UID_DRAFT_SRDetailStorage,
      UID_DRAFT_SRTextStorage,
      UID_DRAFT_WaveformStorage,
      UID_DRAFT_RTBeamsDeliveryInstructionStorage,
      NULL
    };

    const int orthancStorageSOPClassUIDsCount = (sizeof(orthancStorageSOPClassUIDs) / sizeof(const char*)) - 1;



    OFCondition AssociationCleanup(T_ASC_Association *assoc)
    {
      OFCondition cond = ASC_dropSCPAssociation(assoc);
      if (cond.bad())
      {
        LOG(FATAL) << cond.text();
        return cond;
      }

      cond = ASC_destroyAssociation(&assoc);
      if (cond.bad())
      {
        LOG(FATAL) << cond.text();
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
        LOG(ERROR) << "Receiving Association failed: " << cond.text();
        // no matter what kind of error occurred, we need to do a cleanup
        AssociationCleanup(assoc);
        return NULL;
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
        if (ASC_getAPTitles(assoc->params, remoteAet_C, calledAet_C, NULL).bad() ||
            ASC_getPresentationAddresses(assoc->params, remoteIp_C, calledIP_C).bad())
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

      LOG(INFO) << "Association Received from AET " << remoteAet 
                << " on IP " << remoteIp;


      std::vector<const char*> transferSyntaxes;

      // This is the list of the transfer syntaxes that were supported up to Orthanc 0.7.1
      transferSyntaxes.push_back(UID_LittleEndianExplicitTransferSyntax);
      transferSyntaxes.push_back(UID_BigEndianExplicitTransferSyntax);
      transferSyntaxes.push_back(UID_LittleEndianImplicitTransferSyntax);

      // New transfer syntaxes supported since Orthanc 0.7.2
      if (!server.HasApplicationEntityFilter() ||
          server.GetApplicationEntityFilter().IsAllowedTransferSyntax(remoteIp, remoteAet, calledAet, TransferSyntax_Deflated))
      {
        transferSyntaxes.push_back(UID_DeflatedExplicitVRLittleEndianTransferSyntax); 
      }

      if (!server.HasApplicationEntityFilter() ||
          server.GetApplicationEntityFilter().IsAllowedTransferSyntax(remoteIp, remoteAet, calledAet, TransferSyntax_Jpeg))
      {
        transferSyntaxes.push_back(UID_JPEGProcess1TransferSyntax);
        transferSyntaxes.push_back(UID_JPEGProcess2_4TransferSyntax);
        transferSyntaxes.push_back(UID_JPEGProcess3_5TransferSyntax);
        transferSyntaxes.push_back(UID_JPEGProcess6_8TransferSyntax);
        transferSyntaxes.push_back(UID_JPEGProcess7_9TransferSyntax);
        transferSyntaxes.push_back(UID_JPEGProcess10_12TransferSyntax);
        transferSyntaxes.push_back(UID_JPEGProcess11_13TransferSyntax);
        transferSyntaxes.push_back(UID_JPEGProcess14TransferSyntax);
        transferSyntaxes.push_back(UID_JPEGProcess15TransferSyntax);
        transferSyntaxes.push_back(UID_JPEGProcess16_18TransferSyntax);
        transferSyntaxes.push_back(UID_JPEGProcess17_19TransferSyntax);
        transferSyntaxes.push_back(UID_JPEGProcess20_22TransferSyntax);
        transferSyntaxes.push_back(UID_JPEGProcess21_23TransferSyntax);
        transferSyntaxes.push_back(UID_JPEGProcess24_26TransferSyntax);
        transferSyntaxes.push_back(UID_JPEGProcess25_27TransferSyntax);
        transferSyntaxes.push_back(UID_JPEGProcess28TransferSyntax);
        transferSyntaxes.push_back(UID_JPEGProcess29TransferSyntax);
        transferSyntaxes.push_back(UID_JPEGProcess14SV1TransferSyntax);
        transferSyntaxes.push_back(UID_JPEGLSLosslessTransferSyntax);
        transferSyntaxes.push_back(UID_JPEGLSLossyTransferSyntax);
      }

      if (!server.HasApplicationEntityFilter() ||
          server.GetApplicationEntityFilter().IsAllowedTransferSyntax(remoteIp, remoteAet, calledAet, TransferSyntax_Jpeg2000))
      {
        transferSyntaxes.push_back(UID_JPEG2000LosslessOnlyTransferSyntax);
        transferSyntaxes.push_back(UID_JPEG2000TransferSyntax);
      }

      if (!server.HasApplicationEntityFilter() ||
          server.GetApplicationEntityFilter().IsAllowedTransferSyntax(remoteIp, remoteAet, calledAet, TransferSyntax_JpegLossless))
      {
        transferSyntaxes.push_back(UID_JPEG2000LosslessOnlyTransferSyntax);
        transferSyntaxes.push_back(UID_JPEG2000TransferSyntax);
        transferSyntaxes.push_back(UID_JPEG2000Part2MulticomponentImageCompressionLosslessOnlyTransferSyntax);
        transferSyntaxes.push_back(UID_JPEG2000Part2MulticomponentImageCompressionTransferSyntax);
      }

      if (!server.HasApplicationEntityFilter() ||
          server.GetApplicationEntityFilter().IsAllowedTransferSyntax(remoteIp, remoteAet, calledAet, TransferSyntax_Jpip))
      {
        transferSyntaxes.push_back(UID_JPIPReferencedTransferSyntax);
        transferSyntaxes.push_back(UID_JPIPReferencedDeflateTransferSyntax);
      }

      if (!server.HasApplicationEntityFilter() ||
          server.GetApplicationEntityFilter().IsAllowedTransferSyntax(remoteIp, remoteAet, calledAet, TransferSyntax_Mpeg2))
      {
        transferSyntaxes.push_back(UID_MPEG2MainProfileAtMainLevelTransferSyntax);
        transferSyntaxes.push_back(UID_MPEG2MainProfileAtHighLevelTransferSyntax);
      }

      if (!server.HasApplicationEntityFilter() ||
          server.GetApplicationEntityFilter().IsAllowedTransferSyntax(remoteIp, remoteAet, calledAet, TransferSyntax_Rle))
      {
        transferSyntaxes.push_back(UID_RLELosslessTransferSyntax);
      }

      /* accept the Verification SOP Class if presented */
      cond = ASC_acceptContextsWithPreferredTransferSyntaxes(assoc->params, &knownAbstractSyntaxes[0], knownAbstractSyntaxes.size(), &transferSyntaxes[0], transferSyntaxes.size());
      if (cond.bad())
      {
        LOG(INFO) << cond.text();
        AssociationCleanup(assoc);
        return NULL;
      }

      /* the array of Storage SOP Class UIDs comes from dcuid.h */
      cond = ASC_acceptContextsWithPreferredTransferSyntaxes(assoc->params, orthancStorageSOPClassUIDs, orthancStorageSOPClassUIDsCount, &transferSyntaxes[0], transferSyntaxes.size());
      if (cond.bad())
      {
        LOG(INFO) << cond.text();
        AssociationCleanup(assoc);
        return NULL;
      }

      if (!server.HasApplicationEntityFilter() ||
          server.GetApplicationEntityFilter().IsUnknownSopClassAccepted(remoteIp, remoteAet, calledAet))
      {
        /*
         * Promiscous mode is enabled: Accept everything not known not
         * to be a storage SOP class.
         **/
        cond = acceptUnknownContextsWithPreferredTransferSyntaxes(
          assoc->params, &transferSyntaxes[0], transferSyntaxes.size());
        if (cond.bad())
        {
          LOG(INFO) << cond.text();
          AssociationCleanup(assoc);
          return NULL;
        }
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

        LOG(INFO) << "Association Rejected: Bad Application Context Name: " << buf;
        cond = ASC_rejectAssociation(assoc, &rej);
        if (cond.bad())
        {
          LOG(INFO) << cond.text();
        }
        AssociationCleanup(assoc);
        return NULL;
      }

      /* check the AETs */
      if (!server.IsMyAETitle(calledAet))
      {
        LOG(WARNING) << "Rejected association, because of a bad called AET in the request (" << calledAet << ")";
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
        LOG(WARNING) << "Rejected association for remote AET " << remoteAet << " on IP " << remoteIp;
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

        LOG(INFO) << "Association Rejected: No Implementation Class UID provided";
        cond = ASC_rejectAssociation(assoc, &rej);
        if (cond.bad())
        {
          LOG(INFO) << cond.text();
        }
        AssociationCleanup(assoc);
        return NULL;
      }

      {
        cond = ASC_acknowledgeAssociation(assoc);
        if (cond.bad())
        {
          LOG(ERROR) << cond.text();
          AssociationCleanup(assoc);
          return NULL;
        }
        LOG(INFO) << "Association Acknowledged (Max Send PDV: " << assoc->sendPDVLength << ")";
        if (ASC_countAcceptedPresentationContexts(assoc->params) == 0)
          LOG(INFO) << "    (but no valid presentation contexts)";
      }

      IApplicationEntityFilter* filter = server.HasApplicationEntityFilter() ? &server.GetApplicationEntityFilter() : NULL;
      return new CommandDispatcher(server, assoc, remoteIp, remoteAet, calledAet, filter);
    }


    CommandDispatcher::CommandDispatcher(const DicomServer& server,
                                         T_ASC_Association* assoc,
                                         const std::string& remoteIp,
                                         const std::string& remoteAet,
                                         const std::string& calledAet,
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
        LOG(ERROR) << "Some association was not cleanly aborted";
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
        //LOG4CPP_WARN(Internals::GetLogger(), "Status Detail:" << OFendl << DcmObject::PrintHelper(*statusDetail));
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

          case DIMSE_C_FIND_RQ:
            request = DicomRequestType_Find;
            supported = true;
            break;

          default:
            // we cannot handle this kind of message
            cond = DIMSE_BADCOMMANDTYPE;
            LOG(ERROR) << "cannot handle command: 0x" << std::hex << msg.CommandField;
            break;
        }


        // Check whether this request is allowed by the security filter
        if (supported && 
            filter_ != NULL &&
            !filter_->IsAllowedRequest(remoteIp_, remoteAet_, calledAet_, request))
        {
          LOG(WARNING) << "Rejected " << EnumerationToString(request)
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
                std::auto_ptr<IStoreRequestHandler> handler
                  (server_.GetStoreRequestHandlerFactory().ConstructStoreRequestHandler());

                if (handler.get() != NULL)
                {
                  cond = Internals::storeScp(assoc_, &msg, presID, *handler, remoteIp_);
                }
              }
              break;

            case DicomRequestType_Move:
              if (server_.HasMoveRequestHandlerFactory()) // Should always be true
              {
                std::auto_ptr<IMoveRequestHandler> handler
                  (server_.GetMoveRequestHandlerFactory().ConstructMoveRequestHandler());

                if (handler.get() != NULL)
                {
                  cond = Internals::moveScp(assoc_, &msg, presID, *handler, remoteIp_, remoteAet_, calledAet_);
                }
              }
              break;

            case DicomRequestType_Find:
              if (server_.HasFindRequestHandlerFactory() || // Should always be true
                  server_.HasWorklistRequestHandlerFactory())
              {
                std::auto_ptr<IFindRequestHandler> findHandler;
                if (server_.HasFindRequestHandlerFactory())
                {
                  findHandler.reset(server_.GetFindRequestHandlerFactory().ConstructFindRequestHandler());
                }

                std::auto_ptr<IWorklistRequestHandler> worklistHandler;
                if (server_.HasWorklistRequestHandlerFactory())
                {
                  worklistHandler.reset(server_.GetWorklistRequestHandlerFactory().ConstructWorklistRequestHandler());
                }

                cond = Internals::findScp(assoc_, &msg, presID, server_.GetRemoteModalities(),
                                          findHandler.get(), worklistHandler.get(),
                                          remoteIp_, remoteAet_, calledAet_);
              }
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

        LOG(INFO) << cond.text();
      }
    
      if (finished)
      {
        if (cond == DUL_PEERREQUESTEDRELEASE)
        {
          LOG(INFO) << "Association Release";
          ASC_acknowledgeRelease(assoc_);
        }
        else if (cond == DUL_PEERABORTEDASSOCIATION)
        {
          LOG(INFO) << "Association Aborted";
        }
        else
        {
          OFString temp_str;
          LOG(INFO) << "DIMSE failure (aborting association): " << cond.text();
          /* some kind of error so abort the association */
          ASC_abortAssociation(assoc_);
        }
      }

      return !finished;
    }


    OFCondition EchoScp(T_ASC_Association * assoc, T_DIMSE_Message * msg, T_ASC_PresentationContextID presID)
    {
      OFString temp_str;
      LOG(INFO) << "Received Echo Request";
      //LOG(DEBUG) << DIMSE_dumpMessage(temp_str, msg->msg.CEchoRQ, DIMSE_INCOMING, NULL, presID));

      /* the echo succeeded !! */
      OFCondition cond = DIMSE_sendEchoResponse(assoc, presID, &msg->msg.CEchoRQ, STATUS_Success, NULL);
      if (cond.bad())
      {
        LOG(ERROR) << "Echo SCP Failed: " << cond.text();
      }
      return cond;
    }
  }
}
