/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
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

#include "ParsedDicomFile.h"

namespace Orthanc
{
  class DicomModification : public boost::noncopyable
  {
    /**
     * Process:
     * (1) Remove private tags
     * (2) Remove tags specified by the user
     * (3) Replace tags
     **/

  private:
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

    void MapDicomIdentifier(ParsedDicomFile& dicom,
                            ResourceType level);

    void MarkNotOrthancAnonymization();

    void ClearReplacements();

    bool CancelReplacement(const DicomTag& tag);

    void ReplaceInternal(const DicomTag& tag,
                         const Json::Value& value);

    void SetupAnonymization2008();

    void SetupAnonymization2017c();

  public:
    DicomModification();

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

    bool ArePrivateTagsRemoved() const
    {
      return removePrivateTags_;
    }

    void SetLevel(ResourceType level);

    ResourceType GetLevel() const
    {
      return level_;
    }

    void SetupAnonymization(DicomVersion version);

    void Apply(ParsedDicomFile& toModify);

    void SetAllowManualIdentifiers(bool check)
    {
      allowManualIdentifiers_ = check;
    }

    bool AreAllowManualIdentifiers() const
    {
      return allowManualIdentifiers_;
    }
  };
}
