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


#include "../PrecompiledHeaders.h"
#include "DicomUserConnection.h"

#include "../DicomFormat/DicomArray.h"
#include "../Logging.h"
#include "../OrthancException.h"
#include "../DicomParsing/FromDcmtkBridge.h"
#include "../DicomParsing/ToDcmtkBridge.h"

#include <dcmtk/dcmdata/dcistrmb.h>
#include <dcmtk/dcmdata/dcistrmf.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcmetinf.h>
#include <dcmtk/dcmnet/diutil.h>

#include <set>


#ifdef _WIN32
/**
 * "The maximum length, in bytes, of the string returned in the buffer 
 * pointed to by the name parameter is dependent on the namespace provider,
 * but this string must be 256 bytes or less.
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms738527(v=vs.85).aspx
 **/
#  define HOST_NAME_MAX 256
#  include <winsock.h>
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
  // By default, the timeout for DICOM SCU (client) connections is set to 10 seconds
  static uint32_t defaultTimeout_ = 10;

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

    void Store(DcmInputStream& is, 
               DicomUserConnection& connection,
               const std::string& moveOriginatorAET,
               uint16_t moveOriginatorID);
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


  void DicomUserConnection::PImpl::Store(DcmInputStream& is, 
                                         DicomUserConnection& connection,
                                         const std::string& moveOriginatorAET,
                                         uint16_t moveOriginatorID)
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

    bool renegotiate;
    if (isGeneric)
    {
      // Are we making a generic-to-specific or specific-to-generic change of
      // the transfer syntax? If this is the case, renegotiate the connection.
      renegotiate = !IsGenericTransferSyntax(connection.GetPreferredTransferSyntax());
    }
    else
    {
      // We are using a specific transfer syntax. Renegotiate if the
      // current connection does not match this transfer syntax.
      renegotiate = (syntax != connection.GetPreferredTransferSyntax());
    }

    if (renegotiate)
    {
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
      throw OrthancException(ErrorCode_NoSopClassOrInstance);
    }

    // Figure out which of the accepted presentation contexts should be used
    int presID = ASC_findAcceptedPresentationContextID(assoc_, sopClass);
    if (presID == 0)
    {
      const char *modalityName = dcmSOPClassUIDToModality(sopClass);
      if (!modalityName) modalityName = dcmFindNameOfUID(sopClass);
      if (!modalityName) modalityName = "unknown SOP class";
      throw OrthancException(ErrorCode_NoPresentationContext);
    }

    // Prepare the transmission of data
    T_DIMSE_C_StoreRQ request;
    memset(&request, 0, sizeof(request));
    request.MessageID = assoc_->nextMsgID++;
    strncpy(request.AffectedSOPClassUID, sopClass, DIC_UI_LEN);
    request.Priority = DIMSE_PRIORITY_MEDIUM;
    request.DataSetType = DIMSE_DATASET_PRESENT;
    strncpy(request.AffectedSOPInstanceUID, sopInstance, DIC_UI_LEN);

    if (!moveOriginatorAET.empty())
    {
      strncpy(request.MoveOriginatorApplicationEntityTitle, 
              moveOriginatorAET.c_str(), DIC_AE_LEN);
      request.opts = O_STORE_MOVEORIGINATORAETITLE;

      request.MoveOriginatorID = moveOriginatorID;  // The type DIC_US is an alias for uint16_t
      request.opts |= O_STORE_MOVEORIGINATORID;
    }

    // Finally conduct transmission of data
    T_DIMSE_C_StoreRSP rsp;
    DcmDataset* statusDetail = NULL;
    Check(DIMSE_storeUser(assoc_, presID, &request,
                          NULL, dcmff.getDataset(), /*progressCallback*/ NULL, NULL,
                          /*opt_blockMode*/ DIMSE_BLOCKING, /*opt_dimse_timeout*/ dimseTimeout_,
                          &rsp, &statusDetail, NULL));

    if (statusDetail != NULL) 
    {
      delete statusDetail;
    }
  }


  namespace
  {
    struct FindPayload
    {
      DicomFindAnswers* answers;
      const char*       level;
      bool              isWorklist;
    };
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
    FindPayload& payload = *reinterpret_cast<FindPayload*>(callbackData);

    if (responseIdentifiers != NULL)
    {
      if (payload.isWorklist)
      {
        ParsedDicomFile answer(*responseIdentifiers);
        payload.answers->Add(answer);
      }
      else
      {
        DicomMap m;
        FromDcmtkBridge::ExtractDicomSummary(m, *responseIdentifiers);
        
        if (!m.HasTag(DICOM_TAG_QUERY_RETRIEVE_LEVEL))
        {
          m.SetValue(DICOM_TAG_QUERY_RETRIEVE_LEVEL, payload.level, false);
        }

        payload.answers->Add(m);
      }
    }
  }


  static void FixFindQuery(DicomMap& fixedQuery,
                           ResourceType level,
                           const DicomMap& fields)
  {
    std::set<DicomTag> allowedTags;

    // WARNING: Do not add "break" or reorder items in this switch-case!
    switch (level)
    {
      case ResourceType_Instance:
        DicomTag::AddTagsForModule(allowedTags, DicomModule_Instance);

      case ResourceType_Series:
        DicomTag::AddTagsForModule(allowedTags, DicomModule_Series);

      case ResourceType_Study:
        DicomTag::AddTagsForModule(allowedTags, DicomModule_Study);

      case ResourceType_Patient:
        DicomTag::AddTagsForModule(allowedTags, DicomModule_Patient);
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    switch (level)
    {
      case ResourceType_Patient:
        allowedTags.insert(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_STUDIES);
        allowedTags.insert(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_SERIES);
        allowedTags.insert(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_INSTANCES);
        break;

      case ResourceType_Study:
        allowedTags.insert(DICOM_TAG_MODALITIES_IN_STUDY);
        allowedTags.insert(DICOM_TAG_NUMBER_OF_STUDY_RELATED_SERIES);
        allowedTags.insert(DICOM_TAG_NUMBER_OF_STUDY_RELATED_INSTANCES);
        allowedTags.insert(DICOM_TAG_SOP_CLASSES_IN_STUDY);
        break;

      case ResourceType_Series:
        allowedTags.insert(DICOM_TAG_NUMBER_OF_SERIES_RELATED_INSTANCES);
        break;

      default:
        break;
    }

    allowedTags.insert(DICOM_TAG_SPECIFIC_CHARACTER_SET);

    DicomArray query(fields);
    for (size_t i = 0; i < query.GetSize(); i++)
    {
      const DicomTag& tag = query.GetElement(i).GetTag();
      if (allowedTags.find(tag) == allowedTags.end())
      {
        LOG(WARNING) << "Tag not allowed for this C-Find level, will be ignored: " << tag;
      }
      else
      {
        fixedQuery.SetValue(tag, query.GetElement(i).GetValue());
      }
    }
  }


  static ParsedDicomFile* ConvertQueryFields(const DicomMap& fields,
                                             ModalityManufacturer manufacturer)
  {
    // Fix outgoing C-Find requests issue for Syngo.Via and its
    // solution was reported by Emsy Chan by private mail on
    // 2015-06-17. According to Robert van Ommen (2015-11-30), the
    // same fix is required for Agfa Impax. This was generalized for
    // generic manufacturer since it seems to affect PhilipsADW,
    // GEWAServer as well:
    // https://bitbucket.org/sjodogne/orthanc/issues/31/

    switch (manufacturer)
    {
      case ModalityManufacturer_GenericNoWildcardInDates:
      case ModalityManufacturer_GenericNoUniversalWildcard:
      {
        std::auto_ptr<DicomMap> fix(fields.Clone());

        std::set<DicomTag> tags;
        fix->GetTags(tags);

        for (std::set<DicomTag>::const_iterator it = tags.begin(); it != tags.end(); ++it)
        {
          // Replace a "*" wildcard query by an empty query ("") for
          // "date" or "all" value representations depending on the
          // type of manufacturer.
          if (manufacturer == ModalityManufacturer_GenericNoUniversalWildcard ||
              (manufacturer == ModalityManufacturer_GenericNoWildcardInDates &&
               FromDcmtkBridge::LookupValueRepresentation(*it) == ValueRepresentation_Date))
          {
            const DicomValue* value = fix->TestAndGetValue(*it);

            if (value != NULL && 
                !value->IsNull() &&
                value->GetContent() == "*")
            {
              fix->SetValue(*it, "", false);
            }
          }
        }

        return new ParsedDicomFile(*fix);
      }

      default:
        return new ParsedDicomFile(fields);
    }
  }


  static void ExecuteFind(DicomFindAnswers& answers,
                          T_ASC_Association* association,
                          DcmDataset* dataset,
                          const char* sopClass,
                          bool isWorklist,
                          const char* level,
                          uint32_t dimseTimeout)
  {
    assert(isWorklist ^ (level != NULL));

    FindPayload payload;
    payload.answers = &answers;
    payload.level = level;
    payload.isWorklist = isWorklist;

    // Figure out which of the accepted presentation contexts should be used
    int presID = ASC_findAcceptedPresentationContextID(association, sopClass);
    if (presID == 0)
    {
      throw OrthancException(ErrorCode_DicomFindUnavailable);
    }

    T_DIMSE_C_FindRQ request;
    memset(&request, 0, sizeof(request));
    request.MessageID = association->nextMsgID++;
    strncpy(request.AffectedSOPClassUID, sopClass, DIC_UI_LEN);
    request.Priority = DIMSE_PRIORITY_MEDIUM;
    request.DataSetType = DIMSE_DATASET_PRESENT;

    T_DIMSE_C_FindRSP response;
    DcmDataset* statusDetail = NULL;
    OFCondition cond = DIMSE_findUser(association, presID, &request, dataset,
                                      FindCallback, &payload,
                                      /*opt_blockMode*/ DIMSE_BLOCKING, 
                                      /*opt_dimse_timeout*/ dimseTimeout,
                                      &response, &statusDetail);

    if (statusDetail)
    {
      delete statusDetail;
    }

    Check(cond);
  }


  void DicomUserConnection::Find(DicomFindAnswers& result,
                                 ResourceType level,
                                 const DicomMap& originalFields)
  {
    DicomMap fields;
    FixFindQuery(fields, level, originalFields);

    CheckIsOpen();

    std::auto_ptr<ParsedDicomFile> query(ConvertQueryFields(fields, manufacturer_));
    DcmDataset* dataset = query->GetDcmtkObject().getDataset();

    const char* clevel = NULL;
    const char* sopClass = NULL;

    switch (level)
    {
      case ResourceType_Patient:
        clevel = "PATIENT";
        DU_putStringDOElement(dataset, DcmTagKey(0x0008, 0x0052), "PATIENT");
        sopClass = UID_FINDPatientRootQueryRetrieveInformationModel;
        break;

      case ResourceType_Study:
        clevel = "STUDY";
        DU_putStringDOElement(dataset, DcmTagKey(0x0008, 0x0052), "STUDY");
        sopClass = UID_FINDStudyRootQueryRetrieveInformationModel;
        break;

      case ResourceType_Series:
        clevel = "SERIES";
        DU_putStringDOElement(dataset, DcmTagKey(0x0008, 0x0052), "SERIES");
        sopClass = UID_FINDStudyRootQueryRetrieveInformationModel;
        break;

      case ResourceType_Instance:
        clevel = "INSTANCE";
        if (manufacturer_ == ModalityManufacturer_ClearCanvas ||
            manufacturer_ == ModalityManufacturer_Dcm4Chee)
        {
          // This is a particular case for ClearCanvas, thanks to Peter Somlo <peter.somlo@gmail.com>.
          // https://groups.google.com/d/msg/orthanc-users/j-6C3MAVwiw/iolB9hclom8J
          // http://www.clearcanvas.ca/Home/Community/OldForums/tabid/526/aff/11/aft/14670/afv/topic/Default.aspx
          DU_putStringDOElement(dataset, DcmTagKey(0x0008, 0x0052), "IMAGE");
        }
        else
        {
          DU_putStringDOElement(dataset, DcmTagKey(0x0008, 0x0052), "INSTANCE");
        }

        sopClass = UID_FINDStudyRootQueryRetrieveInformationModel;
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    // Add the expected tags for this query level.
    // WARNING: Do not reorder or add "break" in this switch-case!
    switch (level)
    {
      case ResourceType_Instance:
        // SOP Instance UID
        if (!fields.HasTag(0x0008, 0x0018))
          DU_putStringDOElement(dataset, DcmTagKey(0x0008, 0x0018), "");

      case ResourceType_Series:
        // Series instance UID
        if (!fields.HasTag(0x0020, 0x000e))
          DU_putStringDOElement(dataset, DcmTagKey(0x0020, 0x000e), "");

      case ResourceType_Study:
        // Accession number
        if (!fields.HasTag(0x0008, 0x0050))
          DU_putStringDOElement(dataset, DcmTagKey(0x0008, 0x0050), "");

        // Study instance UID
        if (!fields.HasTag(0x0020, 0x000d))
          DU_putStringDOElement(dataset, DcmTagKey(0x0020, 0x000d), "");

      case ResourceType_Patient:
        // Patient ID
        if (!fields.HasTag(0x0010, 0x0020))
          DU_putStringDOElement(dataset, DcmTagKey(0x0010, 0x0020), "");

        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    assert(clevel != NULL && sopClass != NULL);
    ExecuteFind(result, pimpl_->assoc_, dataset, sopClass, false, clevel, pimpl_->dimseTimeout_);
  }


  void DicomUserConnection::MoveInternal(const std::string& targetAet,
                                         ResourceType level,
                                         const DicomMap& fields)
  {
    CheckIsOpen();

    std::auto_ptr<ParsedDicomFile> query(ConvertQueryFields(fields, manufacturer_));
    DcmDataset* dataset = query->GetDcmtkObject().getDataset();

    const char* sopClass = UID_MOVEStudyRootQueryRetrieveInformationModel;
    switch (level)
    {
      case ResourceType_Patient:
        DU_putStringDOElement(dataset, DcmTagKey(0x0008, 0x0052), "PATIENT");
        break;

      case ResourceType_Study:
        DU_putStringDOElement(dataset, DcmTagKey(0x0008, 0x0052), "STUDY");
        break;

      case ResourceType_Series:
        DU_putStringDOElement(dataset, DcmTagKey(0x0008, 0x0052), "SERIES");
        break;

      case ResourceType_Instance:
        if (manufacturer_ == ModalityManufacturer_ClearCanvas ||
            manufacturer_ == ModalityManufacturer_Dcm4Chee)
        {
          // This is a particular case for ClearCanvas, thanks to Peter Somlo <peter.somlo@gmail.com>.
          // https://groups.google.com/d/msg/orthanc-users/j-6C3MAVwiw/iolB9hclom8J
          // http://www.clearcanvas.ca/Home/Community/OldForums/tabid/526/aff/11/aft/14670/afv/topic/Default.aspx
          DU_putStringDOElement(dataset, DcmTagKey(0x0008, 0x0052), "IMAGE");
        }
        else
        {
          DU_putStringDOElement(dataset, DcmTagKey(0x0008, 0x0052), "INSTANCE");
        }
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    // Figure out which of the accepted presentation contexts should be used
    int presID = ASC_findAcceptedPresentationContextID(pimpl_->assoc_, sopClass);
    if (presID == 0)
    {
      throw OrthancException(ErrorCode_DicomMoveUnavailable);
    }

    T_DIMSE_C_MoveRQ request;
    memset(&request, 0, sizeof(request));
    request.MessageID = pimpl_->assoc_->nextMsgID++;
    strncpy(request.AffectedSOPClassUID, sopClass, DIC_UI_LEN);
    request.Priority = DIMSE_PRIORITY_MEDIUM;
    request.DataSetType = DIMSE_DATASET_PRESENT;
    strncpy(request.MoveDestination, targetAet.c_str(), DIC_AE_LEN);

    T_DIMSE_C_MoveRSP response;
    DcmDataset* statusDetail = NULL;
    DcmDataset* responseIdentifiers = NULL;
    OFCondition cond = DIMSE_moveUser(pimpl_->assoc_, presID, &request, dataset,
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
    // room for the 5 SOP classes reserved for C-ECHO, C-FIND, C-MOVE at (**).

    std::set<std::string> uncommon;
    uncommon.insert(UID_BlendingSoftcopyPresentationStateStorage);
    uncommon.insert(UID_GrayscaleSoftcopyPresentationStateStorage);
    uncommon.insert(UID_ColorSoftcopyPresentationStateStorage);
    uncommon.insert(UID_PseudoColorSoftcopyPresentationStateStorage);
    uncommon.insert(UID_XAXRFGrayscaleSoftcopyPresentationStateStorage);

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
    remoteAet_("ANY-SCP"),
    remoteHost_("127.0.0.1")
  {
    remotePort_ = 104;
    manufacturer_ = ModalityManufacturer_Generic;

    SetTimeout(defaultTimeout_);
    pimpl_->net_ = NULL;
    pimpl_->params_ = NULL;
    pimpl_->assoc_ = NULL;

    // SOP classes for C-ECHO, C-FIND and C-MOVE (**)
    reservedStorageSOPClasses_.push_back(UID_VerificationSOPClass);
    reservedStorageSOPClasses_.push_back(UID_FINDPatientRootQueryRetrieveInformationModel);
    reservedStorageSOPClasses_.push_back(UID_FINDStudyRootQueryRetrieveInformationModel);
    reservedStorageSOPClasses_.push_back(UID_MOVEStudyRootQueryRetrieveInformationModel);
    reservedStorageSOPClasses_.push_back(UID_FINDModalityWorklistInformationModel);

    ResetStorageSOPClasses();
  }

  DicomUserConnection::~DicomUserConnection()
  {
    Close();
  }


  void DicomUserConnection::SetRemoteModality(const RemoteModalityParameters& parameters)
  {
    SetRemoteApplicationEntityTitle(parameters.GetApplicationEntityTitle());
    SetRemoteHost(parameters.GetHost());
    SetRemotePort(parameters.GetPort());
    SetRemoteManufacturer(parameters.GetManufacturer());
  }


  void DicomUserConnection::SetLocalApplicationEntityTitle(const std::string& aet)
  {
    if (localAet_ != aet)
    {
      Close();
      localAet_ = aet;
    }
  }

  void DicomUserConnection::SetRemoteApplicationEntityTitle(const std::string& aet)
  {
    if (remoteAet_ != aet)
    {
      Close();
      remoteAet_ = aet;
    }
  }

  void DicomUserConnection::SetRemoteManufacturer(ModalityManufacturer manufacturer)
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


  void DicomUserConnection::SetRemoteHost(const std::string& host)
  {
    if (remoteHost_ != host)
    {
      if (host.size() > HOST_NAME_MAX - 10)
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange);
      }

      Close();
      remoteHost_ = host;
    }
  }

  void DicomUserConnection::SetRemotePort(uint16_t port)
  {
    if (remotePort_ != port)
    {
      Close();
      remotePort_ = port;
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
              << "\" to AET \"" << GetRemoteApplicationEntityTitle() << "\" on host "
              << GetRemoteHost() << ":" << GetRemotePort() 
              << " (manufacturer: " << EnumerationToString(GetRemoteManufacturer()) << ")";

    Check(ASC_initializeNetwork(NET_REQUESTOR, 0, /*opt_acse_timeout*/ pimpl_->acseTimeout_, &pimpl_->net_));
    Check(ASC_createAssociationParameters(&pimpl_->params_, /*opt_maxReceivePDULength*/ ASC_DEFAULTMAXPDU));

    // Set this application's title and the called application's title in the params
    Check(ASC_setAPTitles(pimpl_->params_, localAet_.c_str(), remoteAet_.c_str(), NULL));

    // Set the network addresses of the local and remote entities
    char localHost[HOST_NAME_MAX];
    gethostname(localHost, HOST_NAME_MAX - 1);

    char remoteHostAndPort[HOST_NAME_MAX];

#ifdef _MSC_VER
    _snprintf
#else
      snprintf
#endif
      (remoteHostAndPort, HOST_NAME_MAX - 1, "%s:%d", remoteHost_.c_str(), remotePort_);

    Check(ASC_setPresentationAddresses(pimpl_->params_, localHost, remoteHostAndPort));

    // Set various options
    Check(ASC_setTransportLayerType(pimpl_->params_, /*opt_secureConnection*/ false));

    SetupPresentationContexts(preferredTransferSyntax_);

    // Do the association
    Check(ASC_requestAssociation(pimpl_->net_, pimpl_->params_, &pimpl_->assoc_));

    if (ASC_countAcceptedPresentationContexts(pimpl_->params_) == 0)
    {
      throw OrthancException(ErrorCode_NoPresentationContext);
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

  void DicomUserConnection::Store(const char* buffer, 
                                  size_t size,
                                  const std::string& moveOriginatorAET,
                                  uint16_t moveOriginatorID)
  {
    // Prepare an input stream for the memory buffer
    DcmInputBufferStream is;
    if (size > 0)
      is.setBuffer(buffer, size);
    is.setEos();
      
    pimpl_->Store(is, *this, moveOriginatorAET, moveOriginatorID);
  }

  void DicomUserConnection::Store(const std::string& buffer,
                                  const std::string& moveOriginatorAET,
                                  uint16_t moveOriginatorID)
  {
    if (buffer.size() > 0)
      Store(reinterpret_cast<const char*>(&buffer[0]), buffer.size(), moveOriginatorAET, moveOriginatorID);
    else
      Store(NULL, 0, moveOriginatorAET, moveOriginatorID);
  }

  void DicomUserConnection::StoreFile(const std::string& path,
                                      const std::string& moveOriginatorAET,
                                      uint16_t moveOriginatorID)
  {
    // Prepare an input stream for the file
    DcmInputFileStream is(path.c_str());
    pimpl_->Store(is, *this, moveOriginatorAET, moveOriginatorID);
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


  static void TestAndCopyTag(DicomMap& result,
                             const DicomMap& source,
                             const DicomTag& tag)
  {
    if (!source.HasTag(tag))
    {
      throw OrthancException(ErrorCode_BadRequest);
    }
    else
    {
      result.SetValue(tag, source.GetValue(tag));
    }
  }


  void DicomUserConnection::Move(const std::string& targetAet,
                                 ResourceType level,
                                 const DicomMap& findResult)
  {
    DicomMap move;
    switch (level)
    {
      case ResourceType_Patient:
        TestAndCopyTag(move, findResult, DICOM_TAG_PATIENT_ID);
        break;

      case ResourceType_Study:
        TestAndCopyTag(move, findResult, DICOM_TAG_STUDY_INSTANCE_UID);
        break;

      case ResourceType_Series:
        TestAndCopyTag(move, findResult, DICOM_TAG_STUDY_INSTANCE_UID);
        TestAndCopyTag(move, findResult, DICOM_TAG_SERIES_INSTANCE_UID);
        break;

      case ResourceType_Instance:
        TestAndCopyTag(move, findResult, DICOM_TAG_STUDY_INSTANCE_UID);
        TestAndCopyTag(move, findResult, DICOM_TAG_SERIES_INSTANCE_UID);
        TestAndCopyTag(move, findResult, DICOM_TAG_SOP_INSTANCE_UID);
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    MoveInternal(targetAet, level, move);
  }


  void DicomUserConnection::Move(const std::string& targetAet,
                                 const DicomMap& findResult)
  {
    if (!findResult.HasTag(DICOM_TAG_QUERY_RETRIEVE_LEVEL))
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    const std::string tmp = findResult.GetValue(DICOM_TAG_QUERY_RETRIEVE_LEVEL).GetContent();
    ResourceType level = StringToResourceType(tmp.c_str());

    Move(targetAet, level, findResult);
  }


  void DicomUserConnection::MovePatient(const std::string& targetAet,
                                        const std::string& patientId)
  {
    DicomMap query;
    query.SetValue(DICOM_TAG_PATIENT_ID, patientId, false);
    MoveInternal(targetAet, ResourceType_Patient, query);
  }

  void DicomUserConnection::MoveStudy(const std::string& targetAet,
                                      const std::string& studyUid)
  {
    DicomMap query;
    query.SetValue(DICOM_TAG_STUDY_INSTANCE_UID, studyUid, false);
    MoveInternal(targetAet, ResourceType_Study, query);
  }

  void DicomUserConnection::MoveSeries(const std::string& targetAet,
                                       const std::string& studyUid,
                                       const std::string& seriesUid)
  {
    DicomMap query;
    query.SetValue(DICOM_TAG_STUDY_INSTANCE_UID, studyUid, false);
    query.SetValue(DICOM_TAG_SERIES_INSTANCE_UID, seriesUid, false);
    MoveInternal(targetAet, ResourceType_Series, query);
  }

  void DicomUserConnection::MoveInstance(const std::string& targetAet,
                                         const std::string& studyUid,
                                         const std::string& seriesUid,
                                         const std::string& instanceUid)
  {
    DicomMap query;
    query.SetValue(DICOM_TAG_STUDY_INSTANCE_UID, studyUid, false);
    query.SetValue(DICOM_TAG_SERIES_INSTANCE_UID, seriesUid, false);
    query.SetValue(DICOM_TAG_SOP_INSTANCE_UID, instanceUid, false);
    MoveInternal(targetAet, ResourceType_Instance, query);
  }


  void DicomUserConnection::SetTimeout(uint32_t seconds)
  {
    if (seconds == 0)
    {
      DisableTimeout();
    }
    else
    {
      dcmConnectionTimeout.set(seconds);
      pimpl_->dimseTimeout_ = seconds;
      pimpl_->acseTimeout_ = 10;  // Timeout used during association negociation
    }
  }


  void DicomUserConnection::DisableTimeout()
  {
    /**
     * Global timeout (seconds) for connecting to remote hosts.
     * Default value is -1 which selects infinite timeout, i.e. blocking connect().
     */
    dcmConnectionTimeout.set(-1);
    pimpl_->dimseTimeout_ = 0;
    pimpl_->acseTimeout_ = 10;  // Timeout used during association negociation
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
      assert(!defaultStorageSOPClasses_.empty());  // Necessarily true because condition (*) is false
      defaultStorageSOPClasses_.erase(*defaultStorageSOPClasses_.rbegin());
    }

    // Explicitly register the new storage syntax
    storageSOPClasses_.insert(sop);

    CheckStorageSOPClassesInvariant();
  }


  void DicomUserConnection::FindWorklist(DicomFindAnswers& result,
                                         ParsedDicomFile& query)
  {
    CheckIsOpen();

    DcmDataset* dataset = query.GetDcmtkObject().getDataset();
    const char* sopClass = UID_FINDModalityWorklistInformationModel;

    ExecuteFind(result, pimpl_->assoc_, dataset, sopClass, true, NULL, pimpl_->dimseTimeout_);
  }

  
  void DicomUserConnection::SetDefaultTimeout(uint32_t seconds)
  {
    LOG(INFO) << "Default timeout for DICOM connections if Orthanc acts as SCU (client): " 
              << seconds << " seconds (0 = no timeout)";
    defaultTimeout_ = seconds;
  }  
}
