#include "OrthancPluginLogger.h"

namespace OrthancHelpers
{

  OrthancPluginLogger::OrthancPluginLogger(OrthancPluginContext *context)
    : pluginContext_(context),
      hasAlreadyLoggedTraceWarning_(false)
  {
  }

  void OrthancPluginLogger::Trace(const char *message)
  {
    Trace(std::string(message));
  }

  void OrthancPluginLogger::Trace(const std::string &message)
  {
    if (!hasAlreadyLoggedTraceWarning_)
    {
      Warning("Trying to log 'TRACE' level information in a plugin is not possible.  These logs won't appear.");
      hasAlreadyLoggedTraceWarning_ = true;
    }
  }

  void OrthancPluginLogger::Info(const char *message)
  {
    Info(std::string(message));
  }

  void OrthancPluginLogger::Info(const std::string &message)
  {
    OrthancPluginLogInfo(pluginContext_, (GetContext() + " " + message).c_str());
  }

  void OrthancPluginLogger::Warning(const char *message)
  {
    Warning(std::string(message));
  }

  void OrthancPluginLogger::Warning(const std::string &message)
  {
    OrthancPluginLogWarning(pluginContext_, (GetContext() + " " + message).c_str());
  }

  void OrthancPluginLogger::Error(const char *message)
  {
    Error(std::string(message));
  }

  void OrthancPluginLogger::Error(const std::string &message)
  {
    OrthancPluginLogError(pluginContext_, (GetContext() + " " + message).c_str());
  }
} // namespace OrthancHelpers
