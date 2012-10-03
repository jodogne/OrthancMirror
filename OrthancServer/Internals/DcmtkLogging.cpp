#include "DcmtkLogging.h"

namespace Orthanc
{
  namespace Internals
  {
    Logger& GetLogger()
    {
      return GetLoggerInstance("Dcmtk");
    }
  }
}
