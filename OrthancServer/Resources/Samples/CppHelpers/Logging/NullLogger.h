#pragma once

#include "ILogger.h"

namespace OrthancHelpers
{
  // a logger ... that does not log.
  // Instead of writing:
  // if (logger != NULL)
  // {
  //   logger->Info("hello")   ;
  // }
  // you should create a NullLogger:
  // logger = new NullLogger();
  // ...
  // logger->Info("hello");
  class NullLogger : public BaseLogger {
  public:
    NullLogger() {}

    virtual void Trace(const char* message) {}
    virtual void Trace(const std::string& message) {}
    virtual void Info(const char* message) {}
    virtual void Info(const std::string& message) {}
    virtual void Warning(const char* message) {}
    virtual void Warning(const std::string& message) {}
    virtual void Error(const char* message) {}
    virtual void Error(const std::string& message) {}
  };
}
