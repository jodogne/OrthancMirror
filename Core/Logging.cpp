/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
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


#include "PrecompiledHeaders.h"
#include "Logging.h"


namespace Orthanc
{  
  namespace Logging
  {
#if ORTHANC_ENABLE_GOOGLE_LOG == 1
    void Initialize()
    {
      // Initialize Google's logging library.
      FLAGS_logtostderr = true;
      FLAGS_minloglevel = 1;   // Do not print LOG(INFO) by default
      FLAGS_v = 0;             // Do not print trace-level VLOG(1) by default

      google::InitGoogleLogging("Orthanc");
    }

    void Finalize()
    {
      google::ShutdownGoogleLogging();
    }

    void EnableInfoLevel(bool enabled)
    {
      FLAGS_minloglevel = (enabled ? 0 : 1);
    }

    void EnableTraceLevel(bool enabled)
    {
      if (enabled)
      {
        FLAGS_minloglevel = 0;
        FLAGS_v = 1;
      }
      else
      {
        FLAGS_v = 0;
      }
    }
#endif
  }
}
