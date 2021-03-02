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

#include "ITagVisitor.h"
#include "../DicomFormat/DicomElement.h"
#include "../DicomFormat/DicomMap.h"

#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcmetinf.h>
#include <dcmtk/dcmdata/dcpixseq.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <json/value.h>

#if ORTHANC_ENABLE_DCMTK != 1
#  error The macro ORTHANC_ENABLE_DCMTK must be set to 1
#endif

#if ORTHANC_BUILD_UNIT_TESTS == 1
#  include <gtest/gtest_prod.h>
#endif

#if !defined(ORTHANC_ENABLE_DCMTK_JPEG)
#  error The macro ORTHANC_ENABLE_DCMTK_JPEG must be defined
#endif

#if !defined(ORTHANC_ENABLE_DCMTK_JPEG_LOSSLESS)
#  error The macro ORTHANC_ENABLE_DCMTK_JPEG_LOSSLESS must be defined
#endif


namespace Orthanc
{
  class ORTHANC_PUBLIC FromDcmtkBridge : public boost::noncopyable
  {
#if ORTHANC_BUILD_UNIT_TESTS == 1
    FRIEND_TEST(FromDcmtkBridge, FromJson);
#endif

    friend class ParsedDicomFile;

  private:
    FromDcmtkBridge();  // Pure static class

    static void DatasetToJson(Json::Value& parent,
                              DcmItem& item,
                              DicomToJsonFormat format,
                              DicomToJsonFlags flags,
                              unsigned int maxStringLength,
                              Encoding encoding,
                              bool hasCodeExtensions,
                              const std::set<DicomTag>& ignoreTagLength,
                              unsigned int depth);

    static void ElementToJson(Json::Value& parent,
                              DcmElement& element,
                              DicomToJsonFormat format,
                              DicomToJsonFlags flags,
                              unsigned int maxStringLength,
                              Encoding dicomEncoding,
                              bool hasCodeExtensions,
                              const std::set<DicomTag>& ignoreTagLength,
                              unsigned int depth);

    static void ChangeStringEncoding(DcmItem& dataset,
                                     Encoding source,
                                     bool hasSourceCodeExtensions,
                                     Encoding target);

  public:
    static void InitializeDictionary(bool loadPrivateDictionary);

    static void RegisterDictionaryTag(const DicomTag& tag,
                                      ValueRepresentation vr,
                                      const std::string& name,
                                      unsigned int minMultiplicity,
                                      unsigned int maxMultiplicity,
                                      const std::string& privateCreator);

    static Encoding DetectEncoding(bool& hasCodeExtensions,
                                   DcmItem& dataset,
                                   Encoding defaultEncoding);

    // Compatibility wrapper for Orthanc <= 1.5.4
    static Encoding DetectEncoding(DcmItem& dataset,
                                   Encoding defaultEncoding);

    static DicomTag Convert(const DcmTag& tag);

    static DicomTag GetTag(const DcmElement& element);

    static bool IsUnknownTag(const DicomTag& tag);

    static DicomValue* ConvertLeafElement(DcmElement& element,
                                          DicomToJsonFlags flags,
                                          unsigned int maxStringLength,
                                          Encoding encoding,
                                          bool hasCodeExtensions,
                                          const std::set<DicomTag>& ignoreTagLength);

    static void ExtractHeaderAsJson(Json::Value& target, 
                                    DcmMetaInfo& header,
                                    DicomToJsonFormat format,
                                    DicomToJsonFlags flags,
                                    unsigned int maxStringLength);

    static std::string GetTagName(const DicomTag& tag,
                                  const std::string& privateCreator);

    static std::string GetTagName(const DcmElement& element);

    static std::string GetTagName(const DicomElement& element);

    static DicomTag ParseTag(const char* name);

    static DicomTag ParseTag(const std::string& name);

    static bool HasTag(const DicomMap& fields,
                       const std::string& tagName);

    static const DicomValue& GetValue(const DicomMap& fields,
                                      const std::string& tagName);

    static void SetValue(DicomMap& target,
                         const std::string& tagName,
                         DicomValue* value);

    static void ToJson(Json::Value& result,
                       const DicomMap& values,
                       bool simplify);

    static std::string GenerateUniqueIdentifier(ResourceType level);

    static bool SaveToMemoryBuffer(std::string& buffer,
                                   DcmDataset& dataSet);

    static bool Transcode(DcmFileFormat& dicom,
                          DicomTransferSyntax syntax,
                          const DcmRepresentationParameter* representation);

    static ValueRepresentation Convert(DcmEVR vr);

    static ValueRepresentation LookupValueRepresentation(const DicomTag& tag);

    static DcmElement* CreateElementForTag(const DicomTag& tag,
                                           const std::string& privateCreator);
    
    static void FillElementWithString(DcmElement& element,
                                      const std::string& utf8alue,  // Encoded using UTF-8
                                      bool decodeDataUriScheme,
                                      Encoding dicomEncoding);

    static DcmElement* FromJson(const DicomTag& tag,
                                const Json::Value& element,  // Encoded using UTF-8
                                bool decodeDataUriScheme,
                                Encoding dicomEncoding,
                                const std::string& privateCreator);

    static DcmPixelSequence* GetPixelSequence(DcmDataset& dataset);

    static Encoding ExtractEncoding(const Json::Value& json,
                                    Encoding defaultEncoding);

    static DcmDataset* FromJson(const Json::Value& json,  // Encoded using UTF-8
                                bool generateIdentifiers,
                                bool decodeDataUriScheme,
                                Encoding defaultEncoding,
                                const std::string& privateCreator);

    static DcmFileFormat* LoadFromMemoryBuffer(const void* buffer,
                                               size_t size);

    static void FromJson(DicomMap& values,
                         const Json::Value& result);

    static void ExtractDicomSummary(DicomMap& target, 
                                    DcmItem& dataset,
                                    unsigned int maxStringLength,
                                    const std::set<DicomTag>& ignoreTagLength);

    static void ExtractDicomAsJson(Json::Value& target, 
                                   DcmDataset& dataset,
                                   DicomToJsonFormat format,
                                   DicomToJsonFlags flags,
                                   unsigned int maxStringLength,
                                   const std::set<DicomTag>& ignoreTagLength);

    static void InitializeCodecs();

    static void FinalizeCodecs();

    static void Apply(DcmItem& dataset,
                      ITagVisitor& visitor,
                      Encoding defaultEncoding);

    static bool LookupDcmtkTransferSyntax(E_TransferSyntax& target,
                                          DicomTransferSyntax source);

    static bool LookupOrthancTransferSyntax(DicomTransferSyntax& target,
                                            E_TransferSyntax source);

    static bool LookupOrthancTransferSyntax(DicomTransferSyntax& target,
                                            DcmFileFormat& dicom);

    static bool LookupOrthancTransferSyntax(DicomTransferSyntax& target,
                                            DcmDataset& dicom);

    static void LogMissingTagsForStore(DcmDataset& dicom);
  };
}
