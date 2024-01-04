/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2024 Osimis S.A., Belgium
 * Copyright (C) 2021-2024 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
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

#include "ILogger.h"

namespace OrthancHelpers
{
  // a logger ... that does not log.
  // Instead of writing:
  // if (logger != NULL)
  // {
  //   logger->Info("hello")   ;
  // }
  // you should create a NullLogger:
  // logger = new NullLogger();
  // ...
  // logger->Info("hello");
  class NullLogger : public BaseLogger {
  public:
    NullLogger() {}

    virtual void Trace(const char* message) {}
    virtual void Trace(const std::string& message) {}
    virtual void Info(const char* message) {}
    virtual void Info(const std::string& message) {}
    virtual void Warning(const char* message) {}
    virtual void Warning(const std::string& message) {}
    virtual void Error(const char* message) {}
    virtual void Error(const std::string& message) {}
  };
}
