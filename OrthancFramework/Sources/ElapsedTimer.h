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


#pragma once

// The header "posix_time.hpp" of Boost is very heavy to import, don't
// define those classes in "Toolbox.h"

#include "OrthancFramework.h"

#include <boost/noncopyable.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <stdint.h>


namespace Orthanc
{
  class ORTHANC_PUBLIC ElapsedTimer : public boost::noncopyable
  {
  private:
    boost::posix_time::ptime  start_;

  public:
    ElapsedTimer();

    uint64_t GetElapsedMilliseconds();
    uint64_t GetElapsedMicroseconds();
    uint64_t GetElapsedNanoseconds();

    std::string GetHumanElapsedDuration();
    std::string GetHumanTransferSpeed(bool full, uint64_t sizeInBytes);

    void Restart();
  };

  // This is a helper class to measure and log time spend e.g in a method.
  // This should be used only during debugging and should likely not ever be used in a release.
  // By default, you should use it as a RAII but you may force Restart/StopAndLog manually if needed.
  class ORTHANC_PUBLIC DebugElapsedTimeLogger : public boost::noncopyable
  {
  private:
    ElapsedTimer      timer_;
    const std::string message_;
    bool              logged_;

  public:
    explicit DebugElapsedTimeLogger(const std::string& message);

    ~DebugElapsedTimeLogger();

    void Restart();
    void StopAndLog();
  };

  // This variant logs the same message when entering the method and when exiting (with the elapsed time).
  // Logs goes to verbose-http.
  class ORTHANC_PUBLIC ApiElapsedTimeLogger : public boost::noncopyable
  {
  private:
    ElapsedTimer      timer_;
    const std::string message_;

  public:
    explicit ApiElapsedTimeLogger(const std::string& message);

    ~ApiElapsedTimeLogger();
  };
}
