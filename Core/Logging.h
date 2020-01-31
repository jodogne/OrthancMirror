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
#pragma once

#include <iostream>

#if !defined(ORTHANC_ENABLE_LOGGING)
#  error The macro ORTHANC_ENABLE_LOGGING must be defined
#endif

#if !defined(ORTHANC_ENABLE_LOGGING_PLUGIN)
#  if ORTHANC_ENABLE_LOGGING == 1
#    error The macro ORTHANC_ENABLE_LOGGING_PLUGIN must be defined
#  else
#    define ORTHANC_ENABLE_LOGGING_PLUGIN 0
#  endif
#endif

#if !defined(ORTHANC_ENABLE_LOGGING_STDIO)
#  if ORTHANC_ENABLE_LOGGING == 1
#    error The macro ORTHANC_ENABLE_LOGGING_STDIO must be defined
#  else
#    define ORTHANC_ENABLE_LOGGING_STDIO 0
#  endif
#endif

#if ORTHANC_ENABLE_LOGGING_PLUGIN == 1
#  include <orthanc/OrthancCPlugin.h>
#endif

#include <boost/lexical_cast.hpp>

namespace Orthanc
{
  namespace Logging
  {
#if ORTHANC_ENABLE_LOGGING_PLUGIN == 1
    void Initialize(OrthancPluginContext* context);
#else
    void Initialize();
#endif

    void Finalize();

    void Reset();

    void Flush();

    void EnableInfoLevel(bool enabled);

    void EnableTraceLevel(bool enabled);

    bool IsTraceLevelEnabled();

    bool IsInfoLevelEnabled();

    void SetTargetFile(const std::string& path);

    void SetTargetFolder(const std::string& path);

#if ORTHANC_ENABLE_LOGGING_STDIO == 1
    typedef void (*LoggingFunction)(const char*);
    void SetErrorWarnInfoTraceLoggingFunctions(
      LoggingFunction errorLogFunc,
      LoggingFunction warningLogfunc,
      LoggingFunction infoLogFunc,
      LoggingFunction traceLogFunc);
#endif


    struct NullStream : public std::ostream 
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

#if ORTHANC_ENABLE_LOGGING != 1

#  define LOG(level)   ::Orthanc::Logging::NullStream()
#  define VLOG(level)  ::Orthanc::Logging::NullStream()

#elif (ORTHANC_ENABLE_LOGGING_PLUGIN == 1 ||    \
       ORTHANC_ENABLE_LOGGING_STDIO == 1)

#  include <boost/noncopyable.hpp>
#  define LOG(level)  ::Orthanc::Logging::InternalLogger \
  (::Orthanc::Logging::InternalLevel_ ## level, __FILE__, __LINE__)
#  define VLOG(level) ::Orthanc::Logging::InternalLogger \
  (::Orthanc::Logging::InternalLevel_TRACE, __FILE__, __LINE__)

namespace Orthanc
{
  namespace Logging
  {
    enum InternalLevel
    {
      InternalLevel_ERROR,
      InternalLevel_WARNING,
      InternalLevel_INFO,
      InternalLevel_TRACE
    };
    
    class InternalLogger : public boost::noncopyable
    {
    private:
      InternalLevel       level_;
      std::stringstream   messageStream_;

    public:
      InternalLogger(InternalLevel level,
                     const char* file,
                     int line);

      ~InternalLogger();
      
      template <typename T>
      InternalLogger& operator<< (const T& message)
      {
        messageStream_ << message;
        return *this;
      }
    };
  }
}




#else  /* ORTHANC_ENABLE_LOGGING_PLUGIN == 0 && 
          ORTHANC_ENABLE_LOGGING_STDIO == 0 && 
          ORTHANC_ENABLE_LOGGING == 1 */

#  include <boost/thread/mutex.hpp>
#  define LOG(level)  ::Orthanc::Logging::InternalLogger(#level,  __FILE__, __LINE__)
#  define VLOG(level) ::Orthanc::Logging::InternalLogger("TRACE", __FILE__, __LINE__)

namespace Orthanc
{
  namespace Logging
  {
    class InternalLogger
    {
    private:
      boost::mutex::scoped_lock lock_;
      NullStream                null_;
      std::ostream*             stream_;

    public:
      InternalLogger(const char* level,
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
    opaque pointer that represents the state of the logging configuration
    */
    typedef void* LoggingMemento;

    /**
    Returns an object that contains the logging configuration.

    This function allocates resources that you must dispose of by
    using either RestoreLoggingMemento or DiscardLoggingMemento.

    This function is only to be used by tests.
    */
    LoggingMemento CreateLoggingMemento();

    /**
    Restores the logging configuration. The logging system is restored in 
    the state it was in when the memento object was created through 
    GetLoggingMemento().

    After calling this function, the memento object may not be used 
    again

    This function is only to be used by tests.
    */
    void RestoreLoggingMemento(LoggingMemento memento);

    /**
    Call this function if you do not plan on restoring the logging 
    configuration state that you captured with CreateLoggingMemento

    This function is only to be used by tests.
    */
    void DiscardLoggingMemento(LoggingMemento memento);

    /**
      Set custom logging streams for the error, warning and info logs.
      This function may not be called if a log file or folder has been 
      set beforehand. All three pointers must be valid and cannot be NULL.

      Please ensure the supplied streams remain alive and valid as long as
      logging calls are performed.

      In order to prevent dangling pointer usage, it is recommended to call
      Orthanc::Logging::Reset() before the stream objects are destroyed and 
      the pointers become invalid.
    */
    void SetErrorWarnInfoLoggingStreams(std::ostream* errorStream,
                                        std::ostream* warningStream, 
                                        std::ostream* infoStream);

#ifdef __EMSCRIPTEN__
    /**
      This function will change the logging streams so that the logging functions 
      provided by emscripten html5.h API functions are used : it will change the 
      error_, warning_ and info_  stream objects so that their operator<< writes 
      into the browser console using emscripten_console_error(), 
      emscripten_console_warn() and emscripten_console_log(). This will allow for
      logging levels to be correctly handled by the browser when the code executes
      in Web Assembly
    */
    void EnableEmscriptenLogging();
#endif
  }
}

#endif  // ORTHANC_ENABLE_LOGGING
