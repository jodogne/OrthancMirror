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
#include "ReaderWriterLock.h"

#include <boost/thread/shared_mutex.hpp>

namespace Orthanc
{
  namespace
  {
    // Anonymous namespace to avoid clashes between compilation
    // modules.

    class ReaderLockable : public ILockable
    {
    private:
      boost::shared_mutex& lock_;

    protected:
      virtual void Lock()
      {
        lock_.lock_shared();
      }

      virtual void Unlock()
      {
        lock_.unlock_shared();        
      }

    public:
      explicit ReaderLockable(boost::shared_mutex& lock) : lock_(lock)
      {
      }
    };


    class WriterLockable : public ILockable
    {
    private:
      boost::shared_mutex& lock_;

    protected:
      virtual void Lock()
      {
        lock_.lock();
      }

      virtual void Unlock()
      {
        lock_.unlock();        
      }

    public:
      explicit WriterLockable(boost::shared_mutex& lock) : lock_(lock)
      {
      }
    };
  }

  struct ReaderWriterLock::PImpl
  {
    boost::shared_mutex lock_;
    ReaderLockable reader_;
    WriterLockable writer_;

    PImpl() : reader_(lock_), writer_(lock_)
    {
    }
  };


  ReaderWriterLock::ReaderWriterLock()
  {
    pimpl_ = new PImpl;
  }


  ReaderWriterLock::~ReaderWriterLock()
  {
    delete pimpl_;
  }


  ILockable&  ReaderWriterLock::ForReader()
  {
    return pimpl_->reader_;
  }


  ILockable&  ReaderWriterLock::ForWriter()
  {
    return pimpl_->writer_;
  }
}
