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

#include "MemoryObjectCache.h"

namespace Orthanc
{
  /**
   * Facade object around "MemoryObjectCache" that caches a dictionary
   * of strings, using the "fetch/add" paradigm of memcached.
   * 
   * Note: this class is thread safe
   **/
  class ORTHANC_PUBLIC MemoryStringCache : public boost::noncopyable
  {
  private:
    class StringValue;

    MemoryObjectCache  cache_;

  public:
    size_t GetMaximumSize();
    
    void SetMaximumSize(size_t size);

    void Add(const std::string& key,
             const std::string& value);

    void Add(const std::string& key,
             const void* buffer,
             size_t size);

    void Invalidate(const std::string& key);

    bool Fetch(std::string& value,
               const std::string& key);
  };
}
