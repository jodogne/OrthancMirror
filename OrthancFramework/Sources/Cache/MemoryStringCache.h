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

#include "../OrthancFramework.h"
#include "ICacheable.h"
#include "LeastRecentlyUsedIndex.h"

#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>


namespace Orthanc
{
  /**
   * Class that caches a dictionary
   * of strings, using the "fetch/add" paradigm of memcached.
   * 
   * Starting from 1.12.2, if multiple clients are trying to access
   * an inexistent item at the same time, only one of them will load it
   * and the others will wait until the first one has loaded the data.
   * 
   * The MemoryStringCache is only accessible through an Accessor.
   * 
   * Note: this class is thread safe
   **/
  class ORTHANC_PUBLIC MemoryStringCache : public boost::noncopyable
  {
  public:
    class Accessor : public boost::noncopyable
    {
      MemoryStringCache& cache_;
      bool                shouldAdd_;  // when this accessor is the one who should load and add the data
      std::string         keyToAdd_;
    public:
      Accessor(MemoryStringCache& cache);
      ~Accessor();

      bool Fetch(std::string& value, const std::string& key);
      void Add(const std::string& key, const std::string& value);
      void Add(const std::string& key,const char* buffer, size_t size);
    };

  private:
    class StringValue;

    boost::mutex              cacheMutex_;  // note: we can not use recursive_mutex with condition_variable
    boost::condition_variable cacheCond_;
    std::set<std::string>     itemsBeingLoaded_;

    size_t currentSize_;
    size_t maxSize_;
    LeastRecentlyUsedIndex<std::string, StringValue*>  content_;

    void Recycle(size_t targetSize);

  public:
    MemoryStringCache();

    ~MemoryStringCache();

    size_t GetMaximumSize();
    
    void SetMaximumSize(size_t size);

    void Invalidate(const std::string& key);


  private:
    void Add(const std::string& key,
             const std::string& value);

    void Add(const std::string& key,
             const void* buffer,
             size_t size);

    bool Fetch(std::string& value,
               const std::string& key);

    void RemoveFromItemsBeingLoaded(const std::string& key);
    void RemoveFromItemsBeingLoadedInternal(const std::string& key);

    void AddToItemsBeingLoadedInternal(const std::string& key);
  };
}
