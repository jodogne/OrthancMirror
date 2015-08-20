/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
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


#pragma once

#include "../Core/DicomFormat/DicomInstanceHasher.h"
#include "../Core/RestApi/RestApiOutput.h"
#include "ServerEnumerations.h"
#include "../Core/ImageFormats/ImageAccessor.h"
#include "../Core/ImageFormats/ImageBuffer.h"

namespace Orthanc
{
  class ParsedDicomFile : public IDynamicObject
  {
  private:
    struct PImpl;
    PImpl* pimpl_;

    ParsedDicomFile(ParsedDicomFile& other);

    void Setup(const char* content,
               size_t size);

    void RemovePrivateTagsInternal(const std::set<DicomTag>* toKeep);

  public:
    ParsedDicomFile();  // Create a minimal DICOM instance

    ParsedDicomFile(const char* content,
                    size_t size);

    ParsedDicomFile(const std::string& content);

    ~ParsedDicomFile();

    void* GetDcmtkObject();

    ParsedDicomFile* Clone();

    void SendPathValue(RestApiOutput& output,
                       const UriComponents& uri);

    void Answer(RestApiOutput& output);

    void Remove(const DicomTag& tag);

    void Insert(const DicomTag& tag,
                const std::string& value);

    void Replace(const DicomTag& tag,
                 const std::string& value,
                 DicomReplaceMode mode = DicomReplaceMode_InsertIfAbsent);

    void RemovePrivateTags()
    {
      RemovePrivateTagsInternal(NULL);
    }

    void RemovePrivateTags(const std::set<DicomTag>& toKeep)
    {
      RemovePrivateTagsInternal(&toKeep);
    }

    bool GetTagValue(std::string& value,
                     const DicomTag& tag);

    DicomInstanceHasher GetHasher();

    void SaveToMemoryBuffer(std::string& buffer);

    void SaveToFile(const std::string& path);

    void EmbedImage(const ImageAccessor& accessor);

    void EmbedImage(const std::string& dataUriScheme);

    void EmbedImage(const std::string& mime,
                    const std::string& content);

    void ExtractImage(ImageBuffer& result,
                      unsigned int frame);

    void ExtractImage(ImageBuffer& result,
                      unsigned int frame,
                      ImageExtractionMode mode);

    void ExtractPngImage(std::string& result,
                         unsigned int frame,
                         ImageExtractionMode mode);

    Encoding GetEncoding() const;

    void SetEncoding(Encoding encoding);

    void ToJson(Json::Value& target, 
                bool simplify);

    bool HasTag(const DicomTag& tag) const;

    void EmbedPdf(const std::string& pdf);

    bool ExtractPdf(std::string& pdf);
  };

}
