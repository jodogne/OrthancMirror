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

#if ORTHANC_ENABLE_DCMTK_TRANSCODING != 1
#  error Transcoding is disabled, cannot compile this file
#endif

#include "IDicomTranscoder.h"

namespace Orthanc
{
  class ORTHANC_PUBLIC DcmtkTranscoder : public IDicomTranscoder
  {
  private:
    unsigned int  lossyQuality_;
    
    bool InplaceTranscode(DicomTransferSyntax& selectedSyntax /* out */,
                          DcmFileFormat& dicom,
                          const std::set<DicomTransferSyntax>& allowedSyntaxes,
                          bool allowNewSopInstanceUid);
    
  public:
    DcmtkTranscoder();

    void SetLossyQuality(unsigned int quality);

    unsigned int GetLossyQuality() const;
    
    static bool IsSupported(DicomTransferSyntax syntax);

    virtual bool Transcode(DicomImage& target,
                           DicomImage& source /* in, "GetParsed()" possibly modified */,
                           const std::set<DicomTransferSyntax>& allowedSyntaxes,
                           bool allowNewSopInstanceUid) ORTHANC_OVERRIDE;
  };
}
