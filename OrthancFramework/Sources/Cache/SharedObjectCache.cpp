/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
#include "SharedObjectCache.h"


namespace Orthanc
{
  class SharedObjectCache::Item : public boost::noncopyable
  {
  private:
    boost::shared_ptr<IDynamicObject>  value_;
    size_t                             size_;

  public:
    Item(boost::shared_ptr<IDynamicObject> value,
         size_t size) :
      value_(value),
      size_(size)
    {
    }

    const boost::shared_ptr<IDynamicObject>& GetValue() const
    {
      return value_;
    }

    size_t GetSize() const
    {
      return size_;
    }
  };


  // The mutex must be locked
  void SharedObjectCache::MakeRoom(size_t newObjectSize)
  {
    assert(newObjectSize != 0);

    while (!content_.empty() &&
           currentSize_ + newObjectSize > capacity_)
    {
      std::string oldest = lru_.RemoveOldest();
      assert(!lru_.Contains(oldest));

      Content::iterator found = content_.find(oldest);
      assert(found != content_.end());
      assert(found->second != NULL);

      assert(currentSize_ >= found->second->GetSize());
      currentSize_ -= found->second->GetSize();

      delete found->second;
      content_.erase(found);

      // Any thread holding a shared_ptr to "oldest" is unaffected.
      // The object is destroyed only when the last reference drops.
    }
  }


  SharedObjectCache::SharedObjectCache(IObjectSizeProvider* provider,
                                       size_t capacity) :
    provider_(provider),
    capacity_(capacity),
    currentSize_(0)
  {
    if (provider == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
  }


  SharedObjectCache::~SharedObjectCache()
  {
    for (Content::iterator it = content_.begin(); it != content_.end(); ++it)
    {
      assert(it->second != NULL);
      delete it->second;
    }
  }


  boost::shared_ptr<IDynamicObject> SharedObjectCache::GetCachedValue(const std::string& id)
  {
    Mutex::ScopedLock lock(mutex_);

    Content::const_iterator found = content_.find(id);

    if (found == content_.end())
    {
      assert(!lru_.Contains(id));
      return boost::shared_ptr<IDynamicObject>();  // Initialization to NULL
    }
    else
    {
      assert(lru_.Contains(id));
      lru_.MakeMostRecent(id);

      assert(found->second != NULL);
      return found->second->GetValue();
    }
  }


  void SharedObjectCache::Store(const std::string& id,
                                const boost::shared_ptr<IDynamicObject>& value)
  {
    if (value.get() == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }

    assert(provider_.get() != NULL);
    size_t size = provider_->GetSize(*value) + sizeof(Item);

    // Caching items with zero size would break the logic of "MakeRoom()", hence "sizeof(Item)"
    assert(size != 0);

    if (size > capacity_)
    {
      return;  // This value is too large to be cached
    }
    else
    {
      Mutex::ScopedLock lock(mutex_);

      MakeRoom(size);

      Content::iterator found = content_.find(id);

      if (found == content_.end())
      {
        assert(!lru_.Contains(id));
        lru_.Add(id);

        content_[id] = new Item(value, size);
      }
      else
      {
        assert(lru_.Contains(id));
        lru_.MakeMostRecent(id);

        assert(found->second != NULL);
        delete found->second;

        found->second = new Item(value, size);
      }

      assert(lru_.Contains(id));

      currentSize_ += size;
    }
  }


  void SharedObjectCache::Invalidate(const std::string& id)
  {
    Mutex::ScopedLock lock(mutex_);

    Content::iterator found = content_.find(id);

    if (found != content_.end())
    {
      assert(lru_.Contains(id));
      lru_.Invalidate(id);

      assert(found->second != NULL);
      delete found->second;

      content_.erase(found);
    }

    assert(!lru_.Contains(id));
  }
}
