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
#include "MemoryBufferTranscoder.h"

#include "../Logging.h"
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

    DicomTransferSyntax a, b;
    if (!const_cast<const ParsedDicomFile&>(parsed).LookupTransferSyntax(b) ||
        !FromDcmtkBridge::LookupOrthancTransferSyntax(a, parsed.GetDcmtkObject()) ||
        a != b ||
        allowedSyntaxes.find(a) == allowedSyntaxes.end())
    {
      throw OrthancException(
        ErrorCode_Plugin,
        "DEBUG - The transcoding plugin has not written to one of the allowed transfer syntaxes");
    }
#endif
  }
    

  bool MemoryBufferTranscoder::Transcode(DicomImage& target,
                                         DicomImage& source,
                                         const std::set<DicomTransferSyntax>& allowedSyntaxes,
                                         bool allowNewSopInstanceUid)
  {
    target.Clear();
    
#if !defined(NDEBUG)
    // Don't run this code in release mode, as it implies parsing the DICOM file
    DicomTransferSyntax sourceSyntax;
    if (!FromDcmtkBridge::LookupOrthancTransferSyntax(sourceSyntax, source.GetParsed()))
    {
      LOG(ERROR) << "Unsupport transfer syntax for transcoding";
      return false;
    }
    
    const std::string sourceSopInstanceUid = GetSopInstanceUid(source.GetParsed());
#endif

    std::string buffer;
    if (TranscodeBuffer(buffer, source.GetBufferData(), source.GetBufferSize(),
                        allowedSyntaxes, allowNewSopInstanceUid))
    {
      CheckTargetSyntax(buffer, allowedSyntaxes);  // For debug only

      target.AcquireBuffer(buffer);
      
#if !defined(NDEBUG)
      // Only run the sanity check in debug mode
      CheckTranscoding(target, sourceSyntax, sourceSopInstanceUid,
                       allowedSyntaxes, allowNewSopInstanceUid);
#endif

      return true;
    }
    else
    {
      return false;
    }
  }
}
