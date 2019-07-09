#pragma once

#include "ILogger.h"

namespace OrthancHelpers
{

  class OrthancLogger : public BaseLogger
  {
  public:
    virtual void Trace(const char *message);
    virtual void Trace(const std::string &message);
    virtual void Info(const char *message);
    virtual void Info(const std::string &message);
    virtual void Warning(const char *message);
    virtual void Warning(const std::string &message);
    virtual void Error(const char *message);
    virtual void Error(const std::string &message);
  };
} // namespace OrthancHelpers
