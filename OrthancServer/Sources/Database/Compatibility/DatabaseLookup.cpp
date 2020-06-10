/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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

#include "../../../Core/OrthancException.h"
#include "../../Search/DicomTagConstraint.h"
#include "../../ServerToolbox.h"
#include "SetOfResources.h"

namespace Orthanc
{
  namespace Compatibility
  {
    namespace
    {
      // Anonymous namespace to avoid clashes between compiler modules
      class MainTagsConstraints : boost::noncopyable
      {
      private:
        std::vector<DicomTagConstraint*>  constraints_;

      public:
        ~MainTagsConstraints()
        {
          for (size_t i = 0; i < constraints_.size(); i++)
          {
            assert(constraints_[i] != NULL);
            delete constraints_[i];
          }
        }

        void Reserve(size_t n)
        {
          constraints_.reserve(n);
        }

        size_t GetSize() const
        {
          return constraints_.size();
        }

        DicomTagConstraint& GetConstraint(size_t i) const
        {
          if (i >= constraints_.size())
          {
            throw OrthancException(ErrorCode_ParameterOutOfRange);
          }
          else
          {
            assert(constraints_[i] != NULL);
            return *constraints_[i];
          }
        }
        
        void Add(const DatabaseConstraint& constraint)
        {
          constraints_.push_back(new DicomTagConstraint(constraint));
        }          
      };
    }
    
    
    static void ApplyIdentifierConstraint(SetOfResources& candidates,
                                          ILookupResources& compatibility,
                                          const DatabaseConstraint& constraint,
                                          ResourceType level)
    {
      std::list<int64_t> matches;

      switch (constraint.GetConstraintType())
      {
        case ConstraintType_Equal:
          compatibility.LookupIdentifier(matches, level, constraint.GetTag(),
                                    IdentifierConstraintType_Equal, constraint.GetSingleValue());
          break;
          
        case ConstraintType_SmallerOrEqual:
          compatibility.LookupIdentifier(matches, level, constraint.GetTag(),
                                    IdentifierConstraintType_SmallerOrEqual, constraint.GetSingleValue());
          break;
          
        case ConstraintType_GreaterOrEqual:
          compatibility.LookupIdentifier(matches, level, constraint.GetTag(),
                                    IdentifierConstraintType_GreaterOrEqual, constraint.GetSingleValue());

          break;
          
        case ConstraintType_Wildcard:
          compatibility.LookupIdentifier(matches, level, constraint.GetTag(),
                                    IdentifierConstraintType_Wildcard, constraint.GetSingleValue());

          break;
          
        case ConstraintType_List:
          for (size_t i = 0; i < constraint.GetValuesCount(); i++)
          {
            std::list<int64_t> tmp;
            compatibility.LookupIdentifier(tmp, level, constraint.GetTag(),
                                      IdentifierConstraintType_Wildcard, constraint.GetValue(i));
            matches.splice(matches.end(), tmp);
          }

          break;
          
        default:
          throw OrthancException(ErrorCode_InternalError);
      }

      candidates.Intersect(matches);
    }

    
    static void ApplyIdentifierRange(SetOfResources& candidates,
                                     ILookupResources& compatibility,
                                     const DatabaseConstraint& smaller,
                                     const DatabaseConstraint& greater,
                                     ResourceType level)
    {
      assert(smaller.GetConstraintType() == ConstraintType_SmallerOrEqual &&
             greater.GetConstraintType() == ConstraintType_GreaterOrEqual &&
             smaller.GetTag() == greater.GetTag() &&
             ServerToolbox::IsIdentifier(smaller.GetTag(), level));

      std::list<int64_t> matches;
      compatibility.LookupIdentifierRange(matches, level, smaller.GetTag(),
                                     greater.GetSingleValue(), smaller.GetSingleValue());
      candidates.Intersect(matches);
    }

    
    static void ApplyLevel(SetOfResources& candidates,
                           IDatabaseWrapper& database,
                           ILookupResources& compatibility,
                           const std::vector<DatabaseConstraint>& lookup,
                           ResourceType level)
    {
      typedef std::set<const DatabaseConstraint*>  SetOfConstraints;
      typedef std::map<DicomTag, SetOfConstraints> Identifiers;

      // (1) Select which constraints apply to this level, and split
      // them between "identifier tags" constraints and "main DICOM
      // tags" constraints

      Identifiers       identifiers;
      SetOfConstraints  mainTags;
      
      for (size_t i = 0; i < lookup.size(); i++)
      {
        if (lookup[i].GetLevel() == level)
        {
          if (lookup[i].IsIdentifier())
          {
            identifiers[lookup[i].GetTag()].insert(&lookup[i]);
          }
          else
          {
            mainTags.insert(&lookup[i]);
          }
        }
      }

      
      // (2) Apply the constraints over the identifiers
      
      for (Identifiers::const_iterator it = identifiers.begin();
           it != identifiers.end(); ++it)
      {
        // Check whether some range constraint over identifiers is
        // present at this level
        const DatabaseConstraint* smaller = NULL;
        const DatabaseConstraint* greater = NULL;
        
        for (SetOfConstraints::const_iterator it2 = it->second.begin();
             it2 != it->second.end(); ++it2)
        {
          assert(*it2 != NULL);
        
          if ((*it2)->GetConstraintType() == ConstraintType_SmallerOrEqual)
          {
            smaller = *it2;
          }

          if ((*it2)->GetConstraintType() == ConstraintType_GreaterOrEqual)
          {
            greater = *it2;
          }
        }

        if (smaller != NULL &&
            greater != NULL)
        {
          // There is a range constraint: Apply it, as it is more efficient
          ApplyIdentifierRange(candidates, compatibility, *smaller, *greater, level);
        }
        else
        {
          smaller = NULL;
          greater = NULL;
        }

        for (SetOfConstraints::const_iterator it2 = it->second.begin();
             it2 != it->second.end(); ++it2)
        {
          // Check to avoid applying twice the range constraint
          if (*it2 != smaller &&
              *it2 != greater)
          {
            ApplyIdentifierConstraint(candidates, compatibility, **it2, level);
          }
        }
      }


      // (3) Apply the constraints over the main DICOM tags (no index
      // here, so this is less efficient than filtering over the
      // identifiers)
      if (!mainTags.empty())
      {
        MainTagsConstraints c;
        c.Reserve(mainTags.size());
        
        for (SetOfConstraints::const_iterator it = mainTags.begin();
             it != mainTags.end(); ++it)
        {
          assert(*it != NULL);
          c.Add(**it);
        }

        std::list<int64_t>  source;
        candidates.Flatten(compatibility, source);
        candidates.Clear();

        std::list<int64_t>  filtered;
        for (std::list<int64_t>::const_iterator candidate = source.begin(); 
             candidate != source.end(); ++candidate)
        {
          DicomMap tags;
          database.GetMainDicomTags(tags, *candidate);

          bool match = true;

          for (size_t i = 0; i < c.GetSize(); i++)
          {
            if (!c.GetConstraint(i).IsMatch(tags))
            {
              match = false;
              break;
            }
          }
        
          if (match)
          {
            filtered.push_back(*candidate);
          }
        }

        candidates.Intersect(filtered);
      }
    }


    static std::string GetOneInstance(IDatabaseWrapper& compatibility,
                                      int64_t resource,
                                      ResourceType level)
    {
      for (int i = level; i < ResourceType_Instance; i++)
      {
        assert(compatibility.GetResourceType(resource) == static_cast<ResourceType>(i));

        std::list<int64_t> children;
        compatibility.GetChildrenInternalId(children, resource);
          
        if (children.empty())
        {
          throw OrthancException(ErrorCode_Database);
        }
          
        resource = children.front();
      }

      return compatibility.GetPublicId(resource);
    }
                           

    void DatabaseLookup::ApplyLookupResources(std::list<std::string>& resourcesId,
                                              std::list<std::string>* instancesId,
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
        ApplyLevel(candidates, database_, compatibility_, lookup, static_cast<ResourceType>(level));

        if (level != lowerLevel)
        {
          candidates.GoDown();
        }
      }

      std::list<int64_t> resources;
      candidates.Flatten(compatibility_, resources);

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

      size_t pos = 0;

      for (std::list<int64_t>::const_iterator
             it = resources.begin(); it != resources.end(); ++it, pos++)
      {
        assert(database_.GetResourceType(*it) == queryLevel);

        const std::string resource = database_.GetPublicId(*it);
        resourcesId.push_back(resource);

        if (instancesId != NULL)
        {
          if (queryLevel == ResourceType_Instance)
          {
            // The resource is itself the instance
            instancesId->push_back(resource);
          }
          else
          {
            // Collect one child instance for each of the selected resources
            instancesId->push_back(GetOneInstance(database_, *it, queryLevel));
          }
        }
      }
    }
  }
}
