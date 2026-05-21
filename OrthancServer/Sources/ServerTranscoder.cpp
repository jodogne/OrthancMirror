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

#include "../../OrthancFramework/Sources/Cache/LeastRecentlyUsedIndex.h"
#include "../../OrthancFramework/Sources/DataSource/DicomDataSource.h"
#include "../../OrthancFramework/Sources/DataSource/StorageAreaDataSource.h"
#include "../../OrthancFramework/Sources/DataSource/TranscoderDataSource.h"
#include "../../OrthancFramework/Sources/DicomParsing/DcmtkTranscoder.h"
#include "../../OrthancFramework/Sources/Logging.h"
#include "../../OrthancFramework/Sources/OrthancException.h"
#include "../Plugins/Engine/OrthancPlugins.h"
#include "DicomInstanceToStore.h"
#include "OrthancConfiguration.h"

#include <boost/thread.hpp>
#include <dcmtk/dcmdata/dcfilefo.h>


static const size_t MAX_CACHE_SIZE = 10000;

namespace Orthanc
{
  namespace
  {
    enum WorkingSource
    {
      WorkingSource_Unknown,
      WorkingSource_Builtin,
      WorkingSource_PluginsDecoder,
      WorkingSource_PluginsTranscoder
    };
  }

  class ServerTranscoder::PImpl
  {
  private:
    boost::mutex  mutex_;
    LeastRecentlyUsedIndex<std::string, WorkingSource>  workingSources_;

  public:
    PImpl()
    {
    }

    WorkingSource LookupWorkingSource(const std::string& attachmentId)
    {
      boost::mutex::scoped_lock lock(mutex_);

      WorkingSource source;
      if (workingSources_.Contains(attachmentId, source))
      {
        return source;
      }
      else
      {
        return WorkingSource_Unknown;
      }
    }

    void SetWorkingSource(const std::string& attachmentId,
                          WorkingSource source)
    {
      if (source == WorkingSource_Unknown)
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
      else
      {
        boost::mutex::scoped_lock lock(mutex_);

        assert(MAX_CACHE_SIZE > 0);

        if (workingSources_.GetSize() >= MAX_CACHE_SIZE &&
            !workingSources_.Contains(attachmentId))
        {
          workingSources_.RemoveOldest();
        }

        workingSources_.AddOrMakeMostRecent(attachmentId, source);
      }
    }
  };

  ServerTranscoder::ServerTranscoder(unsigned int maxConcurrentDcmtkTranscoder) :
#if ORTHANC_ENABLE_PLUGINS == 1
    plugins_(NULL),
#endif
    pimpl_(new PImpl),
    dcmtkTranscoder_(new DcmtkTranscoder(maxConcurrentDcmtkTranscoder)),
    builtinDecoderTranscoderOrder_(BuiltinDecoderTranscoderOrder_After)
  {
    OrthancConfiguration::ReaderLock lock;

    // New option in Orthanc 1.7.0
    builtinDecoderTranscoderOrder_ = StringToBuiltinDecoderTranscoderOrder(lock.GetConfiguration().GetStringParameter("BuiltinDecoderTranscoderOrder", "After"));

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


  bool ServerTranscoder::HasPluginsDecoder() const
  {
#if ORTHANC_ENABLE_PLUGINS == 1
    return (plugins_ != NULL &&
            plugins_->HasCustomImageDecoder());
#else
    return false;
#endif
  }


  bool ServerTranscoder::HasPluginsTranscoder() const
  {
#if ORTHANC_ENABLE_PLUGINS == 1
    return (plugins_ != NULL &&
            plugins_->HasCustomTranscoder());
#else
    return false;
#endif
  }


  ImageAccessor* ServerTranscoder::DecodeFrameBuiltin(const ParsedDicomFile& dicom,
                                                      unsigned int frameIndex)
  {
    // Use Orthanc's built-in decoder

    try
    {
      return dicom.DecodeFrame(frameIndex);
    }
    catch (const OrthancException&) // NOLINT(bugprone-empty-catch)
    {
    }

    return NULL;
  }


  ImageAccessor* ServerTranscoder::DecodeFrameUsingPluginsDecoder(const void* buffer,
                                                                  size_t size,
                                                                  unsigned int frameIndex)
  {
#if ORTHANC_ENABLE_PLUGINS == 1
    if (HasPluginsDecoder())
    {
      try
      {
        return plugins_->Decode(buffer, size, frameIndex);
      }
      catch (const OrthancException&) // NOLINT(bugprone-empty-catch)
      {
      }
    }
#endif

    return NULL;
  }


  ImageAccessor* ServerTranscoder::DecodeFrameUsingPluginsTranscoder(const void* buffer,
                                                                     size_t size,
                                                                     unsigned int frameIndex)
  {
#if ORTHANC_ENABLE_PLUGINS == 1
    if (HasPluginsTranscoder())
    {
      DicomImage transcoded;
      DicomImage source;
      std::set<DicomTransferSyntax> allowedSyntaxes;

      source.SetExternalBuffer(buffer, size);
      allowedSyntaxes.insert(DicomTransferSyntax_LittleEndianExplicit);

      if (plugins_->Transcode(transcoded, source, allowedSyntaxes, TranscodingSopInstanceUidMode_AllowNew))
      {
        try
        {
          std::unique_ptr<ParsedDicomFile> dicom(transcoded.ReleaseAsParsedDicomFile());
          return DecodeFrameBuiltin(*dicom, frameIndex);
        }
        catch (const OrthancException&) // NOLINT(bugprone-empty-catch)
        {
        }
      }
    }
#endif

    return NULL;
  }


  ImageAccessor* ServerTranscoder::DecodeFrameBuiltin(const boost::shared_ptr<DataSourceReader>& dicomReader,
                                                      const FileInfo& attachment,
                                                      unsigned int frameIndex,
                                                      bool checkMD5)
  {
    std::unique_ptr<DicomDataSource::Dicom> dicom(
      DicomDataSource::ReadWhole(*dicomReader, attachment, checkMD5));

    DicomDataSource::Dicom::Lock lock(*dicom);

    return DecodeFrameBuiltin(lock.GetContent(), frameIndex);
  }


  ImageAccessor* ServerTranscoder::DecodeFrameUsingPluginsDecoder(const boost::shared_ptr<DataSourceReader>& storageAreaReader,
                                                                  const FileInfo& attachment,
                                                                  unsigned int frameIndex,
                                                                  bool checkMD5)
  {
    if (!HasPluginsDecoder())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      std::unique_ptr<StorageAreaDataSource::Range> range(
        StorageAreaDataSource::ReadAttachment(*storageAreaReader, attachment, true /* uncompress */, checkMD5));

      return DecodeFrameUsingPluginsDecoder(range->GetData(), range->GetSize(), frameIndex);
    }
  }


  ImageAccessor* ServerTranscoder::DecodeFrameUsingPluginsTranscoder(const boost::shared_ptr<DataSourceReader>& transcoderReader,
                                                                     const FileInfo& attachment,
                                                                     unsigned int frameIndex,
                                                                     bool checkMD5)
  {
    if (!HasPluginsDecoder())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      std::unique_ptr<TranscoderDataSource::Transcoded> transcoded(
        TranscoderDataSource::Transcode(*transcoderReader, attachment, DicomTransferSyntax_LittleEndianExplicit, checkMD5));

      TranscoderDataSource::Transcoded::Lock lock(*transcoded);

      return DecodeFrameBuiltin(lock.GetDicom(), frameIndex);
    }
  }


  ImageAccessor* ServerTranscoder::DecodeFrame(const DicomInstanceToStore& image,
                                               unsigned int frameIndex)
  {
    std::unique_ptr<ImageAccessor> decoded;

    if (builtinDecoderTranscoderOrder_ == BuiltinDecoderTranscoderOrder_Before)
    {
      decoded.reset(DecodeFrameBuiltin(image.GetParsedDicomFile(), frameIndex));
      if (decoded.get() != NULL)
      {
        return decoded.release();
      }
    }

    decoded.reset(DecodeFrameUsingPluginsDecoder(image.GetBufferData(), image.GetBufferSize(), frameIndex));
    if (decoded.get() != NULL)
    {
      return decoded.release();
    }

    if (builtinDecoderTranscoderOrder_ == BuiltinDecoderTranscoderOrder_After)
    {
      decoded.reset(DecodeFrameBuiltin(image.GetParsedDicomFile(), frameIndex));
      if (decoded.get() != NULL)
      {
        return decoded.release();
      }
    }

    decoded.reset(DecodeFrameUsingPluginsTranscoder(image.GetBufferData(), image.GetBufferSize(), frameIndex));
    if (decoded.get() != NULL)
    {
      return decoded.release();
    }

    return NULL;
  }


  ImageAccessor* ServerTranscoder::DecodeFrame(const boost::shared_ptr<DataSourceReader>& dicomReader,        // For built-in decoding
                                               const boost::shared_ptr<DataSourceReader>& storageAreaReader,  // For plugin-based decoding
                                               const boost::shared_ptr<DataSourceReader>& transcoderReader,   // For transcoding-based decoding
                                               const FileInfo& attachment,
                                               unsigned int frameIndex,
                                               bool checkMD5)
  {
    // Step 1: Try with the last decoder that successfully handled this DICOM attachment

    const WorkingSource source = pimpl_->LookupWorkingSource(attachment.GetUuid());

    std::unique_ptr<ImageAccessor> decoded;

    switch (source)
    {
      case WorkingSource_Unknown:
        break;

      case WorkingSource_Builtin:
        decoded.reset(DecodeFrameBuiltin(dicomReader, attachment, frameIndex, checkMD5));
        break;

      case WorkingSource_PluginsDecoder:
        decoded.reset(DecodeFrameUsingPluginsDecoder(storageAreaReader, attachment, frameIndex, checkMD5));
        break;

      case WorkingSource_PluginsTranscoder:
        decoded.reset(DecodeFrameUsingPluginsTranscoder(transcoderReader, attachment, frameIndex, checkMD5));
        break;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    if (decoded.get() != NULL)
    {
      return decoded.release();
    }

    // Step 2: If this attachment is unknown, try the available decoders

    if (builtinDecoderTranscoderOrder_ == BuiltinDecoderTranscoderOrder_Before)
    {
      decoded.reset(DecodeFrameBuiltin(dicomReader, attachment, frameIndex, checkMD5));

      if (decoded.get() != NULL)
      {
        pimpl_->SetWorkingSource(attachment.GetUuid(), WorkingSource_Builtin);
        return decoded.release();
      }
    }

    if (HasPluginsDecoder())
    {
      decoded.reset(DecodeFrameUsingPluginsDecoder(storageAreaReader, attachment, frameIndex, checkMD5));

      if (decoded.get() != NULL)
      {
        pimpl_->SetWorkingSource(attachment.GetUuid(), WorkingSource_PluginsDecoder);
        return decoded.release();
      }
    }

    if (builtinDecoderTranscoderOrder_ == BuiltinDecoderTranscoderOrder_After)
    {
      decoded.reset(DecodeFrameBuiltin(dicomReader, attachment, frameIndex, checkMD5));

      if (decoded.get() != NULL)
      {
        pimpl_->SetWorkingSource(attachment.GetUuid(), WorkingSource_Builtin);
        return decoded.release();
      }
    }

    if (HasPluginsTranscoder())
    {
      decoded.reset(DecodeFrameUsingPluginsTranscoder(transcoderReader, attachment, frameIndex, checkMD5));

      if (decoded.get() != NULL)
      {
        pimpl_->SetWorkingSource(attachment.GetUuid(), WorkingSource_PluginsTranscoder);
        return decoded.release();
      }
    }

    return NULL;
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
