/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include "../../../OrthancFramework/Sources/Enumerations.h"

#include <boost/noncopyable.hpp>
#include <stdint.h>
#include <vector>

namespace Orthanc
{
  class DatabaseDicomTagConstraints;
  class FindRequest;

  enum LabelsConstraint
  {
    LabelsConstraint_All,
    LabelsConstraint_Any,
    LabelsConstraint_None
  };

  class ISqlLookupFormatter : public boost::noncopyable
  {
  public:
    virtual ~ISqlLookupFormatter()
    {
    }

    virtual std::string GenerateParameter(const std::string& value) = 0;

    virtual std::string FormatResourceType(ResourceType level) = 0;

    virtual std::string FormatWildcardEscape() = 0;

    virtual std::string FormatLimits(uint64_t since, uint64_t count) = 0;

    /**
     * Whether to escape '[' and ']', which is only needed for
     * MSSQL. New in Orthanc 1.10.0, from the following changeset:
     * https://orthanc.uclouvain.be/hg/orthanc-databases/rev/389c037387ea
     **/
    virtual bool IsEscapeBrackets() const = 0;

    static void GetLookupLevels(ResourceType& lowerLevel,
                                ResourceType& upperLevel,
                                const ResourceType& queryLevel,
                                const DatabaseDicomTagConstraints& lookup);

    static void Apply(std::string& sql,
                      ISqlLookupFormatter& formatter,
                      const DatabaseDicomTagConstraints& lookup,
                      ResourceType queryLevel,
                      const std::set<std::string>& labels,  // New in Orthanc 1.12.0
                      LabelsConstraint labelsConstraint,    // New in Orthanc 1.12.0
                      size_t limit);

    static void ApplySingleLevel(std::string& sql,
                                 ISqlLookupFormatter& formatter,
                                 const DatabaseDicomTagConstraints& lookup,
                                 ResourceType queryLevel,
                                 const std::set<std::string>& labels,  // New in Orthanc 1.12.0
                                 LabelsConstraint labelsConstraint,    // New in Orthanc 1.12.0
                                 size_t limit);

    static void Apply(std::string& sql,
                      ISqlLookupFormatter& formatter,
                      const FindRequest& request);
  };
}
