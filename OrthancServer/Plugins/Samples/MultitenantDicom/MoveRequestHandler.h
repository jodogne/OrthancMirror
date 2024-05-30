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

#include "PluginEnumerations.h"

#include "../../../../OrthancFramework/Sources/DicomNetworking/IMoveRequestHandler.h"


class MoveRequestHandler : public Orthanc::IMoveRequestHandler
{
private:
  class Iterator;

  // Everything is constant, so no need for a mutex
  const std::set<std::string>  labels_;
  const LabelsConstraint       constraint_;
  const bool                   isStrictAet_;
  const bool                   isSynchronous_;

  void ExecuteLookup(std::set<std::string>& publicIds,
                     Orthanc::ResourceType level,
                     const Orthanc::DicomTag& tag,
                     const std::string& value) const;

  void LookupIdentifiers(std::set<std::string>& publicIds,
                         Orthanc::ResourceType level,
                         const Orthanc::DicomMap& input) const;
  
public:
  MoveRequestHandler(const std::set<std::string>& labels,
                     LabelsConstraint constraint,
                     bool isStrictAet,
                     bool isSynchronous) :
    labels_(labels),
    constraint_(constraint),
    isStrictAet_(isStrictAet),
    isSynchronous_(isSynchronous)
  {
  }

  virtual Orthanc::IMoveRequestIterator* Handle(const std::string& targetAet,
                                                const Orthanc::DicomMap& input,
                                                const std::string& originatorIp,
                                                const std::string& originatorAet,
                                                const std::string& calledAet,
                                                uint16_t originatorId) ORTHANC_OVERRIDE;
};
