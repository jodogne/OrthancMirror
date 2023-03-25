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


#pragma once

#include <list>
#include <map>
#include <vector>
#include <boost/noncopyable.hpp>
#include <cassert>

#include "../OrthancException.h"
#include "../Toolbox.h"

namespace Orthanc
{
  /**
   * This class implements the index of a cache with least recently
   * used (LRU) recycling policy. All the items of the cache index
   * can be associated with a payload.
   * Reference: http://stackoverflow.com/a/2504317
   **/
  template <typename T, typename Payload = NullType>
  class LeastRecentlyUsedIndex : public boost::noncopyable
  {
  private:
    typedef std::list< std::pair<T, Payload> >  Queue;
    typedef std::map<T, typename Queue::iterator>  Index;

    Index  index_;
    Queue  queue_;

    /**
     * Internal method for debug builds to check whether the internal
     * data structures are not corrupted.
     **/
    void CheckInvariants() const;

  public:
    /**
     * Add a new element to the cache index, and make it the most
     * recent element.
     * \param id The ID of the element.
     * \param payload The payload of the element.
     **/
    void Add(T id, Payload payload = Payload());

    void AddOrMakeMostRecent(T id, Payload payload = Payload());

    /**
     * When accessing an element of the cache, this method tags the
     * element as the most recently used.
     * \param id The most recently accessed item.
     **/
    void MakeMostRecent(T id);

    void MakeMostRecent(T id, Payload updatedPayload);

    /**
     * Remove an element from the cache index.
     * \param id The item to remove.
     **/
    Payload Invalidate(T id);

    /**
     * Get the oldest element in the cache and remove it.
     * \return The oldest item.
     **/
    T RemoveOldest();

    /**
     * Get the oldest element in the cache, remove it and return the
     * associated payload.
     * \param payload Where to store the associated payload.
     * \return The oldest item.
     **/
    T RemoveOldest(Payload& payload);

    /**
     * Check whether an element is contained in the cache.
     * \param id The item.
     * \return \c true iff the item is indexed by the cache.
     **/
    bool Contains(T id) const
    {
      return index_.find(id) != index_.end();
    }

    bool Contains(T id, Payload& payload) const
    {
      typename Index::const_iterator it = index_.find(id);
      if (it == index_.end())
      {
        return false;
      }
      else
      {
        payload = it->second->second;
        return true;
      }
    }

    /**
     * Return the number of elements in the cache.
     * \return The number of elements.
     **/
    size_t GetSize() const
    {
      assert(index_.size() == queue_.size());
      return queue_.size();
    }

    /**
     * Check whether the cache index is empty.
     * \return \c true iff the cache is empty.
     **/
    bool IsEmpty() const
    {
      return index_.empty();
    }

    const T& GetOldest() const;
    
    const Payload& GetOldestPayload() const;

    void GetAllKeys(std::vector<T>& keys) const
    {
      keys.clear();
      keys.reserve(GetSize());
      for (typename Index::const_iterator it = index_.begin(); it != index_.end(); ++it)
      {
        keys.push_back(it->first);
      }
    }
  };




  /******************************************************************
   ** Implementation of the template
   ******************************************************************/

  template <typename T, typename Payload>
  void LeastRecentlyUsedIndex<T, Payload>::CheckInvariants() const
  {
#ifndef NDEBUG
    assert(index_.size() == queue_.size());

    for (typename Index::const_iterator 
           it = index_.begin(); it != index_.end(); ++it)
    {
      assert(it->second != queue_.end());
      assert(it->second->first == it->first);
    }
#endif
  }


  template <typename T, typename Payload>
  void LeastRecentlyUsedIndex<T, Payload>::Add(T id, Payload payload)
  {
    if (Contains(id))
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    queue_.push_front(std::make_pair(id, payload));
    index_[id] = queue_.begin();

    CheckInvariants();
  }


  template <typename T, typename Payload>
  void LeastRecentlyUsedIndex<T, Payload>::MakeMostRecent(T id)
  {
    if (!Contains(id))
    {
      throw OrthancException(ErrorCode_InexistentItem);
    }

    typename Index::iterator it = index_.find(id);
    assert(it != index_.end());

    std::pair<T, Payload> item = *(it->second);
    
    queue_.erase(it->second);
    queue_.push_front(item);
    index_[id] = queue_.begin();

    CheckInvariants();
  }


  template <typename T, typename Payload>
  void LeastRecentlyUsedIndex<T, Payload>::AddOrMakeMostRecent(T id, Payload payload)
  {
    typename Index::iterator it = index_.find(id);

    if (it != index_.end())
    {
      // Already existing. Make it most recent.
      std::pair<T, Payload> item = *(it->second);
      item.second = payload;
      queue_.erase(it->second);
      queue_.push_front(item);
    }
    else
    {
      // New item
      queue_.push_front(std::make_pair(id, payload));
    }

    index_[id] = queue_.begin();

    CheckInvariants();
  }


  template <typename T, typename Payload>
  void LeastRecentlyUsedIndex<T, Payload>::MakeMostRecent(T id, Payload updatedPayload)
  {
    if (!Contains(id))
    {
      throw OrthancException(ErrorCode_InexistentItem);
    }

    typename Index::iterator it = index_.find(id);
    assert(it != index_.end());

    std::pair<T, Payload> item = *(it->second);
    item.second = updatedPayload;
    
    queue_.erase(it->second);
    queue_.push_front(item);
    index_[id] = queue_.begin();

    CheckInvariants();
  }


  template <typename T, typename Payload>
  Payload LeastRecentlyUsedIndex<T, Payload>::Invalidate(T id)
  {
    if (!Contains(id))
    {
      throw OrthancException(ErrorCode_InexistentItem);
    }

    typename Index::iterator it = index_.find(id);
    assert(it != index_.end());

    Payload payload = it->second->second;
    queue_.erase(it->second);
    index_.erase(it);

    CheckInvariants();
    return payload;
  }


  template <typename T, typename Payload>
  T LeastRecentlyUsedIndex<T, Payload>::RemoveOldest(Payload& payload)
  {
    if (IsEmpty())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    std::pair<T, Payload> item = queue_.back();
    T oldest = item.first;
    payload = item.second;

    queue_.pop_back();
    assert(index_.find(oldest) != index_.end());
    index_.erase(oldest);

    CheckInvariants();

    return oldest;
  }


  template <typename T, typename Payload>
  T LeastRecentlyUsedIndex<T, Payload>::RemoveOldest()
  {
    if (IsEmpty())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    std::pair<T, Payload> item = queue_.back();
    T oldest = item.first;

    queue_.pop_back();
    assert(index_.find(oldest) != index_.end());
    index_.erase(oldest);

    CheckInvariants();

    return oldest;
  }


  template <typename T, typename Payload>
  const T& LeastRecentlyUsedIndex<T, Payload>::GetOldest() const
  {
    if (IsEmpty())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    return queue_.back().first;
  }


  template <typename T, typename Payload>
  const Payload& LeastRecentlyUsedIndex<T, Payload>::GetOldestPayload() const
  {
    if (IsEmpty())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    return queue_.back().second;
  }
}
