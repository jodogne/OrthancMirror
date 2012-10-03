#pragma once

#if DCMTK_BUNDLES_LOG4CPLUS == 0
#include <log4cpp/Category.hh>
#else
#include <dcmtk/oflog/logger.h>
#endif

namespace Orthanc
{
#if DCMTK_BUNDLES_LOG4CPLUS == 0
  typedef log4cpp::Category Logger;
#else
  typedef OFLogger Logger;
#endif

  Logger& GetLoggerInstance(const char* package);
}
