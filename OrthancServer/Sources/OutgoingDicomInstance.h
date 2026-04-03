/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include "../../OrthancFramework/Sources/DicomFormat/DicomMap.h"
#include "DicomInstanceDestination.h"
#include "ServerEnumerations.h"

#include <boost/shared_ptr.hpp>

class DcmDataset;

namespace Orthanc
{
  class ParsedDicomFile;

  class OutgoingDicomInstance : public boost::noncopyable
  {
    DicomInstanceDestination            destination_;
    std::unique_ptr<ParsedDicomFile>    dicom_;

    explicit OutgoingDicomInstance(ParsedDicomFile* dicom) :
      dicom_(dicom)
    {}

  public:
    // WARNING: The source in the factory methods is *not* copied and
    // must *not* be deallocated as long as this wrapper object is alive
    static OutgoingDicomInstance* CreateFromBuffer(const void* buffer,
                                                   size_t size);

    static OutgoingDicomInstance* CreateFromBuffer(const std::string& buffer);

    void SetDestination(const DicomInstanceDestination& destination)
    {
      destination_ = destination;
    }

    const DicomInstanceDestination& GetDestination() const
    {
      return destination_;
    } 
    
    void GetSimplifiedJson(Json::Value& dicomAsJson) const;
  };
}
