/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * In addition, as a special exception, the copyright holders of this
 * program give permission to link the code of its release with the
 * OpenSSL project's "OpenSSL" library (or with modified versions of it
 * that use the same license as the "OpenSSL" library), and distribute
 * the linked executables. You must obey the GNU General Public License
 * in all respects for all of the code used other than "OpenSSL". If you
 * modify file(s) with this exception, you may extend this exception to
 * your version of the file(s), but you are not obligated to do so. If
 * you do not wish to do so, delete this exception statement from your
 * version. If you delete this exception statement from all source files
 * in the program, then also delete it here.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include "../PrecompiledHeaders.h"
#include "SharedArchive.h"

#include "../SystemToolbox.h"


namespace Orthanc
{
  void SharedArchive::RemoveInternal(const std::string& id)
  {
    Archive::iterator it = archive_.find(id);

    if (it != archive_.end())
    {
      delete it->second;
      archive_.erase(it);
    }
  }


  SharedArchive::Accessor::Accessor(SharedArchive& that,
                                    const std::string& id) :
    lock_(that.mutex_)
  {
    Archive::iterator it = that.archive_.find(id);

    if (it == that.archive_.end())
    {
      throw OrthancException(ErrorCode_InexistentItem);
    }
    else
    {
      that.lru_.MakeMostRecent(id);
      item_ = it->second;
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
      std::string oldest = lru_.RemoveOldest();
      RemoveInternal(oldest);
    }

    std::string id = SystemToolbox::GenerateUuid();
    RemoveInternal(id);  // Should never be useful because of UUID
    archive_[id] = obj;
    lru_.Add(id);

    return id;
  }


  void SharedArchive::Remove(const std::string& id)
  {
    boost::mutex::scoped_lock lock(mutex_);
    RemoveInternal(id);      
    lru_.Invalidate(id);
  }


  void SharedArchive::List(std::list<std::string>& items)
  {
    items.clear();

    boost::mutex::scoped_lock lock(mutex_);

    for (Archive::const_iterator it = archive_.begin();
         it != archive_.end(); ++it)
    {
      items.push_back(it->first);
    }
  }
}


