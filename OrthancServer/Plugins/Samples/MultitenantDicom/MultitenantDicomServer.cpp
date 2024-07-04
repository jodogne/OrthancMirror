/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2024 Orthanc Team SRL, Belgium
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


#include "MultitenantDicomServer.h"

#include "FindRequestHandler.h"
#include "MoveRequestHandler.h"
#include "PluginToolbox.h"
#include "StoreRequestHandler.h"

#include "../../../../OrthancFramework/Sources/Logging.h"
#include "../../../../OrthancFramework/Sources/SerializationToolbox.h"

#include "../Common/OrthancPluginCppWrapper.h"


bool MultitenantDicomServer::IsSameAETitle(const std::string& aet1,
                                           const std::string& aet2)
{
  boost::mutex::scoped_lock lock(mutex_);
  return PluginToolbox::IsSameAETitle(isStrictAet_, aet1, aet2);
}


bool MultitenantDicomServer::LookupAETitle(Orthanc::RemoteModalityParameters& parameters,
                                           const std::string& aet)
{
  boost::mutex::scoped_lock lock(mutex_);

  std::string name;
  return PluginToolbox::LookupAETitle(name, parameters, isStrictAet_, server_->GetApplicationEntityTitle());
}


Orthanc::IFindRequestHandler* MultitenantDicomServer::ConstructFindRequestHandler()
{
  boost::mutex::scoped_lock lock(mutex_);
  return new FindRequestHandler(server_->GetApplicationEntityTitle(), labels_, labelsConstraint_);
}


Orthanc::IMoveRequestHandler* MultitenantDicomServer::ConstructMoveRequestHandler()
{
  boost::mutex::scoped_lock lock(mutex_);
  return new MoveRequestHandler(labels_, labelsConstraint_, isStrictAet_, isSynchronousCMove_);
}


Orthanc::IStoreRequestHandler* MultitenantDicomServer::ConstructStoreRequestHandler()
{
  boost::mutex::scoped_lock lock(mutex_);
  return new StoreRequestHandler(labels_, labelsStoreLevels_);
}


MultitenantDicomServer::MultitenantDicomServer(const Json::Value& serverConfig)
{
  PluginToolbox::ParseLabels(labels_, labelsConstraint_, serverConfig);

  if (serverConfig.isMember(KEY_LABELS_STORE_LEVELS))
  {
    std::set<std::string> levels;
    Orthanc::SerializationToolbox::ReadSetOfStrings(levels, serverConfig, KEY_LABELS_STORE_LEVELS);
    for (std::set<std::string>::const_iterator it = levels.begin(); it != levels.end(); ++it)
    {
      labelsStoreLevels_.insert(Orthanc::StringToResourceType(it->c_str()));
    }
  }
  else
  {
    labelsStoreLevels_.insert(Orthanc::ResourceType_Study);
    labelsStoreLevels_.insert(Orthanc::ResourceType_Series);
    labelsStoreLevels_.insert(Orthanc::ResourceType_Instance);
  }
  
  server_.reset(new Orthanc::DicomServer);

  {
    OrthancPlugins::OrthancConfiguration globalConfig;
    isSynchronousCMove_ = globalConfig.GetBooleanValue(KEY_SYNCHRONOUS_C_MOVE, true);
    isStrictAet_ = globalConfig.GetBooleanValue(KEY_STRICT_AET_COMPARISON, false);

    server_->SetCalledApplicationEntityTitleCheck(globalConfig.GetBooleanValue("DicomCheckCalledAet", false));
    server_->SetAssociationTimeout(globalConfig.GetUnsignedIntegerValue("DicomScpTimeout", 30));
    server_->SetThreadsCount(globalConfig.GetUnsignedIntegerValue("DicomThreadsCount", 1));
    server_->SetMaximumPduLength(globalConfig.GetUnsignedIntegerValue("MaximumPduLength", 16384));
  }

  server_->SetRemoteModalities(*this);
  server_->SetApplicationEntityFilter(filter_);
  server_->SetPortNumber(Orthanc::SerializationToolbox::ReadUnsignedInteger(serverConfig, "Port"));
  server_->SetApplicationEntityTitle(Orthanc::SerializationToolbox::ReadString(serverConfig, KEY_AET));
  server_->SetFindRequestHandlerFactory(*this);
  server_->SetMoveRequestHandlerFactory(*this);
  server_->SetStoreRequestHandlerFactory(*this);
}


void MultitenantDicomServer::Start()
{
  boost::mutex::scoped_lock lock(mutex_);

  if (server_->GetPortNumber() < 1024)
  {
    LOG(WARNING) << "The DICOM port is privileged ("
                 << server_->GetPortNumber() << " is below 1024), "
                 << "make sure you run Orthanc as root/administrator";
  }

  server_->Start();
  LOG(WARNING) << "Started multitenant DICOM server listening with AET " << server_->GetApplicationEntityTitle()
               << " on port: " << server_->GetPortNumber();
}


void MultitenantDicomServer::Stop()
{
  boost::mutex::scoped_lock lock(mutex_);

  if (server_.get() != NULL)
  {
    LOG(WARNING) << "Stopping multitenant DICOM server listening with AET " << server_->GetApplicationEntityTitle()
                 << " on port: " << server_->GetPortNumber();
    server_->Stop();
  }
}
