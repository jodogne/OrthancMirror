/**
 * Palanthir - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012 Medical Physics Department, CHU of Liege,
 * Belgium
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


#pragma once

#include "../Core/DicomFormat/DicomMap.h"
#include <dcmtk/dcmdata/dcdatset.h>
#include <json/json.h>

namespace Palanthir
{
  enum ImageExtractionMode
  {
    ImageExtractionMode_Preview,
    ImageExtractionMode_UInt8,
    ImageExtractionMode_UInt16
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

    static DicomTag FindTag(const char* name);

    static DicomTag FindTag(const std::string& name)
    {
      return FindTag(name.c_str());
    }

    static bool HasTag(const DicomMap& fields,
                       const std::string& tagName)
    {
      return fields.HasTag(FindTag(tagName));
    }

    static const DicomValue& GetValue(const DicomMap& fields,
                                      const std::string& tagName)
    {
      return fields.GetValue(FindTag(tagName));
    }

    static void SetValue(DicomMap& target,
                         const std::string& tagName,
                         DicomValue* value)
    {
      target.SetValue(FindTag(tagName), value);
    }

    static void Print(FILE* fp, 
                      const DicomMap& m);

    static void ToJson(Json::Value& result,
                       const DicomMap& values);
  };
}
