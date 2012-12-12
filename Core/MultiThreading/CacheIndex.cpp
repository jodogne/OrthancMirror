/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012 Medical Physics Department, CHU of Liege,
 * Belgium
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


#include "CacheIndex.h"

#include <cassert>
#include <string>
#include "../OrthancException.h"
#include "../IDynamicObject.h"

namespace Orthanc
{
  template <typename T, typename Payload>
  void CacheIndex<T, Payload>::CheckInvariants() const
  {
#ifndef NDEBUG
    assert(index_.size() == queue_.size());

    for (typename Index::const_iterator 
           it = index_.begin(); it != index_.end(); it++)
    {
      assert(it->second != queue_.end());
      assert(it->second->first == it->first);
    }
#endif
  }


  template <typename T, typename Payload>
  void CacheIndex<T, Payload>::Add(T id, Payload payload)
  {
    if (Contains(id))
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    queue_.push_front(std::make_pair(id, payload));
    index_[id] = queue_.begin();

    CheckInvariants();
  }


  template <typename T, typename Payload>
  void CacheIndex<T, Payload>::TagAsMostRecent(T id)
  {
    if (!Contains(id))
    {
      throw OrthancException(ErrorCode_InexistentItem);
    }

    typename Index::iterator it = index_.find(id);
    assert(it != index_.end());

    std::pair<T, Payload> item = *(it->second);
    
    queue_.erase(it->second);
    queue_.push_front(item);
    index_[id] = queue_.begin();

    CheckInvariants();
  }


  template <typename T, typename Payload>
  Payload CacheIndex<T, Payload>::Invalidate(T id)
  {
    if (!Contains(id))
    {
      throw OrthancException(ErrorCode_InexistentItem);
    }

    typename Index::iterator it = index_.find(id);
    assert(it != index_.end());

    Payload payload = it->second->second;
    queue_.erase(it->second);
    index_.erase(it);

    CheckInvariants();
    return payload;
  }


  template <typename T, typename Payload>
  T CacheIndex<T, Payload>::RemoveOldest(Payload& payload)
  {
    if (IsEmpty())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    std::pair<T, Payload> item = queue_.back();
    T oldest = item.first;
    payload = item.second;

    queue_.pop_back();
    assert(index_.find(oldest) != index_.end());
    index_.erase(oldest);

    CheckInvariants();

    return oldest;
  }


  // Explicit template instanciation for some data types
  template class CacheIndex<std::string, NullType>;
  template class CacheIndex<std::string, int>;
  template class CacheIndex<const char*, IDynamicObject*>;
}
