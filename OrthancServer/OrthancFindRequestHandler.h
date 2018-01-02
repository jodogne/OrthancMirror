/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * In addition, as a special exception, the copyright holders of this
 * program give permission to link the code of its release with the
 * OpenSSL project's "OpenSSL" library (or with modified versions of it
 * that use the same license as the "OpenSSL" library), and distribute
 * the linked executables. You must obey the GNU General Public License
 * in all respects for all of the code used other than "OpenSSL". If you
 * modify file(s) with this exception, you may extend this exception to
 * your version of the file(s), but you are not obligated to do so. If
 * you do not wish to do so, delete this exception statement from your
 * version. If you delete this exception statement from all source files
 * in the program, then also delete it here.
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

#include "../Core/DicomNetworking/IFindRequestHandler.h"

#include "ServerContext.h"

namespace Orthanc
{
  class OrthancFindRequestHandler : public IFindRequestHandler
  {
  private:
    ServerContext& context_;
    unsigned int maxResults_;
    unsigned int maxInstances_;

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
                        const std::string& calledAet);

  public:
    OrthancFindRequestHandler(ServerContext& context) :
      context_(context), 
      maxResults_(0),
      maxInstances_(0)
    {
    }

    virtual void Handle(DicomFindAnswers& answers,
                        const DicomMap& input,
                        const std::list<DicomTag>& sequencesToReturn,
                        const std::string& remoteIp,
                        const std::string& remoteAet,
                        const std::string& calledAet,
                        ModalityManufacturer manufacturer);

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
  };
}
