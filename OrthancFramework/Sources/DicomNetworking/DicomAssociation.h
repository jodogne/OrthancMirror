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


#pragma once

#if ORTHANC_ENABLE_DCMTK_NETWORKING != 1
#  error The macro ORTHANC_ENABLE_DCMTK_NETWORKING must be set to 1
#endif

#if !defined(ORTHANC_ENABLE_SSL)
#  error The macro ORTHANC_ENABLE_SSL must be defined
#endif

#if ORTHANC_ENABLE_SSL == 1
#  include "Internals/DicomTls.h"
#endif

#include "../Compatibility.h"  // For std::unique_ptr<>
#include "DicomAssociationParameters.h"

#include <dcmtk/dcmnet/dimse.h>

#include <stdint.h>   // For uint8_t
#include <boost/noncopyable.hpp>
#include <set>

namespace Orthanc
{
  class DicomAssociation : public boost::noncopyable
  {
  private:
    // This is the maximum number of presentation context IDs (the
    // number of odd integers between 1 and 255)
    // http://dicom.nema.org/medical/dicom/2019e/output/chtml/part08/sect_9.3.2.2.html
    static const size_t MAX_PROPOSED_PRESENTATIONS = 128;
    
    struct ProposedPresentationContext
    {
      std::string                    abstractSyntax_;
      std::set<DicomTransferSyntax>  transferSyntaxes_;
    };

    typedef std::map<std::string, std::map<DicomTransferSyntax, uint8_t> >
    AcceptedPresentationContexts;

    DicomAssociationRole                      role_;
    bool                                      isOpen_;
    std::vector<ProposedPresentationContext>  proposed_;
    AcceptedPresentationContexts              accepted_;
    T_ASC_Network*                            net_;
    T_ASC_Parameters*                         params_;
    T_ASC_Association*                        assoc_;

#if ORTHANC_ENABLE_SSL == 1
    std::unique_ptr<DcmTLSTransportLayer>     tls_;
#endif

    void Initialize();

    void CheckConnecting(const DicomAssociationParameters& parameters,
                         const OFCondition& cond);
    
    void CloseInternal();

    void AddAccepted(const std::string& abstractSyntax,
                     DicomTransferSyntax syntax,
                     uint8_t presentationContextId);

  public:
    DicomAssociation()
    {
      Initialize();
    }

    ~DicomAssociation();

    bool IsOpen() const
    {
      return isOpen_;
    }

    void SetRole(DicomAssociationRole role);

    void ClearPresentationContexts();

    void Open(const DicomAssociationParameters& parameters);
    
    void Close();

    bool LookupAcceptedPresentationContext(
      std::map<DicomTransferSyntax, uint8_t>& target,
      const std::string& abstractSyntax) const;

    void ProposeGenericPresentationContext(const std::string& abstractSyntax);

    void ProposePresentationContext(const std::string& abstractSyntax,
                                    DicomTransferSyntax transferSyntax);

    size_t GetRemainingPropositions() const;

    void ProposePresentationContext(
      const std::string& abstractSyntax,
      const std::set<DicomTransferSyntax>& transferSyntaxes);
    
    T_ASC_Association& GetDcmtkAssociation() const;

    T_ASC_Network& GetDcmtkNetwork() const;

    static void CheckCondition(const OFCondition& cond,
                               const DicomAssociationParameters& parameters,
                               const std::string& command);

    static void ReportStorageCommitment(
      const DicomAssociationParameters& parameters,
      const std::string& transactionUid,
      const std::vector<std::string>& sopClassUids,
      const std::vector<std::string>& sopInstanceUids,
      const std::vector<StorageCommitmentFailureReason>& failureReasons);
    
    static void RequestStorageCommitment(
      const DicomAssociationParameters& parameters,
      const std::string& transactionUid,
      const std::vector<std::string>& sopClassUids,
      const std::vector<std::string>& sopInstanceUids);
  };
}
