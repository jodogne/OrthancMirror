/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include "../OrthancFramework.h"

#if !defined(__EMSCRIPTEN__)
// Multithreading is not supported in WebAssembly
#  include <boost/thread/shared_mutex.hpp>
#  include <boost/thread/lock_types.hpp>  // For boost::unique_lock<> and boost::shared_lock<>
#endif


namespace Orthanc
{
  class ORTHANC_PUBLIC ReaderWriterLock : public boost::noncopyable
  {
  private:
    boost::shared_mutex mutex_;

  public:
    class ReadLock : public boost::noncopyable
    {
    private:
#if !defined(__EMSCRIPTEN__)
      boost::shared_lock<boost::shared_mutex> lock_;
#endif

    public:
      explicit ReadLock(ReaderWriterLock& that)
#if !defined(__EMSCRIPTEN__)
        : lock_(that.mutex_)
#endif
      {
      }
    };

    class WriteLock : public boost::noncopyable
    {
    private:
#if !defined(__EMSCRIPTEN__)
      boost::unique_lock<boost::shared_mutex> lock_;
#endif

    public:
      explicit WriteLock(ReaderWriterLock& that)
#if !defined(__EMSCRIPTEN__)
        : lock_(that.mutex_)
#endif
      {
      }
    };
  };
}
