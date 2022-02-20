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

#include "../IDatabaseWrapper.h"

namespace Orthanc
{
  namespace Compatibility
  {
    /**
     * This is a compatibility class that contains database primitives
     * that were used in Orthanc <= 1.5.1, and that have been removed
     * during the optimization of the database engine.
     **/
    class ILookupResources : public boost::noncopyable
    {     
    public:
      virtual ~ILookupResources()
      {
      }
      
      virtual void GetAllInternalIds(std::list<int64_t>& target,
                                     ResourceType resourceType) = 0;
      
      virtual void LookupIdentifier(std::list<int64_t>& result,
                                    ResourceType level,
                                    const DicomTag& tag,
                                    IdentifierConstraintType type,
                                    const std::string& value) = 0;
 
      virtual void LookupIdentifierRange(std::list<int64_t>& result,
                                         ResourceType level,
                                         const DicomTag& tag,
                                         const std::string& start,
                                         const std::string& end) = 0;

      static void Apply(IDatabaseWrapper::ITransaction& transaction,
                        ILookupResources& compatibility,
                        std::list<std::string>& resourcesId,
                        std::list<std::string>* instancesId,
                        const std::vector<DatabaseConstraint>& lookup,
                        ResourceType queryLevel,
                        size_t limit);
    };
  }
}
