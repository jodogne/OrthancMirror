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

#pragma once

#include "../../OrthancFramework/Sources/DicomNetworking/IFindRequestHandler.h"

namespace Orthanc
{
  class ServerContext;
  
  class OrthancFindRequestHandler : public IFindRequestHandler
  {
  private:
    class LookupVisitor;

    ServerContext& context_;
    unsigned int   maxResults_;
    unsigned int   maxInstances_;

    bool HasReachedLimit(const DicomFindAnswers& answers,
                         ResourceType level) const;

    bool FilterQueryTag(std::string& value /* can be modified */,
                        ResourceType level,
                        const DicomTag& tag,
                        ModalityManufacturer manufacturer);

    bool ApplyLuaFilter(DicomMap& target,
                        const DicomMap& source,
                        const std::string& remoteIp,
                        const std::string& remoteAet,
                        const std::string& calledAet,
                        ModalityManufacturer manufacturer);

  public:
    explicit OrthancFindRequestHandler(ServerContext& context);

    virtual void Handle(DicomFindAnswers& answers,
                        const DicomMap& input,
                        const std::list<DicomTag>& sequencesToReturn,
                        const std::string& remoteIp,
                        const std::string& remoteAet,
                        const std::string& calledAet,
                        ModalityManufacturer manufacturer) ORTHANC_OVERRIDE;

    unsigned int GetMaxResults() const
    {
      return maxResults_;
    }

    void SetMaxResults(unsigned int results)
    {
      maxResults_ = results;
    }

    unsigned int GetMaxInstances() const
    {
      return maxInstances_;
    }

    void SetMaxInstances(unsigned int instances)
    {
      maxInstances_ = instances;
    }

    static void FormatOrigin(Json::Value& origin,
                             const std::string& remoteIp,
                             const std::string& remoteAet,
                             const std::string& calledAet,
                             ModalityManufacturer manufacturer);
  };
}
