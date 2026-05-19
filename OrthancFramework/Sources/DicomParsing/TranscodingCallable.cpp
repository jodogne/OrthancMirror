/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "../PrecompiledHeaders.h"
#include "TranscodingCallable.h"

#include "../OrthancException.h"

#include <dcmtk/dcmdata/dcfilefo.h>


namespace Orthanc
{
  TranscodingCallable::TranscodingCallable() :
    mode_(TranscodingSopInstanceUidMode_AllowNew),
    hasLossyQuality_(false),
    lossyQuality_(0)
  {
  }


  void TranscodingCallable::SetTranscoder(const boost::shared_ptr<IDicomTranscoder>& transcoder)
  {
    if (transcoder_.get() == NULL)
    {
      transcoder_ = transcoder;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  void TranscodingCallable::SetLossyQuality(unsigned int quality)
  {
    hasLossyQuality_ = true;
    lossyQuality_ = quality;
  }


  IDynamicObject* TranscodingCallable::Call()
  {
    if (transcoder_.get() == NULL)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    std::unique_ptr<IDicomTranscoder::DicomImage> transcoded(new IDicomTranscoder::DicomImage);
    bool success;

    if (hasLossyQuality_)
    {
      success = transcoder_->Transcode(*transcoded, source_, allowedSyntaxes_, mode_, lossyQuality_);
    }
    else
    {
      success = transcoder_->Transcode(*transcoded, source_, allowedSyntaxes_, mode_);
    }

    if (success)
    {
      return transcoded.release();
    }
    else
    {
      if (allowedSyntaxes_.size() == 1)
      {
        DicomTransferSyntax syntax = *allowedSyntaxes_.begin();
        throw OrthancException(ErrorCode_InternalError,
                               "Cannot transcode to transfer syntax: " +
                               std::string(GetTransferSyntaxUid(syntax)));
      }
      else
      {
        throw OrthancException(ErrorCode_NotImplemented, "Cannot transcode DICOM image");
      }
    }
  }
}
