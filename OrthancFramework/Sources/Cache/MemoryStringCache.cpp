/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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
      
    const std::string& GetContent() const
    {
      return content_;
    }

    virtual size_t GetMemoryUsage() const
    {
      return content_.size();
    }      
  };

  size_t MemoryStringCache::GetMaximumSize()
  {
    return cache_.GetMaximumSize();
  }

  void MemoryStringCache::SetMaximumSize(size_t size)
  {
    cache_.SetMaximumSize(size);
  }

  void MemoryStringCache::Add(const std::string& key,
                              const std::string& value)
  {
    cache_.Acquire(key, new StringValue(value));
  }

  void MemoryStringCache::Invalidate(const std::string &key)
  {
    cache_.Invalidate(key);
  }
  
  bool MemoryStringCache::Fetch(std::string& value,
                                const std::string& key)
  {
    MemoryObjectCache::Accessor reader(cache_, key, false /* multiple readers are allowed */);

    if (reader.IsValid())
    {
      value = dynamic_cast<StringValue&>(reader.GetValue()).GetContent();
      return true;
    }
    else
    {
      return false;
    }
  }
}
