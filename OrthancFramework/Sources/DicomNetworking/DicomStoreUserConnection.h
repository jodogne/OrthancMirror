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

#if !defined(ORTHANC_ENABLE_DCMTK_TRANSCODING)
#  error Macro ORTHANC_ENABLE_DCMTK_TRANSCODING must be defined to use this file
#endif

#include "DicomAssociationParameters.h"

#if ORTHANC_ENABLE_DCMTK_TRANSCODING == 1
#  include "../DicomParsing/IDicomTranscoder.h"
#endif

#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <set>
#include <stdint.h>  // For uint8_t


class DcmFileFormat;

namespace Orthanc
{
  /**

     Orthanc < 1.7.0:

     Input        | Output
     -------------+---------------------------------------------
     Compressed   | Same transfer syntax
     Uncompressed | Same transfer syntax, or other uncompressed

     Orthanc >= 1.7.0:

     Input        | Output
     -------------+---------------------------------------------
     Compressed   | Same transfer syntax, or uncompressed
     Uncompressed | Same transfer syntax, or other uncompressed

  **/

  class DicomAssociation;  // Forward declaration for PImpl design pattern

  class ORTHANC_PUBLIC DicomStoreUserConnection : public boost::noncopyable
  {
  private:
    typedef std::map<std::string, std::set<DicomTransferSyntax> > RegisteredClasses;

    // "ProposedOriginalClasses" keeps track of the storage classes
    // that were proposed with a single transfer syntax
    typedef std::set< std::pair<std::string, DicomTransferSyntax> > ProposedOriginalClasses;

    DicomAssociationParameters           parameters_;
    boost::shared_ptr<DicomAssociation>  association_;  // "shared_ptr" is for PImpl
    RegisteredClasses                    registeredClasses_;
    ProposedOriginalClasses              proposedOriginalClasses_;
    bool                                 proposeCommonClasses_;
    bool                                 proposeUncompressedSyntaxes_;
    bool                                 proposeRetiredBigEndian_;

    // Return "false" if there is not enough room remaining in the association
    bool ProposeStorageClass(const std::string& sopClassUid,
                             const std::set<DicomTransferSyntax>& sourceSyntaxes,
                             bool hasPreferred,
                             DicomTransferSyntax preferred);

    bool LookupPresentationContext(uint8_t& presentationContextId,
                                   const std::string& sopClassUid,
                                   DicomTransferSyntax transferSyntax);
    
    bool NegotiatePresentationContext(uint8_t& presentationContextId,
                                      const std::string& sopClassUid,
                                      DicomTransferSyntax transferSyntax,
                                      bool hasPreferred,
                                      DicomTransferSyntax preferred);

    void LookupTranscoding(std::set<DicomTransferSyntax>& acceptedSyntaxes,
                           const std::string& sopClassUid,
                           DicomTransferSyntax sourceSyntax,
                           bool hasPreferred,
                           DicomTransferSyntax preferred);

  public:
    explicit DicomStoreUserConnection(const DicomAssociationParameters& params);
    
    const DicomAssociationParameters& GetParameters() const;

    void SetCommonClassesProposed(bool proposed);

    bool IsCommonClassesProposed() const;

    void SetUncompressedSyntaxesProposed(bool proposed);

    bool IsUncompressedSyntaxesProposed() const;

    void SetRetiredBigEndianProposed(bool propose);

    bool IsRetiredBigEndianProposed() const;

    void RegisterStorageClass(const std::string& sopClassUid,
                              DicomTransferSyntax syntax);

    void Store(std::string& sopClassUid,
               std::string& sopInstanceUid,
               DcmFileFormat& dicom,
               bool hasMoveOriginator,
               const std::string& moveOriginatorAET,
               uint16_t moveOriginatorID);

    void Store(std::string& sopClassUid,
               std::string& sopInstanceUid,
               const void* buffer,
               size_t size,
               bool hasMoveOriginator,
               const std::string& moveOriginatorAET,
               uint16_t moveOriginatorID);

    void LookupParameters(std::string& sopClassUid,
                          std::string& sopInstanceUid,
                          DicomTransferSyntax& transferSyntax,
                          DcmFileFormat& dicom);

    void Transcode(std::string& sopClassUid /* out */,
                   std::string& sopInstanceUid /* out */,
                   IDicomTranscoder& transcoder,
                   const void* buffer,
                   size_t size,
                   DicomTransferSyntax preferredTransferSyntax,
                   bool hasMoveOriginator,
                   const std::string& moveOriginatorAET,
                   uint16_t moveOriginatorID);

    void Transcode(std::string& sopClassUid /* out */,
                   std::string& sopInstanceUid /* out */,
                   IDicomTranscoder& transcoder,
                   const void* buffer,
                   size_t size,
                   bool hasMoveOriginator,
                   const std::string& moveOriginatorAET,
                   uint16_t moveOriginatorID);
  };
}
