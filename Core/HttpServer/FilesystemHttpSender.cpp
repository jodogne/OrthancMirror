/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
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
#include "FilesystemHttpSender.h"

#include "../Toolbox.h"
#include "../OrthancException.h"
#include "../Compression/ZlibCompressor.h"

#include <stdio.h>


static const size_t  CHUNK_SIZE = 64 * 1024;   // Use 64KB chunks

namespace Orthanc
{
  void FilesystemHttpSender::Initialize(const boost::filesystem::path& path)
  {
    sourceCompression_ = CompressionType_None;
    targetCompression_ = HttpCompression_None;

    SetContentFilename(path.filename().string());
    file_.open(path.string().c_str(), std::ifstream::binary);

    if (!file_.is_open())
    {
      throw OrthancException(ErrorCode_InexistentFile);
    }

    file_.seekg(0, file_.end);
    size_ = file_.tellg();
    file_.seekg(0, file_.beg);
  }


  HttpCompression FilesystemHttpSender::SetupHttpCompression(bool gzipAllowed, 
                                                             bool deflateAllowed)
  {
    switch (sourceCompression_)
    {
      case CompressionType_None:
      {
        return HttpCompression_None;
      }

      case CompressionType_ZlibWithSize:
      {
        if (size_ == 0)
        {
          return HttpCompression_None;
        }

        if (size_ < sizeof(uint64_t))
        {
          throw OrthancException(ErrorCode_CorruptedFile);
        }

        if (deflateAllowed)
        {
          file_.seekg(sizeof(uint64_t), file_.beg);
          size_ -= sizeof(uint64_t);
          return HttpCompression_Deflate;
        }
        else
        {
          uncompressed_.reset(new BufferHttpSender);

          // TODO Stream-based uncompression
          assert(size_ != 0);
          std::string compressed;
          compressed.resize(size_);

          file_.read(&compressed[0], size_);
          if ((file_.flags() & std::istream::failbit) ||
              !(file_.flags() & std::istream::eofbit) ||
              file_.gcount() < 0 ||
              static_cast<uint64_t>(file_.gcount()) != size_)
          {
            throw OrthancException(ErrorCode_CorruptedFile);
          }
          
          ZlibCompressor compressor;
          IBufferCompressor::Uncompress(uncompressed_->GetBuffer(), compressor, compressed);

          return HttpCompression_None;
        }

        break;
      }

      default:
        throw OrthancException(ErrorCode_NotImplemented);
    }
  }


  uint64_t FilesystemHttpSender::GetContentLength()
  {
    if (uncompressed_.get() != NULL)
    {
      return uncompressed_->GetContentLength();
    }
    else
    {
      return size_;
    }
  }


  bool FilesystemHttpSender::ReadNextChunk()
  {
    if (uncompressed_.get() != NULL)
    {
      return uncompressed_->ReadNextChunk();
    }
    else
    {
      if (chunk_.size() == 0)
      {
        chunk_.resize(CHUNK_SIZE);
      }

      file_.read(&chunk_[0], chunk_.size());

      if (file_.flags() & std::istream::failbit)
      {
        throw OrthancException(ErrorCode_CorruptedFile);
      }

      chunkSize_ = file_.gcount();

      return chunkSize_ > 0;
    }
  }


  const char* FilesystemHttpSender::GetChunkContent()
  {
    if (uncompressed_.get() != NULL)
    {
      return uncompressed_->GetChunkContent();
    }
    else
    {
      return chunk_.c_str();
    }
  }

  
  size_t FilesystemHttpSender::GetChunkSize()
  {
    if (uncompressed_.get() != NULL)
    {
      return uncompressed_->GetChunkSize();
    }
    else
    {
      return chunkSize_;
    }
  }
}
