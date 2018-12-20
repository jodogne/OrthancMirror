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


#include "../../PrecompiledHeadersServer.h"
#include "DatabaseLookup.h"

#include "SetOfResources.h"
#include "../../../Core/OrthancException.h"

namespace Orthanc
{
  namespace Compatibility
  {
    static void ApplyLevel(SetOfResources& candidates,
                           const std::vector<DatabaseConstraint>& lookup,
                           ResourceType level)
    {
      std::vector<DatabaseConstraint> identifiers;
      
      for (size_t i = 0; i < lookup.size(); i++)
      {
        if (lookup[i].GetLevel() == level &&
            lookup[i].IsIdentifier())
        {
          identifiers.push_back(lookup[i]);
        }
      }
    }


    static std::string GetOneInstance(IDatabaseWrapper& database,
                                      int64_t resource,
                                      ResourceType level)
    {
      for (int i = level; i < ResourceType_Instance; i++)
      {
        assert(database.GetResourceType(resource) == static_cast<ResourceType>(i));

        std::list<int64_t> children;
        database.GetChildrenInternalId(children, resource);
          
        if (children.size() == 0)
        {
          throw OrthancException(ErrorCode_Database);
        }
          
        resource = children.front();
      }
        
      return database.GetPublicId(resource);
    }
                           

    void DatabaseLookup::ApplyLookupResources(std::vector<std::string>& resourcesId,
                                              std::vector<std::string>* instancesId,
                                              const std::vector<DatabaseConstraint>& lookup,
                                              ResourceType queryLevel,
                                              size_t limit)
    {
      // This is a re-implementation of
      // "../../../Resources/Graveyard/DatabaseOptimizations/LookupResource.cpp"

      assert(ResourceType_Patient < ResourceType_Study &&
             ResourceType_Study < ResourceType_Series &&
             ResourceType_Series < ResourceType_Instance);
    
      ResourceType upperLevel = queryLevel;
      ResourceType lowerLevel = queryLevel;

      for (size_t i = 0; i < lookup.size(); i++)
      {
        ResourceType level = lookup[i].GetLevel();

        if (level < upperLevel)
        {
          upperLevel = level;
        }

        if (level > lowerLevel)
        {
          lowerLevel = level;
        }
      }

      assert(upperLevel <= queryLevel &&
             queryLevel <= lowerLevel);

      SetOfResources candidates(database_, upperLevel);

      for (int level = upperLevel; level <= lowerLevel; level++)
      {
        ApplyLevel(candidates, lookup, static_cast<ResourceType>(level));

        if (level != lowerLevel)
        {
          candidates.GoDown();
        }
      }

      std::list<int64_t> resources;
      candidates.Flatten(resources);

      // Climb up, up to queryLevel

      for (int level = lowerLevel; level > queryLevel; level--)
      {
        std::list<int64_t> parents;
        for (std::list<int64_t>::const_iterator
               it = resources.begin(); it != resources.end(); ++it)
        {
          int64_t parent;
          if (database_.LookupParent(parent, *it))
          {
            parents.push_back(parent);
          }
        }

        resources.swap(parents);
      }

      // Apply the limit, if given

      if (limit != 0 &&
          resources.size() > limit)
      {
        resources.resize(limit);
      }

      // Get the public ID of all the selected resources

      resourcesId.resize(resources.size());

      if (instancesId != NULL)
      {
        instancesId->resize(resources.size());
      }

      size_t pos = 0;

      for (std::list<int64_t>::const_iterator
             it = resources.begin(); it != resources.end(); ++it, pos++)
      {
        assert(database_.GetResourceType(*it) == queryLevel);

        resourcesId[pos] = database_.GetPublicId(*it);

        if (instancesId != NULL)
        {
          // Collect one child instance for each of the selected resources
          if (queryLevel == ResourceType_Instance)
          {
            (*instancesId) [pos] = resourcesId[pos];
          }
          else
          {
            (*instancesId) [pos] = GetOneInstance(database_, *it, queryLevel);
          }
        }
      }
    }
  }
}
