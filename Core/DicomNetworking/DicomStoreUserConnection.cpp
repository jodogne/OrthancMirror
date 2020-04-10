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


#include "../PrecompiledHeaders.h"
#include "DicomStoreUserConnection.h"

#include "../Logging.h"
#include "../OrthancException.h"

namespace Orthanc
{
  bool DicomStoreUserConnection::ProposeStorageClass(const std::string& sopClassUid,
                                                     const std::set<DicomTransferSyntax>& syntaxes)
  {
    size_t requiredCount = syntaxes.size();
    if (proposeUncompressedSyntaxes_)
    {
      requiredCount += 1;
    }
      
    if (association_.GetRemainingPropositions() <= requiredCount)
    {
      return false;  // Not enough room
    }
      
    for (std::set<DicomTransferSyntax>::const_iterator
           it = syntaxes.begin(); it != syntaxes.end(); ++it)
    {
      association_.ProposePresentationContext(sopClassUid, *it);
    }

    if (proposeUncompressedSyntaxes_)
    {
      std::set<DicomTransferSyntax> uncompressed;
        
      if (syntaxes.find(DicomTransferSyntax_LittleEndianImplicit) == syntaxes.end())
      {
        uncompressed.insert(DicomTransferSyntax_LittleEndianImplicit);
      }
        
      if (syntaxes.find(DicomTransferSyntax_LittleEndianExplicit) == syntaxes.end())
      {
        uncompressed.insert(DicomTransferSyntax_LittleEndianExplicit);
      }
        
      if (proposeRetiredBigEndian_ &&
          syntaxes.find(DicomTransferSyntax_BigEndianExplicit) == syntaxes.end())
      {
        uncompressed.insert(DicomTransferSyntax_BigEndianExplicit);
      }

      if (!uncompressed.empty())
      {
        association_.ProposePresentationContext(sopClassUid, uncompressed);
      }
    }      

    return true;
  }


  bool DicomStoreUserConnection::LookupPresentationContext(
    uint8_t& presentationContextId,
    const std::string& sopClassUid,
    DicomTransferSyntax transferSyntax)
  {
    typedef std::map<DicomTransferSyntax, uint8_t>  PresentationContexts;

    PresentationContexts pc;
    if (association_.IsOpen() &&
        association_.LookupAcceptedPresentationContext(pc, sopClassUid))
    {
      PresentationContexts::const_iterator found = pc.find(transferSyntax);
      if (found != pc.end())
      {
        presentationContextId = found->second;
        return true;
      }
    }

    return false;
  }
    
        
  DicomStoreUserConnection::DicomStoreUserConnection(
    const DicomAssociationParameters& params) :
    parameters_(params),
    proposeCommonClasses_(true),
    proposeUncompressedSyntaxes_(true),
    proposeRetiredBigEndian_(false)
  {
  }
    

  void DicomStoreUserConnection::PrepareStorageClass(const std::string& sopClassUid,
                                                     DicomTransferSyntax syntax)
  {
    StorageClasses::iterator found = storageClasses_.find(sopClassUid);

    if (found == storageClasses_.end())
    {
      std::set<DicomTransferSyntax> ts;
      ts.insert(syntax);
      storageClasses_[sopClassUid] = ts;
    }
    else
    {
      found->second.insert(syntax);
    }
  }


  bool DicomStoreUserConnection::NegotiatePresentationContext(
    uint8_t& presentationContextId,
    const std::string& sopClassUid,
    DicomTransferSyntax transferSyntax)
  {
    /**
     * Step 1: Check whether this presentation context is already
     * available in the previously negociated assocation.
     **/

    if (LookupPresentationContext(presentationContextId, sopClassUid, transferSyntax))
    {
      return true;
    }

    // The association must be re-negotiated
    LOG(INFO) << "Re-negociating DICOM association with "
              << parameters_.GetRemoteApplicationEntityTitle();
    association_.ClearPresentationContexts();
    PrepareStorageClass(sopClassUid, transferSyntax);

      
    /**
     * Step 2: Propose at least the mandatory SOP class.
     **/

    {
      StorageClasses::const_iterator mandatory = storageClasses_.find(sopClassUid);

      if (mandatory == storageClasses_.end() ||
          mandatory->second.find(transferSyntax) == mandatory->second.end())
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      if (!ProposeStorageClass(sopClassUid, mandatory->second))
      {
        // Should never happen in real life: There are no more than
        // 128 transfer syntaxes in DICOM!
        throw OrthancException(ErrorCode_InternalError,
                               "Too many transfer syntaxes for SOP class UID: " + sopClassUid);
      }
    }

      
    /**
     * Step 3: Propose all the previously spotted SOP classes, as
     * registered through the "PrepareStorageClass()" method.
     **/
      
    for (StorageClasses::const_iterator it = storageClasses_.begin();
         it != storageClasses_.end(); ++it)
    {
      if (it->first != sopClassUid)
      {
        ProposeStorageClass(it->first, it->second);
      }
    }
      

    /**
     * Step 4: As long as there is room left in the proposed
     * presentation contexts, propose the uncompressed transfer syntaxes
     * for the most common SOP classes, as can be found in the
     * "dcmShortSCUStorageSOPClassUIDs" array from DCMTK. The
     * preferred transfer syntax is "LittleEndianImplicit".
     **/

    if (proposeCommonClasses_)
    {
      std::set<DicomTransferSyntax> ts;
      ts.insert(DicomTransferSyntax_LittleEndianImplicit);
        
      for (int i = 0; i < numberOfDcmShortSCUStorageSOPClassUIDs; i++)
      {
        std::string c(dcmShortSCUStorageSOPClassUIDs[i]);
          
        if (c != sopClassUid &&
            storageClasses_.find(c) == storageClasses_.end())
        {
          ProposeStorageClass(c, ts);
        }
      }
    }


    /**
     * Step 5: Open the association, and check whether the pair (SOP
     * class UID, transfer syntax) was accepted by the remote host.
     **/

    association_.Open(parameters_);
    return LookupPresentationContext(presentationContextId, sopClassUid, transferSyntax);
  }
}
