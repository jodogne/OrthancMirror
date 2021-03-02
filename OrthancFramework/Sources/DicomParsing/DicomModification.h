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

#include "ParsedDicomFile.h"

namespace Orthanc
{
  class ORTHANC_PUBLIC DicomModification : public boost::noncopyable
  {
    /**
     * Process:
     * (1) Remove private tags
     * (2) Remove tags specified by the user
     * (3) Replace tags
     **/

  public:
    enum TagOperation
    {
      TagOperation_Keep,
      TagOperation_Remove
    };

    class IDicomIdentifierGenerator : public boost::noncopyable
    {
    public:
      virtual ~IDicomIdentifierGenerator()
      {
      }

      virtual bool Apply(std::string& target,
                         const std::string& sourceIdentifier,
                         ResourceType level,
                         const DicomMap& sourceDicom) = 0;                       
    };

  private:
    class RelationshipsVisitor;

    typedef std::set<DicomTag> SetOfTags;
    typedef std::map<DicomTag, Json::Value*> Replacements;
    typedef std::map< std::pair<ResourceType, std::string>, std::string>  UidMap;

    SetOfTags removals_;
    SetOfTags clearings_;
    Replacements replacements_;
    bool removePrivateTags_;
    ResourceType level_;
    UidMap uidMap_;
    SetOfTags privateTagsToKeep_;
    bool allowManualIdentifiers_;
    bool keepStudyInstanceUid_;
    bool keepSeriesInstanceUid_;
    bool keepSopInstanceUid_;
    bool updateReferencedRelationships_;
    bool isAnonymization_;
    DicomMap currentSource_;
    std::string privateCreator_;

    IDicomIdentifierGenerator* identifierGenerator_;

    std::string MapDicomIdentifier(const std::string& original,
                                   ResourceType level);

    void RegisterMappedDicomIdentifier(const std::string& original,
                                       const std::string& mapped,
                                       ResourceType level);

    void MapDicomTags(ParsedDicomFile& dicom,
                      ResourceType level);

    void MarkNotOrthancAnonymization();

    void ClearReplacements();

    bool CancelReplacement(const DicomTag& tag);

    void ReplaceInternal(const DicomTag& tag,
                         const Json::Value& value);

    void SetupAnonymization2008();

    void SetupAnonymization2017c();

    void UnserializeUidMap(ResourceType level,
                           const Json::Value& serialized,
                           const char* field);

  public:
    DicomModification();

    explicit DicomModification(const Json::Value& serialized);

    ~DicomModification();

    void Keep(const DicomTag& tag);

    void Remove(const DicomTag& tag);

    // Replace the DICOM tag as a NULL/empty value (e.g. for anonymization)
    void Clear(const DicomTag& tag);

    bool IsRemoved(const DicomTag& tag) const;

    bool IsCleared(const DicomTag& tag) const;

    // "safeForAnonymization" tells Orthanc that this replacement does
    // not break the anonymization process it implements (for internal use only)
    void Replace(const DicomTag& tag,
                 const Json::Value& value,   // Encoded using UTF-8
                 bool safeForAnonymization);

    bool IsReplaced(const DicomTag& tag) const;

    const Json::Value& GetReplacement(const DicomTag& tag) const;

    std::string GetReplacementAsString(const DicomTag& tag) const;

    void SetRemovePrivateTags(bool removed);

    bool ArePrivateTagsRemoved() const;

    void SetLevel(ResourceType level);

    ResourceType GetLevel() const;

    void SetupAnonymization(DicomVersion version);

    void Apply(ParsedDicomFile& toModify);

    void SetAllowManualIdentifiers(bool check);

    bool AreAllowManualIdentifiers() const;

    void ParseModifyRequest(const Json::Value& request);

    void ParseAnonymizationRequest(bool& patientNameReplaced,
                                   const Json::Value& request);

    void SetDicomIdentifierGenerator(IDicomIdentifierGenerator& generator);

    void Serialize(Json::Value& value) const;

    void SetPrivateCreator(const std::string& privateCreator);

    const std::string& GetPrivateCreator() const;
  };
}
