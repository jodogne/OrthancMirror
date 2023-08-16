/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "DicomFilter.h"

#include "PluginToolbox.h"

#include "../../../../OrthancFramework/Sources/Logging.h"
#include "../../../../OrthancFramework/Sources/OrthancException.h"

#include "../Common/OrthancPluginCppWrapper.h"


DicomFilter::DicomFilter() :
  hasAcceptedTransferSyntaxes_(false)
{
  {
    OrthancPlugins::OrthancConfiguration config;
    alwaysAllowEcho_ = config.GetBooleanValue("DicomAlwaysAllowEcho", true);
    alwaysAllowFind_ = config.GetBooleanValue("DicomAlwaysAllowFind", false);
    alwaysAllowMove_ = config.GetBooleanValue("DicomAlwaysAllowMove", false);
    alwaysAllowStore_ = config.GetBooleanValue("DicomAlwaysAllowStore", true);
    unknownSopClassAccepted_ = config.GetBooleanValue("UnknownSopClassAccepted", false);
    isStrict_ = config.GetBooleanValue("StrictAetComparison", false);
    checkModalityHost_ = config.GetBooleanValue("DicomCheckModalityHost", false);
  }
}


bool DicomFilter::IsAllowedConnection(const std::string& remoteIp,
                                      const std::string& remoteAet,
                                      const std::string& calledAet)
{
  boost::shared_lock<boost::shared_mutex>  lock(mutex_);

  LOG(INFO) << "Incoming connection from AET " << remoteAet
            << " on IP " << remoteIp << ", calling AET " << calledAet;

  if (alwaysAllowEcho_ ||
      alwaysAllowFind_ ||
      alwaysAllowMove_ ||
      alwaysAllowStore_)
  {
    return true;
  }
  else
  {
    std::string name;
    Orthanc::RemoteModalityParameters parameters;

    if (!PluginToolbox::LookupAETitle(name, parameters, isStrict_, remoteAet))
    {
      LOG(WARNING) << "Modality \"" << remoteAet
                   << "\" is not listed in the \"DicomModalities\" configuration option";
      return false;
    }
    else if (!checkModalityHost_ ||
             remoteIp == parameters.GetHost())
    {
      return true;
    }
    else
    {
      LOG(WARNING) << "Forbidding access from AET \"" << remoteAet
                   << "\" given its hostname (" << remoteIp << ") does not match "
                   << "the \"DicomModalities\" configuration option ("
                   << parameters.GetHost() << " was expected)";
      return false;
    }
  }
}


bool DicomFilter::IsAllowedRequest(const std::string& remoteIp,
                                   const std::string& remoteAet,
                                   const std::string& calledAet,
                                   Orthanc::DicomRequestType type)
{
  boost::shared_lock<boost::shared_mutex>  lock(mutex_);

  LOG(INFO) << "Incoming " << EnumerationToString(type) << " request from AET "
            << remoteAet << " on IP " << remoteIp << ", calling AET " << calledAet;

  if (type == Orthanc::DicomRequestType_Echo &&
      alwaysAllowEcho_)
  {
    // Incoming C-Echo requests are always accepted, even from unknown AET
    return true;
  }
  else if (type == Orthanc::DicomRequestType_Find &&
           alwaysAllowFind_)
  {
    // Incoming C-Find requests are always accepted, even from unknown AET
    return true;
  }
  else if (type == Orthanc::DicomRequestType_Store &&
           alwaysAllowStore_)
  {
    // Incoming C-Store requests are always accepted, even from unknown AET
    return true;
  }
  else if (type == Orthanc::DicomRequestType_Move &&
           alwaysAllowMove_)
  {
    // Incoming C-Move requests are always accepted, even from unknown AET
    return true;
  }
  else
  {
    std::string name;
    Orthanc::RemoteModalityParameters parameters;

    if (!PluginToolbox::LookupAETitle(name, parameters, isStrict_, remoteAet))
    {
      LOG(WARNING) << "DICOM authorization rejected for AET " << remoteAet
                   << " on IP " << remoteIp << ": This AET is not listed in "
                   << "configuration option \"DicomModalities\"";
      return false;
    }
    else
    {
      if (parameters.IsRequestAllowed(type))
      {
        return true;
      }
      else
      {
        LOG(WARNING) << "DICOM authorization rejected for AET " << remoteAet
                     << " on IP " << remoteIp << ": The DICOM command "
                     << EnumerationToString(type) << " is not allowed for this modality "
                     << "according to configuration option \"DicomModalities\"";
        return false;
      }
    }
  }
}


void DicomFilter::GetAcceptedTransferSyntaxes(std::set<Orthanc::DicomTransferSyntax>& target,
                                              const std::string& remoteIp,
                                              const std::string& remoteAet,
                                              const std::string& calledAet)
{
  boost::unique_lock<boost::shared_mutex>  lock(mutex_);

  if (!hasAcceptedTransferSyntaxes_)
  {
    Json::Value syntaxes;

    if (!OrthancPlugins::RestApiGet(syntaxes, "/tools/accepted-transfer-syntaxes", false) ||
        syntaxes.type() != Json::arrayValue)
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_InternalError);
    }
    else
    {
      for (Json::Value::ArrayIndex i = 0; i < syntaxes.size(); i++)
      {
        Orthanc::DicomTransferSyntax syntax;

        if (syntaxes[i].type() != Json::stringValue)
        {
          throw Orthanc::OrthancException(Orthanc::ErrorCode_InternalError);
        }
        else if (Orthanc::LookupTransferSyntax(syntax, syntaxes[i].asString()))
        {
          acceptedTransferSyntaxes_.insert(syntax);
        }
        else
        {
          LOG(WARNING) << "Unknown transfer syntax: " << syntaxes[i].asString();
        }
      }
    }

    hasAcceptedTransferSyntaxes_ = true;
  }

  target = acceptedTransferSyntaxes_;
}


bool DicomFilter::IsUnknownSopClassAccepted(const std::string& remoteIp,
                                            const std::string& remoteAet,
                                            const std::string& calledAet)
{
  boost::shared_lock<boost::shared_mutex>  lock(mutex_);
  return unknownSopClassAccepted_;
}
