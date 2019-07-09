#pragma once

#include "ILogger.h"
#include <orthanc/OrthancCPlugin.h>

namespace OrthancHelpers
{

  class OrthancPluginLogger : public BaseLogger
  {
    OrthancPluginContext *pluginContext_;
    bool hasAlreadyLoggedTraceWarning_;

  public:
    OrthancPluginLogger(OrthancPluginContext *context);

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
