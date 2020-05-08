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

#include "../Compatibility.h"
#include "../Enumerations.h"

#include <boost/noncopyable.hpp>
#include <set>

class DcmFileFormat;

namespace Orthanc
{
  /**
   * WARNING: This class might be called from several threads at
   * once. Make sure to implement proper locking.
   **/
  
  class IDicomTranscoder : public boost::noncopyable
  {
  public:
    virtual ~IDicomTranscoder()
    {
    }

    virtual bool TranscodeToBuffer(std::string& target,
                                   bool& hasSopInstanceUidChanged /* out */,
                                   const void* buffer,
                                   size_t size,
                                   const std::set<DicomTransferSyntax>& allowedSyntaxes,
                                   bool allowNewSopInstanceUid) = 0;

    virtual bool HasInplaceTranscode(DicomTransferSyntax inputSyntax,
                                     const std::set<DicomTransferSyntax>& outputSyntaxes) const = 0;

    /**
     * In-place transcoding. This method is preferred for C-STORE.
     **/
    virtual bool InplaceTranscode(bool& hasSopInstanceUidChanged /* out */,
                                  DcmFileFormat& dicom,
                                  const std::set<DicomTransferSyntax>& allowedSyntaxes,
                                  bool allowNewSopInstanceUid) = 0;



    virtual bool TranscodeParsedToBuffer(std::string& target /* out */,
                                         DicomTransferSyntax& sourceSyntax /* out */,
                                         DicomTransferSyntax& targetSyntax /* out */,
                                         bool& hasSopInstanceUidChanged /* out */,
                                         DcmFileFormat& dicom /* in, possibly modified */,
                                         const std::set<DicomTransferSyntax>& allowedSyntaxes,  // TODO => is a set needed?
                                         bool allowNewSopInstanceUid) = 0;


    class TranscodedDicom : public boost::noncopyable
    {
    private:
      std::unique_ptr<DcmFileFormat>  internal_;
      DcmFileFormat*                  external_;
      bool                            hasSopInstanceUidChanged_;

      TranscodedDicom(bool hasSopInstanceUidChanged);

    public:
      static TranscodedDicom* CreateFromExternal(DcmFileFormat& dicom,
                                                 bool hasSopInstanceUidChanged);
        
      static TranscodedDicom* CreateFromInternal(DcmFileFormat* dicom,
                                                 bool hasSopInstanceUidChanged);

      // TODO - Is this information used somewhere?
      bool HasSopInstanceUidChanged() const
      {
        return hasSopInstanceUidChanged_;
      }
      
      DcmFileFormat& GetDicom() const;      
    };
    
    /**
     * Transcoding flavor that creates a new parsed DICOM file. A
     * "std::set<>" is used to give the possible plugin the
     * possibility to do a single parsing for all the possible
     * transfer syntaxes. This flavor is used by C-STORE.
     **/
    virtual TranscodedDicom* TranscodeToParsed(
      DcmFileFormat& dicom /* in, possibly modified */,
      const void* buffer /* in, same DICOM file as "dicom" */,
      size_t size,
      const std::set<DicomTransferSyntax>& allowedSyntaxes,
      bool allowNewSopInstanceUid) = 0;
  };
}
