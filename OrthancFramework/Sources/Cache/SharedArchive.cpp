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
#include "SharedArchive.h"

#include "../Toolbox.h"


namespace Orthanc
{
  void SharedArchive::RemoveInternal(const std::string& id)
  {
    Archive::iterator it = archive_.find(id);

    if (it != archive_.end())
    {
      delete it->second;
      archive_.erase(it);

      lru_.Invalidate(id);
    }
  }


  SharedArchive::Accessor::Accessor(SharedArchive& that,
                                    const std::string& id) :
    lock_(that.mutex_)
  {
    Archive::iterator it = that.archive_.find(id);

    if (it == that.archive_.end())
    {
      item_ = NULL;
    }
    else
    {
      that.lru_.MakeMostRecent(id);
      item_ = it->second;
    }
  }

  bool SharedArchive::Accessor::IsValid() const
  {
    return item_ != NULL;
  }


  IDynamicObject& SharedArchive::Accessor::GetItem() const
  {
    if (item_ == NULL)
    {
      // "IsValid()" should have been called
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      return *item_;
    }
  }  


  SharedArchive::SharedArchive(size_t maxSize) : 
    maxSize_(maxSize)
  {
    if (maxSize == 0)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  SharedArchive::~SharedArchive()
  {
    for (Archive::iterator it = archive_.begin();
         it != archive_.end(); ++it)
    {
      delete it->second;
    }
  }


  std::string SharedArchive::Add(IDynamicObject* obj)
  {
    boost::mutex::scoped_lock lock(mutex_);

    if (archive_.size() == maxSize_)
    {
      // The quota has been reached, remove the oldest element
      RemoveInternal(lru_.GetOldest());
    }

    std::string id = Toolbox::GenerateUuid();
    RemoveInternal(id);  // Should never be useful because of UUID

    archive_[id] = obj;
    lru_.Add(id);

    return id;
  }


  void SharedArchive::Remove(const std::string& id)
  {
    boost::mutex::scoped_lock lock(mutex_);
    RemoveInternal(id);      
  }


  void SharedArchive::List(std::list<std::string>& items)
  {
    items.clear();

    {
      boost::mutex::scoped_lock lock(mutex_);

      for (Archive::const_iterator it = archive_.begin();
           it != archive_.end(); ++it)
      {
        items.push_back(it->first);
      }
    }
  }
}
