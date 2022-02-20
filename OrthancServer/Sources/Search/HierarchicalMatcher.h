/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include "DatabaseLookup.h"
#include "../../../OrthancFramework/Sources/DicomParsing/ParsedDicomFile.h"

class DcmItem;

namespace Orthanc
{
  class HierarchicalMatcher : public boost::noncopyable
  {
  private:
    typedef std::map<DicomTag, HierarchicalMatcher*>  Sequences;

    std::set<DicomTag>  flatTags_;
    DatabaseLookup      flatConstraints_;
    Sequences           sequences_;

    void Setup(DcmItem& query,
               bool caseSensitivePN,
               Encoding encoding,
               bool hasCodeExtensions);

    HierarchicalMatcher(DcmItem& query,
                        bool caseSensitivePN,
                        Encoding encoding,
                        bool hasCodeExtensions)
    {
      Setup(query, caseSensitivePN, encoding, hasCodeExtensions);
    }

    bool MatchInternal(DcmItem& dicom,
                       Encoding encoding,
                       bool hasCodeExtensions) const;

    DcmDataset* ExtractInternal(DcmItem& dicom,
                                Encoding encoding,
                                bool hasCodeExtensions) const;

  public:
    explicit HierarchicalMatcher(ParsedDicomFile& query);

    ~HierarchicalMatcher();

    std::string Format(const std::string& prefix = "") const;

    bool Match(ParsedDicomFile& dicom) const;

    ParsedDicomFile* Extract(ParsedDicomFile& dicom) const;
  };
}
