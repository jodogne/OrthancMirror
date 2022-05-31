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


#pragma once

#include "ITagVisitor.h"
#include "../DicomFormat/DicomElement.h"
#include "../DicomFormat/DicomMap.h"
#include "../DicomFormat/DicomPath.h"

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

  public:
    // New in Orthanc 1.9.4
    class ORTHANC_PUBLIC IDicomPathVisitor : public boost::noncopyable
    {
    private:
      static void ApplyInternal(FromDcmtkBridge::IDicomPathVisitor& visitor,
                                DcmItem& item,
                                const DicomPath& pattern,
                                const DicomPath& actualPath);
      
    public:
      virtual ~IDicomPathVisitor()
      {
      }

      virtual void Visit(DcmItem& item,
                         const DicomPath& path) = 0;

      static void Apply(IDicomPathVisitor& visitor,
                        DcmDataset& dataset,
                        const DicomPath& path);
    };
    

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
    /**
     * Initialize DCMTK to use the default DICOM dictionaries (either
     * embedded into the binaries for official releases, or using the
     * environment variable "DCM_DICT_ENVIRONMENT_VARIABLE", or using
     * the system-wide path to the DCMTK library for developers)
     **/
    static void InitializeDictionary(bool loadPrivateDictionary);

    /**
     * Replace the default DICOM dictionaries by the manually-provided
     * external dictionaries. This is needed to use DICONDE for
     * instance. Pay attention to the fact that the current dictionary
     * will be reinitialized (all its tags are cleared).
     **/
    static void LoadExternalDictionaries(const std::vector<std::string>& dictionaries);

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

    // parses a list like "0010,0010;PatientBirthDate;0020,0020"
    static void ParseListOfTags(std::set<DicomTag>& result, const std::string& source);

    static void ParseListOfTags(std::set<DicomTag>& result, const Json::Value& source);

    static void FormatListOfTags(std::string& output, const std::set<DicomTag>& tags);

    static void FormatListOfTags(Json::Value& output, const std::set<DicomTag>& tags);

    static bool HasTag(const DicomMap& fields,
                       const std::string& tagName);

    static const DicomValue& GetValue(const DicomMap& fields,
                                      const std::string& tagName);

    static void SetValue(DicomMap& target,
                         const std::string& tagName,
                         DicomValue* value);

    static void ToJson(Json::Value& result,
                       const DicomMap& values,
                       DicomToJsonFormat format);

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
                         const Json::Value& result,
                         const char* fieldName = NULL);

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

    static void RemovePath(DcmDataset& dataset,
                           const DicomPath& path);

    static void ClearPath(DcmDataset& dataset,
                          const DicomPath& path,
                          bool onlyIfExists);

    static void ReplacePath(DcmDataset& dataset,
                            const DicomPath& path,
                            const DcmElement& element,
                            DicomReplaceMode mode);

    static bool LookupSequenceItem(DicomMap& target,
                                   DcmDataset& dataset,
                                   const DicomPath& path,
                                   size_t sequenceIndex);

    static bool LookupStringValue(std::string& target,
                                  DcmDataset& dataset,
                                  const DicomTag& key);
  };
}
