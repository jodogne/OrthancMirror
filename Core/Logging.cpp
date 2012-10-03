#include "Logging.h"

namespace Orthanc
{
  Logger& GetLoggerInstance(const char* package)
    {
#if DCMTK_BUNDLES_LOG4CPLUS == 0
      return Logger::getInstance(package);
#else
      return OFLog::getLogger(package);
#endif
    }
}
