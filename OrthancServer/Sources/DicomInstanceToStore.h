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

#include "../../OrthancFramework/Sources/DicomFormat/DicomMap.h"
#include "DicomInstanceOrigin.h"
#include "ServerEnumerations.h"

#include <boost/shared_ptr.hpp>

class DcmDataset;

namespace Orthanc
{
  class ImageAccessor;
  class ParsedDicomFile;

  class DicomInstanceToStore : public boost::noncopyable
  {
  public:
    typedef std::map<std::pair<ResourceType, MetadataType>, std::string>  MetadataMap;

  private:
    class FromBuffer;
    class FromParsedDicomFile;
    class FromDcmDataset;

    MetadataMap          metadata_;
    DicomInstanceOrigin  origin_;

  public:
    virtual ~DicomInstanceToStore()
    {
    }

    // WARNING: The source in the factory methods is *not* copied and
    // must *not* be deallocated as long as this wrapper object is alive
    static DicomInstanceToStore* CreateFromBuffer(const void* buffer,
                                                  size_t size);

    static DicomInstanceToStore* CreateFromBuffer(const std::string& buffer);

    static DicomInstanceToStore* CreateFromParsedDicomFile(ParsedDicomFile& dicom);

    static DicomInstanceToStore* CreateFromDcmDataset(DcmDataset& dataset);


 
    void SetOrigin(const DicomInstanceOrigin& origin)
    {
      origin_ = origin;
    }
    
    const DicomInstanceOrigin& GetOrigin() const
    {
      return origin_;
    } 
    
    const MetadataMap& GetMetadata() const
    {
      return metadata_;
    }

    void ClearMetadata()
    {
      metadata_.clear();
    }

    // This function is notably used by modify/anonymize operations
    void AddMetadata(ResourceType level,
                     MetadataType metadata,
                     const std::string& value)
    {
      metadata_[std::make_pair(level, metadata)] = value;
    }

    void CopyMetadata(const MetadataMap& metadata);

    bool LookupTransferSyntax(DicomTransferSyntax& result) const;

    virtual ParsedDicomFile& GetParsedDicomFile() const = 0;

    virtual const void* GetBufferData() const = 0;

    virtual size_t GetBufferSize() const = 0;

    virtual bool HasPixelData() const;

    virtual void GetSummary(DicomMap& summary) const;

    virtual void GetDicomAsJson(Json::Value& dicomAsJson,
                                const std::set<DicomTag>& ignoreTagLength) const;

    virtual void DatasetToJson(Json::Value& target, 
                               DicomToJsonFormat format,
                               DicomToJsonFlags flags,
                               unsigned int maxStringLength) const;

    virtual unsigned int GetFramesCount() const;
    
    virtual ImageAccessor* DecodeFrame(unsigned int frame) const;
  };
}
