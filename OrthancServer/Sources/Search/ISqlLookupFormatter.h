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

#if ORTHANC_BUILDING_SERVER_LIBRARY == 1
#  include "../../../OrthancFramework/Sources/Enumerations.h"
#else
#  include <Enumerations.h>
#endif

#include <boost/noncopyable.hpp>
#include <vector>

namespace Orthanc
{
  class DatabaseConstraint;
  
  // This class is also used by the "orthanc-databases" project
  class ISqlLookupFormatter : public boost::noncopyable
  {
  public:
    virtual ~ISqlLookupFormatter()
    {
    }

    virtual std::string GenerateParameter(const std::string& value) = 0;

    virtual std::string FormatResourceType(ResourceType level) = 0;

    virtual std::string FormatWildcardEscape() = 0;

    /**
     * Whether to escape '[' and ']', which is only needed for
     * MSSQL. New in Orthanc 1.10.0, from the following changeset:
     * https://hg.orthanc-server.com/orthanc-databases/rev/389c037387ea
     **/
    virtual bool IsEscapeBrackets() const = 0;

    static void Apply(std::string& sql,
                      ISqlLookupFormatter& formatter,
                      const std::vector<DatabaseConstraint>& lookup,
                      ResourceType queryLevel,
                      size_t limit);
  };
}
