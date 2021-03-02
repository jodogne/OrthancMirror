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

#if ORTHANC_ENABLE_DCMTK != 1
#  error The macro ORTHANC_ENABLE_DCMTK must be set to 1 to use this file
#endif

#include "../DicomFormat/DicomMap.h"

namespace Orthanc
{
  class ORTHANC_PUBLIC ParsedDicomDir : public boost::noncopyable
  {
  private:
    typedef std::map<uint32_t, size_t>  OffsetToIndex;

    std::vector<DicomMap*>  content_;
    std::vector<size_t>     nextOffsets_;
    std::vector<size_t>     lowerOffsets_;
    OffsetToIndex           offsetToIndex_;

    void Clear();

    bool LookupIndexOfOffset(size_t& target,
                             unsigned int offset) const;

  public:
    explicit ParsedDicomDir(const std::string& content);

    ~ParsedDicomDir();

    size_t GetSize() const;

    const DicomMap& GetItem(size_t i) const;

    bool LookupNext(size_t& target,
                    size_t index) const;

    bool LookupLower(size_t& target,
                     size_t index) const;
  };
}
