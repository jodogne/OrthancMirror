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


#pragma once

#include "PluginEnumerations.h"

#include "../../../../OrthancFramework/Sources/DicomNetworking/IFindRequestHandler.h"


class FindRequestHandler : public Orthanc::IFindRequestHandler
{
private:
  // Everything is constant, so no need for a mutex
  const std::string           retrieveAet_;
  const std::set<std::string> labels_;
  const LabelsConstraint      constraint_;
        
public:
  FindRequestHandler(const std::string& retrieveAet,
                     const std::set<std::string>& labels,
                     LabelsConstraint constraint) :
    retrieveAet_(retrieveAet),
    labels_(labels),
    constraint_(constraint)
  {
  }

  virtual void Handle(Orthanc::DicomFindAnswers& answers,
                      const Orthanc::DicomMap& input,
                      const std::list<Orthanc::DicomTag>& sequencesToReturn,
                      const std::string& remoteIp,
                      const std::string& remoteAet,
                      const std::string& calledAet,
                      Orthanc::ModalityManufacturer manufacturer) ORTHANC_OVERRIDE;
};
