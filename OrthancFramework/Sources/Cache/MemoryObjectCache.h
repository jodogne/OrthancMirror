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


#pragma once

#include "../OrthancFramework.h"
#include "ICacheable.h"
#include "LeastRecentlyUsedIndex.h"

#if !defined(__EMSCRIPTEN__)
// Multithreading is not supported in WebAssembly
#  include <boost/thread/mutex.hpp>
#  include <boost/thread/shared_mutex.hpp>
#endif

#include <boost/date_time/posix_time/posix_time.hpp>


namespace Orthanc
{
  /**
   *  Note: this class is thread safe
   **/
  class ORTHANC_PUBLIC MemoryObjectCache : public boost::noncopyable
  {
  private:
    class Item;

#if !defined(__EMSCRIPTEN__)
    typedef boost::unique_lock<boost::shared_mutex> WriterLock;
    typedef boost::shared_lock<boost::shared_mutex> ReaderLock;

    // This mutex protects modifications to the structure of the cache (monitor)
    boost::mutex   cacheMutex_;

    // This mutex protects modifications to the items that are stored in the cache
    boost::shared_mutex contentMutex_;
#endif

    size_t currentSize_;
    size_t maxSize_;
    LeastRecentlyUsedIndex<std::string, Item*>  content_;

    void Recycle(size_t targetSize);
    
  public:
    MemoryObjectCache();

    ~MemoryObjectCache();

    size_t GetNumberOfItems();  // For unit tests only

    size_t GetCurrentSize();  // For unit tests only

    size_t GetMaximumSize();

    void SetMaximumSize(size_t size);

    void Acquire(const std::string& key,
                 ICacheable* value);

    void Invalidate(const std::string& key);

    class Accessor : public boost::noncopyable
    {
    private:
#if !defined(__EMSCRIPTEN__)
      ReaderLock                 readerLock_;
      WriterLock                 writerLock_;
      boost::mutex::scoped_lock  cacheLock_;
#endif
      
      Item*  item_;

    public:
      Accessor(MemoryObjectCache& cache,
               const std::string& key,
               bool unique);

      bool IsValid() const
      {
        return item_ != NULL;
      }

      ICacheable& GetValue() const;

      const boost::posix_time::ptime& GetTime() const;
    };
  };
}
