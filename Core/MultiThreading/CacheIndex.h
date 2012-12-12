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


#pragma once

#include <list>
#include <map>
#include <boost/noncopyable.hpp>

namespace Orthanc
{
  class NullType
  {
  };

  /**
   * This class implements the index of a cache with least recently
   * used (LRU) recycling policy. All the items of the cache index
   * can be associated with a payload.
   * Reference: http://stackoverflow.com/a/2504317
   **/
  template <typename T, typename Payload = NullType>
  class CacheIndex : public boost::noncopyable
  {
  private:
    typedef std::list< std::pair<T, Payload> >  Queue;
    typedef std::map<T, typename Queue::iterator>  Index;

    Index  index_;
    Queue  queue_;

    /**
     * Internal method for debug builds to check whether the internal
     * data structures are not corrupted.
     **/
    void CheckInvariants() const;

  public:
    /**
     * Add a new element to the cache index, and make it the most
     * recent element.
     * \param id The ID of the element.
     * \param payload The payload of the element.
     **/
    void Add(T id, Payload payload = Payload());

    /**
     * When accessing an element of the cache, this method tags the
     * element as the most recently used.
     * \param id The most recently accessed item.
     **/
    void TagAsMostRecent(T id);

    /**
     * Remove an element from the cache index.
     * \param id The item to remove.
     **/
    Payload Invalidate(T id);

    /**
     * Get the oldest element in the cache and remove it.
     * \return The oldest item.
     **/
    T RemoveOldest()
    {
      Payload p;
      return RemoveOldest(p);
    }

    /**
     * Get the oldest element in the cache, remove it and return the
     * associated payload.
     * \param payload Where to store the associated payload.
     * \return The oldest item.
     **/
    T RemoveOldest(Payload& payload);

    /**
     * Check whether an element is contained in the cache.
     * \param id The item.
     * \return \c true iff the item is indexed by the cache.
     **/
    bool Contains(T id) const
    {
      return index_.find(id) != index_.end();
    }

    /**
     * Check whether the cache index is empty.
     * \return \c true iff the cache is empty.
     **/
    bool IsEmpty() const
    {
      return index_.empty();
    }
  };
}
