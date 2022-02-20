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

#include <boost/noncopyable.hpp>

namespace Orthanc
{
  class DicomDirWriter : public boost::noncopyable
  {
  private:
    class PImpl;
    boost::shared_ptr<PImpl>  pimpl_;

  public:
    DicomDirWriter();

    void SetUtcUsed(bool utc);

    bool IsUtcUsed() const;

    void SetFileSetId(const std::string& id);

    void Add(const std::string& directory,
             const std::string& filename,
             ParsedDicomFile& dicom);

    void Encode(std::string& target);

    void EnableExtendedSopClass(bool enable);

    bool IsExtendedSopClass() const;
  };

}
