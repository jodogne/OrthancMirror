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
#include "ParsedDicomCache.h"

#include "../OrthancException.h"

namespace Orthanc
{
  class ParsedDicomCache::Item : public ICacheable
  {
  private:
    std::unique_ptr<ParsedDicomFile>  dicom_;
    size_t                            fileSize_;

  public:
    Item(ParsedDicomFile* dicom,
         size_t fileSize) :
      dicom_(dicom),
      fileSize_(fileSize)
    {
      if (dicom == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }
    }

    virtual size_t GetMemoryUsage() const ORTHANC_OVERRIDE
    {
      return fileSize_;
    }

    ParsedDicomFile& GetDicom() const
    {
      assert(dicom_.get() != NULL);
      return *dicom_;
    }
  };


  ParsedDicomCache::ParsedDicomCache(size_t size) :
    cacheSize_(size),
    largeSize_(0)
  {
    if (size == 0)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }

  
  size_t ParsedDicomCache::GetNumberOfItems()
  {
#if !defined(__EMSCRIPTEN__)
    boost::mutex::scoped_lock lock(mutex_);
#endif

    if (cache_.get() == NULL)
    {
      return (largeDicom_.get() == NULL ? 0 : 1);
    }
    else
    {
      assert(largeDicom_.get() == NULL);
      assert(largeSize_ == 0);
      return cache_->GetNumberOfItems();
    }
  }


  size_t ParsedDicomCache::GetCurrentSize()
  {
#if !defined(__EMSCRIPTEN__)
    boost::mutex::scoped_lock lock(mutex_);
#endif

    if (cache_.get() == NULL)
    {
      return largeSize_;
    }
    else
    {
      assert(largeDicom_.get() == NULL);
      assert(largeSize_ == 0);
      return cache_->GetCurrentSize();
    }
  }

  
  void ParsedDicomCache::Invalidate(const std::string& id)
  {
#if !defined(__EMSCRIPTEN__)
    boost::mutex::scoped_lock lock(mutex_);
#endif
      
    if (cache_.get() != NULL)
    {
      cache_->Invalidate(id);
    }

    if (largeId_ == id)
    {
      largeDicom_.reset(NULL);
      largeSize_ = 0;
    }
  }

  
  void ParsedDicomCache::Acquire(const std::string& id,
                                 ParsedDicomFile* dicom,  // Takes ownership
                                 size_t fileSize)
  {
#if !defined(__EMSCRIPTEN__)
    boost::mutex::scoped_lock lock(mutex_);
#endif
      
    if (fileSize >= cacheSize_)
    {
      cache_.reset(NULL);
      largeDicom_.reset(dicom);
      largeId_ = id;
      largeSize_ = fileSize;
    }
    else
    {
      largeDicom_.reset(NULL);
      largeSize_ = 0;

      if (cache_.get() == NULL)
      {
        cache_.reset(new MemoryObjectCache);
        cache_->SetMaximumSize(cacheSize_);
      }

      cache_->Acquire(id, new Item(dicom, fileSize));
    }
  }


  ParsedDicomCache::Accessor::Accessor(ParsedDicomCache& that,
                                       const std::string& id) :
#if !defined(__EMSCRIPTEN__)
    lock_(that.mutex_),
#endif
    id_(id),
    file_(NULL),
    fileSize_(0)
  {
    if (that.largeDicom_.get() != NULL &&
        that.largeId_ == id)
    {
      file_ = that.largeDicom_.get();
      fileSize_ = that.largeSize_;
    }
    else if (that.cache_.get() != NULL)
    {
      accessor_.reset(new MemoryObjectCache::Accessor(
                        *that.cache_, id, true /* unique */));
      if (accessor_->IsValid())
      {            
        const Item& item = dynamic_cast<const Item&>(accessor_->GetValue());
        file_ = &item.GetDicom();
        fileSize_ = item.GetMemoryUsage();
      }
    }
  }


  bool ParsedDicomCache::Accessor::IsValid() const
  {
    return file_ != NULL;
  }


  ParsedDicomFile& ParsedDicomCache::Accessor::GetDicom() const
  {
    if (IsValid())
    {
      assert(file_ != NULL);
      return *file_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  size_t ParsedDicomCache::Accessor::GetFileSize() const
  {
    if (IsValid())
    {
      return fileSize_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }
}
