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

#include "../OrthancFramework.h"
#include "../DicomFormat/DicomTag.h"

#include <vector>

namespace Orthanc
{
  class ORTHANC_PUBLIC DicomPath
  {
  private:
    class PrefixItem
    {
    private:
      DicomTag  tag_;
      bool      isUniversal_;  // Matches any index
      size_t    index_;

      PrefixItem(DicomTag tag,
                 bool isUniversal,
                 size_t index);
      
    public:
      static PrefixItem CreateUniversal(const DicomTag& tag)
      {
        return PrefixItem(tag, true, 0 /* dummy value */);
      }

      static PrefixItem CreateIndexed(const DicomTag& tag,
                                      size_t index)
      {
        return PrefixItem(tag, false, index);
      }

      const DicomTag& GetTag() const
      {
        return tag_;
      }

      bool IsUniversal() const
      {
        return isUniversal_;
      }

      size_t GetIndex() const;

      void SetIndex(size_t index);
    };

    std::vector<PrefixItem>  prefix_;
    Orthanc::DicomTag        finalTag_;

    static DicomTag ParseTag(const std::string& token);
    
    const PrefixItem& GetLevel(size_t i) const;
    
  public:
    explicit DicomPath(const Orthanc::DicomTag& tag);

    DicomPath(const Orthanc::DicomTag& sequence,
              size_t index,
              const Orthanc::DicomTag& tag);

    DicomPath(const Orthanc::DicomTag& sequence1,
              size_t index1,
              const Orthanc::DicomTag& sequence2,
              size_t index2,
              const Orthanc::DicomTag& tag);

    DicomPath(const Orthanc::DicomTag& sequence1,
              size_t index1,
              const Orthanc::DicomTag& sequence2,
              size_t index2,
              const Orthanc::DicomTag& sequence3,
              size_t index3,
              const Orthanc::DicomTag& tag);

    DicomPath(const std::vector<Orthanc::DicomTag>& parentTags,
              const std::vector<size_t>& parentIndexes,
              const Orthanc::DicomTag& finalTag);

    void AddIndexedTagToPrefix(const Orthanc::DicomTag& tag,
                               size_t index);

    void AddUniversalTagToPrefix(const Orthanc::DicomTag& tag);

    size_t GetPrefixLength() const;

    const Orthanc::DicomTag& GetFinalTag() const;

    const Orthanc::DicomTag& GetPrefixTag(size_t level) const;

    bool IsPrefixUniversal(size_t level) const;

    size_t GetPrefixIndex(size_t level) const;

    bool HasUniversal() const;

    // This method is used for an optimization in Stone
    // (cf. "DicomStructureSet.cpp")
    void SetPrefixIndex(size_t level,
                        size_t index);

    std::string Format() const;

    static DicomPath Parse(const std::string& s);

    static bool IsMatch(const DicomPath& pattern,
                        const DicomPath& path);

    static bool IsMatch(const DicomPath& pattern,
                        const std::vector<Orthanc::DicomTag>& prefixTags,
                        const std::vector<size_t>& prefixIndexes,
                        const DicomTag& finalTag);
  };
}
