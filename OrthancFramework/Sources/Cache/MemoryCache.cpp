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
#include "MemoryCache.h"

#include "../Logging.h"

namespace Orthanc
{
  namespace Deprecated
  {
    MemoryCache::Page& MemoryCache::Load(const std::string& id)
    {
      // Reuse the cache entry if it already exists
      Page* p = NULL;
      if (index_.Contains(id, p))
      {
        LOG(TRACE) << "Reusing a cache page";
        assert(p != NULL);
        index_.MakeMostRecent(id);
        return *p;
      }

      // The id is not in the cache yet. Make some room if the cache
      // is full.
      if (index_.GetSize() == cacheSize_)
      {
        LOG(TRACE) << "Dropping the oldest cache page";
        index_.RemoveOldest(p);
        delete p;
      }

      // Create a new cache page
      std::unique_ptr<Page> result(new Page);
      result->id_ = id;
      result->content_.reset(provider_.Provide(id));

      // Add the newly create page to the cache
      LOG(TRACE) << "Registering new data in a cache page";
      p = result.release();
      index_.Add(id, p);
      return *p;
    }

    MemoryCache::MemoryCache(ICachePageProvider& provider,
                             size_t cacheSize) : 
      provider_(provider),
      cacheSize_(cacheSize)
    {
    }

    void MemoryCache::Invalidate(const std::string& id)
    {
      Page* p = NULL;
      if (index_.Contains(id, p))
      {
        LOG(TRACE) << "Invalidating a cache page";
        assert(p != NULL);
        delete p;
        index_.Invalidate(id);
      }
    }

    MemoryCache::~MemoryCache()
    {
      while (!index_.IsEmpty())
      {
        Page* element = NULL;
        index_.RemoveOldest(element);
        assert(element != NULL);
        delete element;
      }
    }

    IDynamicObject& MemoryCache::Access(const std::string& id)
    {
      return *Load(id).content_;
    }
  }
}
