/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "OrthancPluginLogger.h"

namespace OrthancHelpers
{

  OrthancPluginLogger::OrthancPluginLogger(OrthancPluginContext *context)
    : pluginContext_(context),
      hasAlreadyLoggedTraceWarning_(false)
  {
  }

  void OrthancPluginLogger::Trace(const char *message)
  {
    Trace(std::string(message));
  }

  void OrthancPluginLogger::Trace(const std::string &message)
  {
    if (!hasAlreadyLoggedTraceWarning_)
    {
      Warning("Trying to log 'TRACE' level information in a plugin is not possible.  These logs won't appear.");
      hasAlreadyLoggedTraceWarning_ = true;
    }
  }

  void OrthancPluginLogger::Info(const char *message)
  {
    Info(std::string(message));
  }

  void OrthancPluginLogger::Info(const std::string &message)
  {
    OrthancPluginLogInfo(pluginContext_, (GetContext() + " " + message).c_str());
  }

  void OrthancPluginLogger::Warning(const char *message)
  {
    Warning(std::string(message));
  }

  void OrthancPluginLogger::Warning(const std::string &message)
  {
    OrthancPluginLogWarning(pluginContext_, (GetContext() + " " + message).c_str());
  }

  void OrthancPluginLogger::Error(const char *message)
  {
    Error(std::string(message));
  }

  void OrthancPluginLogger::Error(const std::string &message)
  {
    OrthancPluginLogError(pluginContext_, (GetContext() + " " + message).c_str());
  }
} // namespace OrthancHelpers
