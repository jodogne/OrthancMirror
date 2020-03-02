/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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

    void Reset()
    {
    }

    void Flush()
    {
    }

    void EnableInfoLevel(bool enabled)
    {
    }

    void EnableTraceLevel(bool enabled)
    {
    }
    
    bool IsTraceLevelEnabled()
    {
      return false;
    }

    bool IsInfoLevelEnabled()
    {
      return false;
    }
    
    void SetTargetFile(const std::string& path)
    {
    }

    void SetTargetFolder(const std::string& path)
    {
    }
  }
}


#elif ORTHANC_ENABLE_LOGGING_PLUGIN == 1

/*********************************************************
 * Logger compatible with the Orthanc plugin SDK
 *********************************************************/

#include <boost/lexical_cast.hpp>
#include <sstream>

namespace Orthanc
{
  namespace Logging
  {
    static OrthancPluginContext* context_ = NULL;

    void Initialize(OrthancPluginContext* context)
    {
      context_ = context;
    }

    InternalLogger::InternalLogger(InternalLevel level,
                                   const char* file  /* ignored */,
                                   int line  /* ignored */) :
      level_(level)
    {
    }

    InternalLogger::~InternalLogger()
    {
      std::string message = messageStream_.str();
      if (context_ != NULL)
      {
        switch (level_)
        {
          case InternalLevel_ERROR:
            OrthancPluginLogError(context_, message.c_str());
            break;

          case InternalLevel_WARNING:
            OrthancPluginLogWarning(context_, message.c_str());
            break;

          case InternalLevel_INFO:
            OrthancPluginLogInfo(context_, message.c_str());
            break;

          case InternalLevel_TRACE:
            // Not used by plugins
            break;

          default:
          {
            std::string s = ("Unknown log level (" + boost::lexical_cast<std::string>(level_) +
                             ") for message: " + message);
            OrthancPluginLogError(context_, s.c_str());
            break;
          }
        }
      }
    }
  }
}


#elif ORTHANC_ENABLE_LOGGING_STDIO == 1

/*********************************************************
 * Logger compatible with <stdio.h> OR logger that sends its
 * output to the emscripten html5 api (depending on the 
 * definition of __EMSCRIPTEN__)
 *********************************************************/

#include <stdio.h>
#include <boost/lexical_cast.hpp>

#ifdef __EMSCRIPTEN__
#  include <emscripten/html5.h>
#endif

namespace Orthanc
{
  namespace Logging
  {
    static bool globalVerbose_ = false;
    static bool globalTrace_ = false;
    
#ifdef __EMSCRIPTEN__
    void defaultErrorLogFunc(const char* msg)
    {
      emscripten_console_error(msg);
    }

    void defaultWarningLogFunc(const char* msg)
    {
      emscripten_console_warn(msg);
    }

    void defaultInfoLogFunc(const char* msg)
    {
      emscripten_console_log(msg);
    }

    void defaultTraceLogFunc(const char* msg)
    {
      emscripten_console_log(msg);
    }
#else
// __EMSCRIPTEN__ not #defined
    void defaultErrorLogFunc(const char* msg)
    {
      fprintf(stderr, "E: %s\n", msg);
    }

    void defaultWarningLogFunc(const char*)
    {
      fprintf(stdout, "W: %s\n", msg);
    }

    void defaultInfoLogFunc(const char*)
    {
      fprintf(stdout, "I: %s\n", msg);
    }

    void defaultTraceLogFunc(const char*)
    {
      fprintf(stdout, "T: %s\n", msg);
    }
#endif 
// __EMSCRIPTEN__

    static LoggingFunction globalErrorLogFunc = defaultErrorLogFunc;
    static LoggingFunction globalWarningLogFunc = defaultWarningLogFunc;
    static LoggingFunction globalInfoLogFunc = defaultInfoLogFunc;
    static LoggingFunction globalTraceLogFunc = defaultTraceLogFunc;

    void SetErrorWarnInfoTraceLoggingFunctions(
      LoggingFunction errorLogFunc,
      LoggingFunction warningLogfunc,
      LoggingFunction infoLogFunc,
      LoggingFunction traceLogFunc)
    {
      globalErrorLogFunc = errorLogFunc;
      globalWarningLogFunc = warningLogfunc;
      globalInfoLogFunc = infoLogFunc;
      globalTraceLogFunc = traceLogFunc;
    }

    InternalLogger::InternalLogger(InternalLevel level,
                                   const char* file  /* ignored */,
                                   int line  /* ignored */) :
      level_(level)
    {
    }

    InternalLogger::~InternalLogger()
    {
      std::string message = messageStream_.str();

      switch (level_)
      {
        case InternalLevel_ERROR:
          globalErrorLogFunc(message.c_str());
          break;

        case InternalLevel_WARNING:
          globalWarningLogFunc(message.c_str());
          break;

        case InternalLevel_INFO:
          if (globalVerbose_)
          {
            globalInfoLogFunc(message.c_str());
            // TODO: stone_console_info(message_.c_str());
          }
          break;

        case InternalLevel_TRACE:
          if (globalTrace_)
          {
            globalTraceLogFunc(message.c_str());
          }
          break;

        default:
        {
          std::stringstream ss;
          ss << "Unknown log level (" << level_ << ") for message: " << message;
          std::string s = ss.str();
          globalErrorLogFunc(s.c_str());
        }
      }
    }

    void Initialize()
    {
    }

    void EnableInfoLevel(bool enabled)
    {
      globalVerbose_ = enabled;
    }

    bool IsInfoLevelEnabled()
    {
      return globalVerbose_;
    }

    void EnableTraceLevel(bool enabled)
    {
      globalTrace_ = enabled;
    }

    bool IsTraceLevelEnabled()
    {
      return globalTrace_;
    }

  }
}


#else  /* ORTHANC_ENABLE_LOGGING_PLUGIN == 0 && 
          ORTHANC_ENABLE_LOGGING_STDIO == 0 && 
          ORTHANC_ENABLE_LOGGING == 1 */

/*********************************************************
 * Internal logger of Orthanc, that mimics some
 * behavior from Google Log.
 *********************************************************/

#include "Compatibility.h"
#include "OrthancException.h"
#include "Enumerations.h"
#include "Toolbox.h"

#if ORTHANC_SANDBOXED == 1
#  include <stdio.h>
#else
#  include "SystemToolbox.h"
#endif

#include <fstream>
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>


namespace
{
  struct LoggingContext
  {
    bool infoEnabled_;
    bool traceEnabled_;
    std::string  targetFile_;
    std::string  targetFolder_;

    std::ostream* error_;
    std::ostream* warning_;
    std::ostream* info_;

    std::unique_ptr<std::ofstream> file_;

    LoggingContext() : 
      infoEnabled_(false),
      traceEnabled_(false),
      error_(&std::cerr),
      warning_(&std::cerr),
      info_(&std::cerr)
    {
    }
  };

  struct LoggingMementoImpl
  {
    bool valid_;
    bool infoEnabled_;
    bool traceEnabled_;
    std::string  targetFile_;
    std::string  targetFolder_;

    std::ostream* error_;
    std::ostream* warning_;
    std::ostream* info_;
  };
}



static std::unique_ptr<LoggingContext> loggingContext_;
static boost::mutex  loggingMutex_;



namespace Orthanc
{
  namespace Logging
  {
    static void GetLogPath(boost::filesystem::path& log,
                           boost::filesystem::path& link,
                           const std::string& suffix,
                           const std::string& directory)
    {
      /**
         From Google Log documentation:

         Unless otherwise specified, logs will be written to the filename
         "<program name>.<hostname>.<user name>.log<suffix>.",
         followed by the date, time, and pid (you can't prevent the date,
         time, and pid from being in the filename).

         In this implementation : "hostname" and "username" are not used
      **/

      boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
      boost::filesystem::path root(directory);
      boost::filesystem::path exe(SystemToolbox::GetPathToExecutable());
      
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
              static_cast<int>(now.time_of_day().hours()),
              static_cast<int>(now.time_of_day().minutes()),
              static_cast<int>(now.time_of_day().seconds()),
              SystemToolbox::GetProcessId());

      std::string programName = exe.filename().replace_extension("").string();

      log = (root / (programName + ".log" + suffix + "." + std::string(date)));
      link = (root / (programName + ".log" + suffix));
    }


    static void PrepareLogFolder(std::unique_ptr<std::ofstream>& file,
                                 const std::string& suffix,
                                 const std::string& directory)
    {
      boost::filesystem::path log, link;
      GetLogPath(log, link, suffix, directory);

#if !defined(_WIN32) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))
      boost::filesystem::remove(link);
      boost::filesystem::create_symlink(log.filename(), link);
#endif

      file.reset(new std::ofstream(log.string().c_str()));
    }


    void Initialize()
    {
      boost::mutex::scoped_lock lock(loggingMutex_);
      loggingContext_.reset(new LoggingContext);
    }

    void Finalize()
    {
      boost::mutex::scoped_lock lock(loggingMutex_);
      loggingContext_.reset(NULL);
    }

    void Reset()
    {
      // Recover the old logging context
      std::unique_ptr<LoggingContext> old;

      {
        boost::mutex::scoped_lock lock(loggingMutex_);
        if (loggingContext_.get() == NULL)
        {
          return;
        }
        else
        {
#if __cplusplus < 201103L
          old.reset(loggingContext_.release());
#else
          old = std::move(loggingContext_);
#endif

          // Create a new logging context, 
          loggingContext_.reset(new LoggingContext);
        }
      }
      
      EnableInfoLevel(old->infoEnabled_);
      EnableTraceLevel(old->traceEnabled_);

      if (!old->targetFolder_.empty())
      {
        SetTargetFolder(old->targetFolder_);
      }
      else if (!old->targetFile_.empty())
      {
        SetTargetFile(old->targetFile_);
      }
    }


    LoggingMemento CreateLoggingMemento()
    {
      LoggingMementoImpl* memento = new LoggingMementoImpl();

      memento->valid_ = true;
      {
        boost::mutex::scoped_lock lock(loggingMutex_);
        memento->infoEnabled_ = loggingContext_->infoEnabled_;
        memento->traceEnabled_ = loggingContext_->traceEnabled_;
        memento->targetFile_ = loggingContext_->targetFile_;
        memento->targetFolder_ = loggingContext_->targetFolder_;

        memento->error_ = loggingContext_->error_;
        memento->warning_ = loggingContext_->warning_;
        memento->info_ = loggingContext_->info_;
      }
      return reinterpret_cast<void*>(memento);
    }
    
    void RestoreLoggingMemento(LoggingMemento mementoPtr)
    {
      LoggingMementoImpl* memento = 
        reinterpret_cast<LoggingMementoImpl*>(mementoPtr);
      if (!memento->valid_)
        throw std::runtime_error("Memento already used");
      memento->valid_ = false;
      std::unique_ptr<LoggingMementoImpl> deleter(memento);
      {
        boost::mutex::scoped_lock lock(loggingMutex_);
        loggingContext_.reset(new LoggingContext);
        loggingContext_->error_ = memento->error_;
        loggingContext_->warning_ = memento->warning_;
        loggingContext_->info_ = memento->info_;
      }
      EnableInfoLevel(memento->infoEnabled_);
      EnableTraceLevel(memento->traceEnabled_);
      if (!memento->targetFolder_.empty())
      {
        SetTargetFolder(memento->targetFolder_);
      }
      else if (!memento->targetFile_.empty())
      {
        SetTargetFile(memento->targetFile_);
      }
    }

    void DiscardLoggingMemento(LoggingMemento mementoPtr)
    {
      LoggingMementoImpl* memento =
        reinterpret_cast<LoggingMementoImpl*>(mementoPtr);
      delete memento;
    }

    void EnableInfoLevel(bool enabled)
    {
      boost::mutex::scoped_lock lock(loggingMutex_);
      assert(loggingContext_.get() != NULL);

      loggingContext_->infoEnabled_ = enabled;
      
      if (!enabled)
      {
        // Also disable the "TRACE" level when info-level debugging is disabled
        loggingContext_->traceEnabled_ = false;
      }
    }

    bool IsInfoLevelEnabled()
    {
      boost::mutex::scoped_lock lock(loggingMutex_);
      assert(loggingContext_.get() != NULL);

      return loggingContext_->infoEnabled_;
    }

    void EnableTraceLevel(bool enabled)
    {
      boost::mutex::scoped_lock lock(loggingMutex_);
      assert(loggingContext_.get() != NULL);

      loggingContext_->traceEnabled_ = enabled;
      
      if (enabled)
      {
        // Also enable the "INFO" level when trace-level debugging is enabled
        loggingContext_->infoEnabled_ = true;
      }
    }

    bool IsTraceLevelEnabled()
    {
      boost::mutex::scoped_lock lock(loggingMutex_);
      assert(loggingContext_.get() != NULL);

      return loggingContext_->traceEnabled_;
    }


    static void CheckFile(std::unique_ptr<std::ofstream>& f)
    {
      if (loggingContext_->file_.get() == NULL ||
          !loggingContext_->file_->is_open())
      {
        throw OrthancException(ErrorCode_CannotWriteFile);
      }
    }

    void SetTargetFolder(const std::string& path)
    {
      boost::mutex::scoped_lock lock(loggingMutex_);
      assert(loggingContext_.get() != NULL);

      PrepareLogFolder(loggingContext_->file_, "" /* no suffix */, path);
      CheckFile(loggingContext_->file_);

      loggingContext_->targetFile_.clear();
      loggingContext_->targetFolder_ = path;
      loggingContext_->warning_ = loggingContext_->file_.get();
      loggingContext_->error_ = loggingContext_->file_.get();
      loggingContext_->info_ = loggingContext_->file_.get();
    }


    void SetTargetFile(const std::string& path)
    {
      boost::mutex::scoped_lock lock(loggingMutex_);
      assert(loggingContext_.get() != NULL);

      loggingContext_->file_.reset(new std::ofstream(path.c_str(), std::fstream::app));
      CheckFile(loggingContext_->file_);

      loggingContext_->targetFile_ = path;
      loggingContext_->targetFolder_.clear();
      loggingContext_->warning_ = loggingContext_->file_.get();
      loggingContext_->error_ = loggingContext_->file_.get();
      loggingContext_->info_ = loggingContext_->file_.get();
    }


    InternalLogger::InternalLogger(const char* level,
                                   const char* file,
                                   int line) : 
      lock_(loggingMutex_), 
      stream_(&null_)  // By default, logging to "/dev/null" is simulated
    {
      if (loggingContext_.get() == NULL)
      {
        fprintf(stderr, "ERROR: Trying to log a message after the finalization of the logging engine\n");
        return;
      }

      try
      {
        LogLevel l = StringToLogLevel(level);
      
        if ((l == LogLevel_Info  && !loggingContext_->infoEnabled_) ||
            (l == LogLevel_Trace && !loggingContext_->traceEnabled_))
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

          char date[64];
          sprintf(date, "%c%02d%02d %02d:%02d:%02d.%06d ",
                  level[0],
                  now.date().month().as_number(),
                  now.date().day().as_number(),
                  static_cast<int>(duration.hours()),
                  static_cast<int>(duration.minutes()),
                  static_cast<int>(duration.seconds()),
                  static_cast<int>(duration.fractional_seconds()));

          header = std::string(date) + path.filename().string() + ":" + boost::lexical_cast<std::string>(line) + "] ";
        }


        // The header is computed, we now re-lock the mutex to access
        // the stream objects. Pay attention that "loggingContext_",
        // "infoEnabled_" or "traceEnabled_" might have changed while
        // the mutex was unlocked.
        lock_.lock();

        if (loggingContext_.get() == NULL)
        {
          fprintf(stderr, "ERROR: Trying to log a message after the finalization of the logging engine\n");
          return;
        }

        switch (l)
        {
          case LogLevel_Error:
            stream_ = loggingContext_->error_;
            break;

          case LogLevel_Warning:
            stream_ = loggingContext_->warning_;
            break;

          case LogLevel_Info:
            if (loggingContext_->infoEnabled_)
            {
              stream_ = loggingContext_->info_;
            }

            break;

          case LogLevel_Trace:
            if (loggingContext_->traceEnabled_)
            {
              stream_ = loggingContext_->info_;
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
      catch (...)
      { 
        // Something is going really wrong, probably running out of
        // memory. Fallback to a degraded mode.
        stream_ = loggingContext_->error_;
        (*stream_) << "E???? ??:??:??.?????? ] ";
      }
    }


    InternalLogger::~InternalLogger()
    {
      if (stream_ != &null_)
      {
        *stream_ << "\n";
        stream_->flush();
      }
    }
      

    void Flush()
    {
      boost::mutex::scoped_lock lock(loggingMutex_);

      if (loggingContext_.get() != NULL &&
          loggingContext_->file_.get() != NULL)
      {
        loggingContext_->file_->flush();
      }
    }

    void SetErrorWarnInfoLoggingStreams(std::ostream* errorStream,
      std::ostream* warningStream,
      std::ostream* infoStream)
    {
      boost::mutex::scoped_lock lock(loggingMutex_);
      std::unique_ptr<LoggingContext> old;

#if __cplusplus < 201103L
      old.reset(loggingContext_.release());
#else
      old = std::move(loggingContext_);
#endif
      
      loggingContext_.reset(new LoggingContext);
      loggingContext_->error_ = errorStream;
      loggingContext_->warning_ = warningStream;
      loggingContext_->info_ = infoStream;
      lock.unlock();
      EnableInfoLevel(old->infoEnabled_);
      EnableTraceLevel(old->traceEnabled_);
    }

#ifdef __EMSCRIPTEN__

    FuncStreamBuf<decltype(emscripten_console_error)> 
      globalEmscriptenErrorStreamBuf(emscripten_console_error);
    std::unique_ptr<std::ostream> globalEmscriptenErrorStream;

    FuncStreamBuf<decltype(emscripten_console_warn)>
      globalEmscriptenWarningStreamBuf(emscripten_console_warn);
    std::unique_ptr<std::ostream> globalEmscriptenWarningStream;

    FuncStreamBuf<decltype(emscripten_console_log)>
      globalEmscriptenInfoStreamBuf(emscripten_console_log);
    std::unique_ptr<std::ostream> globalEmscriptenInfoStream;

    void EnableEmscriptenLogging()
    {
      globalEmscriptenErrorStream.reset(new ostream(
        &globalEmscriptenErrorStreamBuf));

      globalEmscriptenWarningStream.reset(new ostream(
        &globalEmscriptenWarningStreamBuf));

      globalEmscriptenInfoStream.reset(new ostream(
        &globalEmscriptenInfoStreamBuf));

      SetErrorWarnInfoLoggingStreams(
        &globalEmscriptenErrorStream,
        &globalEmscriptenWarningStream
        &globalEmscriptenInfoStream);
    }
#endif
  }
}


#endif   // ORTHANC_ENABLE_LOGGING
