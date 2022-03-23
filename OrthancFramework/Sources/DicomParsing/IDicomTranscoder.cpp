/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
#include "IDicomTranscoder.h"

#include "../OrthancException.h"
#include "FromDcmtkBridge.h"
#include "ParsedDicomFile.h"

#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcdeftag.h>

namespace Orthanc
{
  IDicomTranscoder::TranscodingType IDicomTranscoder::GetTranscodingType(DicomTransferSyntax target,
                                                                         DicomTransferSyntax source)
  {
    if (target == source)
    {
      return TranscodingType_Lossless;
    }
    else if (target == DicomTransferSyntax_LittleEndianImplicit ||
             target == DicomTransferSyntax_LittleEndianExplicit ||
             target == DicomTransferSyntax_BigEndianExplicit ||
             target == DicomTransferSyntax_DeflatedLittleEndianExplicit ||
             target == DicomTransferSyntax_JPEGProcess14 ||
             target == DicomTransferSyntax_JPEGProcess14SV1 ||
             target == DicomTransferSyntax_JPEGLSLossless ||
             target == DicomTransferSyntax_JPEG2000LosslessOnly ||
             target == DicomTransferSyntax_JPEG2000MulticomponentLosslessOnly)
    {
      return TranscodingType_Lossless;
    }
    else if (target == DicomTransferSyntax_JPEGProcess1 ||
             target == DicomTransferSyntax_JPEGProcess2_4 ||
             target == DicomTransferSyntax_JPEGLSLossy ||
             target == DicomTransferSyntax_JPEG2000 ||
             target == DicomTransferSyntax_JPEG2000Multicomponent)
    {
      return TranscodingType_Lossy;
    }
    else
    {
      return TranscodingType_Unknown;
    }
  }


  std::string IDicomTranscoder::GetSopInstanceUid(DcmFileFormat& dicom)
  {
    if (dicom.getDataset() == NULL)
    {
      throw OrthancException(ErrorCode_InternalError);
    }
    
    DcmDataset& dataset = *dicom.getDataset();

    std::string s;
    if (FromDcmtkBridge::LookupStringValue(s, dataset, DICOM_TAG_SOP_INSTANCE_UID))
    {
      return s;
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat, "File without SOP instance UID");
    }
  }


  void IDicomTranscoder::CheckTranscoding(IDicomTranscoder::DicomImage& transcoded,
                                          DicomTransferSyntax sourceSyntax,
                                          const std::string& sourceSopInstanceUid,
                                          const std::set<DicomTransferSyntax>& allowedSyntaxes,
                                          bool allowNewSopInstanceUid)
  {
    DcmFileFormat& parsed = transcoded.GetParsed();
    
    if (parsed.getDataset() == NULL)
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    std::string targetSopInstanceUid = GetSopInstanceUid(parsed);

    if (parsed.getDataset()->tagExists(DCM_PixelData))
    {
      if (!allowNewSopInstanceUid && (targetSopInstanceUid != sourceSopInstanceUid))
      {
        throw OrthancException(ErrorCode_InternalError);
      }
    }
    else
    {
      if (targetSopInstanceUid != sourceSopInstanceUid)
      {
        throw OrthancException(ErrorCode_InternalError,
                               "No pixel data: Transcoding must not change the SOP instance UID");
      }
    }

    DicomTransferSyntax targetSyntax;
    if (!FromDcmtkBridge::LookupOrthancTransferSyntax(targetSyntax, parsed))
    {
      return;  // Unknown transfer syntax, cannot do further test
    }

    if (allowedSyntaxes.find(sourceSyntax) != allowedSyntaxes.end())
    {
      // No transcoding should have happened
      if (targetSopInstanceUid != sourceSopInstanceUid)
      {
        throw OrthancException(ErrorCode_InternalError);
      }
    }
        
    if (allowedSyntaxes.find(targetSyntax) == allowedSyntaxes.end())
    {
      throw OrthancException(ErrorCode_InternalError, "An incorrect output transfer syntax was chosen");
    }
    
    if (parsed.getDataset()->tagExists(DCM_PixelData))
    {
      switch (GetTranscodingType(targetSyntax, sourceSyntax))
      {
        case TranscodingType_Lossy:
          if (targetSopInstanceUid == sourceSopInstanceUid)
          {
            throw OrthancException(ErrorCode_InternalError);
          }
          break;

        case TranscodingType_Lossless:
          if (targetSopInstanceUid != sourceSopInstanceUid)
          {
            throw OrthancException(ErrorCode_InternalError);
          }
          break;

        default:
          break;
      }
    }
  }


  void IDicomTranscoder::DicomImage::Parse()
  {
    if (parsed_.get() != NULL)
    {
      // Already parsed
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else if (buffer_.get() != NULL)
    {
      if (isExternalBuffer_)
      {
        throw OrthancException(ErrorCode_InternalError);
      }
      else
      {
        parsed_.reset(FromDcmtkBridge::LoadFromMemoryBuffer(
                        buffer_->empty() ? NULL : buffer_->c_str(), buffer_->size()));
        
        if (parsed_.get() == NULL)
        {
          throw OrthancException(ErrorCode_BadFileFormat);
        }      
      }
    }
    else if (isExternalBuffer_)
    {
      parsed_.reset(FromDcmtkBridge::LoadFromMemoryBuffer(externalBuffer_, externalSize_));
      
      if (parsed_.get() == NULL)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }      
    }
    else
    {
      // No buffer is available
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }
  
  
  void IDicomTranscoder::DicomImage::Serialize()
  {
    if (parsed_.get() == NULL ||
        buffer_.get() != NULL ||
        isExternalBuffer_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else if (parsed_->getDataset() == NULL)
    {
      throw OrthancException(ErrorCode_InternalError);
    }
    else
    {
      buffer_.reset(new std::string);
      FromDcmtkBridge::SaveToMemoryBuffer(*buffer_, *parsed_->getDataset());
    }
  }

  
  IDicomTranscoder::DicomImage::DicomImage() :
    isExternalBuffer_(false),
    externalBuffer_(NULL),
    externalSize_(0)
  {
  }


  void IDicomTranscoder::DicomImage::Clear()
  {
    parsed_.reset(NULL);
    buffer_.reset(NULL);
    isExternalBuffer_ = false;
  }

  
  void IDicomTranscoder::DicomImage::AcquireParsed(ParsedDicomFile& parsed)
  {
    AcquireParsed(parsed.ReleaseDcmtkObject());
  }
  
      
  void IDicomTranscoder::DicomImage::AcquireParsed(DcmFileFormat* parsed)
  {
    if (parsed == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
    else if (parsed->getDataset() == NULL)
    {
      throw OrthancException(ErrorCode_InternalError);
    }
    else if (parsed_.get() != NULL)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      parsed_.reset(parsed);
    }
  }
  

  void IDicomTranscoder::DicomImage::AcquireParsed(DicomImage& other)
  {
    AcquireParsed(other.ReleaseParsed());
  }
  

  void IDicomTranscoder::DicomImage::AcquireBuffer(std::string& buffer /* will be swapped */)
  {
    if (buffer_.get() != NULL ||
        isExternalBuffer_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      buffer_.reset(new std::string);
      buffer_->swap(buffer);
    }
  }


  void IDicomTranscoder::DicomImage::AcquireBuffer(DicomImage& other)
  {
    if (buffer_.get() != NULL ||
        isExternalBuffer_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else if (other.isExternalBuffer_)
    {
      assert(other.buffer_.get() == NULL);
      isExternalBuffer_ = true;
      externalBuffer_ = other.externalBuffer_;
      externalSize_ = other.externalSize_;
    }
    else if (other.buffer_.get() != NULL)
    {
      buffer_.reset(other.buffer_.release());
    }
    else
    {
      buffer_.reset(NULL);
    }    
  }

  
  void IDicomTranscoder::DicomImage::SetExternalBuffer(const void* buffer,
                                                       size_t size)
  {
    if (buffer_.get() != NULL ||
        isExternalBuffer_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      isExternalBuffer_ = true;
      externalBuffer_ = buffer;
      externalSize_ = size;
    }
  }


  void IDicomTranscoder::DicomImage::SetExternalBuffer(const std::string& buffer)
  {
    SetExternalBuffer(buffer.empty() ? NULL : buffer.c_str(), buffer.size());
  }


  DcmFileFormat& IDicomTranscoder::DicomImage::GetParsed()
  {
    if (parsed_.get() != NULL)
    {
      return *parsed_;
    }
    else if (buffer_.get() != NULL ||
             isExternalBuffer_)
    {
      Parse();
      return *parsed_;
    }
    else
    {
      throw OrthancException(
        ErrorCode_BadSequenceOfCalls,
        "AcquireParsed(), AcquireBuffer() or SetExternalBuffer() should have been called");
    }
  }
  

  DcmFileFormat* IDicomTranscoder::DicomImage::ReleaseParsed()
  {
    if (parsed_.get() != NULL)
    {
      buffer_.reset(NULL);
      return parsed_.release();
    }
    else if (buffer_.get() != NULL ||
             isExternalBuffer_)
    {
      Parse();
      buffer_.reset(NULL);
      return parsed_.release();
    }
    else
    {
      throw OrthancException(
        ErrorCode_BadSequenceOfCalls,
        "AcquireParsed(), AcquireBuffer() or SetExternalBuffer() should have been called");
    }
  }


  ParsedDicomFile* IDicomTranscoder::DicomImage::ReleaseAsParsedDicomFile()
  {
    return ParsedDicomFile::AcquireDcmtkObject(ReleaseParsed());
  }

  
  const void* IDicomTranscoder::DicomImage::GetBufferData()
  {
    if (isExternalBuffer_)
    {
      assert(buffer_.get() == NULL);
      return externalBuffer_;
    }
    else
    {    
      if (buffer_.get() == NULL)
      {
        Serialize();
      }

      assert(buffer_.get() != NULL);
      return buffer_->empty() ? NULL : buffer_->c_str();
    }
  }

  
  size_t IDicomTranscoder::DicomImage::GetBufferSize()
  {
    if (isExternalBuffer_)
    {
      assert(buffer_.get() == NULL);
      return externalSize_;
    }
    else
    {    
      if (buffer_.get() == NULL)
      {
        Serialize();
      }

      assert(buffer_.get() != NULL);
      return buffer_->size();
    }
  }
}
