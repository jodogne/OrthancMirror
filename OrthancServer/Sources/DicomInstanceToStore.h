/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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

    bool LookupTransferSyntax(DicomTransferSyntax& result) const;

    virtual ParsedDicomFile& GetParsedDicomFile() const = 0;

    virtual const void* GetBufferData() const = 0;

    virtual size_t GetBufferSize() const = 0;

    virtual bool HasPixelData() const;

    virtual void GetSummary(DicomMap& summary) const;

    virtual void GetDicomAsJson(Json::Value& dicomAsJson) const;

    virtual void DatasetToJson(Json::Value& target, 
                               DicomToJsonFormat format,
                               DicomToJsonFlags flags,
                               unsigned int maxStringLength) const;

    virtual unsigned int GetFramesCount() const;
    
    virtual ImageAccessor* DecodeFrame(unsigned int frame) const;
  };
}
