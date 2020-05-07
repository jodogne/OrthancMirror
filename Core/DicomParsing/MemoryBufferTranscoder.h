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

#if !defined(ORTHANC_ENABLE_DCMTK_TRANSCODING)
#  error Macro ORTHANC_ENABLE_DCMTK_TRANSCODING must be defined to use this file
#endif

#if ORTHANC_ENABLE_DCMTK_TRANSCODING == 1
#  include "DcmtkTranscoder.h"
#endif

namespace Orthanc
{
  // This is the basis class for transcoding plugins
  class MemoryBufferTranscoder : public IDicomTranscoder
  {
  private:
    bool  useDcmtk_;

#if ORTHANC_ENABLE_DCMTK_TRANSCODING == 1
    DcmtkTranscoder  dcmtk_;
#endif

  protected:
    virtual bool Transcode(std::string& target,
                           bool& hasSopInstanceUidChanged /* out */,
                           const void* buffer,
                           size_t size,
                           const std::set<DicomTransferSyntax>& allowedSyntaxes,
                           bool allowNewSopInstanceUid) = 0;
    
  public:
    /**
     * If "useDcmtk" is "true", the transcoder will first try and call
     * DCMTK, before calling its own "Transcode()" implementation.
     **/
    MemoryBufferTranscoder();

    void SetDcmtkUsed(bool used);

    bool IsDcmtkUsed() const
    {
      return useDcmtk_;
    }
    
    virtual bool TranscodeToBuffer(std::string& target,
                                   bool& hasSopInstanceUidChanged /* out */,
                                   const void* buffer,
                                   size_t size,
                                   const std::set<DicomTransferSyntax>& allowedSyntaxes,
                                   bool allowNewSopInstanceUid) ORTHANC_OVERRIDE;
    
    virtual DcmFileFormat* TranscodeToParsed(bool& hasSopInstanceUidChanged /* out */,
                                             const void* buffer,
                                             size_t size,
                                             const std::set<DicomTransferSyntax>& allowedSyntaxes,
                                             bool allowNewSopInstanceUid) ORTHANC_OVERRIDE;

    virtual bool HasInplaceTranscode(
      DicomTransferSyntax inputSyntax,
      const std::set<DicomTransferSyntax>& outputSyntaxes) const ORTHANC_OVERRIDE;
    
    virtual bool InplaceTranscode(bool& hasSopInstanceUidChanged /* out */,
                                  DcmFileFormat& dicom,
                                  const std::set<DicomTransferSyntax>& allowedSyntaxes,
                                  bool allowNewSopInstanceUid) ORTHANC_OVERRIDE;
  };
}
