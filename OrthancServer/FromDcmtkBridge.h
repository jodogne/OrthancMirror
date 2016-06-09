/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
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

#include "ServerEnumerations.h"

#include "../Core/DicomFormat/DicomMap.h"

#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcmetinf.h>
#include <dcmtk/dcmdata/dcpixseq.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <json/json.h>

namespace Orthanc
{
  class FromDcmtkBridge
  {
  public:
    static void InitializeDictionary();

    static void RegisterDictionaryTag(const DicomTag& tag,
                                      ValueRepresentation vr,
                                      const std::string& name,
                                      unsigned int minMultiplicity,
                                      unsigned int maxMultiplicity);

    static Encoding DetectEncoding(DcmItem& dataset,
                                   Encoding defaultEncoding);

    static void Convert(DicomMap& target, 
                        DcmItem& dataset,
                        unsigned int maxStringLength,
                        Encoding defaultEncoding);

    static DicomTag Convert(const DcmTag& tag);

    static DicomTag GetTag(const DcmElement& element);

    static bool IsUnknownTag(const DicomTag& tag);

    static DicomValue* ConvertLeafElement(DcmElement& element,
                                          DicomToJsonFlags flags,
                                          unsigned int maxStringLength,
                                          Encoding encoding);

    static void ToJson(Json::Value& parent,
                       DcmElement& element,
                       DicomToJsonFormat format,
                       DicomToJsonFlags flags,
                       unsigned int maxStringLength,
                       Encoding dicomEncoding);

    static void ToJson(Json::Value& target, 
                       DcmDataset& dataset,
                       DicomToJsonFormat format,
                       DicomToJsonFlags flags,
                       unsigned int maxStringLength,
                       Encoding defaultEncoding);

    static void ToJson(Json::Value& target, 
                       DcmMetaInfo& header,
                       DicomToJsonFormat format,
                       DicomToJsonFlags flags,
                       unsigned int maxStringLength);

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

    static void ToJson(Json::Value& result,
                       const DicomMap& values,
                       bool simplify);

    static std::string GenerateUniqueIdentifier(ResourceType level);

    static bool SaveToMemoryBuffer(std::string& buffer,
                                   DcmDataset& dataSet);

    static ValueRepresentation Convert(DcmEVR vr);

    static ValueRepresentation LookupValueRepresentation(const DicomTag& tag);

    static DcmElement* CreateElementForTag(const DicomTag& tag);
    
    static void FillElementWithString(DcmElement& element,
                                      const DicomTag& tag,
                                      const std::string& utf8alue,  // Encoded using UTF-8
                                      bool decodeDataUriScheme,
                                      Encoding dicomEncoding);

    static DcmElement* FromJson(const DicomTag& tag,
                                const Json::Value& element,  // Encoded using UTF-8
                                bool decodeDataUriScheme,
                                Encoding dicomEncoding);

    static DcmPixelSequence* GetPixelSequence(DcmDataset& dataset);

    static Encoding ExtractEncoding(const Json::Value& json,
                                    Encoding defaultEncoding);

    static DcmDataset* FromJson(const Json::Value& json,  // Encoded using UTF-8
                                bool generateIdentifiers,
                                bool decodeDataUriScheme,
                                Encoding defaultEncoding);

    static DcmFileFormat* LoadFromMemoryBuffer(const void* buffer,
                                               size_t size);

    static void FromJson(DicomMap& values,
                         const Json::Value& result);
  };
}
