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

#if !defined(NDEBUG)  // For debugging
#  include "ParsedDicomFile.h"
#endif

namespace Orthanc
{
  static void CheckTargetSyntax(const std::string& transcoded,
                                const std::set<DicomTransferSyntax>& allowedSyntaxes)
  {
#if !defined(NDEBUG)
    // Debug mode
    ParsedDicomFile parsed(transcoded);

    std::string s;
    DicomTransferSyntax a, b;
    if (!parsed.LookupTransferSyntax(s) ||
        !FromDcmtkBridge::LookupOrthancTransferSyntax(a, parsed.GetDcmtkObject()) ||
        !LookupTransferSyntax(b, s) ||
        a != b ||
        allowedSyntaxes.find(a) == allowedSyntaxes.end())
    {
      throw OrthancException(
        ErrorCode_Plugin,
        "DEBUG - The transcoding plugin has not written to one of the allowed transfer syntaxes");
    }
#endif
  }
    

  bool MemoryBufferTranscoder::TranscodeParsedToBuffer(
    std::string& target /* out */,
    bool& hasSopInstanceUidChanged /* out */,
    DcmFileFormat& dicom /* in, possibly modified */,
    DicomTransferSyntax targetSyntax,
    bool allowNewSopInstanceUid)
  {
    if (dicom.getDataset() == NULL)
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    std::string source;
    FromDcmtkBridge::SaveToMemoryBuffer(source, *dicom.getDataset());

    const void* data = source.empty() ? NULL : source.c_str();

    std::set<DicomTransferSyntax> allowedSyntaxes;
    allowedSyntaxes.insert(targetSyntax);

    if (Transcode(target, hasSopInstanceUidChanged,
                  data, source.size(), allowedSyntaxes, allowNewSopInstanceUid))
    {
      CheckTargetSyntax(target, allowedSyntaxes);
      return true;
    }
    else
    {
      return false;
    }    
  }
  

  IDicomTranscoder::TranscodedDicom* MemoryBufferTranscoder::TranscodeToParsed(
    DcmFileFormat& dicom /* in, possibly modified */,
    const void* buffer /* in, same DICOM file as "dicom" */,
    size_t size,
    const std::set<DicomTransferSyntax>& allowedSyntaxes,
    bool allowNewSopInstanceUid)
  {
    bool hasSopInstanceUidChanged;
    
    std::string target;
    if (Transcode(target, hasSopInstanceUidChanged,
                  buffer, size, allowedSyntaxes, allowNewSopInstanceUid))
    {
      CheckTargetSyntax(target, allowedSyntaxes);
      
      const void* data = target.empty() ? NULL : target.c_str();
      return IDicomTranscoder::TranscodedDicom::CreateFromInternal(
        FromDcmtkBridge::LoadFromMemoryBuffer(data, target.size()), hasSopInstanceUidChanged);
    }
    else
    {
      return NULL;
    }
  }
}
