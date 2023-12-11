/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include "../Cache/MemoryStringCache.h"

#include "../Compatibility.h"  // For ORTHANC_OVERRIDE

#include <boost/thread/mutex.hpp>
#include <map>

namespace Orthanc
{
   /**
   *  Note: this class is thread safe
   **/
   class ORTHANC_PUBLIC StorageCache : public boost::noncopyable
    {
    public:

      // The StorageCache is only accessible through this accessor.
      // It will make sure that only one user will fill load data and fill
      // the cache if multiple users try to access the same item at the same time.
      // This scenario happens a lot when multiple workers from a viewer access 
      // the same file.
      class Accessor : public MemoryStringCache::Accessor
      {
        StorageCache& storageCache_;
      public:
        explicit Accessor(StorageCache& cache);

        void Add(const std::string& uuid, 
                 FileContentType contentType,
                 const std::string& value);

        void AddStartRange(const std::string& uuid, 
                           FileContentType contentType,
                           const std::string& value);

        void Add(const std::string& uuid, 
                 FileContentType contentType,
                 const void* buffer,
                 size_t size);

        bool Fetch(std::string& value, 
                   const std::string& uuid,
                   FileContentType contentType);

        bool FetchStartRange(std::string& value, 
                             const std::string& uuid,
                             FileContentType contentType,
                             uint64_t end /* exclusive */);

        bool FetchTranscodedInstance(std::string& value, 
                                     const std::string& uuid,
                                     DicomTransferSyntax targetSyntax);

        void AddTranscodedInstance(const std::string& uuid,
                                   DicomTransferSyntax targetSyntax,
                                   const void* buffer,
                                   size_t size);
      };

    private:
      MemoryStringCache             cache_;
      std::set<DicomTransferSyntax> subKeysTransferSyntax_;
      boost::mutex                  subKeysMutex_;

    public:
      void SetMaximumSize(size_t size);

      void Invalidate(const std::string& uuid,
                      FileContentType contentType);

      size_t GetCurrentSize() const;
      
      size_t GetNumberOfItems() const;

    private:
      void Add(const std::string& uuid, 
               FileContentType contentType,
               const std::string& value);

      void AddStartRange(const std::string& uuid, 
                         FileContentType contentType,
                         const std::string& value);

      void Add(const std::string& uuid, 
               FileContentType contentType,
               const void* buffer,
               size_t size);

      bool Fetch(std::string& value, 
                 const std::string& uuid,
                 FileContentType contentType);

      bool FetchStartRange(std::string& value, 
                           const std::string& uuid,
                           FileContentType contentType,
                           uint64_t end /* exclusive */);

    };
}
