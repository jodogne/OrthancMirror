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


#pragma once

#include "../Compatibility.h"
#include "../IDynamicObject.h"
#include "../MultiThreading/Mutex.h"
#include "LeastRecentlyUsedIndex.h"

#include <boost/shared_ptr.hpp>


namespace Orthanc
{
  class ORTHANC_PUBLIC SharedObjectCache : public boost::noncopyable
  {
  public:
    class ORTHANC_PUBLIC ISizeProvider : public boost::noncopyable
    {
    public:
      virtual ~ISizeProvider()
      {
      }

      virtual size_t GetSize(const IDynamicObject& object) = 0;
    };

    class StringSizeProvider : public ISizeProvider
    {
    public:
      virtual size_t GetSize(const IDynamicObject& object) ORTHANC_OVERRIDE
      {
        return dynamic_cast<const SingleValueObject<std::string>&>(object).GetValue().size();
      }
    };

  private:
    class Item;

    typedef std::map<std::string, Item*>  Content;

    Mutex                                mutex_;
    std::unique_ptr<ISizeProvider>       provider_;
    LeastRecentlyUsedIndex<std::string>  lru_;
    Content                              content_;
    size_t                               capacity_;
    size_t                               currentSize_;

    void MakeRoom(size_t newObjectSize);

  public:
    SharedObjectCache(ISizeProvider* provider /* takes ownership */,
                      size_t capacity);

    ~SharedObjectCache();

    /**
     * This method will return NULL if the object if not cached. The
     * returned object must be accessed in read-only mode or implement
     * its own mutex.
     **/
    boost::shared_ptr<IDynamicObject> GetCachedValue(const std::string& id);

    /**
     * As soon as the object is stored into the cache, the value could
     * be read by other threads.
     **/
    void Store(const std::string& id,
               const boost::shared_ptr<IDynamicObject>& value);

    void Invalidate(const std::string& id);
  };
}
