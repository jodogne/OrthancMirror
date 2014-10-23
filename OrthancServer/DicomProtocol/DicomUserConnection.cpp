/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2014 Medical Physics Department, CHU of Liege,
 * Belgium
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


#include "../PrecompiledHeadersServer.h"
#include "DicomUserConnection.h"

#include "../../Core/OrthancException.h"
#include "../ToDcmtkBridge.h"
#include "../FromDcmtkBridge.h"

#include <dcmtk/dcmdata/dcistrmb.h>
#include <dcmtk/dcmdata/dcistrmf.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcmetinf.h>
#include <dcmtk/dcmnet/diutil.h>

#include <set>
#include <glog/logging.h>



#ifdef _WIN32
/**
 * "The maximum length, in bytes, of the string returned in the buffer 
 * pointed to by the name parameter is dependent on the namespace provider,
 * but this string must be 256 bytes or less.
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms738527(v=vs.85).aspx
 **/
#define HOST_NAME_MAX 256
#endif 


#if !defined(HOST_NAME_MAX) && defined(_POSIX_HOST_NAME_MAX)
/**
 * TO IMPROVE: "_POSIX_HOST_NAME_MAX is only the minimum value that
 * HOST_NAME_MAX can ever have [...] Therefore you cannot allocate an
 * array of size _POSIX_HOST_NAME_MAX, invoke gethostname() and expect
 * that the result will fit."
 * http://lists.gnu.org/archive/html/bug-gnulib/2009-08/msg00128.html
 **/
#define HOST_NAME_MAX _POSIX_HOST_NAME_MAX
#endif


static const char* DEFAULT_PREFERRED_TRANSFER_SYNTAX = UID_LittleEndianImplicitTransferSyntax;

/**
 * "If we have more than 64 storage SOP classes, tools such as
 * storescu will fail because they attempt to negotiate two
 * presentation contexts for each SOP class, and there is a total
 * limit of 128 contexts for one association."
 **/
static const unsigned int MAXIMUM_STORAGE_SOP_CLASSES = 64;


namespace Orthanc
{
  struct DicomUserConnection::PImpl
  {
    // Connection state
    uint32_t dimseTimeout_;
    uint32_t acseTimeout_;
    T_ASC_Network* net_;
    T_ASC_Parameters* params_;
    T_ASC_Association* assoc_;

    bool IsOpen() const
    {
      return assoc_ != NULL;
    }

    void CheckIsOpen() const;

    void Store(DcmInputStream& is, DicomUserConnection& connection);
  };


  static void Check(const OFCondition& cond)
  {
    if (cond.bad())
    {
      LOG(ERROR) << "DicomUserConnection: " << std::string(cond.text());
       throw OrthancException(ErrorCode_NetworkProtocol);
    }
  }

  void DicomUserConnection::PImpl::CheckIsOpen() const
  {
    if (!IsOpen())
    {
      LOG(ERROR) << "DicomUserConnection: First open the connection";
      throw OrthancException(ErrorCode_NetworkProtocol);
    }
  }


  void DicomUserConnection::CheckIsOpen() const
  {
    pimpl_->CheckIsOpen();
  }


  static void RegisterStorageSOPClass(T_ASC_Parameters* params,
                                      unsigned int& presentationContextId,
                                      const std::string& sopClass,
                                      const char* asPreferred[],
                                      std::vector<const char*>& asFallback)
  {
    Check(ASC_addPresentationContext(params, presentationContextId, 
                                     sopClass.c_str(), asPreferred, 1));
    presentationContextId += 2;

    if (asFallback.size() > 0)
    {
      Check(ASC_addPresentationContext(params, presentationContextId, 
                                       sopClass.c_str(), &asFallback[0], asFallback.size()));
      presentationContextId += 2;
    }
  }
  
    
  void DicomUserConnection::SetupPresentationContexts(const std::string& preferredTransferSyntax)
  {
    // Flatten an array with the preferred transfer syntax
    const char* asPreferred[1] = { preferredTransferSyntax.c_str() };

    // Setup the fallback transfer syntaxes
    std::set<std::string> fallbackSyntaxes;
    fallbackSyntaxes.insert(UID_LittleEndianExplicitTransferSyntax);
    fallbackSyntaxes.insert(UID_BigEndianExplicitTransferSyntax);
    fallbackSyntaxes.insert(UID_LittleEndianImplicitTransferSyntax);
    fallbackSyntaxes.erase(preferredTransferSyntax);

    // Flatten an array with the fallback transfer syntaxes
    std::vector<const char*> asFallback;
    asFallback.reserve(fallbackSyntaxes.size());
    for (std::set<std::string>::const_iterator 
           it = fallbackSyntaxes.begin(); it != fallbackSyntaxes.end(); ++it)
    {
      asFallback.push_back(it->c_str());
    }

    CheckStorageSOPClassesInvariant();
    unsigned int presentationContextId = 1;

    for (std::list<std::string>::const_iterator it = reservedStorageSOPClasses_.begin();
         it != reservedStorageSOPClasses_.end(); ++it)
    {
      RegisterStorageSOPClass(pimpl_->params_, presentationContextId, 
                              *it, asPreferred, asFallback);
    }

    for (std::set<std::string>::const_iterator it = storageSOPClasses_.begin();
         it != storageSOPClasses_.end(); ++it)
    {
      RegisterStorageSOPClass(pimpl_->params_, presentationContextId, 
                              *it, asPreferred, asFallback);
    }

    for (std::set<std::string>::const_iterator it = defaultStorageSOPClasses_.begin();
         it != defaultStorageSOPClasses_.end(); ++it)
    {
      RegisterStorageSOPClass(pimpl_->params_, presentationContextId, 
                              *it, asPreferred, asFallback);
    }
  }


  static bool IsGenericTransferSyntax(const std::string& syntax)
  {
    return (syntax == UID_LittleEndianExplicitTransferSyntax ||
            syntax == UID_BigEndianExplicitTransferSyntax ||
            syntax == UID_LittleEndianImplicitTransferSyntax);
  }


  void DicomUserConnection::PImpl::Store(DcmInputStream& is, DicomUserConnection& connection)
  {
    CheckIsOpen();

    DcmFileFormat dcmff;
    Check(dcmff.read(is, EXS_Unknown, EGL_noChange, DCM_MaxReadLength));

    // Determine the storage SOP class UID for this instance
    static const DcmTagKey DCM_SOP_CLASS_UID(0x0008, 0x0016);
    OFString sopClassUid;
    if (dcmff.getDataset()->findAndGetOFString(DCM_SOP_CLASS_UID, sopClassUid).good())
    {
      connection.AddStorageSOPClass(sopClassUid.c_str());
    }

    // Determine whether a new presentation context must be
    // negotiated, depending on the transfer syntax of this instance
    DcmXfer xfer(dcmff.getDataset()->getOriginalXfer());
    const std::string syntax(xfer.getXferID());
    bool isGeneric = IsGenericTransferSyntax(syntax);

    if (isGeneric ^ IsGenericTransferSyntax(connection.GetPreferredTransferSyntax()))
    {
      // Making a generic-to-specific or specific-to-generic change of
      // the transfer syntax. Renegotiate the connection.
      LOG(INFO) << "Change in the transfer syntax: the C-Store associated must be renegotiated";

      if (isGeneric)
      {
        connection.ResetPreferredTransferSyntax();
      }
      else
      {
        connection.SetPreferredTransferSyntax(syntax);
      }
    }

    if (!connection.IsOpen())
    {
      LOG(INFO) << "Renegotiating a C-Store association due to a change in the parameters";
      connection.Open();
    }

    // Figure out which SOP class and SOP instance is encapsulated in the file
    DIC_UI sopClass;
    DIC_UI sopInstance;
    if (!DU_findSOPClassAndInstanceInDataSet(dcmff.getDataset(), sopClass, sopInstance))
    {
      throw OrthancException("DicomUserConnection: Unable to find the SOP class and instance");
    }

    // Figure out which of the accepted presentation contexts should be used
    int presID = ASC_findAcceptedPresentationContextID(assoc_, sopClass);
    if (presID == 0)
    {
      const char *modalityName = dcmSOPClassUIDToModality(sopClass);
      if (!modalityName) modalityName = dcmFindNameOfUID(sopClass);
      if (!modalityName) modalityName = "unknown SOP class";
      throw OrthancException("DicomUserConnection: No presentation context for modality " + 
                             std::string(modalityName));
    }

    // Prepare the transmission of data
    T_DIMSE_C_StoreRQ req;
    memset(&req, 0, sizeof(req));
    req.MessageID = assoc_->nextMsgID++;
    strcpy(req.AffectedSOPClassUID, sopClass);
    strcpy(req.AffectedSOPInstanceUID, sopInstance);
    req.DataSetType = DIMSE_DATASET_PRESENT;
    req.Priority = DIMSE_PRIORITY_MEDIUM;

    // Finally conduct transmission of data
    T_DIMSE_C_StoreRSP rsp;
    DcmDataset* statusDetail = NULL;
    Check(DIMSE_storeUser(assoc_, presID, &req,
                          NULL, dcmff.getDataset(), /*progressCallback*/ NULL, NULL,
                          /*opt_blockMode*/ DIMSE_BLOCKING, /*opt_dimse_timeout*/ dimseTimeout_,
                          &rsp, &statusDetail, NULL));

    if (statusDetail != NULL) 
    {
      delete statusDetail;
    }
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
    DicomFindAnswers& answers = *reinterpret_cast<DicomFindAnswers*>(callbackData);

    if (responseIdentifiers != NULL)
    {
      DicomMap m;
      FromDcmtkBridge::Convert(m, *responseIdentifiers);
      answers.Add(m);
    }
  }

  void DicomUserConnection::Find(DicomFindAnswers& result,
                                 FindRootModel model,
                                 const DicomMap& fields)
  {
    CheckIsOpen();

    const char* sopClass;
    std::auto_ptr<DcmDataset> dataset(ToDcmtkBridge::Convert(fields));
    switch (model)
    {
      case FindRootModel_Patient:
        DU_putStringDOElement(dataset.get(), DcmTagKey(0x0008, 0x0052), "PATIENT");
        sopClass = UID_FINDPatientRootQueryRetrieveInformationModel;
      
        // Accession number
        if (!fields.HasTag(0x0008, 0x0050))
          DU_putStringDOElement(dataset.get(), DcmTagKey(0x0008, 0x0050), "");

        // Patient ID
        if (!fields.HasTag(0x0010, 0x0020))
          DU_putStringDOElement(dataset.get(), DcmTagKey(0x0010, 0x0020), "");

        break;

      case FindRootModel_Study:
        DU_putStringDOElement(dataset.get(), DcmTagKey(0x0008, 0x0052), "STUDY");
        sopClass = UID_FINDStudyRootQueryRetrieveInformationModel;

        // Accession number
        if (!fields.HasTag(0x0008, 0x0050))
          DU_putStringDOElement(dataset.get(), DcmTagKey(0x0008, 0x0050), "");

        // Study instance UID
        if (!fields.HasTag(0x0020, 0x000d))
          DU_putStringDOElement(dataset.get(), DcmTagKey(0x0020, 0x000d), "");

        break;

      case FindRootModel_Series:
        DU_putStringDOElement(dataset.get(), DcmTagKey(0x0008, 0x0052), "SERIES");
        sopClass = UID_FINDStudyRootQueryRetrieveInformationModel;

        // Accession number
        if (!fields.HasTag(0x0008, 0x0050))
          DU_putStringDOElement(dataset.get(), DcmTagKey(0x0008, 0x0050), "");

        // Study instance UID
        if (!fields.HasTag(0x0020, 0x000d))
          DU_putStringDOElement(dataset.get(), DcmTagKey(0x0020, 0x000d), "");

        // Series instance UID
        if (!fields.HasTag(0x0020, 0x000e))
          DU_putStringDOElement(dataset.get(), DcmTagKey(0x0020, 0x000e), "");

        break;

      case FindRootModel_Instance:
        if (manufacturer_ == ModalityManufacturer_ClearCanvas ||
            manufacturer_ == ModalityManufacturer_Dcm4Chee)
        {
          // This is a particular case for ClearCanvas, thanks to Peter Somlo <peter.somlo@gmail.com>.
          // https://groups.google.com/d/msg/orthanc-users/j-6C3MAVwiw/iolB9hclom8J
          // http://www.clearcanvas.ca/Home/Community/OldForums/tabid/526/aff/11/aft/14670/afv/topic/Default.aspx
          DU_putStringDOElement(dataset.get(), DcmTagKey(0x0008, 0x0052), "IMAGE");
        }
        else
        {
          DU_putStringDOElement(dataset.get(), DcmTagKey(0x0008, 0x0052), "INSTANCE");
        }

        sopClass = UID_FINDStudyRootQueryRetrieveInformationModel;

        // Accession number
        if (!fields.HasTag(0x0008, 0x0050))
          DU_putStringDOElement(dataset.get(), DcmTagKey(0x0008, 0x0050), "");

        // Study instance UID
        if (!fields.HasTag(0x0020, 0x000d))
          DU_putStringDOElement(dataset.get(), DcmTagKey(0x0020, 0x000d), "");

        // Series instance UID
        if (!fields.HasTag(0x0020, 0x000e))
          DU_putStringDOElement(dataset.get(), DcmTagKey(0x0020, 0x000e), "");

        // SOP Instance UID
        if (!fields.HasTag(0x0008, 0x0018))
          DU_putStringDOElement(dataset.get(), DcmTagKey(0x0008, 0x0018), "");

        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    // Figure out which of the accepted presentation contexts should be used
    int presID = ASC_findAcceptedPresentationContextID(pimpl_->assoc_, sopClass);
    if (presID == 0)
    {
      throw OrthancException("DicomUserConnection: The C-FIND command is not supported by the distant AET");
    }

    T_DIMSE_C_FindRQ request;
    memset(&request, 0, sizeof(request));
    request.MessageID = pimpl_->assoc_->nextMsgID++;
    strcpy(request.AffectedSOPClassUID, sopClass);
    request.DataSetType = DIMSE_DATASET_PRESENT;
    request.Priority = DIMSE_PRIORITY_MEDIUM;

    T_DIMSE_C_FindRSP response;
    DcmDataset* statusDetail = NULL;
    OFCondition cond = DIMSE_findUser(pimpl_->assoc_, presID, &request, dataset.get(),
                                      FindCallback, &result,
                                      /*opt_blockMode*/ DIMSE_BLOCKING, 
                                      /*opt_dimse_timeout*/ pimpl_->dimseTimeout_,
                                      &response, &statusDetail);

    if (statusDetail)
    {
      delete statusDetail;
    }

    Check(cond);
  }


  void DicomUserConnection::FindPatient(DicomFindAnswers& result,
                                        const DicomMap& fields)
  {
    // Only keep the filters from "fields" that are related to the patient
    DicomMap s;
    fields.ExtractPatientInformation(s);
    Find(result, FindRootModel_Patient, s);
  }

  void DicomUserConnection::FindStudy(DicomFindAnswers& result,
                                      const DicomMap& fields)
  {
    // Only keep the filters from "fields" that are related to the study
    DicomMap s;
    fields.ExtractStudyInformation(s);

    s.CopyTagIfExists(fields, DICOM_TAG_PATIENT_ID);
    s.CopyTagIfExists(fields, DICOM_TAG_ACCESSION_NUMBER);
    s.CopyTagIfExists(fields, DICOM_TAG_MODALITIES_IN_STUDY);

    Find(result, FindRootModel_Study, s);
  }

  void DicomUserConnection::FindSeries(DicomFindAnswers& result,
                                       const DicomMap& fields)
  {
    // Only keep the filters from "fields" that are related to the series
    DicomMap s;
    fields.ExtractSeriesInformation(s);

    s.CopyTagIfExists(fields, DICOM_TAG_PATIENT_ID);
    s.CopyTagIfExists(fields, DICOM_TAG_ACCESSION_NUMBER);
    s.CopyTagIfExists(fields, DICOM_TAG_STUDY_INSTANCE_UID);

    Find(result, FindRootModel_Series, s);
  }

  void DicomUserConnection::FindInstance(DicomFindAnswers& result,
                                         const DicomMap& fields)
  {
    // Only keep the filters from "fields" that are related to the instance
    DicomMap s;
    fields.ExtractInstanceInformation(s);

    s.CopyTagIfExists(fields, DICOM_TAG_PATIENT_ID);
    s.CopyTagIfExists(fields, DICOM_TAG_ACCESSION_NUMBER);
    s.CopyTagIfExists(fields, DICOM_TAG_STUDY_INSTANCE_UID);
    s.CopyTagIfExists(fields, DICOM_TAG_SERIES_INSTANCE_UID);

    Find(result, FindRootModel_Instance, s);
  }


  void DicomUserConnection::Move(const std::string& targetAet,
                                 const DicomMap& fields)
  {
    CheckIsOpen();

    const char* sopClass = UID_MOVEStudyRootQueryRetrieveInformationModel;
    std::auto_ptr<DcmDataset> dataset(ToDcmtkBridge::Convert(fields));

    // Figure out which of the accepted presentation contexts should be used
    int presID = ASC_findAcceptedPresentationContextID(pimpl_->assoc_, sopClass);
    if (presID == 0)
    {
      throw OrthancException("DicomUserConnection: The C-MOVE command is not supported by the distant AET");
    }

    T_DIMSE_C_MoveRQ request;
    memset(&request, 0, sizeof(request));
    request.MessageID = pimpl_->assoc_->nextMsgID++;
    strcpy(request.AffectedSOPClassUID, sopClass);
    request.DataSetType = DIMSE_DATASET_PRESENT;
    request.Priority = DIMSE_PRIORITY_MEDIUM;
    strncpy(request.MoveDestination, targetAet.c_str(), sizeof(DIC_AE) / sizeof(char));

    T_DIMSE_C_MoveRSP response;
    DcmDataset* statusDetail = NULL;
    DcmDataset* responseIdentifiers = NULL;
    OFCondition cond = DIMSE_moveUser(pimpl_->assoc_, presID, &request, dataset.get(),
                                      NULL, NULL,
                                      /*opt_blockMode*/ DIMSE_BLOCKING, 
                                      /*opt_dimse_timeout*/ pimpl_->dimseTimeout_,
                                      pimpl_->net_, NULL, NULL,
                                      &response, &statusDetail, &responseIdentifiers);

    if (statusDetail)
    {
      delete statusDetail;
    }

    if (responseIdentifiers)
    {
      delete responseIdentifiers;
    }

    Check(cond);
  }


  void DicomUserConnection::ResetStorageSOPClasses()
  {
    CheckStorageSOPClassesInvariant();

    storageSOPClasses_.clear();
    defaultStorageSOPClasses_.clear();

    // Copy the short list of storage SOP classes from DCMTK, making
    // room for the 4 SOP classes reserved for C-ECHO, C-FIND, C-MOVE.

    std::set<std::string> uncommon;
    uncommon.insert(UID_BlendingSoftcopyPresentationStateStorage);
    uncommon.insert(UID_GrayscaleSoftcopyPresentationStateStorage);
    uncommon.insert(UID_ColorSoftcopyPresentationStateStorage);
    uncommon.insert(UID_PseudoColorSoftcopyPresentationStateStorage);

    // Add the storage syntaxes for C-STORE
    for (int i = 0; i < numberOfDcmShortSCUStorageSOPClassUIDs - 1; i++)
    {
      if (uncommon.find(dcmShortSCUStorageSOPClassUIDs[i]) == uncommon.end())
      {
        defaultStorageSOPClasses_.insert(dcmShortSCUStorageSOPClassUIDs[i]);
      }
    }

    CheckStorageSOPClassesInvariant();
  }


  DicomUserConnection::DicomUserConnection() : 
    pimpl_(new PImpl),
    preferredTransferSyntax_(DEFAULT_PREFERRED_TRANSFER_SYNTAX),
    localAet_("STORESCU"),
    distantAet_("ANY-SCP"),
    distantHost_("127.0.0.1")
  {
    distantPort_ = 104;
    manufacturer_ = ModalityManufacturer_Generic;

    SetTimeout(10); 
    pimpl_->net_ = NULL;
    pimpl_->params_ = NULL;
    pimpl_->assoc_ = NULL;

    // SOP classes for C-ECHO, C-FIND and C-MOVE
    reservedStorageSOPClasses_.push_back(UID_VerificationSOPClass);
    reservedStorageSOPClasses_.push_back(UID_FINDPatientRootQueryRetrieveInformationModel);
    reservedStorageSOPClasses_.push_back(UID_FINDStudyRootQueryRetrieveInformationModel);
    reservedStorageSOPClasses_.push_back(UID_MOVEStudyRootQueryRetrieveInformationModel);

    ResetStorageSOPClasses();
  }

  DicomUserConnection::~DicomUserConnection()
  {
    Close();
  }


  void DicomUserConnection::Connect(const RemoteModalityParameters& parameters)
  {
    SetDistantApplicationEntityTitle(parameters.GetApplicationEntityTitle());
    SetDistantHost(parameters.GetHost());
    SetDistantPort(parameters.GetPort());
    SetDistantManufacturer(parameters.GetManufacturer());
  }


  void DicomUserConnection::SetLocalApplicationEntityTitle(const std::string& aet)
  {
    if (localAet_ != aet)
    {
      Close();
      localAet_ = aet;
    }
  }

  void DicomUserConnection::SetDistantApplicationEntityTitle(const std::string& aet)
  {
    if (distantAet_ != aet)
    {
      Close();
      distantAet_ = aet;
    }
  }

  void DicomUserConnection::SetDistantManufacturer(ModalityManufacturer manufacturer)
  {
    if (manufacturer_ != manufacturer)
    {
      Close();
      manufacturer_ = manufacturer;
    }
  }

  void DicomUserConnection::ResetPreferredTransferSyntax()
  {
    SetPreferredTransferSyntax(DEFAULT_PREFERRED_TRANSFER_SYNTAX);
  }

  void DicomUserConnection::SetPreferredTransferSyntax(const std::string& preferredTransferSyntax)
  {
    if (preferredTransferSyntax_ != preferredTransferSyntax)
    {
      Close();
      preferredTransferSyntax_ = preferredTransferSyntax;
    }
  }


  void DicomUserConnection::SetDistantHost(const std::string& host)
  {
    if (distantHost_ != host)
    {
      if (host.size() > HOST_NAME_MAX - 10)
      {
        throw OrthancException("Distant host name is too long");
      }

      Close();
      distantHost_ = host;
    }
  }

  void DicomUserConnection::SetDistantPort(uint16_t port)
  {
    if (distantPort_ != port)
    {
      Close();
      distantPort_ = port;
    }
  }

  void DicomUserConnection::Open()
  {
    if (IsOpen())
    {
      // Don't reopen the connection
      return;
    }

    LOG(INFO) << "Opening a DICOM SCU connection from AET \"" << GetLocalApplicationEntityTitle() 
              << "\" to AET \"" << GetDistantApplicationEntityTitle() << "\" on host "
              << GetDistantHost() << ":" << GetDistantPort() 
              << " (manufacturer: " << EnumerationToString(GetDistantManufacturer()) << ")";

    Check(ASC_initializeNetwork(NET_REQUESTOR, 0, /*opt_acse_timeout*/ pimpl_->acseTimeout_, &pimpl_->net_));
    Check(ASC_createAssociationParameters(&pimpl_->params_, /*opt_maxReceivePDULength*/ ASC_DEFAULTMAXPDU));

    // Set this application's title and the called application's title in the params
    Check(ASC_setAPTitles(pimpl_->params_, localAet_.c_str(), distantAet_.c_str(), NULL));

    // Set the network addresses of the local and distant entities
    char localHost[HOST_NAME_MAX];
    gethostname(localHost, HOST_NAME_MAX - 1);

    char distantHostAndPort[HOST_NAME_MAX];

#ifdef _MSC_VER
    _snprintf
#else
      snprintf
#endif
      (distantHostAndPort, HOST_NAME_MAX - 1, "%s:%d", distantHost_.c_str(), distantPort_);

    Check(ASC_setPresentationAddresses(pimpl_->params_, localHost, distantHostAndPort));

    // Set various options
    Check(ASC_setTransportLayerType(pimpl_->params_, /*opt_secureConnection*/ false));

    SetupPresentationContexts(preferredTransferSyntax_);

    // Do the association
    Check(ASC_requestAssociation(pimpl_->net_, pimpl_->params_, &pimpl_->assoc_));

    if (ASC_countAcceptedPresentationContexts(pimpl_->params_) == 0)
    {
      throw OrthancException("DicomUserConnection: No Acceptable Presentation Contexts");
    }
  }

  void DicomUserConnection::Close()
  {
    if (pimpl_->assoc_ != NULL)
    {
      ASC_releaseAssociation(pimpl_->assoc_);
      ASC_destroyAssociation(&pimpl_->assoc_);
      pimpl_->assoc_ = NULL;
      pimpl_->params_ = NULL;
    }
    else
    {
      if (pimpl_->params_ != NULL)
      {
        ASC_destroyAssociationParameters(&pimpl_->params_);
        pimpl_->params_ = NULL;
      }
    }

    if (pimpl_->net_ != NULL)
    {
      ASC_dropNetwork(&pimpl_->net_);
      pimpl_->net_ = NULL;
    }
  }

  bool DicomUserConnection::IsOpen() const
  {
    return pimpl_->IsOpen();
  }

  void DicomUserConnection::Store(const char* buffer, size_t size)
  {
    // Prepare an input stream for the memory buffer
    DcmInputBufferStream is;
    if (size > 0)
      is.setBuffer(buffer, size);
    is.setEos();
      
    pimpl_->Store(is, *this);
  }

  void DicomUserConnection::Store(const std::string& buffer)
  {
    if (buffer.size() > 0)
      Store(reinterpret_cast<const char*>(&buffer[0]), buffer.size());
    else
      Store(NULL, 0);
  }

  void DicomUserConnection::StoreFile(const std::string& path)
  {
    // Prepare an input stream for the file
    DcmInputFileStream is(path.c_str());
    pimpl_->Store(is, *this);
  }

  bool DicomUserConnection::Echo()
  {
    CheckIsOpen();
    DIC_US status;
    Check(DIMSE_echoUser(pimpl_->assoc_, pimpl_->assoc_->nextMsgID++, 
                         /*opt_blockMode*/ DIMSE_BLOCKING, 
                         /*opt_dimse_timeout*/ pimpl_->dimseTimeout_,
                         &status, NULL));
    return status == STATUS_Success;
  }


  void DicomUserConnection::MoveSeries(const std::string& targetAet,
                                       const DicomMap& findResult)
  {
    DicomMap simplified;
    simplified.SetValue(DICOM_TAG_STUDY_INSTANCE_UID, findResult.GetValue(DICOM_TAG_STUDY_INSTANCE_UID));
    simplified.SetValue(DICOM_TAG_SERIES_INSTANCE_UID, findResult.GetValue(DICOM_TAG_SERIES_INSTANCE_UID));
    Move(targetAet, simplified);
  }

  void DicomUserConnection::MoveSeries(const std::string& targetAet,
                                       const std::string& studyUid,
                                       const std::string& seriesUid)
  {
    DicomMap map;
    map.SetValue(DICOM_TAG_STUDY_INSTANCE_UID, studyUid);
    map.SetValue(DICOM_TAG_SERIES_INSTANCE_UID, seriesUid);
    Move(targetAet, map);
  }

  void DicomUserConnection::MoveInstance(const std::string& targetAet,
                                         const DicomMap& findResult)
  {
    DicomMap simplified;
    simplified.SetValue(DICOM_TAG_STUDY_INSTANCE_UID, findResult.GetValue(DICOM_TAG_STUDY_INSTANCE_UID));
    simplified.SetValue(DICOM_TAG_SERIES_INSTANCE_UID, findResult.GetValue(DICOM_TAG_SERIES_INSTANCE_UID));
    simplified.SetValue(DICOM_TAG_SOP_INSTANCE_UID, findResult.GetValue(DICOM_TAG_SOP_INSTANCE_UID));
    Move(targetAet, simplified);
  }

  void DicomUserConnection::MoveInstance(const std::string& targetAet,
                                         const std::string& studyUid,
                                         const std::string& seriesUid,
                                         const std::string& instanceUid)
  {
    DicomMap map;
    map.SetValue(DICOM_TAG_STUDY_INSTANCE_UID, studyUid);
    map.SetValue(DICOM_TAG_SERIES_INSTANCE_UID, seriesUid);
    map.SetValue(DICOM_TAG_SOP_INSTANCE_UID, instanceUid);
    Move(targetAet, map);
  }


  void DicomUserConnection::SetTimeout(uint32_t seconds)
  {
    if (seconds <= 0)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    dcmConnectionTimeout.set(seconds);
    pimpl_->dimseTimeout_ = seconds;
    pimpl_->acseTimeout_ = 10;
  }


  void DicomUserConnection::DisableTimeout()
  {
    /**
     * Global timeout (seconds) for connecting to remote hosts.
     * Default value is -1 which selects infinite timeout, i.e. blocking connect().
     */
    dcmConnectionTimeout.set(-1);
    pimpl_->dimseTimeout_ = 0;
    pimpl_->acseTimeout_ = 10;
  }


  void DicomUserConnection::CheckStorageSOPClassesInvariant() const
  {
    assert(storageSOPClasses_.size() + 
           defaultStorageSOPClasses_.size() + 
           reservedStorageSOPClasses_.size() <= MAXIMUM_STORAGE_SOP_CLASSES);
  }

  void DicomUserConnection::AddStorageSOPClass(const char* sop)
  {
    CheckStorageSOPClassesInvariant();

    if (storageSOPClasses_.find(sop) != storageSOPClasses_.end())
    {
      // This storage SOP class is already explicitly registered. Do
      // nothing.
      return;
    }

    if (defaultStorageSOPClasses_.find(sop) != defaultStorageSOPClasses_.end())
    {
      // This storage SOP class is not explicitly registered, but is
      // used by default. Just register it explicitly.
      defaultStorageSOPClasses_.erase(sop);
      storageSOPClasses_.insert(sop);

      CheckStorageSOPClassesInvariant();
      return;
    }

    // This storage SOP class is neither explicitly, nor implicitly
    // registered. Close the connection and register it explicitly.

    Close();

    if (reservedStorageSOPClasses_.size() + 
        storageSOPClasses_.size() >= MAXIMUM_STORAGE_SOP_CLASSES)  // (*)
    {
      // The maximum number of SOP classes is reached
      ResetStorageSOPClasses();
      defaultStorageSOPClasses_.erase(sop);
    }
    else if (reservedStorageSOPClasses_.size() + storageSOPClasses_.size() + 
             defaultStorageSOPClasses_.size() >= MAXIMUM_STORAGE_SOP_CLASSES)
    {
      // Make room in the default storage syntaxes
      assert(defaultStorageSOPClasses_.size() > 0);  // Necessarily true because condition (*) is false
      defaultStorageSOPClasses_.erase(*defaultStorageSOPClasses_.rbegin());
    }

    // Explicitly register the new storage syntax
    storageSOPClasses_.insert(sop);

    CheckStorageSOPClassesInvariant();
  }

}
