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

#include "../DicomFormat/DicomTag.h"

#include <vector>
#include <boost/noncopyable.hpp>

namespace Orthanc
{
  class ITagVisitor : public boost::noncopyable
  {
  public:
    enum Action
    {
      Action_Replace,
      Action_Remove,  // New in Orthanc 1.9.5
      Action_None
    };

    virtual ~ITagVisitor()
    {
    }

    // Visiting a DICOM element that is internal to DCMTK. Can return
    // "Remove" or "None".
    virtual Action VisitNotSupported(const std::vector<DicomTag>& parentTags,
                                     const std::vector<size_t>& parentIndexes,
                                     const DicomTag& tag,
                                     ValueRepresentation vr) = 0;

    // SQ - can return "Remove" or "None"
    virtual Action VisitSequence(const std::vector<DicomTag>& parentTags,
                                 const std::vector<size_t>& parentIndexes,
                                 const DicomTag& tag,
                                 size_t countItems) = 0;

    // SL, SS, UL, US - can return "Remove" or "None"
    virtual Action VisitIntegers(const std::vector<DicomTag>& parentTags,
                                 const std::vector<size_t>& parentIndexes,
                                 const DicomTag& tag,
                                 ValueRepresentation vr,
                                 const std::vector<int64_t>& values) = 0;

    // FL, FD, OD, OF - can return "Remove" or "None"
    virtual Action VisitDoubles(const std::vector<DicomTag>& parentTags,
                                const std::vector<size_t>& parentIndexes,
                                const DicomTag& tag,
                                ValueRepresentation vr,
                                const std::vector<double>& values) = 0;

    // AT - can return "Remove" or "None"
    virtual Action VisitAttributes(const std::vector<DicomTag>& parentTags,
                                   const std::vector<size_t>& parentIndexes,
                                   const DicomTag& tag,
                                   const std::vector<DicomTag>& values) = 0;

    // OB, OL, OW, UN - can return "Remove" or "None"
    virtual Action VisitBinary(const std::vector<DicomTag>& parentTags,
                               const std::vector<size_t>& parentIndexes,
                               const DicomTag& tag,
                               ValueRepresentation vr,
                               const void* data,
                               size_t size) = 0;

    // Visiting an UTF-8 string - can return "Replace", "Remove" or "None"
    virtual Action VisitString(std::string& newValue,
                               const std::vector<DicomTag>& parentTags,
                               const std::vector<size_t>& parentIndexes,
                               const DicomTag& tag,
                               ValueRepresentation vr,
                               const std::string& value) = 0;
  };
}
