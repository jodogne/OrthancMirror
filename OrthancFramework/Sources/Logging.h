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


#pragma once

// To have ORTHANC_ENABLE_LOGGING defined if using the shared library
#include "OrthancFramework.h"
#include "Compatibility.h"

#include <iostream>

#if !defined(ORTHANC_ENABLE_LOGGING)
#  error The macro ORTHANC_ENABLE_LOGGING must be defined
#endif

#if !defined(ORTHANC_ENABLE_LOGGING_STDIO)
#  if ORTHANC_ENABLE_LOGGING == 1
#    error The macro ORTHANC_ENABLE_LOGGING_STDIO must be defined
#  else
#    define ORTHANC_ENABLE_LOGGING_STDIO 0
#  endif
#endif


namespace Orthanc
{
  namespace Logging
  {
    enum LogLevel
    {
      LogLevel_ERROR,
      LogLevel_WARNING,
      LogLevel_INFO,
      LogLevel_TRACE
    };

    /**
     * NB: The log level for each category is encoded as a bit
     * mask. As a consequence, there can be up to 31 log categories
     * (not 32, as the value GENERIC is reserved for the log commands
     * that don't fall in a specific category).
     **/
    enum LogCategory
    {
      LogCategory_GENERIC = (1 << 0),
      LogCategory_PLUGINS = (1 << 1),
      LogCategory_HTTP    = (1 << 2),
      LogCategory_SQLITE  = (1 << 3),
      LogCategory_DICOM   = (1 << 4),
      LogCategory_JOBS    = (1 << 5),
      LogCategory_LUA     = (1 << 6),
    };
    
    ORTHANC_PUBLIC const char* EnumerationToString(LogLevel level);

    ORTHANC_PUBLIC LogLevel StringToLogLevel(const char* level);

    // "pluginContext" must be of type "OrthancPluginContext"
    ORTHANC_PUBLIC void InitializePluginContext(void* pluginContext);

    ORTHANC_PUBLIC void Initialize();

    ORTHANC_PUBLIC void Finalize();

    ORTHANC_PUBLIC void Reset();

    ORTHANC_PUBLIC void Flush();

    ORTHANC_PUBLIC void EnableInfoLevel(bool enabled);

    ORTHANC_PUBLIC void EnableTraceLevel(bool enabled);

    ORTHANC_PUBLIC bool IsTraceLevelEnabled();

    ORTHANC_PUBLIC bool IsInfoLevelEnabled();

    ORTHANC_PUBLIC void SetCategoryEnabled(LogLevel level,
                                           LogCategory category,
                                           bool enabled);

    ORTHANC_PUBLIC bool IsCategoryEnabled(LogLevel level,
                                          LogCategory category);
    
    ORTHANC_PUBLIC bool LookupCategory(LogCategory& target,
                                       const std::string& category);

    ORTHANC_PUBLIC unsigned int GetCategoriesCount();

    ORTHANC_PUBLIC const char* GetCategoryName(unsigned int i);

    ORTHANC_PUBLIC const char* GetCategoryName(LogCategory category);

    ORTHANC_PUBLIC void SetTargetFile(const std::string& path);

    ORTHANC_PUBLIC void SetTargetFolder(const std::string& path);

    struct ORTHANC_LOCAL NullStream : public std::ostream 
    {
      NullStream() : 
        std::ios(0), 
        std::ostream(0)
      {
      }
      
      template <typename T>
      std::ostream& operator<< (const T& message)
      {
        return *this;
      }
    };
  }
}



/**
 * NB:
 * - The "VLOG(unused)" macro is for backward compatibility with
 *   Orthanc <= 1.8.0.
 * - The "CLOG()" macro stands for "category log" (new in Orthanc 1.8.1)
 **/

#if defined(LOG)
#  error The macro LOG cannot be defined beforehand
#endif

#if defined(VLOG)
#  error The macro VLOG cannot be defined beforehand
#endif

#if defined(CLOG)
#  error The macro CLOG cannot be defined beforehand
#endif

#if ORTHANC_ENABLE_LOGGING != 1
#  define LOG(level)            ::Orthanc::Logging::NullStream()
#  define VLOG(unused)          ::Orthanc::Logging::NullStream()
#  define CLOG(level, category) ::Orthanc::Logging::NullStream()
#else /* ORTHANC_ENABLE_LOGGING == 1 */
#  define LOG(level)     ::Orthanc::Logging::InternalLogger     \
  (::Orthanc::Logging::LogLevel_ ## level,                      \
   ::Orthanc::Logging::LogCategory_GENERIC, __FILE__, __LINE__)
#  define VLOG(unused)   ::Orthanc::Logging::InternalLogger     \
  (::Orthanc::Logging::LogLevel_TRACE,                          \
   ::Orthanc::Logging::LogCategory_GENERIC, __FILE__, __LINE__)
#  define CLOG(level, category) ::Orthanc::Logging::InternalLogger      \
  (::Orthanc::Logging::LogLevel_ ## level,                              \
   ::Orthanc::Logging::LogCategory_ ## category, __FILE__, __LINE__)
#endif



#if (ORTHANC_ENABLE_LOGGING == 1 &&             \
     ORTHANC_ENABLE_LOGGING_STDIO == 1)
// This is notably for WebAssembly

#include <boost/lexical_cast.hpp>
#include <boost/noncopyable.hpp>
#include <sstream>

namespace Orthanc
{
  namespace Logging
  {
    class ORTHANC_PUBLIC InternalLogger : public boost::noncopyable
    {
    private:
      LogLevel           level_;
      LogCategory        category_;
      std::stringstream  messageStream_;

    public:
      InternalLogger(LogLevel level,
                     LogCategory category,
                     const char* file  /* ignored */,
                     int line  /* ignored */) :
        level_(level),
        category_(category)
      {
      }

      // For backward binary compatibility with Orthanc Framework <= 1.8.0
      InternalLogger(LogLevel level,
                     const char* file  /* ignored */,
                     int line  /* ignored */) :
        level_(level),
        category_(LogCategory_GENERIC)
      {
      }

      ~InternalLogger();
      
      template <typename T>
        std::ostream& operator<< (const T& message)
      {
        return messageStream_ << boost::lexical_cast<std::string>(message);
      }
    };
  }
}

#endif



#if (ORTHANC_ENABLE_LOGGING == 1 &&             \
     ORTHANC_ENABLE_LOGGING_STDIO == 0)

#include <boost/lexical_cast.hpp>
#include <boost/noncopyable.hpp>
#include <boost/thread/mutex.hpp>
#include <sstream>

namespace Orthanc
{
  namespace Logging
  {
    class ORTHANC_PUBLIC InternalLogger : public boost::noncopyable
    {
    private:
      boost::mutex::scoped_lock           lock_;
      LogLevel                            level_;
      std::unique_ptr<std::stringstream>  pluginStream_;
      std::ostream*                       stream_;

      void Setup(LogCategory category,
                 const char* file,
                 int line);

    public:
      InternalLogger(LogLevel level,
                     LogCategory category,
                     const char* file,
                     int line);

      // For backward binary compatibility with Orthanc Framework <= 1.8.0
      InternalLogger(LogLevel level,
                     const char* file,
                     int line);

      ~InternalLogger();
      
      template <typename T>
        std::ostream& operator<< (const T& message)
      {
        return (*stream_) << boost::lexical_cast<std::string>(message);
      }
    };

    /**
     * Set custom logging streams for the error, warning and info
     * logs. This function may not be called if a log file or folder
     * has been set beforehand. All three references must be valid.
     *
     * Please ensure the supplied streams remain alive and valid as
     * long as logging calls are performed. In order to prevent
     * dangling pointer usage, it is mandatory to call
     * Orthanc::Logging::Reset() before the stream objects are
     * destroyed and the references become invalid.
     *
     * This function must only be used by unit tests. It is ignored if
     * InitializePluginContext() was called.
     **/
    ORTHANC_PUBLIC void SetErrorWarnInfoLoggingStreams(std::ostream& errorStream,
                                                       std::ostream& warningStream, 
                                                       std::ostream& infoStream);
  }
}

#endif
