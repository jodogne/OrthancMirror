/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2014 Medical Physics Department, CHU of Liege,
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


#include "../PrecompiledHeaders.h"
#include "ArrayFilledByThreads.h"

#include "../MultiThreading/ThreadedCommandProcessor.h"
#include "../OrthancException.h"

namespace Orthanc
{
  class ArrayFilledByThreads::Command : public ICommand
  {
  private:
    ArrayFilledByThreads&  that_;
    size_t  index_;

  public:
    Command(ArrayFilledByThreads& that,
            size_t index) :
      that_(that),
      index_(index)
    {
    }

    virtual bool Execute()
    {
      std::auto_ptr<IDynamicObject> obj(that_.filler_.GetFillerItem(index_));
      if (obj.get() == NULL)
      {
        return false;
      }
      else
      {
        boost::mutex::scoped_lock lock(that_.mutex_);
        that_.array_[index_] = obj.release();
        return true;
      }
    }
  };

  void ArrayFilledByThreads::Clear()
  {
    for (size_t i = 0; i < array_.size(); i++)
    {
      if (array_[i])
        delete array_[i];
    }

    array_.clear();
    filled_ = false;
  }

  void ArrayFilledByThreads::Update()
  {
    if (!filled_)
    {
      array_.resize(filler_.GetFillerSize());

      Orthanc::ThreadedCommandProcessor processor(threadCount_);
      for (size_t i = 0; i < array_.size(); i++)
      {
        processor.Post(new Command(*this, i));
      }

      processor.Join();
      filled_ = true;
    }
  }


  ArrayFilledByThreads::ArrayFilledByThreads(IFiller& filler) : filler_(filler)
  {
    filled_ = false;
    threadCount_ = 4;
  }


  ArrayFilledByThreads::~ArrayFilledByThreads()
  {
    Clear();
  }

  
  void ArrayFilledByThreads::Reload()
  {
    Clear();
    Update();
  }


  void ArrayFilledByThreads::Invalidate()
  {
    Clear();
  }


  void ArrayFilledByThreads::SetThreadCount(unsigned int t)
  {
    if (t < 1)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    threadCount_ = t;
  }


  size_t ArrayFilledByThreads::GetSize()
  {
    Update();
    return array_.size();
  }


  IDynamicObject& ArrayFilledByThreads::GetItem(size_t index)
  {
    if (index >= GetSize())
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_ParameterOutOfRange);
    }

    return *array_[index];
  }
}
