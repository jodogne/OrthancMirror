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

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "ZipWriter.h"

#include <limits>
#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "../../Resources/ThirdParty/minizip/zip.h"
#include "../OrthancException.h"
#include "../Logging.h"


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
  struct ZipWriter::PImpl
  {
    zipFile file_;

    PImpl() : file_(NULL)
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
    Close();
  }

  void ZipWriter::Close()
  {
    if (IsOpen())
    {
      zipClose(pimpl_->file_, "Created by Orthanc");
      pimpl_->file_ = NULL;
      hasFileInZip_ = false;
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

    if (path_.size() == 0)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls,
                             "Please call SetOutputPath() before creating the file");
    }

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
    Close();
    isZip64_ = isZip64;
  }

  void ZipWriter::SetCompressionLevel(uint8_t level)
  {
    if (level >= 10)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange,
                             "ZIP compression level must be between 0 (no compression) "
                             "and 9 (highest compression)");
    }

    Close();
    compressionLevel_ = level;
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
}
