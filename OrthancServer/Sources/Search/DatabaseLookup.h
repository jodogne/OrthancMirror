/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2024 Osimis S.A., Belgium
 * Copyright (C) 2021-2024 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include "../Search/ISqlLookupFormatter.h"
#include "DicomTagConstraint.h"

class DcmItem;

namespace Orthanc
{
  class DatabaseLookup : public boost::noncopyable
  {
  private:
    std::vector<DicomTagConstraint*>  constraints_;
    std::set<std::string>             labels_;
    LabelsConstraint                  labelsConstraint_;

    void AddDicomConstraintInternal(const DicomTag& tag,
                                    ValueRepresentation vr,
                                    const std::string& dicomQuery,
                                    bool caseSensitive,
                                    bool mandatoryTag);

    void AddConstraintInternal(DicomTagConstraint* constraint);  // Takes ownership

  public:
    DatabaseLookup() :
      labelsConstraint_(LabelsConstraint_All)
    {
    }

    ~DatabaseLookup();

    DatabaseLookup* Clone() const;

    void Reserve(size_t n)
    {
      constraints_.reserve(n);
    }

    size_t GetConstraintsCount() const
    {
      return constraints_.size();
    }

    const DicomTagConstraint& GetConstraint(size_t index) const;

    bool GetConstraint(const DicomTagConstraint*& constraint, const DicomTag& tag) const;

    bool IsMatch(const DicomMap& value) const;

    bool IsMatch(DcmItem& item,
                 Encoding encoding,
                 bool hasCodeExtensions) const;

    void AddDicomConstraint(const DicomTag& tag,
                            const std::string& dicomQuery,
                            bool caseSensitivePN,
                            bool mandatoryTag);

    void AddRestConstraint(const DicomTag& tag,
                           const std::string& dicomQuery,
                           bool caseSensitive,
                           bool mandatoryTag);

    void AddConstraint(const DicomTagConstraint& constraint)
    {
      AddConstraintInternal(new DicomTagConstraint(constraint));
    }

    bool HasOnlyMainDicomTags() const;

    std::string Format() const;

    bool HasTag(const DicomTag& tag) const;

    void RemoveConstraint(const DicomTag& tag);

    void AddLabel(const std::string& label);

    void SetLabelsConstraint(LabelsConstraint constraint)
    {
      labelsConstraint_ = constraint;
    }

    const std::set<std::string>& GetLabels() const
    {
      return labels_;
    }

    LabelsConstraint GetLabelsConstraint() const
    {
      return labelsConstraint_;
    }
  };
}
