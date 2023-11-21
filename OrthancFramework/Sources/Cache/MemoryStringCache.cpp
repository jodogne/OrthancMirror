/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
#include "MemoryStringCache.h"

namespace Orthanc
{
  class MemoryStringCache::StringValue : public ICacheable
  {
  private:
    std::string  content_;

  public:
    explicit StringValue(const std::string& content) :
      content_(content)
    {
    }

    explicit StringValue(const char* buffer, size_t size) :
      content_(buffer, size)
    {
    }

    const std::string& GetContent() const
    {
      return content_;
    }

    virtual size_t GetMemoryUsage() const
    {
      return content_.size();
    }      
  };


  MemoryStringCache::Accessor::Accessor(MemoryStringCache& cache)
  : cache_(cache),
    shouldAdd_(false)
  {
  }


  MemoryStringCache::Accessor::~Accessor()
  {
    // if this accessor was the one in charge of loading and adding the data into the cache
    // and it failed to add, remove the key from the list to make sure others accessor
    // stop waiting for it.
    if (shouldAdd_)
    {
      cache_.RemoveFromItemsBeingLoaded(keyToAdd_);
    }
  }


  bool MemoryStringCache::Accessor::Fetch(std::string& value, const std::string& key)
  {
    // if multiple accessors are fetching at the same time:
    // the first one will return false and will be in charge of adding to the cache.
    // others will wait.
    // if the first one fails to add, or, if the content was too large to fit in the cache,
    // the next one will be in charge of adding ...
    if (!cache_.Fetch(value, key))
    {
      shouldAdd_ = true;
      keyToAdd_ = key;
      return false;
    }

    shouldAdd_ = false;
    keyToAdd_.clear();

    return true;
  }


  void MemoryStringCache::Accessor::Add(const std::string& key, const std::string& value)
  {
    cache_.Add(key, value);
    shouldAdd_ = false;
  }


  void MemoryStringCache::Accessor::Add(const std::string& key, const char* buffer, size_t size)
  {
    cache_.Add(key, buffer, size);
    shouldAdd_ = false;
  }


  MemoryStringCache::MemoryStringCache() :
    currentSize_(0),
    maxSize_(100 * 1024 * 1024)  // 100 MB
  {
  }


  MemoryStringCache::~MemoryStringCache()
  {
    Recycle(0);
    assert(content_.IsEmpty());
  }


  size_t MemoryStringCache::GetMaximumSize()
  {
    return maxSize_;
  }


  void MemoryStringCache::SetMaximumSize(size_t size)
  {
    if (size == 0)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
      
    // // Make sure no accessor is currently open (as its data may be
    // // removed if recycling is needed)
    // WriterLock contentLock(contentMutex_);

    // Lock the global structure of the cache
    boost::mutex::scoped_lock cacheLock(cacheMutex_);

    Recycle(size);
    maxSize_ = size;
  }


  void MemoryStringCache::Add(const std::string& key,
                               const std::string& value)
  {
    std::unique_ptr<StringValue> item(new StringValue(value));
    size_t size = value.size();

    boost::mutex::scoped_lock cacheLock(cacheMutex_);

    if (size > maxSize_)
    {
      // This object is too large to be stored in the cache, discard it
    }
    else if (content_.Contains(key))
    {
      // Value already stored, don't overwrite the old value but put it on top of the cache
      content_.MakeMostRecent(key);
    }
    else
    {
      Recycle(maxSize_ - size);   // Post-condition: currentSize_ <= maxSize_ - size
      assert(currentSize_ + size <= maxSize_);

      content_.Add(key, item.release());
      currentSize_ += size;
    }

    RemoveFromItemsBeingLoadedInternal(key);
  }


  void MemoryStringCache::Add(const std::string& key,
                              const void* buffer,
                              size_t size)
  {
    Add(key, std::string(reinterpret_cast<const char*>(buffer), size));
  }


  void MemoryStringCache::Invalidate(const std::string &key)
  {
    boost::mutex::scoped_lock cacheLock(cacheMutex_);

    StringValue* item = NULL;
    if (content_.Contains(key, item))
    {
      assert(item != NULL);
      const size_t size = item->GetMemoryUsage();
      delete item;

      content_.Invalidate(key);
          
      assert(currentSize_ >= size);
      currentSize_ -= size;
    }

    RemoveFromItemsBeingLoadedInternal(key);
  }


  bool MemoryStringCache::Fetch(std::string& value,
                                const std::string& key)
  {
    boost::mutex::scoped_lock cacheLock(cacheMutex_);

    StringValue* item;

    // if another client is currently loading the item, wait for it.
    while (itemsBeingLoaded_.find(key) != itemsBeingLoaded_.end() && !content_.Contains(key, item))
    {
      cacheCond_.wait(cacheLock);
    }

    if (content_.Contains(key, item))
    {
      value = dynamic_cast<StringValue&>(*item).GetContent();
      content_.MakeMostRecent(key);

      return true;
    }
    else
    {
      // note that this accessor will be in charge of loading and adding.
      itemsBeingLoaded_.insert(key);
      return false;
    }
  }


  void MemoryStringCache::RemoveFromItemsBeingLoaded(const std::string& key)
  {
    boost::mutex::scoped_lock cacheLock(cacheMutex_);
    RemoveFromItemsBeingLoadedInternal(key);
  }


  void MemoryStringCache::RemoveFromItemsBeingLoadedInternal(const std::string& key)
  {
    // notify all waiting users, some of them potentially waiting for this item
    itemsBeingLoaded_.erase(key);
    cacheCond_.notify_all();
  }

  void MemoryStringCache::Recycle(size_t targetSize)
  {
    // WARNING: "cacheMutex_" must be locked
    while (currentSize_ > targetSize)
    {
      assert(!content_.IsEmpty());
        
      StringValue* item = NULL;
      content_.RemoveOldest(item);

      assert(item != NULL);
      const size_t size = item->GetMemoryUsage();
      delete item;

      assert(currentSize_ >= size);
      currentSize_ -= size;
    }

    // Post-condition: "currentSize_ <= targetSize"
  }

  size_t MemoryStringCache::GetCurrentSize() const
  {
    boost::mutex::scoped_lock cacheLock(cacheMutex_);

    return currentSize_;
  }
    
  size_t MemoryStringCache::GetNumberOfItems() const
  {
    boost::mutex::scoped_lock cacheLock(cacheMutex_);
    return content_.GetSize();

  }

}
