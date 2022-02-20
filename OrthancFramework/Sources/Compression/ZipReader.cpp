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

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "ZipReader.h"

#include "../OrthancException.h"
#include "../../Resources/ThirdParty/minizip/unzip.h"

#if ORTHANC_SANDBOXED != 1
#  include "../SystemToolbox.h"
#endif


/**
 * I have not been able to correctly define "ssize_t" on all versions
 * of Visual Studio. As a consequence, I preferred to switch "ssize_t"
 * to "SSIZE_T", that is properly defined on both MSVC 2008 and 2015.
 * I define the macro "SSIZE_T" as an alias to "ssize_t" on
 * POSIX-compliant platforms that wouldn't have "SSIZE_T" defined.
 **/
#if defined(_MSC_VER)
#  include <BaseTsd.h>   // Definition of SSIZE_T
#else
#  if !defined(SSIZE_T)
typedef ssize_t SSIZE_T;
#  endif
#endif

#include <string.h>


namespace Orthanc
{
  // ZPOS64_T corresponds to "uint64_t"
  
  class ZipReader::MemoryBuffer : public boost::noncopyable
  {
  private:
    const uint8_t*  content_;
    size_t          size_;
    size_t          pos_;

  public:
    MemoryBuffer(const void* p,
                 size_t size) :
      content_(reinterpret_cast<const uint8_t*>(p)),
      size_(size),
      pos_(0)
    {
    }
  
    explicit MemoryBuffer(const std::string& s) :
      content_(s.empty() ? NULL : reinterpret_cast<const uint8_t*>(s.c_str())),
      size_(s.size()),
      pos_(0)
    {
    }

    // Returns the number of bytes actually read
    uLong Read(void *target,
               uLong size)
    {
      if (size <= 0)
      {
        return 0;
      }
      else
      {
        size_t s = static_cast<size_t>(size);
        if (s + pos_ > size_)
        {
          s = size_ - pos_;
        }

        if (s != 0)
        {
          memcpy(target, content_ + pos_, s);
        }
      
        pos_ += s;
        return static_cast<uLong>(s);
      }             
    }

    ZPOS64_T Tell() const
    {
      return static_cast<ZPOS64_T>(pos_);
    }

    long Seek(ZPOS64_T offset,
              int origin)
    {
      SSIZE_T next;
    
      switch (origin)
      {
        case ZLIB_FILEFUNC_SEEK_CUR:
          next = static_cast<SSIZE_T>(offset) + static_cast<SSIZE_T>(pos_);
          break;

        case ZLIB_FILEFUNC_SEEK_SET:
          next = static_cast<SSIZE_T>(offset);
          break;

        case ZLIB_FILEFUNC_SEEK_END:
          next = static_cast<SSIZE_T>(offset) + static_cast<SSIZE_T>(size_);
          break;

        default:  // Should never occur
          return 1;  // Error
      }

      if (next < 0)
      {
        pos_ = 0;
      }
      else if (next >= static_cast<long>(size_))
      {
        pos_ = size_;
      }
      else
      {
        pos_ = static_cast<long>(next);
      }

      return 0;
    }


    static voidpf OpenWrapper(voidpf opaque,
                              const void* filename,
                              int mode)
    {
      // Don't return NULL to make "unzip.c" happy
      return opaque;
    }

    static uLong ReadWrapper(voidpf opaque,
                             voidpf stream,
                             void* buf,
                             uLong size)
    {
      assert(opaque != NULL);
      return reinterpret_cast<MemoryBuffer*>(opaque)->Read(buf, size);
    }

    static ZPOS64_T TellWrapper(voidpf opaque,
                                voidpf stream)
    {
      assert(opaque != NULL);
      return reinterpret_cast<MemoryBuffer*>(opaque)->Tell();
    }

    static long SeekWrapper(voidpf opaque,
                            voidpf stream,
                            ZPOS64_T offset,
                            int origin)
    {
      assert(opaque != NULL);
      return reinterpret_cast<MemoryBuffer*>(opaque)->Seek(offset, origin);
    }

    static int CloseWrapper(voidpf opaque,
                            voidpf stream)
    {
      return 0;
    }

    static int TestErrorWrapper(voidpf opaque,
                                voidpf stream)
    {
      return 0;  // ??
    }
  };



  ZipReader* ZipReader::CreateFromMemory(const std::string& buffer)
  {
    if (buffer.empty())
    {
      return CreateFromMemory(NULL, 0);
    }
    else
    {
      return CreateFromMemory(buffer.c_str(), buffer.size());
    }
  }


  bool ZipReader::IsZipMemoryBuffer(const void* buffer,
                                    size_t size)
  {
    if (size < 4)
    {
      return false;
    }
    else
    {
      const uint8_t* c = reinterpret_cast<const uint8_t*>(buffer);
      return (c[0] == 0x50 &&  // 'P'
              c[1] == 0x4b &&  // 'K'
              ((c[2] == 0x03 && c[3] == 0x04) ||
               (c[2] == 0x05 && c[3] == 0x06) ||
               (c[2] == 0x07 && c[3] == 0x08)));
    }
  }
  

  bool ZipReader::IsZipMemoryBuffer(const std::string& content)
  {
    if (content.empty())
    {
      return false;
    }
    else
    {
      return IsZipMemoryBuffer(content.c_str(), content.size());
    }
  }


#if ORTHANC_SANDBOXED != 1
  bool ZipReader::IsZipFile(const std::string& path)
  {
    std::string content;
    SystemToolbox::ReadFileRange(content, path, 0, 4,
                                 false /* don't throw if file is too small */);

    return IsZipMemoryBuffer(content);
  }
#endif


  struct ZipReader::PImpl
  {
    unzFile                       unzip_;
    std::unique_ptr<MemoryBuffer> reader_;
    bool                          done_;

    PImpl() :
      unzip_(NULL),
      done_(true)
    {
    }
  };


  ZipReader::ZipReader() :
    pimpl_(new PImpl)
  {
  }

  
  ZipReader::~ZipReader()
  {
    if (pimpl_->unzip_ != NULL)
    {
      unzClose(pimpl_->unzip_);
      pimpl_->unzip_ = NULL;
    }
  }

  
  uint64_t ZipReader::GetFilesCount() const
  {
    assert(pimpl_->unzip_ != NULL);
    
    unz_global_info64_s info;
    
    if (unzGetGlobalInfo64(pimpl_->unzip_, &info) == 0)
    {
      return info.number_entry;
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
  }

  
  void ZipReader::SeekFirst()
  {
    assert(pimpl_->unzip_ != NULL);    
    pimpl_->done_ = (unzGoToFirstFile(pimpl_->unzip_) != 0);
  }


  bool ZipReader::ReadNextFile(std::string& filename,
                               std::string& content)
  {
    assert(pimpl_->unzip_ != NULL);

    if (pimpl_->done_)
    {
      return false;
    }
    else
    {
      unz_file_info64_s info;
      if (unzGetCurrentFileInfo64(pimpl_->unzip_, &info, NULL, 0, NULL, 0, NULL, 0) != 0)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }

      filename.resize(info.size_filename);
      if (!filename.empty() &&
          unzGetCurrentFileInfo64(pimpl_->unzip_, &info, &filename[0],
                                  static_cast<uLong>(filename.size()), NULL, 0, NULL, 0) != 0)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }

      content.resize(info.uncompressed_size);

      if (!content.empty())
      {
        if (unzOpenCurrentFile(pimpl_->unzip_) == 0)
        {
          bool success = (unzReadCurrentFile(pimpl_->unzip_, &content[0],
                                             static_cast<uLong>(content.size())) != 0);
                          
          if (unzCloseCurrentFile(pimpl_->unzip_) != 0 ||
              !success)
          {
            throw OrthancException(ErrorCode_BadFileFormat);
          }
        }
        else
        {
          throw OrthancException(ErrorCode_BadFileFormat, "Invalid file or unsupported compression method (e.g. Deflate64)");
        }
      }
      
      pimpl_->done_ = (unzGoToNextFile(pimpl_->unzip_) != 0);
 
      return true;
    }
  }    

  
  ZipReader* ZipReader::CreateFromMemory(const void* buffer,
                                         size_t size)
  {
    if (!IsZipMemoryBuffer(buffer, size))
    {
      throw OrthancException(ErrorCode_BadFileFormat, "The memory buffer doesn't contain a ZIP archive");
    }
    else
    {
      std::unique_ptr<ZipReader> reader(new ZipReader);

      reader->pimpl_->reader_.reset(new MemoryBuffer(buffer, size));
      if (reader->pimpl_->reader_.get() == NULL)
      {
        throw OrthancException(ErrorCode_InternalError);
      }
    
      zlib_filefunc64_def funcs;
      memset(&funcs, 0, sizeof(funcs));

      funcs.opaque = reader->pimpl_->reader_.get();
      funcs.zopen64_file = MemoryBuffer::OpenWrapper;
      funcs.zread_file = MemoryBuffer::ReadWrapper;
      funcs.ztell64_file = MemoryBuffer::TellWrapper;
      funcs.zseek64_file = MemoryBuffer::SeekWrapper;
      funcs.zclose_file = MemoryBuffer::CloseWrapper;
      funcs.zerror_file = MemoryBuffer::TestErrorWrapper;

      reader->pimpl_->unzip_ = unzOpen2_64(NULL, &funcs);
      if (reader->pimpl_->unzip_ == NULL)
      {
        throw OrthancException(ErrorCode_BadFileFormat, "Cannot open ZIP archive from memory buffer");
      }
      else
      {
        reader->SeekFirst();
        return reader.release();
      }
    }
  }
  

#if ORTHANC_SANDBOXED != 1
  ZipReader* ZipReader::CreateFromFile(const std::string& path)
  {
    if (!IsZipFile(path))
    {
      throw OrthancException(ErrorCode_BadFileFormat, "The file doesn't contain a ZIP archive: " + path);
    }
    else
    {
      std::unique_ptr<ZipReader> reader(new ZipReader);

      reader->pimpl_->unzip_ = unzOpen64(path.c_str());
      if (reader->pimpl_->unzip_ == NULL)
      {
        throw OrthancException(ErrorCode_BadFileFormat, "Cannot open ZIP archive from file: " + path);
      }
      else
      {
        reader->SeekFirst();
        return reader.release();
      }
    }
  }
#endif
}
