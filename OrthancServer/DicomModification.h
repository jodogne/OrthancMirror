/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2014 Medical Physics Department, CHU of Liege,
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

#include "FromDcmtkBridge.h"

namespace Orthanc
{
  class DicomModification
  {
    /**
     * Process:
     * (1) Remove private tags
     * (2) Remove tags specified by the user
     * (3) Replace tags
     **/

  private:
    typedef std::set<DicomTag> Removals;
    typedef std::map<DicomTag, std::string> Replacements;
    typedef std::map< std::pair<DicomRootLevel, std::string>, std::string>  UidMap;

    Removals removals_;
    Replacements replacements_;
    bool removePrivateTags_;
    DicomRootLevel level_;
    UidMap uidMap_;

    void MapDicomIdentifier(ParsedDicomFile& dicom,
                            DicomRootLevel level);

  public:
    DicomModification();

    void Reset(const DicomTag& tag);

    void Remove(const DicomTag& tag);

    bool IsRemoved(const DicomTag& tag) const;

    void Replace(const DicomTag& tag,
                 const std::string& value);

    bool IsReplaced(const DicomTag& tag) const;

    const std::string& GetReplacement(const DicomTag& tag) const;

    void SetRemovePrivateTags(bool removed);

    bool ArePrivateTagsRemoved() const
    {
      return removePrivateTags_;
    }

    void SetLevel(DicomRootLevel level);

    DicomRootLevel GetLevel() const
    {
      return level_;
    }

    void SetupAnonymization();

    void Apply(ParsedDicomFile& toModify);
  };
}
