/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
 * Copyright (C) 2021-2021 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "../PrecompiledHeaders.h"
#include "StorageCache.h"

#include "../Compatibility.h"
#include "../OrthancException.h"



namespace Orthanc
{
  bool IsAcceptedContentType(FileContentType contentType)
  {
    return contentType == FileContentType_Dicom ||
      contentType == FileContentType_DicomUntilPixelData ||
      contentType == FileContentType_DicomAsJson;
  }

  const char* ToString(FileContentType contentType)
  {
    switch (contentType)
    {
      case FileContentType_Dicom:
        return "dicom";
      case FileContentType_DicomUntilPixelData:
        return "dicom-header";
      case FileContentType_DicomAsJson:
        return "dicom-json";
      default:
        throw OrthancException(ErrorCode_InternalError,
                               "ContentType not supported in StorageCache");
    }
  }

  bool GetCacheKey(std::string& key, const std::string& uuid, FileContentType contentType)
  {
    if (contentType == FileContentType_Unknown || contentType >= FileContentType_StartUser)
    {
      return false;
    }

    key = uuid + ":" + std::string(ToString(contentType));

    return true;
  }
  
  void StorageCache::SetMaximumSize(size_t size)
  {
    cache_.SetMaximumSize(size);
  }

  void StorageCache::Add(const std::string& uuid, 
                         FileContentType contentType,
                         const std::string& value)
  {
    if (!IsAcceptedContentType(contentType))
    {
      return;
    }

    std::string key;

    if (GetCacheKey(key, uuid, contentType))
    {
      cache_.Add(key, value);
    }
  }

  void StorageCache::Add(const std::string& uuid, 
                         FileContentType contentType,
                         const void* buffer,
                         size_t size)
  {
    if (!IsAcceptedContentType(contentType))
    {
      return;
    }
    
    std::string key;

    if (GetCacheKey(key, uuid, contentType))
    {
      cache_.Add(key, buffer, size);
    }
  }

  void StorageCache::Invalidate(const std::string& uuid, FileContentType contentType)
  {
    std::string key;
    
    if (GetCacheKey(key, uuid, contentType))
    {
      cache_.Invalidate(key);
    }
  }

  bool StorageCache::Fetch(std::string& value, 
                           const std::string& uuid,
                           FileContentType contentType)
  {
    if (!IsAcceptedContentType(contentType))
    {
      return false;
    }

    std::string key;
    if (GetCacheKey(key, uuid, contentType))
    {
      return cache_.Fetch(value, key);
    }

    return false;
  }


}