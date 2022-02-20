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

#include "ParsedDicomFile.h"

#include <list>


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

    class DicomTagRange
    {
    private:
      uint16_t   groupFrom_;
      uint16_t   groupTo_;
      uint16_t   elementFrom_;
      uint16_t   elementTo_;

    public:
      DicomTagRange(uint16_t groupFrom,
                    uint16_t groupTo,
                    uint16_t elementFrom,
                    uint16_t elementTo);

      uint16_t GetGroupFrom() const
      {
        return groupFrom_;
      }

      uint16_t GetGroupTo() const
      {
        return groupTo_;
      }

      uint16_t GetElementFrom() const
      {
        return elementFrom_;
      }

      uint16_t GetElementTo() const
      {
        return elementTo_;
      }

      bool Contains(const DicomTag& tag) const;
    };

    class SequenceReplacement : public boost::noncopyable
    {
    private:
      DicomPath    path_;
      Json::Value  value_;

    public:
      SequenceReplacement(const DicomPath& path,
                          const Json::Value& value) :
        path_(path),
        value_(value)
      {
      }

      const DicomPath& GetPath() const
      {
        return path_;
      }

      const Json::Value& GetValue() const
      {
        return value_;
      }
    };
    
    typedef std::set<DicomTag>                SetOfTags;
    typedef std::map<DicomTag, Json::Value*>  Replacements;
    typedef std::list<DicomTagRange>          RemovedRanges;
    typedef std::list<DicomPath>              ListOfPaths;
    typedef std::list<SequenceReplacement*>   SequenceReplacements;

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

    // New in Orthanc 1.9.4
    SetOfTags            uids_;
    RemovedRanges        removedRanges_;
    ListOfPaths          keepSequences_;         // Can *possibly* be a path whose prefix is empty
    ListOfPaths          removeSequences_;       // Must *never* be a path whose prefix is empty
    SequenceReplacements sequenceReplacements_;  // Must *never* be a path whose prefix is empty

    std::string MapDicomIdentifier(const std::string& original,
                                   ResourceType level);

    void RegisterMappedDicomIdentifier(const std::string& original,
                                       const std::string& mapped,
                                       ResourceType level);

    void MapDicomTags(ParsedDicomFile& dicom,
                      ResourceType level);

    void MarkNotOrthancAnonymization();

    void ClearReplacements();

    void CancelReplacement(const DicomTag& tag);

    void ReplaceInternal(const DicomTag& tag,
                         const Json::Value& value);

    void SetupUidsFromOrthanc_1_9_3();

    void SetupAnonymization2008();

    void SetupAnonymization2017c();

    void SetupAnonymization2021b();

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

    // "patientNameOverridden" is set to "true" iff. the PatientName
    // (0010,0010) tag is manually replaced, removed, cleared or kept
    void ParseAnonymizationRequest(bool& patientNameOverridden /* out */,
                                   const Json::Value& request);

    void SetDicomIdentifierGenerator(IDicomIdentifierGenerator& generator);

    void Serialize(Json::Value& value) const;

    void SetPrivateCreator(const std::string& privateCreator);

    const std::string& GetPrivateCreator() const;

    // New in Orthanc 1.9.4
    void Keep(const DicomPath& path);

    // New in Orthanc 1.9.4
    void Remove(const DicomPath& path);

    // New in Orthanc 1.9.4
    void Replace(const DicomPath& path,
                 const Json::Value& value,   // Encoded using UTF-8
                 bool safeForAnonymization);

    bool IsAlteredTag(const DicomTag& tag) const;
  };
}
