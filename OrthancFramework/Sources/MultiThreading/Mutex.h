/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2024 Osimis S.A., Belgium
 * Copyright (C) 2021-2024 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#if !defined(__EMSCRIPTEN__)
#  include <boost/thread/mutex.hpp>
#endif

namespace Orthanc
{
  // Wrapper class for compatibility with Emscripten

#if defined(__EMSCRIPTEN__)

  class ORTHANC_PUBLIC Mutex : public boost::noncopyable
  {
  public:
    class ORTHANC_PUBLIC ScopedLock : public boost::noncopyable
    {
    public:
      explicit ScopedLock(Mutex& mutex)
      {
      }
    };
  };

#else

  class ORTHANC_PUBLIC Mutex : public boost::noncopyable
  {
  private:
    boost::mutex mutex_;

  public:
    class ORTHANC_PUBLIC ScopedLock : public boost::noncopyable
    {
    private:
      boost::mutex::scoped_lock lock_;

    public:
      explicit ScopedLock(Mutex& mutex) :
        lock_(mutex.mutex_)
      {
      }
    };
  };
#endif
}
