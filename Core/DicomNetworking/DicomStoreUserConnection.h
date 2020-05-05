/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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


#pragma once

#include "DicomAssociationParameters.h"

#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <set>
#include <stdint.h>  // For uint8_t


class DcmDataset;

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

  class DicomStoreUserConnection : public boost::noncopyable
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
                             const std::set<DicomTransferSyntax>& syntaxes);

    void LookupParameters(std::string& sopClassUid,
                          std::string& sopInstanceUid,
                          DcmDataset& dataset);

    bool LookupPresentationContext(uint8_t& presentationContextId,
                                   const std::string& sopClassUid,
                                   DicomTransferSyntax transferSyntax);
    
    bool NegotiatePresentationContext(uint8_t& presentationContextId,
                                      const std::string& sopClassUid,
                                      DicomTransferSyntax transferSyntax);

  public:
    DicomStoreUserConnection(const DicomAssociationParameters& params);
    
    const DicomAssociationParameters& GetParameters() const
    {
      return parameters_;
    }

    void SetCommonClassesProposed(bool proposed)
    {
      proposeCommonClasses_ = proposed;
    }

    bool IsCommonClassesProposed() const
    {
      return proposeCommonClasses_;
    }

    void SetUncompressedSyntaxesProposed(bool proposed)
    {
      proposeUncompressedSyntaxes_ = proposed;
    }

    bool IsUncompressedSyntaxesProposed() const
    {
      return proposeUncompressedSyntaxes_;
    }

    void SetRetiredBigEndianProposed(bool propose)
    {
      proposeRetiredBigEndian_ = propose;
    }

    bool IsRetiredBigEndianProposed() const
    {
      return proposeRetiredBigEndian_;
    }      

    void RegisterStorageClass(const std::string& sopClassUid,
                              DicomTransferSyntax syntax);

    void Store(std::string& sopClassUid,
               std::string& sopInstanceUid,
               DcmDataset& dataset,
               const std::string& moveOriginatorAET,
               uint16_t moveOriginatorID);

    void Store(std::string& sopClassUid,
               std::string& sopInstanceUid,
               const void* buffer,
               size_t size,
               const std::string& moveOriginatorAET,
               uint16_t moveOriginatorID);

    void Store(std::string& sopClassUid,
               std::string& sopInstanceUid,
               DcmDataset& dataset)
    {
      Store(sopClassUid, sopInstanceUid, dataset, "", 0);  // Not a C-Move
    }

    void Store(std::string& sopClassUid,
               std::string& sopInstanceUid,
               const void* buffer,
               size_t size)
    {
      Store(sopClassUid, sopInstanceUid, buffer, size, "", 0);  // Not a C-Move
    }

    bool LookupTranscoding(std::set<DicomTransferSyntax>& acceptedSyntaxes,
                           const std::string& sopClassUid,
                           DicomTransferSyntax sourceSyntax);
  };
}
