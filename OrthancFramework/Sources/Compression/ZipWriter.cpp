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

#include "ZipWriter.h"

#include <limits>
#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "../../Resources/ThirdParty/minizip/zip.h"
#include "../Logging.h"
#include "../OrthancException.h"
#include "../SystemToolbox.h"


static void PrepareFileInfo(zip_fileinfo& zfi)
{
  memset(&zfi, 0, sizeof(zfi));

  using namespace boost::posix_time;
  ptime now = second_clock::local_time();

  boost::gregorian::date today = now.date();
  ptime midnight(today);

  time_duration sinceMidnight = now - midnight;
  zfi.tmz_date.tm_sec = static_cast<unsigned int>(sinceMidnight.seconds());  // seconds after the minute - [0,59]
  zfi.tmz_date.tm_min = static_cast<unsigned int>(sinceMidnight.minutes());  // minutes after the hour - [0,59]
  zfi.tmz_date.tm_hour = static_cast<unsigned int>(sinceMidnight.hours());  // hours since midnight - [0,23]

  // http://www.boost.org/doc/libs/1_35_0/doc/html/boost/gregorian/greg_day.html
  zfi.tmz_date.tm_mday = today.day();  // day of the month - [1,31]

  // http://www.boost.org/doc/libs/1_35_0/doc/html/boost/gregorian/greg_month.html
  zfi.tmz_date.tm_mon = today.month() - 1;  // months since January - [0,11]

  // http://www.boost.org/doc/libs/1_35_0/doc/html/boost/gregorian/greg_year.html
  zfi.tmz_date.tm_year = today.year();  // years - [1980..2044]
}



namespace Orthanc
{
  ZipWriter::MemoryStream::MemoryStream(std::string& target) :
    target_(target),
    archiveSize_(0)
  {
  }

  
  void ZipWriter::MemoryStream::Write(const std::string& chunk)
  {
    chunked_.AddChunk(chunk);
    archiveSize_ += chunk.size();
  }
  
  
  uint64_t ZipWriter::MemoryStream::GetArchiveSize() const
  {
    return archiveSize_;
  }


  void ZipWriter::MemoryStream::Close()
  {
    chunked_.Flatten(target_);
  }
  

  void ZipWriter::BufferWithSeek::CheckInvariants() const
  {
#if !defined(NDEBUG)
    assert(chunks_.GetNumBytes() == 0 ||
           flattened_.empty());

    assert(currentPosition_ <= GetSize());
    
    if (currentPosition_ < GetSize())
    {
      assert(chunks_.GetNumBytes() == 0);
      assert(!flattened_.empty());
    }
#endif
  }
  

  ZipWriter::BufferWithSeek::BufferWithSeek() :
    currentPosition_(0)
  {
    CheckInvariants();
  }

  
  ZipWriter::BufferWithSeek::~BufferWithSeek()
  {
    CheckInvariants();
  }
  
  
  size_t ZipWriter::BufferWithSeek::GetPosition() const
  {
    return currentPosition_;
  }
  
  
  size_t ZipWriter::BufferWithSeek::GetSize() const
  {
    if (flattened_.empty())
    {
      return chunks_.GetNumBytes();
    }
    else
    {
      return flattened_.size();
    }
  }

  
  void ZipWriter::BufferWithSeek::Write(const void* data,
                                        size_t size)
  {
    CheckInvariants();

    if (size != 0)
    {
      if (currentPosition_ < GetSize())
      {
        if (currentPosition_ + size > flattened_.size())
        {
          throw OrthancException(ErrorCode_ParameterOutOfRange);
        }
        else
        {
          memcpy(&flattened_[currentPosition_], data, size);
        }
      }
      else
      {
        if (!flattened_.empty())
        {
          assert(chunks_.GetNumBytes() == 0);
          chunks_.AddChunk(flattened_);
          flattened_.clear();
        }
        
        chunks_.AddChunk(data, size);
      }

      currentPosition_ += size;
    }

    CheckInvariants();
  }

      
  void ZipWriter::BufferWithSeek::Write(const std::string& data)
  {
    if (!data.empty())
    {
      Write(data.c_str(), data.size());
    }
  }

      
  void ZipWriter::BufferWithSeek::Seek(size_t position)
  {
    CheckInvariants();

    if (currentPosition_ != position)
    {
      if (position < GetSize())
      {
        if (chunks_.GetNumBytes() != 0)
        {
          assert(flattened_.empty());
          chunks_.Flatten(flattened_);
        }

        assert(chunks_.GetNumBytes() == 0);
      }
      else if (position > GetSize())
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange);
      }

      currentPosition_ = position;
    }

    CheckInvariants();
  }
      

  void ZipWriter::BufferWithSeek::Flush(std::string& target)
  {
    CheckInvariants();

    if (flattened_.empty())
    {
      chunks_.Flatten(target);
    }
    else
    {
      flattened_.swap(target);
      flattened_.clear();
    }

    currentPosition_ = 0;

    CheckInvariants();
  }


  /**
   * Inside a ZIP archive, compressed files are concatenated, each
   * file being prefixed by its "Local file header". The ZIP archive
   * ends with the "central directory" structure.
   * https://en.wikipedia.org/wiki/ZIP_(file_format)
   * 
   * When writing one file, the minizip implementation first TELLS to
   * know the current size of the archive, then WRITES the header and
   * data bytes, then SEEKS backward to update the "local file header"
   * with info about the compressed data (at the 14 offset, containing
   * CRC-32, compressed size and uncompressed size), and finally SEEKS
   * to get back at the end of the stream in order to continue adding
   * files.
   * 
   * The minizip implementation will *never* SEEK *before* the "local
   * file header" of the current file. However, the current file must
   * *not* be immediately sent to the stream as new bytes are written,
   * because the "local file header" will be updated.
   *
   * Consequently, this buffer class only sends the pending bytes to
   * the output stream once it receives a SEEK command that moves the
   * cursor at the end of the archive. In the minizip implementation,
   * such a SEEK indicates that the current file has been properly
   * added to the archive.
   **/  
  class ZipWriter::StreamBuffer : public boost::noncopyable
  {
  private:
    IOutputStream&  stream_;
    bool            success_;
    ZPOS64_T        startCurrentFile_;
    BufferWithSeek  buffer_;
    
  public:
    explicit StreamBuffer(IOutputStream& stream) :
      stream_(stream),
      success_(true),
      startCurrentFile_(0)
    {
    }
    
    int Close()
    {
      try
      {
        if (success_)
        {
          std::string s;
          buffer_.Flush(s);
          stream_.Write(s);
        }
        
        return 0;
      }
      catch (...)
      {
        success_ = false;
        return 1;
      }
    }

    ZPOS64_T Tell() const
    {
      return startCurrentFile_ + static_cast<ZPOS64_T>(buffer_.GetPosition());
    }

    uLong Write(const void* buf,
                uLong size)
    {
      if (size == 0)
      {
        return 0;
      }
      else if (!success_)
      {
        return 0;  // Error
      }
      else
      {
        try
        {
          buffer_.Write(buf, size);
          return size;
        }
        catch (...)
        {
          return 0;
        }
      }
    }
    

    long Seek(ZPOS64_T offset,
              int origin)
    {
      try
      {
        if (origin == ZLIB_FILEFUNC_SEEK_SET &&
            offset >= startCurrentFile_ &&
            success_)
        {
          ZPOS64_T fullSize = startCurrentFile_ + static_cast<ZPOS64_T>(buffer_.GetSize());
          assert(offset <= fullSize);

          if (offset == fullSize)
          {
            // We can flush to the output stream
            std::string s;
            buffer_.Flush(s);
            stream_.Write(s);
            startCurrentFile_ = fullSize;
          }
          else
          {          
            buffer_.Seek(offset - startCurrentFile_);
          }
          
          return 0;  // OK
        }
        else
        {
          return 1;
        }
      }
      catch (...)
      {
        return 1;
      }
    }


    void Cancel()
    {
      success_ = false;
    }
    

    static int CloseWrapper(voidpf opaque,
                            voidpf stream)
    {
      assert(opaque != NULL);
      return reinterpret_cast<StreamBuffer*>(opaque)->Close();
    }

    static voidpf OpenWrapper(voidpf opaque,
                              const void* filename,
                              int mode)
    {
      assert(opaque != NULL);
      return opaque;
    }

    static long SeekWrapper(voidpf opaque,
                            voidpf stream,
                            ZPOS64_T offset,
                            int origin)
    {
      assert(opaque != NULL);
      return reinterpret_cast<StreamBuffer*>(opaque)->Seek(offset, origin);
    }

    static ZPOS64_T TellWrapper(voidpf opaque,
                                voidpf stream)
    {
      assert(opaque != NULL);
      return reinterpret_cast<StreamBuffer*>(opaque)->Tell();
    }

    static int TestErrorWrapper(voidpf opaque,
                                voidpf stream)
    {
      assert(opaque != NULL);
      return reinterpret_cast<StreamBuffer*>(opaque)->success_ ? 0 : 1;
    }

    static uLong WriteWrapper(voidpf opaque,
                              voidpf stream,
                              const void* buf,
                              uLong size)
    {
      assert(opaque != NULL);
      return reinterpret_cast<StreamBuffer*>(opaque)->Write(buf, size);
    }
  };
  

  struct ZipWriter::PImpl : public boost::noncopyable
  {
    zipFile file_;
    std::unique_ptr<StreamBuffer> streamBuffer_;
    uint64_t  archiveSize_;

    PImpl() :
      file_(NULL),
      archiveSize_(0)
    {
    }
  };

  ZipWriter::ZipWriter() :
    pimpl_(new PImpl),
    isZip64_(false),
    hasFileInZip_(false),
    append_(false),
    compressionLevel_(6)
  {
  }

  ZipWriter::~ZipWriter()
  {
    try
    {
      Close();
    }
    catch (OrthancException& e)  // Don't throw exceptions in destructors
    {
      LOG(ERROR) << "Caught exception in destructor: " << e.What();
    }
  }

  void ZipWriter::Close()
  {
    if (IsOpen())
    {
      zipClose(pimpl_->file_, "Created by Orthanc");
      pimpl_->file_ = NULL;
      hasFileInZip_ = false;

      pimpl_->streamBuffer_.reset(NULL);

      if (outputStream_.get() != NULL)
      {
        outputStream_->Close();
        pimpl_->archiveSize_ = outputStream_->GetArchiveSize();
        outputStream_.reset(NULL);
      }
    }
  }

  bool ZipWriter::IsOpen() const
  {
    return pimpl_->file_ != NULL;
  }

  void ZipWriter::Open()
  {
    if (IsOpen())
    {
      return;
    }
    else if (outputStream_.get() != NULL)
    {
      // New in Orthanc 1.9.4
      if (IsAppendToExisting())
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls, "Cannot append to output streams");
      }
      
      hasFileInZip_ = false;

      zlib_filefunc64_def funcs;
      memset(&funcs, 0, sizeof(funcs));

      pimpl_->streamBuffer_.reset(new StreamBuffer(*outputStream_));
      funcs.opaque = pimpl_->streamBuffer_.get();
      funcs.zclose_file = StreamBuffer::CloseWrapper;
      funcs.zerror_file = StreamBuffer::TestErrorWrapper;
      funcs.zopen64_file = StreamBuffer::OpenWrapper;
      funcs.ztell64_file = StreamBuffer::TellWrapper;
      funcs.zwrite_file = StreamBuffer::WriteWrapper;
      funcs.zseek64_file = StreamBuffer::SeekWrapper;

      /**
       * "funcs.zread_file" (ZREAD64) also appears in "minizip/zip.c",
       * but is only needed by function "LoadCentralDirectoryRecord()"
       * that is only used if appending new files to an already
       * existing ZIP, which makes no sense for an output stream.
       **/

      pimpl_->file_ = zipOpen2_64(NULL /* no output path */, APPEND_STATUS_CREATE,
                                  NULL /* global comment */, &funcs);

      if (!pimpl_->file_)
      {
        throw OrthancException(ErrorCode_CannotWriteFile,
                               "Cannot create new ZIP archive into an output stream");
      }
    }
    else if (path_.empty())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls,
                             "Please call SetOutputPath() before creating the file");
    }
    else
    {
      hasFileInZip_ = false;

      int mode = APPEND_STATUS_CREATE;
      if (append_ && 
          boost::filesystem::exists(path_))
      {
        mode = APPEND_STATUS_ADDINZIP;
      }

      if (isZip64_)
      {
        pimpl_->file_ = zipOpen64(path_.c_str(), mode);
      }
      else
      {
        pimpl_->file_ = zipOpen(path_.c_str(), mode);
      }

      if (!pimpl_->file_)
      {
        throw OrthancException(ErrorCode_CannotWriteFile,
                               "Cannot create new ZIP archive: " + path_);
      }
    }
  }

  void ZipWriter::SetOutputPath(const char* path)
  {
    Close();
    path_ = path;
  }

  const std::string &ZipWriter::GetOutputPath() const
  {
    return path_;
  }

  void ZipWriter::SetZip64(bool isZip64)
  {
    if (outputStream_.get() == NULL)
    {
      Close();
      isZip64_ = isZip64;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls,
                             "SetZip64() must be given to AcquireOutputStream()");
    }
  }

  void ZipWriter::SetCompressionLevel(uint8_t level)
  {
    if (level >= 10)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange,
                             "ZIP compression level must be between 0 (no compression) "
                             "and 9 (highest compression)");
    }
    else
    {
      compressionLevel_ = level;
    }
  }

  uint8_t ZipWriter::GetCompressionLevel() const
  {
    return compressionLevel_;
  }

  void ZipWriter::OpenFile(const char* path)
  {
    Open();

    zip_fileinfo zfi;
    PrepareFileInfo(zfi);

    int result;

    if (isZip64_)
    {
      result = zipOpenNewFileInZip64(pimpl_->file_, path,
                                     &zfi,
                                     NULL,   0,
                                     NULL,   0,
                                     "",  // Comment
                                     Z_DEFLATED,
                                     compressionLevel_, 1);
    }
    else
    {
      result = zipOpenNewFileInZip(pimpl_->file_, path,
                                   &zfi,
                                   NULL,   0,
                                   NULL,   0,
                                   "",  // Comment
                                   Z_DEFLATED,
                                   compressionLevel_);
    }

    if (result != 0)
    {
      throw OrthancException(ErrorCode_CannotWriteFile,
                             "Cannot add new file inside ZIP archive: " + std::string(path));
    }

    hasFileInZip_ = true;
  }


  void ZipWriter::Write(const std::string& data)
  {
    if (data.size())
    {
      Write(&data[0], data.size());
    }
  }


  void ZipWriter::Write(const void* data, size_t length)
  {
    if (!hasFileInZip_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls, "Call first OpenFile()");
    }

    const size_t maxBytesInAStep = std::numeric_limits<int32_t>::max();

    const char* p = reinterpret_cast<const char*>(data);
    
    while (length > 0)
    {
      int bytes = static_cast<int32_t>(length <= maxBytesInAStep ? length : maxBytesInAStep);

      if (zipWriteInFileInZip(pimpl_->file_, p, bytes))
      {
        throw OrthancException(ErrorCode_CannotWriteFile,
                               "Cannot write data to ZIP archive: " + path_);
      }
      
      p += bytes;
      length -= bytes;
    }
  }


  void ZipWriter::SetAppendToExisting(bool append)
  {
    Close();
    append_ = append;
  }

  bool ZipWriter::IsAppendToExisting() const
  {
    return append_;
  }

  bool ZipWriter::IsZip64() const
  {
    return isZip64_;
  }
  

  void ZipWriter::AcquireOutputStream(IOutputStream* stream,
                                      bool isZip64)
  {
    std::unique_ptr<IOutputStream> protection(stream);
    
    if (stream == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
    else
    {
      Close();
      path_.clear();
      isZip64_ = isZip64;
      outputStream_.reset(protection.release());
    }
  }


  void ZipWriter::SetMemoryOutput(std::string& target,
                                  bool isZip64)
  {
    AcquireOutputStream(new MemoryStream(target), isZip64);
  }


  void ZipWriter::CancelStream()
  {
    if (outputStream_.get() == NULL ||
        pimpl_->streamBuffer_.get() == NULL)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls, "Only applicable after AcquireOutputStream() and Open()");
    }
    else
    {
      pimpl_->streamBuffer_->Cancel();
    }
  }


  uint64_t ZipWriter::GetArchiveSize() const
  {
    if (outputStream_.get() != NULL)
    {
      return outputStream_->GetArchiveSize();
    }
    else if (path_.empty())
    {
      // This is the case after a call to "Close()"
      return pimpl_->archiveSize_;
    }
    else
    {
      return SystemToolbox::GetFileSize(path_);
    }
  }
}
