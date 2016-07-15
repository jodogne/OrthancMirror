/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
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


#pragma once

#include <iostream>

namespace Orthanc
{
  namespace Logging
  {
    void Initialize();

    void Finalize();

    void Reset();

    void Flush();

    void EnableInfoLevel(bool enabled);

    void EnableTraceLevel(bool enabled);

    void SetTargetFile(const std::string& path);

    void SetTargetFolder(const std::string& path);

    struct NullStream : public std::ostream 
    {
      NullStream() : 
        std::ios(0), 
        std::ostream(0)
      {
      }
      
      std::ostream& operator<< (const std::string& message)
      {
        return *this;
      }

      // This overload fixes build problems with Visual Studio 2015
      std::ostream& operator<< (const char* message)
      {
        return *this;
      }
    };
  }
}


#if ORTHANC_ENABLE_LOGGING != 1

#  define LOG(level)   ::Orthanc::Logging::NullStream()
#  define VLOG(level)  ::Orthanc::Logging::NullStream()

#else  /* ORTHANC_ENABLE_LOGGING == 1 */

#  include <boost/thread/mutex.hpp>
#  define LOG(level)  ::Orthanc::Logging::InternalLogger(#level,  __FILE__, __LINE__)
#  define VLOG(level) ::Orthanc::Logging::InternalLogger("TRACE", __FILE__, __LINE__)

namespace Orthanc
{
  namespace Logging
  {
    class InternalLogger
    {
    private:
      boost::mutex::scoped_lock lock_;
      NullStream                null_;
      std::ostream*             stream_;

    public:
      InternalLogger(const char* level,
                     const char* file,
                     int line);

      ~InternalLogger();
      
      std::ostream& operator<< (const std::string& message)
      {
        return (*stream_) << message;
      }
    };
  }
}

#endif  // ORTHANC_ENABLE_LOGGING
