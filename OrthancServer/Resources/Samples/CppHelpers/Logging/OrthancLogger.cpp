/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
 * Copyright (C) 2021-2021 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
