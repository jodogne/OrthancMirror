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


#include "../PrecompiledHeaders.h"
#include "StorageCache.h"

#include "../Compatibility.h"
#include "../Logging.h"
#include "../OrthancException.h"

#include <boost/lexical_cast.hpp>


namespace Orthanc
{
  static std::string GetCacheKeyFullFile(const std::string& uuid,
                                         FileContentType contentType)
  {
    return uuid + ":" + boost::lexical_cast<std::string>(contentType) + ":1";
  }
  
  static std::string GetCacheKeyStartRange(const std::string& uuid,
                                           FileContentType contentType)
  {
    return uuid + ":" + boost::lexical_cast<std::string>(contentType) + ":0";
  }
  
  void StorageCache::SetMaximumSize(size_t size)
  {
    cache_.SetMaximumSize(size);
  }
  

  void StorageCache::Add(const std::string& uuid, 
                         FileContentType contentType,
                         const std::string& value)
  {
    const std::string key = GetCacheKeyFullFile(uuid, contentType);
    cache_.Add(key, value);
  }
  

  void StorageCache::Add(const std::string& uuid, 
                         FileContentType contentType,
                         const void* buffer,
                         size_t size)
  {
    const std::string key = GetCacheKeyFullFile(uuid, contentType);
    cache_.Add(key, buffer, size);
  }


  void StorageCache::AddStartRange(const std::string& uuid, 
                                   FileContentType contentType,
                                   const std::string& value)
  {
    const std::string key = GetCacheKeyStartRange(uuid, contentType);
    cache_.Add(key, value);
  }


  void StorageCache::Invalidate(const std::string& uuid,
                                FileContentType contentType)
  {
    // invalidate both full file + start range file
    const std::string keyFullFile = GetCacheKeyFullFile(uuid, contentType);
    cache_.Invalidate(keyFullFile);

    const std::string keyPartialFile = GetCacheKeyStartRange(uuid, contentType);
    cache_.Invalidate(keyPartialFile);
  }
  

  bool StorageCache::Fetch(std::string& value, 
                           const std::string& uuid,
                           FileContentType contentType)
  {
    const std::string key = GetCacheKeyFullFile(uuid, contentType);
    if (cache_.Fetch(value, key))
    {
      LOG(INFO) << "Read attachment \"" << uuid << "\" with content type "
                << boost::lexical_cast<std::string>(contentType) << " from cache";
      return true;
    }
    else
    {
      return false;
    }
  }

  bool StorageCache::FetchStartRange(std::string& value, 
                                     const std::string& uuid,
                                     FileContentType contentType,
                                     uint64_t end)
  {
    // first try to get the start of file only from cache
    const std::string keyPartialFile = GetCacheKeyStartRange(uuid, contentType);
    if (cache_.Fetch(value, keyPartialFile) && value.size() >= end)
    {
      if (value.size() > end)  // the start range that has been cached is larger than the requested value
      {
        value.resize(end);
      }

      LOG(INFO) << "Read start of attachment \"" << uuid << "\" with content type "
                << boost::lexical_cast<std::string>(contentType) << " from cache";
      return true;
    }
    else
    {
      // try to get the full file from cache
      if (Fetch(value, uuid, contentType))
      {
        if (value.size() < end)
        {
          throw OrthancException(ErrorCode_CorruptedFile);
        }

        value.resize(end);
        return true;
      }
      else
      {
        return false;
      }
    }
  }
}
