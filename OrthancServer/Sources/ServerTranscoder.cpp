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


#include "PrecompiledHeadersServer.h"
#include "ServerTranscoder.h"

#include "../../OrthancFramework/Sources/DicomParsing/DcmtkTranscoder.h"
#include "../../OrthancFramework/Sources/DicomFormat/DicomImageInformation.h"
#include "../../OrthancFramework/Sources/Logging.h"
#include "../../OrthancFramework/Sources/OrthancException.h"
#include "../Plugins/Engine/OrthancPlugins.h"
#include "OrthancConfiguration.h"

#include <dcmtk/dcmdata/dcfilefo.h>


namespace Orthanc
{
  ServerTranscoder::ServerTranscoder(unsigned int maxConcurrentDcmtkTranscoder) :
#if ORTHANC_ENABLE_PLUGINS == 1
    plugins_(NULL),
#endif
    dcmtkTranscoder_(new DcmtkTranscoder(maxConcurrentDcmtkTranscoder)),
    builtinDecoderTranscoderOrder_(BuiltinDecoderTranscoderOrder_After)
  {
    OrthancConfiguration::ReaderLock lock;

    // New option in Orthanc 1.7.0
    builtinDecoderTranscoderOrder_ = StringToBuiltinDecoderTranscoderOrder(lock.GetConfiguration().GetStringParameter("BuiltinDecoderTranscoderOrder"));

    // New option in Orthanc 1.12.6
    dynamic_cast<DcmtkTranscoder&>(*dcmtkTranscoder_).SetDefaultLossyQuality(lock.GetConfiguration().GetDicomLossyTranscodingQuality());
  }


#if ORTHANC_ENABLE_PLUGINS == 1
  void ServerTranscoder::SetPlugins(OrthancPlugins& plugins)
  {
    if (plugins_ == NULL)
    {
      plugins_ = &plugins;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }
#endif


  ImageAccessor* ServerTranscoder::DecodeFrame(const ParsedDicomFile& parsedDicom,
                                               const void* buffer,
                                               size_t size,
                                               unsigned int frameIndex)
  {
    { // check that the target image has a valid/reasonable size before decoding to avoid possible crash or OOB during transcoding
      DicomMap summary;
      parsedDicom.ExtractDicomSummary(summary, 128);

      DicomImageInformation imageInfo(summary);
      imageInfo.ThrowIfInvalidFrameSize();
    }

    std::unique_ptr<ImageAccessor> decoded;

    if (builtinDecoderTranscoderOrder_ == BuiltinDecoderTranscoderOrder_Before)
    {
      // Use Orthanc's built-in decoder

      try
      {
        decoded.reset(parsedDicom.DecodeFrame(frameIndex));
        if (decoded.get() != NULL)
        {
          return decoded.release();
        }
      }
      catch (const OrthancException&) // NOLINT(bugprone-empty-catch)
      { // ignore, we'll try other alternatives
      }
    }

#if ORTHANC_ENABLE_PLUGINS == 1
    if (plugins_ != NULL &&
        plugins_->HasCustomImageDecoder())
    {
      try
      {
        decoded.reset(plugins_->Decode(buffer, size, frameIndex));
      }
      catch (const OrthancException&) // NOLINT(bugprone-empty-catch)
      {
      }

      if (decoded.get() != NULL)
      {
        return decoded.release();
      }
      else if (builtinDecoderTranscoderOrder_ == BuiltinDecoderTranscoderOrder_After)
      {
        LOG(INFO) << "The installed image decoding plugins cannot handle an image, "
                  << "fallback to the built-in DCMTK decoder";
      }
    }
#endif

    if (builtinDecoderTranscoderOrder_ == BuiltinDecoderTranscoderOrder_After)
    {
      try
      {
        decoded.reset(parsedDicom.DecodeFrame(frameIndex));
        if (decoded.get() != NULL)
        {
          return decoded.release();
        }
      }
      catch (OrthancException& e)
      {
        LOG(INFO) << "Failed to decode a DICOM frame: " << e.GetDetails();
      }
    }

#if ORTHANC_ENABLE_PLUGINS == 1
    if (plugins_ != NULL &&
        plugins_->HasCustomTranscoder())
    {
      LOG(INFO) << "The plugins and built-in image decoders failed to decode a frame, "
                << "trying to transcode the file to LittleEndianExplicit using the plugins.";
      DicomImage explicitTemporaryImage;
      DicomImage source;
      std::set<DicomTransferSyntax> allowedSyntaxes;

      source.SetExternalBuffer(buffer, size);
      allowedSyntaxes.insert(DicomTransferSyntax_LittleEndianExplicit);

      if (Transcode(explicitTemporaryImage, source, allowedSyntaxes, TranscodingSopInstanceUidMode_AllowNew))
      {
        std::unique_ptr<ParsedDicomFile> file(explicitTemporaryImage.ReleaseAsParsedDicomFile());
        return file->DecodeFrame(frameIndex);
      }
    }
#endif

    return NULL;  // TODO-Streaming - Throw exception here?
  }


  bool ServerTranscoder::Transcode(DicomImage& target,
                                   DicomImage& source,
                                   const std::set<DicomTransferSyntax>& allowedSyntaxes,
                                   TranscodingSopInstanceUidMode mode)
  {
    unsigned int lossyQuality;

    {
      OrthancConfiguration::ReaderLock lock;
      lossyQuality = lock.GetConfiguration().GetDicomLossyTranscodingQuality();
    }

    return Transcode(target, source, allowedSyntaxes, mode, lossyQuality);
  }


  bool ServerTranscoder::Transcode(DicomImage& target,
                                   DicomImage& source,
                                   const std::set<DicomTransferSyntax>& allowedSyntaxes,
                                   TranscodingSopInstanceUidMode mode,
                                   unsigned int lossyQuality)
  {
    if (builtinDecoderTranscoderOrder_ == BuiltinDecoderTranscoderOrder_Before)
    {
      if (dcmtkTranscoder_->Transcode(target, source, allowedSyntaxes, mode, lossyQuality))
      {
        return true;
      }
    }

#if ORTHANC_ENABLE_PLUGINS == 1
    if (plugins_ != NULL &&
        plugins_->HasCustomTranscoder())
    {
      if (plugins_->Transcode(target, source, allowedSyntaxes, mode))  // TODO: pass lossyQuality to plugins -> needs a new plugin interface
      {
        return true;
      }
      else if (builtinDecoderTranscoderOrder_ == BuiltinDecoderTranscoderOrder_After)
      {
        LOG(INFO) << "The installed transcoding plugins cannot handle an image, "
                  << "fallback to the built-in DCMTK transcoder";
      }
    }
#endif

    if (builtinDecoderTranscoderOrder_ == BuiltinDecoderTranscoderOrder_After)
    {
      return dcmtkTranscoder_->Transcode(target, source, allowedSyntaxes, mode, lossyQuality);
    }
    else
    {
      return false;
    }
  }
}
