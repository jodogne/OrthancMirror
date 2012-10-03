#include "ZipWriter.h"

#include "../../Resources/minizip/zip.h"
#include <boost/date_time/posix_time/posix_time.hpp>

#include "../OrthancException.h"


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
  };

  ZipWriter::ZipWriter() : pimpl_(new PImpl)
  {
    compressionLevel_ = 6;
    hasFileInZip_ = false;

    pimpl_->file_ = NULL;
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
      throw OrthancException("Please call SetOutputPath() before creating the file");
    }

    hasFileInZip_ = false;
    pimpl_->file_ = zipOpen64(path_.c_str(), APPEND_STATUS_CREATE);
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

  void ZipWriter::SetCompressionLevel(uint8_t level)
  {
    if (level >= 10)
    {
      throw OrthancException("ZIP compression level must be between 0 (no compression) and 9 (highest compression");
    }

    compressionLevel_ = level;
  }

  void ZipWriter::CreateFileInZip(const char* path)
  {
    Open();

    zip_fileinfo zfi;
    PrepareFileInfo(zfi);

    if (zipOpenNewFileInZip64(pimpl_->file_, path,
                              &zfi,
                              NULL,   0,
                              NULL,   0,
                              "",  // Comment
                              Z_DEFLATED,
                              compressionLevel_, 1) != 0)
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
      throw OrthancException("Call first CreateFileInZip()");
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
}
