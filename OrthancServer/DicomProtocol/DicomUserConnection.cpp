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
#define HOST_NAME_MAX _POSIX_HOST_NAME_MAX
#endif


static const char* DEFAULT_PREFERRED_TRANSFER_SYNTAX = UID_LittleEndianImplicitTransferSyntax;

namespace Orthanc
{
  struct DicomUserConnection::PImpl
  {
    // Connection state
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
      throw OrthancException("DicomUserConnection: " + std::string(cond.text()));
    }
  }

  void DicomUserConnection::PImpl::CheckIsOpen() const
  {
    if (!IsOpen())
    {
      throw OrthancException("DicomUserConnection: First open the connection");
    }
  }


  void DicomUserConnection::CheckIsOpen() const
  {
    pimpl_->CheckIsOpen();
  }


  void DicomUserConnection::CopyParameters(const DicomUserConnection& other)
  {
    Close();
    localAet_ = other.localAet_;
    distantAet_ = other.distantAet_;
    distantHost_ = other.distantHost_;
    distantPort_ = other.distantPort_;
    manufacturer_ = other.manufacturer_;
    preferredTransferSyntax_ = other.preferredTransferSyntax_;
  }


  void DicomUserConnection::SetupPresentationContexts(const std::string& preferredTransferSyntax)
  {
    // Fallback transfer syntaxes
    std::set<std::string> fallbackSyntaxes;
    fallbackSyntaxes.insert(UID_LittleEndianExplicitTransferSyntax);
    fallbackSyntaxes.insert(UID_BigEndianExplicitTransferSyntax);
    fallbackSyntaxes.insert(UID_LittleEndianImplicitTransferSyntax);

    // Transfer syntaxes for C-ECHO, C-FIND and C-MOVE
    std::vector<std::string> transferSyntaxes;
    transferSyntaxes.push_back(UID_VerificationSOPClass);
    transferSyntaxes.push_back(UID_FINDPatientRootQueryRetrieveInformationModel);
    transferSyntaxes.push_back(UID_FINDStudyRootQueryRetrieveInformationModel);
    transferSyntaxes.push_back(UID_MOVEStudyRootQueryRetrieveInformationModel);

    // TODO: Allow the set below to be configured
    std::set<std::string> uselessSyntaxes;
    uselessSyntaxes.insert(UID_BlendingSoftcopyPresentationStateStorage);
    uselessSyntaxes.insert(UID_GrayscaleSoftcopyPresentationStateStorage);
    uselessSyntaxes.insert(UID_ColorSoftcopyPresentationStateStorage);
    uselessSyntaxes.insert(UID_PseudoColorSoftcopyPresentationStateStorage);

    // Add the transfer syntaxes for C-STORE
    for (int i = 0; i < numberOfDcmShortSCUStorageSOPClassUIDs - 1; i++)
    {
      // Test to make some room to allow the ECHO and FIND requests
      if (uselessSyntaxes.find(dcmShortSCUStorageSOPClassUIDs[i]) == uselessSyntaxes.end())
      {
        transferSyntaxes.push_back(dcmShortSCUStorageSOPClassUIDs[i]);
      }
    }

    // Flatten the fallback transfer syntaxes array
    const char* asPreferred[1] = { preferredTransferSyntax.c_str() };

    fallbackSyntaxes.erase(preferredTransferSyntax);

    std::vector<const char*> asFallback;
    asFallback.reserve(fallbackSyntaxes.size());
    for (std::set<std::string>::const_iterator 
           it = fallbackSyntaxes.begin(); it != fallbackSyntaxes.end(); ++it)
    {
      asFallback.push_back(it->c_str());
    }

    unsigned int presentationContextId = 1;
    for (size_t i = 0; i < transferSyntaxes.size(); i++)
    {
      Check(ASC_addPresentationContext(pimpl_->params_, presentationContextId, 
                                       transferSyntaxes[i].c_str(), asPreferred, 1));
      presentationContextId += 2;

      if (asFallback.size() > 0)
      {
        Check(ASC_addPresentationContext(pimpl_->params_, presentationContextId, 
                                         transferSyntaxes[i].c_str(), &asFallback[0], asFallback.size()));
        presentationContextId += 2;
      }
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

    // Determine whether a new presentation context must be
    // negociated, depending on the transfer syntax of this instance
    DcmXfer xfer(dcmff.getDataset()->getOriginalXfer());
    const std::string syntax(xfer.getXferID());
    bool isGeneric = IsGenericTransferSyntax(syntax);

    if (isGeneric ^ IsGenericTransferSyntax(connection.GetPreferredTransferSyntax()))
    {
      // Making a generic-to-specific or specific-to-generic change of
      // the transfer syntax. Renegociate the connection.
      LOG(INFO) << "Renegociating a C-Store association due to a change in the transfer syntax";

      if (isGeneric)
      {
        connection.ResetPreferredTransferSyntax();
      }
      else
      {
        connection.SetPreferredTransferSyntax(syntax);
      }

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
                          /*opt_blockMode*/ DIMSE_BLOCKING, /*opt_dimse_timeout*/ 0,
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
      if (manufacturer_ == ModalityManufacturer_ClearCanvas)
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
                                      /*opt_blockMode*/ DIMSE_BLOCKING, /*opt_dimse_timeout*/ 0,
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
                                      /*opt_blockMode*/ DIMSE_BLOCKING, /*opt_dimse_timeout*/ 0,
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


  DicomUserConnection::DicomUserConnection() : 
    pimpl_(new PImpl),
    preferredTransferSyntax_(DEFAULT_PREFERRED_TRANSFER_SYNTAX),
    localAet_("STORESCU"),
    distantAet_("ANY-SCP"),
    distantHost_("127.0.0.1")
  {
    distantPort_ = 104;
    manufacturer_ = ModalityManufacturer_Generic;

    pimpl_->net_ = NULL;
    pimpl_->params_ = NULL;
    pimpl_->assoc_ = NULL;
  }

  DicomUserConnection::~DicomUserConnection()
  {
    Close();
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

    Check(ASC_initializeNetwork(NET_REQUESTOR, 0, /*opt_acse_timeout*/ 30, &pimpl_->net_));
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
                         /*opt_blockMode*/ DIMSE_BLOCKING, /*opt_dimse_timeout*/ 0,
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

  void DicomUserConnection::SetConnectionTimeout(uint32_t seconds)
  {
    dcmConnectionTimeout.set(seconds);
  }

}
