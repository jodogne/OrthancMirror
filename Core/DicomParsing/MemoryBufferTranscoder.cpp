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
#include "MemoryBufferTranscoder.h"

#include "../OrthancException.h"
#include "FromDcmtkBridge.h"

namespace Orthanc
{
  MemoryBufferTranscoder::MemoryBufferTranscoder(bool tryDcmtk) :
    tryDcmtk_(tryDcmtk)
  {
#if ORTHANC_ENABLE_DCMTK_TRANSCODING != 1
    if (tryDcmtk)
    {
      throw OrthancException(ErrorCode_NotImplemented,
                             "Orthanc was built without support for DMCTK transcoding");
    }
#endif    
  }

  bool MemoryBufferTranscoder::TranscodeToBuffer(std::string& target,
                                                 const void* buffer,
                                                 size_t size,
                                                 const std::set<DicomTransferSyntax>& allowedSyntaxes,
                                                 bool allowNewSopInstanceUid)
  {
#if ORTHANC_ENABLE_DCMTK_TRANSCODING == 1
    if (tryDcmtk_)
    {
      return dcmtk_.TranscodeToBuffer(target, buffer, size, allowedSyntaxes, allowNewSopInstanceUid);
    }
    else
#endif
    {
      return Transcode(target, buffer, size, allowedSyntaxes, allowNewSopInstanceUid);
    }
  }

  
  DcmFileFormat* MemoryBufferTranscoder::TranscodeToParsed(const void* buffer,
                                                           size_t size,
                                                           const std::set<DicomTransferSyntax>& allowedSyntaxes,
                                                           bool allowNewSopInstanceUid)
  {
#if ORTHANC_ENABLE_DCMTK_TRANSCODING == 1
    if (tryDcmtk_)
    {
      return dcmtk_.TranscodeToParsed(buffer, size, allowedSyntaxes, allowNewSopInstanceUid);
    }
    else
#endif
    {
      std::string transcoded;
      if (Transcode(transcoded, buffer, size, allowedSyntaxes, allowNewSopInstanceUid))
      {
        return FromDcmtkBridge::LoadFromMemoryBuffer(
          transcoded.empty() ? NULL : transcoded.c_str(), transcoded.size());
      }
      else
      {
        return NULL;
      }
    }
  }


  bool MemoryBufferTranscoder::InplaceTranscode(DcmFileFormat& dicom,
                                                const std::set<DicomTransferSyntax>& allowedSyntaxes,
                                                bool allowNewSopInstanceUid)
  {
#if ORTHANC_ENABLE_DCMTK_TRANSCODING == 1
    if (tryDcmtk_)
    {
      return dcmtk_.InplaceTranscode(dicom, allowedSyntaxes, allowNewSopInstanceUid);
    }
    else
#endif
    {
      // "HasInplaceTranscode()" should have been called
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }
}
