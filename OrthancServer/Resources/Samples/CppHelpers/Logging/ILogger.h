/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
 * Copyright (C) 2021-2021 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
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

#include <string>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <boost/thread.hpp>

namespace OrthancHelpers
{


  inline std::string ShortenId(const std::string& orthancUuid)
  {
    size_t firstHyphenPos = orthancUuid.find_first_of('-');
    if (firstHyphenPos == std::string::npos)
    {
      return orthancUuid;
    }
    else
    {
      return orthancUuid.substr(0, firstHyphenPos);
    }
  }


  // Interface for loggers providing the same interface
  // in Orthanc framework or in an Orthanc plugins.
  // Furthermore, compared to the LOG and VLOG macros,
  // these loggers will provide "contexts".
  class ILogger
  {
  public:
    virtual ~ILogger() {}
    virtual void Trace(const char* message) = 0;
    virtual void Trace(const std::string& message) = 0;
    virtual void Info(const char* message) = 0;
    virtual void Info(const std::string& message) = 0;
    virtual void Warning(const char* message) = 0;
    virtual void Warning(const std::string& message) = 0;
    virtual void Error(const char* message) = 0;
    virtual void Error(const std::string& message) = 0;

    virtual void EnterContext(const char* message, bool forceLogContextChange = false) = 0;
    virtual void EnterContext(const std::string& message, bool forceLogContextChange = false) = 0;
    virtual void LeaveContext(bool forceLogContextChange = false) = 0;
  };


  // Implements ILogger by providing contexts.  Contexts defines
  // the "call-stack" of the logs and are prepended to the log.
  // check LogContext class for more details
  class BaseLogger : public ILogger
  {
#if ORTHANC_ENABLE_THREADS == 1
    boost::thread_specific_ptr<std::vector<std::string>> contexts_;
#else
    std::auto_ptr<std::vector<std::string>> contexts_;
#endif
    bool logContextChanges_;

  public:

    BaseLogger()
      : logContextChanges_(false)
    {
    }

    void EnableLogContextChanges(bool enable)
    {
      logContextChanges_ = enable;
    }

    virtual void EnterContext(const char* message, bool forceLogContextChange = false)
    {
      EnterContext(std::string(message), forceLogContextChange);
    }

    virtual void EnterContext(const std::string& message, bool forceLogContextChange = false)
    {
      if (!contexts_.get())
      {
        contexts_.reset(new std::vector<std::string>());
      }
      contexts_->push_back(message);

      if (logContextChanges_ || forceLogContextChange)
      {
        Info(".. entering");
      }
    }

    virtual void LeaveContext(bool forceLogContextChange = false)
    {
      if (logContextChanges_ || forceLogContextChange)
      {
        Info(".. leaving");
      }

      contexts_->pop_back();
      if (contexts_->size() == 0)
      {
        contexts_.reset(NULL);
      }
    }

  protected:

    virtual std::string GetContext()
    {
      if (contexts_.get() != NULL && contexts_->size() > 0)
      {
        return "|" + boost::algorithm::join(*contexts_, " | ") + "|";
      }
      else
      {
        return std::string("|");
      }
    }
  };


  /* RAII to set a Log context.
  * Example:
  * ILogger* logger = new OrthancPluginLogger(..);
  * {
  *   LogContext logContext(logger, "A");
  *   {
  *     LogContext nestedLogContext(logger, "B");
  *     logger->Error("out of memory");
  *   }
  * }
  * will produce:
  * |A | B| out of memory
  *
  * furthermore, if LogContextChanges are enabled in the BaseLogger,
  * you'll get;
  * |A| .. entering
  * |A | B| .. entering
  * |A | B| out of memory
  * |A | B| .. leaving
  * |A| .. leaving
  */
  class LogContext
  {
    ILogger* logger_;
    bool     forceLogContextChange_;
  public:
    LogContext(ILogger* logger, const char* context, bool forceLogContextChange = false) :
      logger_(logger),
      forceLogContextChange_(forceLogContextChange)
    {
      logger_->EnterContext(context, forceLogContextChange_);
    }

    LogContext(ILogger* logger, const std::string& context, bool forceLogContextChange = false) :
      logger_(logger),
      forceLogContextChange_(forceLogContextChange)
    {
      logger_->EnterContext(context, forceLogContextChange_);
    }

    ~LogContext()
    {
      logger_->LeaveContext(forceLogContextChange_);
    }

  };
}
