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
#include "../../OrthancFramework/Sources/FileStorage/FileInfo.h"
#include "../../OrthancFramework/Sources/Images/ImageAccessor.h"
#include "ServerEnumerations.h"

#include <boost/shared_ptr.hpp>


namespace Orthanc
{
  class DataSourceReader;
  class DicomInstanceToStore;

#if ORTHANC_ENABLE_PLUGINS == 1
  class OrthancPlugins;
#endif

  class ServerTranscoder : public IDicomTranscoder
  {
  private:
    class PImpl;

#if ORTHANC_ENABLE_PLUGINS == 1
    OrthancPlugins* plugins_;
#endif

    boost::shared_ptr<PImpl>           pimpl_;
    std::unique_ptr<IDicomTranscoder>  dcmtkTranscoder_;
    BuiltinDecoderTranscoderOrder      builtinDecoderTranscoderOrder_;

    ImageAccessor* DecodeFrameBuiltin(const ParsedDicomFile& dicom,
                                      unsigned int frameIndex);

    ImageAccessor* DecodeFrameUsingPluginsDecoder(const void* buffer,
                                                  size_t size,
                                                  unsigned int frameIndex);

    ImageAccessor* DecodeFrameUsingPluginsTranscoder(const void* buffer,
                                                     size_t size,
                                                     unsigned int frameIndex);

    ImageAccessor* DecodeFrameBuiltin(const boost::shared_ptr<DataSourceReader>& dicomReader,
                                      const FileInfo& attachment,
                                      unsigned int frameIndex);

    ImageAccessor* DecodeFrameUsingPluginsDecoder(const boost::shared_ptr<DataSourceReader>& storageAreaReader,
                                                  const FileInfo& attachment,
                                                  unsigned int frameIndex);

    ImageAccessor* DecodeFrameUsingPluginsTranscoder(const boost::shared_ptr<DataSourceReader>& transcoderReader,
                                                     const FileInfo& attachment,
                                                     unsigned int frameIndex);

  public:
    ServerTranscoder(unsigned int maxConcurrentDcmtkTranscoder);

#if ORTHANC_ENABLE_PLUGINS == 1
    void SetPlugins(OrthancPlugins& plugins);
#endif

    bool HasPluginsDecoder() const;

    bool HasPluginsTranscoder() const;

    // This version should only be used by plugins
    ImageAccessor* DecodeFrame(const DicomInstanceToStore& image,
                               unsigned int frameIndex);

    ImageAccessor* DecodeFrame(const boost::shared_ptr<DataSourceReader>& dicomReader,        // For built-in decoding
                               const boost::shared_ptr<DataSourceReader>& storageAreaReader,  // For plugin-based decoding
                               const boost::shared_ptr<DataSourceReader>& transcoderReader,   // For transcoding-based decoding
                               const FileInfo& attachment,
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
