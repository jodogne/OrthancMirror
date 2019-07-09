#include "OrthancLogger.h"
#include "Logging.h"

namespace OrthancHelpers
{

  void OrthancLogger::Trace(const char *message)
  {
    VLOG(1) << GetContext() << " " << message;
  }

  void OrthancLogger::Trace(const std::string& message)
  {
    VLOG(1) << GetContext() << " " << message;
  }

  void OrthancLogger::Info(const char *message)
  {
    LOG(INFO) << GetContext() << " " << message;
  }

  void OrthancLogger::Info(const std::string& message)
  {
    LOG(INFO) << GetContext() << " " << message;
  }

  void OrthancLogger::Warning(const char *message)
  {
    LOG(WARNING) << GetContext() << " " << message;
  }

  void OrthancLogger::Warning(const std::string& message)
  {
    LOG(WARNING) << GetContext() << " " << message;
  }

  void OrthancLogger::Error(const char *message)
  {
    LOG(ERROR) << GetContext() << " " << message;
  }

  void OrthancLogger::Error(const std::string& message)
  {
    LOG(ERROR) << GetContext() << " " << message;
  }
}
