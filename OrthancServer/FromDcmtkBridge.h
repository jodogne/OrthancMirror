/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2013 Medical Physics Department, CHU of Liege,
 * Belgium
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
#include "../Core/Toolbox.h"

#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <json/json.h>
#include <memory>

namespace Orthanc
{
  enum ImageExtractionMode
  {
    ImageExtractionMode_Preview,
    ImageExtractionMode_UInt8,
    ImageExtractionMode_UInt16
  };

  enum DicomRootLevel
  {
    DicomRootLevel_Patient,
    DicomRootLevel_Study,
    DicomRootLevel_Series,
    DicomRootLevel_Instance
  };

  enum DicomReplaceMode
  {
    DicomReplaceMode_InsertIfAbsent,
    DicomReplaceMode_ThrowIfAbsent,
    DicomReplaceMode_IgnoreIfAbsent
  };

  class ParsedDicomFile : public IDynamicObject
  {
  private:
    std::auto_ptr<DcmFileFormat> file_;

    ParsedDicomFile(DcmFileFormat& other) :
      file_(dynamic_cast<DcmFileFormat*>(other.clone()))
    {
    }

    void Setup(const char* content,
               size_t size);

  public:
    ParsedDicomFile(const char* content,
                    size_t size)
    {
      Setup(content, size);
    }

    ParsedDicomFile(const std::string& content)
    {
      if (content.size() == 0)
        Setup(NULL, 0);
      else
        Setup(&content[0], content.size());
    }

    DcmFileFormat& GetDicom()
    {
      return *file_;
    }

    ParsedDicomFile* Clone()
    {
      return new ParsedDicomFile(*file_);
    }

    void SendPathValue(RestApiOutput& output,
                       const UriComponents& uri);

    void Answer(RestApiOutput& output);

    void Remove(const DicomTag& tag);

    void Insert(const DicomTag& tag,
                const std::string& value);

    void Replace(const DicomTag& tag,
                 const std::string& value,
                 DicomReplaceMode mode);

    void RemovePrivateTags();

    bool GetTagValue(std::string& value,
                     const DicomTag& tag);

    DicomInstanceHasher GetHasher();
  };

  class FromDcmtkBridge
  {
  public:
    static void Convert(DicomMap& target, DcmDataset& dataset);

    static DicomTag GetTag(const DcmElement& element);

    static DicomValue* ConvertLeafElement(DcmElement& element);

    static void ToJson(Json::Value& target, 
                       DcmDataset& dataset,
                       unsigned int maxStringLength = 256);       

    static void ToJson(Json::Value& target, 
                       const std::string& path,
                       unsigned int maxStringLength = 256);

    static void ExtractPngImage(std::string& result,
                                DcmDataset& dataset,
                                unsigned int frame,
                                ImageExtractionMode mode);

    static void ExtractPngImage(std::string& result,
                                const std::string& dicomContent,
                                unsigned int frame,
                                ImageExtractionMode mode);

    static std::string GetName(const DicomTag& tag);

    static DicomTag ParseTag(const char* name);

    static DicomTag ParseTag(const std::string& name)
    {
      return ParseTag(name.c_str());
    }

    static bool HasTag(const DicomMap& fields,
                       const std::string& tagName)
    {
      return fields.HasTag(ParseTag(tagName));
    }

    static const DicomValue& GetValue(const DicomMap& fields,
                                      const std::string& tagName)
    {
      return fields.GetValue(ParseTag(tagName));
    }

    static void SetValue(DicomMap& target,
                         const std::string& tagName,
                         DicomValue* value)
    {
      target.SetValue(ParseTag(tagName), value);
    }

    static void Print(FILE* fp, 
                      const DicomMap& m);

    static void ToJson(Json::Value& result,
                       const DicomMap& values);

    static std::string GenerateUniqueIdentifier(DicomRootLevel level);

    static bool SaveToMemoryBuffer(std::string& buffer,
                                   DcmDataset* dataSet);
  };
}
