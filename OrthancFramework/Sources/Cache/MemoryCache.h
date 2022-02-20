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

#include "../Compatibility.h"
#include "ICachePageProvider.h"
#include "LeastRecentlyUsedIndex.h"

#include <memory>

namespace Orthanc
{
  namespace Deprecated
  {
    /**
     * WARNING: This class is NOT thread-safe.
     **/
    class ORTHANC_PUBLIC MemoryCache : public boost::noncopyable
    {
    private:
      struct Page
      {
        std::string id_;
        std::unique_ptr<IDynamicObject> content_;
      };

      ICachePageProvider& provider_;
      size_t cacheSize_;
      LeastRecentlyUsedIndex<std::string, Page*>  index_;

      Page& Load(const std::string& id);

    public:
      MemoryCache(ICachePageProvider& provider,
                  size_t cacheSize);

      ~MemoryCache();

      IDynamicObject& Access(const std::string& id);

      void Invalidate(const std::string& id);
    };
  }
}
