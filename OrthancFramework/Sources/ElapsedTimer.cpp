/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 **/


#include "PrecompiledHeaders.h"
#include "ElapsedTimer.h"

#include "Logging.h"
#include "Toolbox.h"

namespace Orthanc
{
  ElapsedTimer::ElapsedTimer()
  {
    Restart();
  }

  void ElapsedTimer::Restart()
  {
    start_ = boost::posix_time::microsec_clock::universal_time();
  }

  uint64_t ElapsedTimer::GetElapsedMilliseconds()
  {
    return GetElapsedNanoseconds() / 1000000;
  }

  uint64_t ElapsedTimer::GetElapsedMicroseconds()
  {
    return GetElapsedNanoseconds() / 1000;
  }

  uint64_t ElapsedTimer::GetElapsedNanoseconds()
  {
    boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
    boost::posix_time::time_duration diff = now - start_;
    return static_cast<uint64_t>(diff.total_nanoseconds());
  }

  std::string ElapsedTimer::GetHumanElapsedDuration()
  {
    return Toolbox::GetHumanDuration(GetElapsedNanoseconds());
  }

  // in "full" mode, returns " 26.45MB in 2.25s = 94.04Mbps"
  // else, returns "94.04Mbps"
  std::string ElapsedTimer::GetHumanTransferSpeed(bool full, uint64_t sizeInBytes)
  {
    return Toolbox::GetHumanTransferSpeed(full, sizeInBytes, GetElapsedNanoseconds());
  }

  DebugElapsedTimeLogger::DebugElapsedTimeLogger(const std::string& message) :
    message_(message),
    logged_(false)
  {
    Restart();
  }

  DebugElapsedTimeLogger::~DebugElapsedTimeLogger()
  {
    if (!logged_)
    {
      StopAndLog();
    }
  }

  void DebugElapsedTimeLogger::Restart()
  {
    timer_.Restart();
  }

  void DebugElapsedTimeLogger::StopAndLog()
  {
    LOG(WARNING) << "ELAPSED TIMER: " << message_ << " (" << timer_.GetElapsedMicroseconds() << " us)";
    logged_ = true;
  }

  ApiElapsedTimeLogger::ApiElapsedTimeLogger(const std::string& message) :
    message_(message)
  {
    timer_.Restart();
    CLOG(INFO, HTTP) << message_;
  }

  ApiElapsedTimeLogger::~ApiElapsedTimeLogger()
  {
    CLOG(INFO, HTTP) << message_ << " (elapsed: " << timer_.GetElapsedMicroseconds() << " us)";
  }
}
