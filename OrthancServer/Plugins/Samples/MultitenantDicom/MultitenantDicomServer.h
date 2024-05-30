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


#pragma once

#include "DicomFilter.h"
#include "PluginEnumerations.h"

#include "../../../../OrthancFramework/Sources/DicomNetworking/DicomServer.h"

#include <boost/thread/mutex.hpp>


class MultitenantDicomServer :
  private Orthanc::DicomServer::IRemoteModalities,
  private Orthanc::IFindRequestHandlerFactory,
  private Orthanc::IMoveRequestHandlerFactory,
  private Orthanc::IStoreRequestHandlerFactory
{
private:
  virtual bool IsSameAETitle(const std::string& aet1,
                             const std::string& aet2) ORTHANC_OVERRIDE;

  virtual bool LookupAETitle(Orthanc::RemoteModalityParameters& parameters,
                             const std::string& aet) ORTHANC_OVERRIDE;

  virtual Orthanc::IFindRequestHandler* ConstructFindRequestHandler() ORTHANC_OVERRIDE;

  virtual Orthanc::IMoveRequestHandler* ConstructMoveRequestHandler() ORTHANC_OVERRIDE;

  virtual Orthanc::IStoreRequestHandler* ConstructStoreRequestHandler() ORTHANC_OVERRIDE;

  boost::mutex  mutex_;

  std::set<std::string>                  labels_;
  LabelsConstraint                       labelsConstraint_;
  std::set<Orthanc::ResourceType>        labelsStoreLevels_;
  bool                                   isSynchronousCMove_;
  bool                                   isStrictAet_;
  DicomFilter                            filter_;
  std::unique_ptr<Orthanc::DicomServer>  server_;

public:
  explicit MultitenantDicomServer(const Json::Value& serverConfig);

  void Start();

  void Stop();
};
