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

#include "DicomTag.h"
#include "DicomValue.h"
#include "../Enumerations.h"

#include <set>
#include <map>
#include <json/value.h>

#if ORTHANC_BUILD_UNIT_TESTS == 1
#  include <gtest/gtest_prod.h>
#endif

namespace Orthanc
{
  class ORTHANC_PUBLIC DicomMap : public boost::noncopyable
  {
  public:
    typedef std::map<DicomTag, DicomValue*>  Content;

  private:
    class MainDicomTagsConfiguration;
    friend class DicomArray;
    friend class FromDcmtkBridge;
    friend class ParsedDicomFile;

#if ORTHANC_BUILD_UNIT_TESTS == 1
    friend class DicomMapMainTagsTests;
#endif

    Content content_;

    // Warning: This takes the ownership of "value"
    void SetValueInternal(uint16_t group, 
                          uint16_t element, 
                          DicomValue* value);

    // used for unit tests only
    static void ResetDefaultMainDicomTags();

  public:
    ~DicomMap();

    size_t GetSize() const;
    
    DicomMap* Clone() const;

    void Assign(const DicomMap& other);

    void Clear();

    void SetNullValue(uint16_t group,
                      uint16_t element);
    
    void SetNullValue(const DicomTag& tag);
    
    void SetValue(uint16_t group,
                  uint16_t element,
                  const DicomValue& value);

    void SetValue(const DicomTag& tag,
                  const DicomValue& value);

    void SetValue(const DicomTag& tag,
                  const std::string& str,
                  bool isBinary);

    void SetValue(uint16_t group,
                  uint16_t element,
                  const std::string& str,
                  bool isBinary);

    bool HasTag(uint16_t group, uint16_t element) const;

    bool HasTag(const DicomTag& tag) const;

    const DicomValue& GetValue(uint16_t group, uint16_t element) const;

    const DicomValue& GetValue(const DicomTag& tag) const;

    // DO NOT delete the returned value!
    const DicomValue* TestAndGetValue(uint16_t group, uint16_t element) const;

    // DO NOT delete the returned value!
    const DicomValue* TestAndGetValue(const DicomTag& tag) const;

    void Remove(const DicomTag& tag);

    void RemoveTags(const std::set<DicomTag>& tags);

    void ExtractPatientInformation(DicomMap& result) const;

    void ExtractStudyInformation(DicomMap& result) const;

    void ExtractSeriesInformation(DicomMap& result) const;

    void ExtractInstanceInformation(DicomMap& result) const;

    void ExtractResourceInformation(DicomMap& result, ResourceType level) const;

    void ExtractTags(DicomMap& result, const std::set<DicomTag>& tags) const;

    static void SetupFindPatientTemplate(DicomMap& result);

    static void SetupFindStudyTemplate(DicomMap& result);

    static void SetupFindSeriesTemplate(DicomMap& result);

    static void SetupFindInstanceTemplate(DicomMap& result);

    void CopyTagIfExists(const DicomMap& source,
                         const DicomTag& tag);

    static bool IsMainDicomTag(const DicomTag& tag, ResourceType level);

    static bool IsMainDicomTag(const DicomTag& tag);

    static bool IsComputedTag(const DicomTag& tag, ResourceType level);

    static bool IsComputedTag(const DicomTag& tag);

    static bool HasOnlyComputedTags(const std::set<DicomTag>& tags);

    static bool HasComputedTags(const std::set<DicomTag>& tags, ResourceType level);

    static bool HasComputedTags(const std::set<DicomTag>& tags);

#if ORTHANC_ENABLE_DCMTK == 1
    static void ExtractSequences(std::set<DicomTag>& sequences, const std::set<DicomTag>& tags);
#endif

    static const std::set<DicomTag>& GetMainDicomTags(ResourceType level);

    // returns a string uniquely identifying the list of main dicom tags for a level
    static const std::string& GetMainDicomTagsSignature(ResourceType level);

    static const std::string& GetDefaultMainDicomTagsSignature(ResourceType level);

    static const std::set<DicomTag>& GetAllMainDicomTags();

    // adds a main dicom tag to the definition of main dicom tags for each level.
    // this should be done once at startup before you use MainDicomTags methods
    static void AddMainDicomTag(const DicomTag& tag, ResourceType level);

    void GetTags(std::set<DicomTag>& tags) const;

    static bool IsDicomFile(const void* dicom,
                            size_t size);
    
    static bool ParseDicomMetaInformation(DicomMap& result,
                                          const void* dicom,
                                          size_t size);

    void LogMissingTagsForStore() const;

    static void LogMissingTagsForStore(const std::string& patientId,
                                       const std::string& studyInstanceUid,
                                       const std::string& seriesInstanceUid,
                                       const std::string& sopInstanceUid);

    bool LookupStringValue(std::string& result,
                           const DicomTag& tag,
                           bool allowBinary) const;
    
    bool ParseInteger32(int32_t& result,
                        const DicomTag& tag) const;

    bool ParseInteger64(int64_t& result,
                        const DicomTag& tag) const;                                

    bool ParseUnsignedInteger32(uint32_t& result,
                                const DicomTag& tag) const;

    bool ParseUnsignedInteger64(uint64_t& result,
                                const DicomTag& tag) const;

    bool ParseFloat(float& result,
                    const DicomTag& tag) const;

    bool ParseFirstFloat(float& result,
                         const DicomTag& tag) const;

    bool ParseDouble(double& result,
                     const DicomTag& tag) const;

    void FromDicomAsJson(const Json::Value& dicomAsJson, 
                         bool append = false);

    void Merge(const DicomMap& other);

    void MergeMainDicomTags(const DicomMap& other,
                            ResourceType level);

    void ExtractMainDicomTags(const DicomMap& other);

    bool HasOnlyMainDicomTags() const;
    
    void Serialize(Json::Value& target) const;

    void Unserialize(const Json::Value& source);

    void FromDicomWeb(const Json::Value& source);

    std::string GetStringValue(const DicomTag& tag,
                               const std::string& defaultValue,
                               bool allowBinary) const;

    void RemoveBinaryTags();

    void DumpMainDicomTags(Json::Value& target,
                           ResourceType level) const;

    void Print(FILE* fp) const;  // For debugging only
  };
}
