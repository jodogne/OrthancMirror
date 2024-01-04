/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2024 Osimis S.A., Belgium
 * Copyright (C) 2021-2024 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


  static std::string GetCacheKeyTranscodedInstance(const std::string& uuid,
                                                   DicomTransferSyntax transferSyntax)
  {
    return uuid + ":ts:" + GetTransferSyntaxUid(transferSyntax);
  }


  void StorageCache::SetMaximumSize(size_t size)
  {
    cache_.SetMaximumSize(size);
  }
  

  void StorageCache::Invalidate(const std::string& uuid,
                                FileContentType contentType)
  {
    std::set<DicomTransferSyntax> transferSyntaxes;

    {
      boost::mutex::scoped_lock lock(subKeysMutex_);
      transferSyntaxes = subKeysTransferSyntax_;
    }

    // invalidate full file, start range file and possible transcoded instances
    const std::string keyFullFile = GetCacheKeyFullFile(uuid, contentType);
    cache_.Invalidate(keyFullFile);

    const std::string keyPartialFile = GetCacheKeyStartRange(uuid, contentType);
    cache_.Invalidate(keyPartialFile);
    
    for (std::set<DicomTransferSyntax>::const_iterator it = transferSyntaxes.begin(); it != transferSyntaxes.end(); ++it)
    {
      const std::string keyTransferSyntax = GetCacheKeyTranscodedInstance(uuid, *it);
      cache_.Invalidate(keyTransferSyntax);
    }
  }


  StorageCache::Accessor::Accessor(StorageCache& cache)
  : MemoryStringCache::Accessor(cache.cache_),
    storageCache_(cache)
  {
  }

  void StorageCache::Accessor::Add(const std::string& uuid, 
                                   FileContentType contentType,
                                   const std::string& value)
  {

    std::string key = GetCacheKeyFullFile(uuid, contentType);
    MemoryStringCache::Accessor::Add(key, value);
  }

  void StorageCache::Accessor::AddStartRange(const std::string& uuid, 
                                             FileContentType contentType,
                                             const std::string& value)
  {
    const std::string key = GetCacheKeyStartRange(uuid, contentType);
    MemoryStringCache::Accessor::Add(key, value);
  }

  void StorageCache::Accessor::Add(const std::string& uuid, 
                                   FileContentType contentType,
                                   const void* buffer,
                                   size_t size)
  {
    const std::string key = GetCacheKeyFullFile(uuid, contentType);
    MemoryStringCache::Accessor::Add(key, reinterpret_cast<const char*>(buffer), size);
  }                                   

  bool StorageCache::Accessor::Fetch(std::string& value, 
                                     const std::string& uuid,
                                     FileContentType contentType)
  {
    const std::string key = GetCacheKeyFullFile(uuid, contentType);
    if (MemoryStringCache::Accessor::Fetch(value, key))
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

  bool StorageCache::Accessor::FetchTranscodedInstance(std::string& value, 
                                                       const std::string& uuid,
                                                       DicomTransferSyntax targetSyntax)
  {
    const std::string key = GetCacheKeyTranscodedInstance(uuid, targetSyntax);
    if (MemoryStringCache::Accessor::Fetch(value, key))
    {
      LOG(INFO) << "Read instance \"" << uuid << "\" transcoded to "
                << GetTransferSyntaxUid(targetSyntax) << " from cache";
      return true;
    }
    else
    {
      return false;
    }
  }

  void StorageCache::Accessor::AddTranscodedInstance(const std::string& uuid,
                                                     DicomTransferSyntax targetSyntax,
                                                     const void* buffer,
                                                     size_t size)
  {
    {
      boost::mutex::scoped_lock lock(storageCache_.subKeysMutex_);
      storageCache_.subKeysTransferSyntax_.insert(targetSyntax);
    }

    const std::string key = GetCacheKeyTranscodedInstance(uuid, targetSyntax);
    MemoryStringCache::Accessor::Add(key, reinterpret_cast<const char*>(buffer), size);
  }

  bool StorageCache::Accessor::FetchStartRange(std::string& value, 
                                               const std::string& uuid,
                                               FileContentType contentType,
                                               uint64_t end /* exclusive */)
  {
    const std::string keyPartialFile = GetCacheKeyStartRange(uuid, contentType);
    if (MemoryStringCache::Accessor::Fetch(value, keyPartialFile) && value.size() >= end)
    {
      if (value.size() > end)  // the start range that has been cached is larger than the requested value
      {
        value.resize(end);
      }

      LOG(INFO) << "Read start of attachment \"" << uuid << "\" with content type "
                << boost::lexical_cast<std::string>(contentType) << " from cache";
      return true;
    }

    return false;
  }


  size_t StorageCache::GetCurrentSize() const
  {
    return cache_.GetCurrentSize();
  }
  
  size_t StorageCache::GetNumberOfItems() const
  {
    return cache_.GetNumberOfItems();
  }

}
