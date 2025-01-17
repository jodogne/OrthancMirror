/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#if !defined(ORTHANC_SANDBOXED)
#  error The macro ORTHANC_SANDBOXED must be defined
#endif

#if ORTHANC_SANDBOXED == 1
#  error The class StorageAccessor cannot be used in sandboxed environments
#endif

#if !defined(ORTHANC_ENABLE_CIVETWEB)
#  error Macro ORTHANC_ENABLE_CIVETWEB must be defined to use this file
#endif

#if !defined(ORTHANC_ENABLE_MONGOOSE)
#  error Macro ORTHANC_ENABLE_MONGOOSE must be defined to use this file
#endif

#include "IStorageArea.h"
#include "FileInfo.h"

#if ORTHANC_ENABLE_CIVETWEB == 1 || ORTHANC_ENABLE_MONGOOSE == 1
#  include "../HttpServer/BufferHttpSender.h"
#  include "../RestApi/RestApiOutput.h"
#endif

#include <vector>
#include <string>
#include <boost/noncopyable.hpp>
#include <stdint.h>

namespace Orthanc
{
  class MetricsRegistry;
  class StorageCache;

  /**
   * This class handles the compression/decompression of the raw files
   * contained in the storage area, and monitors timing metrics (if
   * enabled).
   **/
  class ORTHANC_PUBLIC StorageAccessor : boost::noncopyable
  {
  public:
    class ORTHANC_PUBLIC Range
    {
    private:
      bool      hasStart_;
      uint64_t  start_;
      bool      hasEnd_;
      uint64_t  end_;

      void SanityCheck() const;

    public:
      Range();

      void SetStartInclusive(uint64_t start);

      void SetEndInclusive(uint64_t end);

      bool HasStart() const
      {
        return hasStart_;
      }

      bool HasEnd() const
      {
        return hasEnd_;
      }

      uint64_t GetStartInclusive() const;

      uint64_t GetEndInclusive() const;

      std::string FormatHttpContentRange(uint64_t fullSize) const;

      void Extract(std::string& target,
                   const std::string& source) const;

      uint64_t GetContentLength(uint64_t fullSize) const;

      static Range ParseHttpRange(const std::string& s);
    };

  private:
    class MetricsTimer;

    IStorageArea&     area_;
    StorageCache*     cache_;
    MetricsRegistry*  metrics_;

#if ORTHANC_ENABLE_CIVETWEB == 1 || ORTHANC_ENABLE_MONGOOSE == 1
    void SetupSender(BufferHttpSender& sender,
                     const FileInfo& info,
                     const std::string& mime);
#endif

  public:
    explicit StorageAccessor(IStorageArea& area);

    StorageAccessor(IStorageArea& area,
                    StorageCache& cache);

    StorageAccessor(IStorageArea& area,
                    MetricsRegistry& metrics);

    StorageAccessor(IStorageArea& area,
                    StorageCache& cache,
                    MetricsRegistry& metrics);

    FileInfo Write(const void* data,
                   size_t size,
                   FileContentType type,
                   CompressionType compression,
                   bool storeMd5);

    FileInfo Write(const std::string& data,
                   FileContentType type,
                   CompressionType compression,
                   bool storeMd5);

    void Read(std::string& content,
              const FileInfo& info);

    void ReadRaw(std::string& content,
                 const FileInfo& info);

    void ReadStartRange(std::string& target,
                        const FileInfo& info,
                        uint64_t end /* exclusive */);

    void Remove(const std::string& fileUuid,
                FileContentType type);

    void Remove(const FileInfo& info);

    void ReadRange(std::string& target,
                   const FileInfo& info,
                   const Range& range,
                   bool uncompressIfNeeded);

#if ORTHANC_ENABLE_CIVETWEB == 1 || ORTHANC_ENABLE_MONGOOSE == 1
    void AnswerFile(HttpOutput& output,
                    const FileInfo& info,
                    MimeType mime);

    void AnswerFile(HttpOutput& output,
                    const FileInfo& info,
                    const std::string& mime);

    void AnswerFile(RestApiOutput& output,
                    const FileInfo& info,
                    MimeType mime);

    void AnswerFile(RestApiOutput& output,
                    const FileInfo& info,
                    const std::string& mime);
#endif
  private:
    void ReadStartRangeInternal(std::string& target,
                                const FileInfo& info,
                                uint64_t end /* exclusive */);

    void ReadWholeInternal(std::string& content,
                           const FileInfo& info);

    void ReadRawInternal(std::string& content,
                         const FileInfo& info);

  };
}
