/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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


#include "../PrecompiledHeaders.h"
#include "MemoryObjectCache.h"

#include "../Compatibility.h"

namespace Orthanc
{
  class MemoryObjectCache::Item : public boost::noncopyable
  {
  private:
    ICacheable*               value_;
    boost::posix_time::ptime  time_;

  public:
    explicit Item(ICacheable* value) :   // Takes ownership
    value_(value),
    time_(boost::posix_time::second_clock::local_time())
    {
      if (value == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }
    }

    ~Item()
    {
      assert(value_ != NULL);
      delete value_;
    }

    ICacheable& GetValue() const
    {
      assert(value_ != NULL);
      return *value_;
    }

    const boost::posix_time::ptime& GetTime() const
    {
      return time_;
    }
  };


  void MemoryObjectCache::Recycle(size_t targetSize)
  {
    // WARNING: "cacheMutex_" must be locked
    while (currentSize_ > targetSize)
    {
      assert(!content_.IsEmpty());
        
      Item* item = NULL;
      content_.RemoveOldest(item);

      assert(item != NULL);
      const size_t size = item->GetValue().GetMemoryUsage();
      delete item;

      assert(currentSize_ >= size);
      currentSize_ -= size;
    }

    // Post-condition: "currentSize_ <= targetSize"
  }
    

  MemoryObjectCache::MemoryObjectCache() :
    currentSize_(0),
    maxSize_(100 * 1024 * 1024)  // 100 MB
  {
  }


  MemoryObjectCache::~MemoryObjectCache()
  {
    Recycle(0);
    assert(content_.IsEmpty());
  }


  size_t MemoryObjectCache::GetMaximumSize()
  {
#if !defined(__EMSCRIPTEN__)
    boost::mutex::scoped_lock lock(cacheMutex_);
#endif

    return maxSize_;
  }


  void MemoryObjectCache::SetMaximumSize(size_t size)
  {
    if (size == 0)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
      
#if !defined(__EMSCRIPTEN__)
    // Make sure no accessor is currently open (as its data may be
    // removed if recycling is needed)
    WriterLock contentLock(contentMutex_);

    // Lock the global structure of the cache
    boost::mutex::scoped_lock cacheLock(cacheMutex_);
#endif

    Recycle(size);
    maxSize_ = size;
  }


  void MemoryObjectCache::Acquire(const std::string& key,
                                  ICacheable* value)
  {
    std::unique_ptr<Item> item(new Item(value));

    if (value == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
    else
    {
#if !defined(__EMSCRIPTEN__)
      // Make sure no accessor is currently open (as its data may be
      // removed if recycling is needed)
      WriterLock contentLock(contentMutex_);

      // Lock the global structure of the cache
      boost::mutex::scoped_lock cacheLock(cacheMutex_);
#endif

      const size_t size = item->GetValue().GetMemoryUsage();

      if (size > maxSize_)
      {
        // This object is too large to be stored in the cache, discard it
      }
      else if (content_.Contains(key))
      {
        // Value already stored, don't overwrite the old value
        content_.MakeMostRecent(key);
      }
      else
      {
        Recycle(maxSize_ - size);   // Post-condition: currentSize_ <= maxSize_ - size
        assert(currentSize_ + size <= maxSize_);

        content_.Add(key, item.release());
        currentSize_ += size;
      }
    }
  }


  void MemoryObjectCache::Invalidate(const std::string& key)
  {
#if !defined(__EMSCRIPTEN__)
    // Make sure no accessor is currently open (as it may correspond
    // to the key to remove)
    WriterLock contentLock(contentMutex_);

    // Lock the global structure of the cache
    boost::mutex::scoped_lock cacheLock(cacheMutex_);
#endif

    Item* item = NULL;
    if (content_.Contains(key, item))
    {
      assert(item != NULL);
      const size_t size = item->GetValue().GetMemoryUsage();
      delete item;

      content_.Invalidate(key);
          
      assert(currentSize_ >= size);
      currentSize_ -= size;
    }
  }


  MemoryObjectCache::Accessor::Accessor(MemoryObjectCache& cache,
                                        const std::string& key,
                                        bool unique) :
    item_(NULL)
  {
#if !defined(__EMSCRIPTEN__)
    if (unique)
    {
      writerLock_ = WriterLock(cache.contentMutex_);
    }
    else
    {
      readerLock_ = ReaderLock(cache.contentMutex_);
    }

    // Lock the global structure of the cache, must be *after* the
    // reader/writer lock
    cacheLock_ = boost::mutex::scoped_lock(cache.cacheMutex_);
#endif

    if (cache.content_.Contains(key, item_))
    {
      cache.content_.MakeMostRecent(key);
    }
    
#if !defined(__EMSCRIPTEN__)
    cacheLock_.unlock();

    if (item_ == NULL)
    {
      // This item does not exist in the cache, we can release the
      // reader/writer lock
      if (unique)
      {
        writerLock_.unlock();
      }
      else
      {
        readerLock_.unlock();
      }
    }
#endif
  }


  ICacheable& MemoryObjectCache::Accessor::GetValue() const
  {
    if (IsValid())
    {
      return item_->GetValue();
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  const boost::posix_time::ptime& MemoryObjectCache::Accessor::GetTime() const
  {
    if (IsValid())
    {
      return item_->GetTime();
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }        
  }
}
