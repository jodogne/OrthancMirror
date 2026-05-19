/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
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

#include "../../OrthancFramework/Sources/DicomParsing/IDicomTranscoder.h"
#include "../../OrthancFramework/Sources/Images/ImageAccessor.h"
#include "ServerEnumerations.h"


namespace Orthanc
{
#if ORTHANC_ENABLE_PLUGINS == 1
  class OrthancPlugins;
#endif

  class ServerTranscoder : public IDicomTranscoder
  {
  private:
#if ORTHANC_ENABLE_PLUGINS == 1
    OrthancPlugins* plugins_;
#endif

    std::unique_ptr<IDicomTranscoder>  dcmtkTranscoder_;
    BuiltinDecoderTranscoderOrder      builtinDecoderTranscoderOrder_;

  public:
    ServerTranscoder(unsigned int maxConcurrentDcmtkTranscoder);

#if ORTHANC_ENABLE_PLUGINS == 1
    void SetPlugins(OrthancPlugins& plugins);
#endif

    ImageAccessor* DecodeFrame(const ParsedDicomFile& parsedDicom,
                               const void* buffer,  // buffer that is the source of the ParsedDicomFile
                               size_t size,
                               unsigned int frameIndex);

    // This method can be used even if the global option
    // "TranscodeDicomProtocol" is set to "false"
    virtual bool Transcode(DicomImage& target,
                           DicomImage& source /* in, "GetParsed()" possibly modified */,
                           const std::set<DicomTransferSyntax>& allowedSyntaxes,
                           TranscodingSopInstanceUidMode mode) ORTHANC_OVERRIDE;

    virtual bool Transcode(DicomImage& target,
                           DicomImage& source /* in, "GetParsed()" possibly modified */,
                           const std::set<DicomTransferSyntax>& allowedSyntaxes,
                           TranscodingSopInstanceUidMode mode,
                           unsigned int lossyQuality)  ORTHANC_OVERRIDE;
  };
}
