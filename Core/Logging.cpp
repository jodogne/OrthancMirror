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

#if ORTHANC_ENABLE_LOGGING != 1

namespace Orthanc
{
  namespace Logging
  {
    void Initialize()
    {
    }

    void Finalize()
    {
    }

    void EnableInfoLevel(bool enabled)
    {
    }

    void EnableTraceLevel(bool enabled)
    {
    }

    void SetTargetFolder(const std::string& path)
    {
    }
  }
}

#elif ORTHANC_ENABLE_GOOGLE_LOG == 1

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
#include "Enumerations.h"
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
  struct LoggingState
  {
    bool infoEnabled_;
    bool traceEnabled_;

    std::ostream* error_;
    std::ostream* warning_;
    std::ostream* info_;

    std::auto_ptr<std::ofstream> errorFile_;
    std::auto_ptr<std::ofstream> warningFile_;
    std::auto_ptr<std::ofstream> infoFile_;

    LoggingState() : 
      infoEnabled_(false),
      traceEnabled_(false),
      error_(&std::cerr),
      warning_(&std::cerr),
      info_(&std::cerr)
    {
    }
  };
}



static std::auto_ptr<LoggingState> loggingState_;
static boost::mutex  loggingMutex_;



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

#if !defined(_WIN32) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))
      boost::filesystem::remove(link);
      boost::filesystem::create_symlink(log.filename(), link);
#endif

      file.reset(new std::ofstream(log.string().c_str()));
      stream = file.get();
    }


    void Initialize()
    {
      boost::mutex::scoped_lock lock(loggingMutex_);
      loggingState_.reset(new LoggingState);
    }

    void Finalize()
    {
      boost::mutex::scoped_lock lock(loggingMutex_);
      loggingState_.reset(NULL);
    }

    void EnableInfoLevel(bool enabled)
    {
      boost::mutex::scoped_lock lock(loggingMutex_);
      assert(loggingState_.get() != NULL);

      loggingState_->infoEnabled_ = enabled;
    }

    void EnableTraceLevel(bool enabled)
    {
      boost::mutex::scoped_lock lock(loggingMutex_);
      assert(loggingState_.get() != NULL);

      loggingState_->traceEnabled_ = enabled;
      
      if (enabled)
      {
        // Also enable the "INFO" level when trace-level debugging is enabled
        loggingState_->infoEnabled_ = true;
      }
    }

    void SetTargetFolder(const std::string& path)
    {
      boost::mutex::scoped_lock lock(loggingMutex_);
      assert(loggingState_.get() != NULL);

      PrepareLogFile(loggingState_->error_,   loggingState_->errorFile_,   "ERROR", path);
      PrepareLogFile(loggingState_->warning_, loggingState_->warningFile_, "WARNING", path);
      PrepareLogFile(loggingState_->info_,    loggingState_->infoFile_,    "INFO", path);
    }

    InternalLogger::InternalLogger(const char* level,
                                   const char* file,
                                   int line) : 
      lock_(loggingMutex_), 
      stream_(&null_)  // By default, logging to "/dev/null" is simulated
    {
      if (loggingState_.get() == NULL)
      {
        fprintf(stderr, "ERROR: Trying to log a message after the finalization of the logging engine\n");
        return;
      }

      LogLevel l = StringToLogLevel(level);
      
      if ((l == LogLevel_Info  && !loggingState_->infoEnabled_) ||
          (l == LogLevel_Trace && !loggingState_->traceEnabled_))
      {
        // This logging level is disabled, directly exit and unlock
        // the mutex to speed-up things. The stream is set to "/dev/null"
        lock_.unlock();
        return;
      }

      // Compute the header of the line, temporary release the lock as
      // this is a time-consuming operation
      lock_.unlock();
      std::string header;

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
        sprintf(date, "%c%02d%02d %02d:%02d:%02d.%06d ",
                level[0],
                now.date().month().as_number(),
                now.date().day().as_number(),
                duration.hours(),
                duration.minutes(),
                duration.seconds(),
                static_cast<int>(duration.fractional_seconds()));

        header = std::string(date) + path.filename().string() + ":" + boost::lexical_cast<std::string>(line) + "] ";
      }


      // The header is computed, we now re-lock the mutex to access
      // the stream objects. Pay attention that "loggingState_",
      // "infoEnabled_" or "traceEnabled_" might have changed while
      // the mutex was unlocked.
      lock_.lock();

      if (loggingState_.get() == NULL)
      {
        fprintf(stderr, "ERROR: Trying to log a message after the finalization of the logging engine\n");
        return;
      }

      switch (l)
      {
        case LogLevel_Error:
          stream_ = loggingState_->error_;
          break;

        case LogLevel_Warning:
          stream_ = loggingState_->warning_;
          break;

        case LogLevel_Info:
          if (loggingState_->infoEnabled_)
          {
            stream_ = loggingState_->info_;
          }

          break;

        case LogLevel_Trace:
          if (loggingState_->traceEnabled_)
          {
            stream_ = loggingState_->info_;
          }

          break;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }

      if (stream_ == &null_)
      {
        // The logging is disabled for this level. The stream is the
        // "null_" member of this object, so we can release the global
        // mutex.
        lock_.unlock();
      }

      (*stream_) << header;
    }


    InternalLogger::~InternalLogger()
    {
      if (stream_ != &null_)
      {
#if defined(_WIN32)
        *stream_ << "\r\n";
#else
        *stream_ << "\n";
#endif

        stream_->flush();
      }
    }
      

  }
}

#endif   // ORTHANC_ENABLE_LOGGING
