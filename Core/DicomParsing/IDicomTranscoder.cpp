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
#include "IDicomTranscoder.h"

#include "../OrthancException.h"
#include "FromDcmtkBridge.h"
#include "ParsedDicomFile.h"

#include <dcmtk/dcmdata/dcfilefo.h>

namespace Orthanc
{
  void IDicomTranscoder::DicomImage::Parse()
  {
    if (parsed_.get() != NULL ||
        buffer_.get() == NULL)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
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
  
  
  void IDicomTranscoder::DicomImage::Serialize()
  {
    if (parsed_.get() == NULL ||
        buffer_.get() != NULL)
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

  
  void IDicomTranscoder::DicomImage::Clear()
  {
    parsed_.reset(NULL);
    buffer_.reset(NULL);
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
    else if (parsed_.get() != NULL)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else if (parsed->getDataset() == NULL)
    {
      throw OrthancException(ErrorCode_InternalError);
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
    if (buffer_.get() != NULL)
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
    if (buffer_.get() != NULL)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else if (other.buffer_.get() == NULL)
    {
      buffer_.reset(NULL);
    }
    else
    {
      buffer_.reset(other.buffer_.release());
    }    
  }

  
  DcmFileFormat& IDicomTranscoder::DicomImage::GetParsed()
  {
    if (parsed_.get() != NULL)
    {
      return *parsed_;
    }
    else if (buffer_.get() != NULL)
    {
      Parse();
      return *parsed_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls,
                             "AcquireParsed() or AcquireBuffer() should have been called");
    }
  }
  

  DcmFileFormat* IDicomTranscoder::DicomImage::ReleaseParsed()
  {
    if (parsed_.get() != NULL)
    {
      buffer_.reset(NULL);
      return parsed_.release();
    }
    else if (buffer_.get() != NULL)
    {
      Parse();
      buffer_.reset(NULL);
      return parsed_.release();
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls,
                             "AcquireParsed() or AcquireBuffer() should have been called");
    }
  }

  
  const void* IDicomTranscoder::DicomImage::GetBufferData()
  {
    if (buffer_.get() == NULL)
    {
      Serialize();
    }

    assert(buffer_.get() != NULL);
    return buffer_->empty() ? NULL : buffer_->c_str();
  }

  
  size_t IDicomTranscoder::DicomImage::GetBufferSize()
  {
    if (buffer_.get() == NULL)
    {
      Serialize();
    }

    assert(buffer_.get() != NULL);
    return buffer_->size();
  }


  IDicomTranscoder::TranscodedDicom::TranscodedDicom(bool hasSopInstanceUidChanged) :
    external_(NULL),
    hasSopInstanceUidChanged_(hasSopInstanceUidChanged)
  {
  }
  

  IDicomTranscoder::TranscodedDicom*
  IDicomTranscoder::TranscodedDicom::CreateFromExternal(DcmFileFormat& dicom,
                                                        bool hasSopInstanceUidChanged)
  {
    std::unique_ptr<TranscodedDicom> transcoded(new TranscodedDicom(hasSopInstanceUidChanged));
    transcoded->external_ = &dicom;
    return transcoded.release();
  }        

  
  IDicomTranscoder::TranscodedDicom*
  IDicomTranscoder::TranscodedDicom::CreateFromInternal(DcmFileFormat* dicom,
                                                        bool hasSopInstanceUidChanged)
  {
    if (dicom == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
    else
    {
      std::unique_ptr<TranscodedDicom> transcoded(new TranscodedDicom(hasSopInstanceUidChanged));
      transcoded->internal_.reset(dicom);
      return transcoded.release();
    }
  }

  
  DcmFileFormat& IDicomTranscoder::TranscodedDicom::GetDicom() const
  {
    if (internal_.get() != NULL)
    {
      return *internal_.get();
    }
    else if (external_ != NULL)
    {
      return *external_;
    }
    else
    {
      // Probably results from a call to "ReleaseDicom()"
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  DcmFileFormat* IDicomTranscoder::TranscodedDicom::ReleaseDicom()
  {
    if (internal_.get() != NULL)
    {
      return internal_.release();
    }
    else if (external_ != NULL)
    {
      return new DcmFileFormat(*external_);  // Clone
    }
    else
    {
      // Probably results from a call to "ReleaseDicom()"
      throw OrthancException(ErrorCode_BadSequenceOfCalls);      
    }        
  }
}
