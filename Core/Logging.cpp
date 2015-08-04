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


#include "PrecompiledHeaders.h"
#include "Logging.h"

#if ORTHANC_ENABLE_LOGGING == 1


#if ORTHANC_ENABLE_GOOGLE_LOG == 1

/*********************************************************
 * Wrapper around Google Log
 *********************************************************/

namespace Orthanc
{  
  namespace Logging
  {
    void Initialize()
    {
      // Initialize Google's logging library.
      FLAGS_logtostderr = true;
      FLAGS_minloglevel = 1;   // Do not print LOG(INFO) by default
      FLAGS_v = 0;             // Do not print trace-level VLOG(1) by default

      google::InitGoogleLogging("Orthanc");
    }

    void Finalize()
    {
      google::ShutdownGoogleLogging();
    }

    void EnableInfoLevel(bool enabled)
    {
      FLAGS_minloglevel = (enabled ? 0 : 1);
    }

    void EnableTraceLevel(bool enabled)
    {
      if (enabled)
      {
        FLAGS_minloglevel = 0;
        FLAGS_v = 1;
      }
      else
      {
        FLAGS_v = 0;
      }
    }

    void SetTargetFolder(const std::string& path)
    {
      FLAGS_logtostderr = false;
      FLAGS_log_dir = path;
    }
  }
}

#else

/*********************************************************
 * Use internal logger, not Google Log
 *********************************************************/

#include "OrthancException.h"
#include "Toolbox.h"

#include <fstream>
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>

#if BOOST_HAS_DATE_TIME == 1
#  include <boost/date_time/posix_time/posix_time.hpp>
#else
#  error Boost::date_time is required
#endif


namespace
{
  struct NullStream : public std::ostream 
  {
    NullStream() : std::ios(0), std::ostream(0)
    {
    }
  };
}


static boost::mutex  mutex_;
static bool infoEnabled_ = false;
static bool traceEnabled_ = false;

static std::ostream* error_ = &std::cerr;
static std::ostream* warning_ = &std::cerr;
static std::ostream* info_ = &std::cerr;
static NullStream null_;

static std::auto_ptr<std::ofstream> errorFile_;
static std::auto_ptr<std::ofstream> warningFile_;
static std::auto_ptr<std::ofstream> infoFile_;

namespace Orthanc
{
  namespace Logging
  {
    static void GetLogPath(boost::filesystem::path& log,
                           boost::filesystem::path& link,
                           const char* level,
                           const std::string& directory)
    {
      /**
         From Google Log documentation:

         Unless otherwise specified, logs will be written to the filename
         "<program name>.<hostname>.<user name>.log.<severity level>.",
         followed by the date, time, and pid (you can't prevent the date,
         time, and pid from being in the filename).

         In this implementation : "hostname" and "username" are not used
      **/

      boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
      boost::filesystem::path root(directory);
      boost::filesystem::path exe(Toolbox::GetPathToExecutable());
      
      if (!boost::filesystem::exists(root) ||
          !boost::filesystem::is_directory(root))
      {
        throw OrthancException(ErrorCode_CannotWriteFile);
      }

      char date[64];
      sprintf(date, "%04d%02d%02d-%02d%02d%02d.%d",
              static_cast<int>(now.date().year()),
              now.date().month().as_number(),
              now.date().day().as_number(),
              now.time_of_day().hours(),
              now.time_of_day().minutes(),
              now.time_of_day().seconds(),
              Toolbox::GetProcessId());

      std::string programName = exe.filename().replace_extension("").string();

      log = (root / (programName + ".log." +
                     std::string(level) + "." +
                     std::string(date)));

      link = (root / (programName + "." + std::string(level)));
    }


    static void PrepareLogFile(std::ostream*& stream,
                               std::auto_ptr<std::ofstream>& file,
                               const char* level,
                               const std::string& directory)
    {
      boost::filesystem::path log, link;
      GetLogPath(log, link, level, directory);

      printf("[%s]\n", log.string().c_str());

#if !defined(_WIN32) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))
      boost::filesystem::remove(link);
      boost::filesystem::create_symlink(log.filename(), link);
#endif

      file.reset(new std::ofstream(log.c_str()));
      stream = file.get();
    }


    void Initialize()
    {
      infoEnabled_ = false;
      traceEnabled_ = false;
    }

    void Finalize()
    {
      errorFile_.reset(NULL);
      warningFile_.reset(NULL);
      infoFile_.reset(NULL);
    }

    void EnableInfoLevel(bool enabled)
    {
      boost::mutex::scoped_lock lock(mutex_);
      infoEnabled_ = enabled;
    }

    void EnableTraceLevel(bool enabled)
    {
      boost::mutex::scoped_lock lock(mutex_);
      traceEnabled_ = enabled;
      
      if (enabled)
      {
        // Also enable the "INFO" level when trace-level debugging is enabled
        infoEnabled_ = true;
      }
    }

    void SetTargetFolder(const std::string& path)
    {
      boost::mutex::scoped_lock lock(mutex_);
      PrepareLogFile(error_, errorFile_, "ERROR", path);
      PrepareLogFile(warning_, warningFile_, "WARNING", path);
      PrepareLogFile(info_, infoFile_, "INFO", path);
    }

    InternalLogger::InternalLogger(const char* level,
                                   const char* file,
                                   int line) : 
      lock_(mutex_)
    {
      char c;

      if (strcmp(level, "ERROR") == 0)
      {
        stream_ = error_;
        c = 'E';
      }
      else if (strcmp(level, "WARNING") == 0)
      {
        stream_ = warning_;
        c = 'W';
      }
      else if (strcmp(level, "INFO") == 0)
      {
        stream_ = infoEnabled_ ? info_ : &null_;
        c = 'I';
      }
      else if (strcmp(level, "TRACE") == 0)
      {
        stream_ = traceEnabled_ ? info_ : &null_;
        c = 'T';
      }
      else 
      {
        // Unknown logging level
        throw OrthancException(ErrorCode_InternalError);
      }

      if (stream_ != &null_)
      {
        boost::filesystem::path path(file);
        boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
        boost::posix_time::time_duration duration = now.time_of_day();

        /**
           From Google Log documentation:

           "Log lines have this form:

           Lmmdd hh:mm:ss.uuuuuu threadid file:line] msg...

           where the fields are defined as follows:

           L                A single character, representing the log level (eg 'I' for INFO)
           mm               The month (zero padded; ie May is '05')
           dd               The day (zero padded)
           hh:mm:ss.uuuuuu  Time in hours, minutes and fractional seconds
           threadid         The space-padded thread ID as returned by GetTID() (this matches the PID on Linux)
           file             The file name
           line             The line number
           msg              The user-supplied message"

           In this implementation, "threadid" is not printed.
         **/

        char date[32];
        sprintf(date, "%c%02d%02d %02d:%02d:%02d.%06d ", c, 
                now.date().month().as_number(),
                now.date().day().as_number(),
                duration.hours(),
                duration.minutes(),
                duration.seconds(),
                static_cast<int>(duration.fractional_seconds()));

        *stream_ << date << path.filename().string() << ":" << line << "] ";
      }
    }
  }
}

#endif


#endif   // ORTHANC_ENABLE_LOGGING
