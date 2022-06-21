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


#include "PrecompiledHeaders.h"
#include "Logging.h"

#include "OrthancException.h"

#include <stdint.h>


/*********************************************************
 * Common section
 *********************************************************/

namespace Orthanc
{
  namespace Logging
  {
    static const uint32_t ALL_CATEGORIES_MASK = 0xffffffff;
    
    static uint32_t infoCategoriesMask_ = 0;
    static uint32_t traceCategoriesMask_ = 0;
    static std::string logTargetFolder_;            // keep a track of the log folder in case of reset of the context
    static std::string logTargetFile_;              // keep a track of the log file in case of reset of the context
    
    const char* EnumerationToString(LogLevel level)
    {
      switch (level)
      {
        case LogLevel_ERROR:
          return "ERROR";

        case LogLevel_WARNING:
          return "WARNING";

        case LogLevel_INFO:
          return "INFO";

        case LogLevel_TRACE:
          return "TRACE";

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
    }


    LogLevel StringToLogLevel(const char *level)
    {
      if (strcmp(level, "ERROR") == 0)
      {
        return LogLevel_ERROR;
      }
      else if (strcmp(level, "WARNING") == 0)
      {
        return LogLevel_WARNING;
      }
      else if (strcmp(level, "INFO") == 0)
      {
        return LogLevel_INFO;
      }
      else if (strcmp(level, "TRACE") == 0)
      {
        return LogLevel_TRACE;
      }
      else 
      {
        throw OrthancException(ErrorCode_InternalError);
      }
    }

    
    void EnableInfoLevel(bool enabled)
    {
      if (enabled)
      {
        infoCategoriesMask_ = ALL_CATEGORIES_MASK;
      }
      else
      {
        // Also disable the "TRACE" level when info-level debugging is disabled
        infoCategoriesMask_ = 0;
        traceCategoriesMask_ = 0;
      }
    }

    
    bool IsInfoLevelEnabled()
    {
      return (infoCategoriesMask_ != 0);
    }

    
    void EnableTraceLevel(bool enabled)
    {
      if (enabled)
      {
        // Also enable the "INFO" level when trace-level debugging is enabled
        infoCategoriesMask_ = ALL_CATEGORIES_MASK;
        traceCategoriesMask_ = ALL_CATEGORIES_MASK;
      }
      else
      {
        traceCategoriesMask_ = 0;
      }
    }


    bool IsTraceLevelEnabled()
    {
      return (traceCategoriesMask_ != 0);
    }

    
    void SetCategoryEnabled(LogLevel level,
                            LogCategory category,
                            bool enabled)
    {
      // Invariant: If a bit is set for "trace", it must also be set
      // for "verbose" (in other words, trace level implies verbose level)
      assert((traceCategoriesMask_ & infoCategoriesMask_) == traceCategoriesMask_);
      
      if (level == LogLevel_INFO)
      {
        if (enabled)
        {
          infoCategoriesMask_ |= static_cast<uint32_t>(category);
        }
        else
        {
          infoCategoriesMask_ &= ~static_cast<uint32_t>(category);
          traceCategoriesMask_ &= ~static_cast<uint32_t>(category);
        }
      }
      else if (level == LogLevel_TRACE)
      {
        if (enabled)
        {
          traceCategoriesMask_ |= static_cast<uint32_t>(category);
          infoCategoriesMask_ |= static_cast<uint32_t>(category);
        }
        else
        {
          traceCategoriesMask_ &= ~static_cast<uint32_t>(category);
        }
      }
      else
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange,
                               "Can only modify the parameters of the INFO and TRACE levels");
      }

      assert((traceCategoriesMask_ & infoCategoriesMask_) == traceCategoriesMask_);
    }

    
    bool IsCategoryEnabled(LogLevel level,
                           LogCategory category)
    {
      if (level == LogLevel_ERROR ||
          level == LogLevel_WARNING)
      {
        return true;
      }
      else if (level == LogLevel_INFO)
      {
        return (infoCategoriesMask_ & category) != 0;
      }
      else if (level == LogLevel_TRACE)
      {
        return (traceCategoriesMask_ & category) != 0;
      }
      else
      {
        return false;
      }
    }


    bool LookupCategory(LogCategory& target,
                        const std::string& category)
    {
      if (category == "generic")
      {
        target = LogCategory_GENERIC;
        return true;
      }
      else if (category == "plugins")
      {
        target = LogCategory_PLUGINS;
        return true;
      }
      else if (category == "http")
      {
        target = LogCategory_HTTP;
        return true;
      }
      else if (category == "dicom")
      {
        target = LogCategory_DICOM;
        return true;
      }
      else if (category == "sqlite")
      {
        target = LogCategory_SQLITE;
        return true;
      }
      else if (category == "jobs")
      {
        target = LogCategory_JOBS;
        return true;
      }
      else if (category == "lua")
      {
        target = LogCategory_LUA;
        return true;
      }
      else
      {
        return false;
      }
    }


    unsigned int GetCategoriesCount()
    {
      return 7;
    }


    const char* GetCategoryName(unsigned int i)
    {
      if (i < GetCategoriesCount())
      {
        return GetCategoryName(static_cast<LogCategory>(1 << i));
      }
      else
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
    }
    

    const char* GetCategoryName(LogCategory category)
    {
      switch (category)
      {
        case LogCategory_GENERIC:
          return "generic";
            
        case LogCategory_PLUGINS:
          return "plugins";
            
        case LogCategory_HTTP:
          return "http";
            
        case LogCategory_DICOM:
          return "dicom";
            
        case LogCategory_SQLITE:
          return "sqlite";

        case LogCategory_JOBS:
          return "jobs";

        case LogCategory_LUA:
          return "lua";

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
    }
  }
}



#if ORTHANC_ENABLE_LOGGING != 1

/*********************************************************
 * Section if logging is disabled
 *********************************************************/

namespace Orthanc
{
  namespace Logging
  {
    void InitializePluginContext(void* pluginContext)
    {
    }

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

    void SetTargetFile(const std::string& path)
    {
    }

    void SetTargetFolder(const std::string& path)
    {
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

#ifdef __EMSCRIPTEN__
#  include <emscripten/html5.h>
#endif

namespace Orthanc
{
  namespace Logging
  {
#ifdef __EMSCRIPTEN__
    static void ErrorLogFunc(const char* msg)
    {
      emscripten_console_error(msg);
    }

    static void WarningLogFunc(const char* msg)
    {
      emscripten_console_warn(msg);
    }

    static void InfoLogFunc(const char* msg)
    {
      emscripten_console_log(msg);
    }

    static void TraceLogFunc(const char* msg)
    {
      emscripten_console_log(msg);
    }
#else  /* __EMSCRIPTEN__ not #defined */
    static void ErrorLogFunc(const char* msg)
    {
      fprintf(stderr, "E: %s\n", msg);
    }

    static void WarningLogFunc(const char*)
    {
      fprintf(stdout, "W: %s\n", msg);
    }

    static void InfoLogFunc(const char*)
    {
      fprintf(stdout, "I: %s\n", msg);
    }

    static void TraceLogFunc(const char*)
    {
      fprintf(stdout, "T: %s\n", msg);
    }
#endif  /* __EMSCRIPTEN__ */


    InternalLogger::~InternalLogger()
    {
      std::string message = messageStream_.str();

      if (IsCategoryEnabled(level_, category_))
      {
        switch (level_)
        {
          case LogLevel_ERROR:
            ErrorLogFunc(message.c_str());
            break;

          case LogLevel_WARNING:
            WarningLogFunc(message.c_str());
            break;

          case LogLevel_INFO:
            InfoLogFunc(message.c_str());
            // TODO: stone_console_info(message_.c_str());
            break;

          case LogLevel_TRACE:
            TraceLogFunc(message.c_str());
            break;

          default:
          {
            std::stringstream ss;
            ss << "Unknown log level (" << level_ << ") for message: " << message;
            std::string s = ss.str();
            ErrorLogFunc(s.c_str());
          }
        }
      }
    }

    void InitializePluginContext(void* pluginContext)
    {
    }

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

    void SetTargetFile(const std::string& path)
    {
    }

    void SetTargetFolder(const std::string& path)
    {
    }
  }
}


#else

/*********************************************************
 * Logger compatible with the Orthanc plugin SDK, or that
 * mimics behavior from Google Log.
 *********************************************************/

#include <cassert>

namespace
{
  /**
   * This is minimal implementation of the context for an Orthanc
   * plugin, limited to the logging facilities, and that is binary
   * compatible with the definitions of "OrthancCPlugin.h"
   **/
  typedef enum 
  {
    _OrthancPluginService_LogInfo = 1,
    _OrthancPluginService_LogWarning = 2,
    _OrthancPluginService_LogError = 3,
    _OrthancPluginService_INTERNAL = 0x7fffffff
  } _OrthancPluginService;

  typedef struct _OrthancPluginContext_t
  {
    void*          pluginsManager;
    const char*    orthancVersion;
    void         (*Free) (void* buffer);
    int32_t      (*InvokeService) (struct _OrthancPluginContext_t* context,
                                   _OrthancPluginService service,
                                   const void* params);
  } OrthancPluginContext;
}
  

#include "Enumerations.h"
#include "SystemToolbox.h"

#include <fstream>
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>


namespace
{
  struct LoggingStreamsContext
  {
    std::string  targetFile_;
    std::string  targetFolder_;

    std::ostream* error_;
    std::ostream* warning_;
    std::ostream* info_;

    std::unique_ptr<std::ofstream> file_;

    LoggingStreamsContext() : 
      error_(&std::cerr),
      warning_(&std::cerr),
      info_(&std::cerr)
    {
    }
  };
}



static std::unique_ptr<LoggingStreamsContext> loggingStreamsContext_;
static boost::mutex                           loggingStreamsMutex_;
static Orthanc::Logging::NullStream           nullStream_;
static OrthancPluginContext*                  pluginContext_ = NULL;


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


    // "loggingStreamsMutex_" must be locked
    static void CheckFile(std::unique_ptr<std::ofstream>& f)
    {
      if (loggingStreamsContext_->file_.get() == NULL ||
          !loggingStreamsContext_->file_->is_open())
      {
        throw OrthancException(ErrorCode_CannotWriteFile);
      }
    }
    

    static void GetLinePrefix(std::string& prefix,
                              LogLevel level,
                              const char* file,
                              int line,
                              LogCategory category)
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

      char c;
      switch (level)
      {
        case LogLevel_ERROR:
          c = 'E';
          break;

        case LogLevel_WARNING:
          c = 'W';
          break;

        case LogLevel_INFO:
          c = 'I';
          break;

        case LogLevel_TRACE:
          c = 'T';
          break;

        default:
          c = '?';
          break;
      }

      char date[64];
      sprintf(date, "%c%02d%02d %02d:%02d:%02d.%06d ",
              c,
              now.date().month().as_number(),
              now.date().day().as_number(),
              static_cast<int>(duration.hours()),
              static_cast<int>(duration.minutes()),
              static_cast<int>(duration.seconds()),
              static_cast<int>(duration.fractional_seconds()));

      prefix = (std::string(date) + path.filename().string() + ":" +
                boost::lexical_cast<std::string>(line) + "] ");

      if (level != LogLevel_ERROR &&
          level != LogLevel_WARNING &&
          category != LogCategory_GENERIC)
      {
        prefix += "(" + std::string(GetCategoryName(category)) + ") ";
      }
    }
    

    void InitializePluginContext(void* pluginContext)
    {
      assert(sizeof(_OrthancPluginService) == sizeof(int32_t));

      boost::mutex::scoped_lock lock(loggingStreamsMutex_);
      loggingStreamsContext_.reset(NULL);
      pluginContext_ = reinterpret_cast<OrthancPluginContext*>(pluginContext);

      EnableInfoLevel(true);  // allow the plugin to log at info level (but the Orthanc Core still decides of the level)
    }


    void Initialize()
    {
      boost::mutex::scoped_lock lock(loggingStreamsMutex_);

      if (loggingStreamsContext_.get() == NULL)
      {
        loggingStreamsContext_.reset(new LoggingStreamsContext);
      }
    }

    void Finalize()
    {
      boost::mutex::scoped_lock lock(loggingStreamsMutex_);
      loggingStreamsContext_.reset(NULL);
    }

    void Reset()
    {
      {
        boost::mutex::scoped_lock lock(loggingStreamsMutex_);
        loggingStreamsContext_.reset(new LoggingStreamsContext);
      }

      // Recover the old logging context if any
      if (!logTargetFile_.empty())
      {
        SetTargetFile(logTargetFile_);
      }
      else if (!logTargetFolder_.empty())
      {
        SetTargetFolder(logTargetFolder_);
      }
    }


    void SetTargetFolder(const std::string& path)
    {
      boost::mutex::scoped_lock lock(loggingStreamsMutex_);
      if (loggingStreamsContext_.get() != NULL)
      {
        PrepareLogFolder(loggingStreamsContext_->file_, "" /* no suffix */, path);
        CheckFile(loggingStreamsContext_->file_);

        loggingStreamsContext_->targetFile_.clear();
        loggingStreamsContext_->targetFolder_ = path;
        loggingStreamsContext_->warning_ = loggingStreamsContext_->file_.get();
        loggingStreamsContext_->error_ = loggingStreamsContext_->file_.get();
        loggingStreamsContext_->info_ = loggingStreamsContext_->file_.get();
        logTargetFolder_ = path;
      }
    }


    void SetTargetFile(const std::string& path)
    {
      boost::mutex::scoped_lock lock(loggingStreamsMutex_);

      if (loggingStreamsContext_.get() != NULL)
      {
        loggingStreamsContext_->file_.reset(new std::ofstream(path.c_str(), std::fstream::app));
        CheckFile(loggingStreamsContext_->file_);

        loggingStreamsContext_->targetFile_ = path;
        loggingStreamsContext_->targetFolder_.clear();
        loggingStreamsContext_->warning_ = loggingStreamsContext_->file_.get();
        loggingStreamsContext_->error_ = loggingStreamsContext_->file_.get();
        loggingStreamsContext_->info_ = loggingStreamsContext_->file_.get();
        logTargetFile_ = path;
      }
    }


    void InternalLogger::Setup(LogCategory category,
                               const char* file,
                               int line)
    {
      if (pluginContext_ != NULL)
      {
        // We are logging using the Orthanc plugin SDK

        if (level_ == LogLevel_TRACE ||
            !IsCategoryEnabled(level_, category))
        {
          // No trace level in plugins, directly exit as the stream is
          // set to "/dev/null"
          return;
        }
        else
        {
          pluginStream_.reset(new std::stringstream);
          stream_ = pluginStream_.get();
        }
      }
      else
      {
        // We are logging in a standalone application, not inside an Orthanc plugin

        if (!IsCategoryEnabled(level_, category))
        {
          // This logging level is disabled, directly exit as the
          // stream is set to "/dev/null"
          return;
        }

        std::string prefix;
        GetLinePrefix(prefix, level_, file, line, category);

        {
          // We lock the global mutex. The mutex is locked until the
          // destructor is called: No change in the output can be done.
          lock_.lock();
      
          if (loggingStreamsContext_.get() == NULL)
          {
            fprintf(stderr, "ERROR: Trying to log a message after the finalization of the logging engine\n");
            lock_.unlock();
            return;
          }

          switch (level_)
          {
            case LogLevel_ERROR:
              stream_ = loggingStreamsContext_->error_;
              break;
              
            case LogLevel_WARNING:
              stream_ = loggingStreamsContext_->warning_;
              break;
              
            case LogLevel_INFO:
            case LogLevel_TRACE:
              stream_ = loggingStreamsContext_->info_;
              break;
              
            default:  // Should not occur
              stream_ = loggingStreamsContext_->error_;
              break;              
          }

          if (stream_ == &nullStream_)
          {
            // The logging is disabled for this level, we can release
            // the global mutex.
            lock_.unlock();
          }
          else
          {
            try
            {
              (*stream_) << prefix;
            }
            catch (...)
            { 
              // Something is going really wrong, probably running out of
              // memory. Fallback to a degraded mode.
              stream_ = loggingStreamsContext_->error_;
              (*stream_) << "E???? ??:??:??.?????? ] ";
            }
          }
        }
      }
    }


    InternalLogger::InternalLogger(LogLevel level,
                                   LogCategory category,
                                   const char* file,
                                   int line) :
      lock_(loggingStreamsMutex_, boost::defer_lock_t()),
      level_(level),
      stream_(&nullStream_)  // By default, logging to "/dev/null" is simulated
    {
      Setup(category, file, line);
    }


    InternalLogger::InternalLogger(LogLevel level,
                                   const char* file,
                                   int line) :
      lock_(loggingStreamsMutex_, boost::defer_lock_t()),
      level_(level),
      stream_(&nullStream_)  // By default, logging to "/dev/null" is simulated
    {
      Setup(LogCategory_GENERIC, file, line);
    }


    InternalLogger::~InternalLogger()
    {
      if (pluginStream_.get() != NULL)
      {
        // We are logging through the Orthanc SDK
        
        std::string message = pluginStream_->str();

        if (pluginContext_ != NULL)
        {
          switch (level_)
          {
            case LogLevel_ERROR:
              pluginContext_->InvokeService(pluginContext_, _OrthancPluginService_LogError, message.c_str());
              break;

            case LogLevel_WARNING:
              pluginContext_->InvokeService(pluginContext_, _OrthancPluginService_LogWarning, message.c_str());
              break;

            case LogLevel_INFO:
              pluginContext_->InvokeService(pluginContext_, _OrthancPluginService_LogInfo, message.c_str());
              break;

            default:
              break;
          }
        }
      }
      else if (stream_ != &nullStream_)
      {
        *stream_ << "\n";
        stream_->flush();
      }
    }
      

    void Flush()
    {
      if (pluginContext_ != NULL)
      {
        boost::mutex::scoped_lock lock(loggingStreamsMutex_);

        if (loggingStreamsContext_.get() != NULL &&
            loggingStreamsContext_->file_.get() != NULL)
        {
          loggingStreamsContext_->file_->flush();
        }
      }
    }
    

    void SetErrorWarnInfoLoggingStreams(std::ostream& errorStream,
                                        std::ostream& warningStream,
                                        std::ostream& infoStream)
    {
      boost::mutex::scoped_lock lock(loggingStreamsMutex_);

      loggingStreamsContext_.reset(new LoggingStreamsContext);
      loggingStreamsContext_->error_ = &errorStream;
      loggingStreamsContext_->warning_ = &warningStream;
      loggingStreamsContext_->info_ = &infoStream;
    }
  }
}


#endif   // ORTHANC_ENABLE_LOGGING
