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

#include "../Cache/MemoryObjectCache.h"
#include "ParsedDicomFile.h"

namespace Orthanc
{
  class ORTHANC_PUBLIC ParsedDicomCache : public boost::noncopyable
  {
  private:
    class Item;

#if !defined(__EMSCRIPTEN__)
    boost::mutex                        mutex_;
#endif
    
    size_t                              cacheSize_;
    std::unique_ptr<MemoryObjectCache>  cache_;
    std::unique_ptr<ParsedDicomFile>    largeDicom_;
    std::string                         largeId_;
    size_t                              largeSize_;

  public:
    explicit ParsedDicomCache(size_t size);

    size_t GetNumberOfItems();  // For unit tests only

    size_t GetCurrentSize();  // For unit tests only

    void Invalidate(const std::string& id);

    void Acquire(const std::string& id,
                 ParsedDicomFile* dicom,  // Takes ownership
                 size_t fileSize);

    class ORTHANC_PUBLIC Accessor : public boost::noncopyable
    {
    private:
#if !defined(__EMSCRIPTEN__)
      boost::mutex::scoped_lock  lock_;
#endif
      
      std::string                id_;
      ParsedDicomFile*           file_;
      size_t                     fileSize_;

      std::unique_ptr<MemoryObjectCache::Accessor>  accessor_;
      
    public:
      Accessor(ParsedDicomCache& that,
               const std::string& id);

      bool IsValid() const;

      ParsedDicomFile& GetDicom() const;

      size_t GetFileSize() const;
    };
  };
}
