/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
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
  zfi.tmz_date.tm_sec = sinceMidnight.seconds();  // seconds after the minute - [0,59]
  zfi.tmz_date.tm_min = sinceMidnight.minutes();  // minutes after the hour - [0,59]
  zfi.tmz_date.tm_hour = sinceMidnight.hours();  // hours since midnight - [0,23]

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
      LOG(ERROR) << "Please call SetOutputPath() before creating the file";
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
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
      throw OrthancException(ErrorCode_CannotWriteFile);
    }
  }

  void ZipWriter::SetOutputPath(const char* path)
  {
    Close();
    path_ = path;
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
      LOG(ERROR) << "ZIP compression level must be between 0 (no compression) and 9 (highest compression)";
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    Close();
    compressionLevel_ = level;
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
      throw OrthancException(ErrorCode_CannotWriteFile);
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


  void ZipWriter::Write(const char* data, size_t length)
  {
    if (!hasFileInZip_)
    {
      LOG(ERROR) << "Call first OpenFile()";
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    const size_t maxBytesInAStep = std::numeric_limits<int32_t>::max();

    while (length > 0)
    {
      int bytes = static_cast<int32_t>(length <= maxBytesInAStep ? length : maxBytesInAStep);

      if (zipWriteInFileInZip(pimpl_->file_, data, bytes))
      {
        throw OrthancException(ErrorCode_CannotWriteFile);
      }
      
      data += bytes;
      length -= bytes;
    }
  }


  void ZipWriter::SetAppendToExisting(bool append)
  {
    Close();
    append_ = append;
  }
    

}
