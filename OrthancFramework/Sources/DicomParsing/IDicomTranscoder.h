/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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


#pragma once

#include "../Compatibility.h"
#include "../Enumerations.h"

#include <boost/noncopyable.hpp>
#include <set>

class DcmFileFormat;

namespace Orthanc
{
  class ParsedDicomFile;
  
  /**
   * WARNING: This class might be called from several threads at
   * once. Make sure to implement proper locking.
   **/
  class ORTHANC_PUBLIC IDicomTranscoder : public boost::noncopyable
  {
  public:
    class ORTHANC_PUBLIC DicomImage : public boost::noncopyable
    {
    private:
      std::unique_ptr<DcmFileFormat>  parsed_;
      std::unique_ptr<std::string>    buffer_;
      bool                            isExternalBuffer_;
      const void*                     externalBuffer_;
      size_t                          externalSize_;

      void Parse();

      void Serialize();

      DcmFileFormat* ReleaseParsed();

    public:
      DicomImage();
      
      void Clear();
      
      // Calling this method will invalidate the "ParsedDicomFile" object
      void AcquireParsed(ParsedDicomFile& parsed);
      
      void AcquireParsed(DcmFileFormat* parsed);

      void AcquireParsed(DicomImage& other);

      void AcquireBuffer(std::string& buffer /* will be swapped */);

      void AcquireBuffer(DicomImage& other);

      void SetExternalBuffer(const void* buffer,
                             size_t size);

      void SetExternalBuffer(const std::string& buffer);

      DcmFileFormat& GetParsed();

      ParsedDicomFile* ReleaseAsParsedDicomFile();

      const void* GetBufferData();

      size_t GetBufferSize();
    };


  protected:
    enum TranscodingType
    {
      TranscodingType_Lossy,
      TranscodingType_Lossless,
      TranscodingType_Unknown
    };

    static TranscodingType GetTranscodingType(DicomTransferSyntax target,
                                              DicomTransferSyntax source);

    static void CheckTranscoding(DicomImage& transcoded,
                                 DicomTransferSyntax sourceSyntax,
                                 const std::string& sourceSopInstanceUid,
                                 const std::set<DicomTransferSyntax>& allowedSyntaxes,
                                 bool allowNewSopInstanceUid);
    
  public:    
    virtual ~IDicomTranscoder()
    {
    }

    virtual bool Transcode(DicomImage& target,
                           DicomImage& source /* in, "GetParsed()" possibly modified */,
                           const std::set<DicomTransferSyntax>& allowedSyntaxes,
                           bool allowNewSopInstanceUid) = 0;

    static std::string GetSopInstanceUid(DcmFileFormat& dicom);
  };
}
