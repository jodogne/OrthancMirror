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


#pragma once

#if !defined(ORTHANC_SANDBOXED)
#  error The macro ORTHANC_SANDBOXED must be defined
#endif

#if ORTHANC_SANDBOXED == 1
#  error The class SharedArchive cannot be used in sandboxed environments
#endif

#include "LeastRecentlyUsedIndex.h"
#include "../IDynamicObject.h"

#include <map>
#include <boost/thread.hpp>

namespace Orthanc
{
  class ORTHANC_PUBLIC SharedArchive : public boost::noncopyable
  {
  private:
    typedef std::map<std::string, IDynamicObject*>  Archive;

    size_t         maxSize_;
    boost::mutex   mutex_;
    Archive        archive_;
    LeastRecentlyUsedIndex<std::string> lru_;

    void RemoveInternal(const std::string& id);

  public:
    class ORTHANC_PUBLIC Accessor : public boost::noncopyable
    {
    private:
      boost::mutex::scoped_lock  lock_;
      IDynamicObject*            item_;

    public:
      Accessor(SharedArchive& that,
               const std::string& id);

      bool IsValid() const;
      
      IDynamicObject& GetItem() const;
    };


    explicit SharedArchive(size_t maxSize);

    ~SharedArchive();

    std::string Add(IDynamicObject* obj);  // Takes the ownership

    void Remove(const std::string& id);

    void List(std::list<std::string>& items);
  };
}


