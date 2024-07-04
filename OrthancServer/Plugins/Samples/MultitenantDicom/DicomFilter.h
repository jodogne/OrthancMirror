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

#include "../../../../OrthancFramework/Sources/Compatibility.h"
#include "../../../../OrthancFramework/Sources/DicomNetworking/IApplicationEntityFilter.h"

#include <boost/thread/shared_mutex.hpp>


class DicomFilter : public Orthanc::IApplicationEntityFilter
{
private:
  boost::shared_mutex mutex_;

  bool alwaysAllowEcho_;
  bool alwaysAllowFind_;
  bool alwaysAllowMove_;
  bool alwaysAllowStore_;
  bool unknownSopClassAccepted_;
  bool isStrict_;
  bool checkModalityHost_;

  bool hasAcceptedTransferSyntaxes_;
  std::set<Orthanc::DicomTransferSyntax>  acceptedTransferSyntaxes_;

public:
  DicomFilter();

  virtual bool IsAllowedConnection(const std::string& remoteIp,
                                   const std::string& remoteAet,
                                   const std::string& calledAet) ORTHANC_OVERRIDE;

  virtual bool IsAllowedRequest(const std::string& remoteIp,
                                const std::string& remoteAet,
                                const std::string& calledAet,
                                Orthanc::DicomRequestType type) ORTHANC_OVERRIDE;

  virtual void GetAcceptedTransferSyntaxes(std::set<Orthanc::DicomTransferSyntax>& target,
                                           const std::string& remoteIp,
                                           const std::string& remoteAet,
                                           const std::string& calledAet) ORTHANC_OVERRIDE;

  virtual bool IsUnknownSopClassAccepted(const std::string& remoteIp,
                                         const std::string& remoteAet,
                                         const std::string& calledAet) ORTHANC_OVERRIDE;
};
