/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
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


#include "PrecompiledHeadersServer.h"
#include "ResourceFinder.h"

#include "../Core/Logging.h"
#include "FromDcmtkBridge.h"
#include "ServerContext.h"

#include <boost/algorithm/string/predicate.hpp>

namespace Orthanc
{
  class ResourceFinder::CandidateResources
  {
  private:
    typedef std::map<DicomTag, std::string>  Query;

    ResourceFinder&        finder_;
    ServerIndex&           index_;
    ResourceType           level_;
    bool                   isFilterApplied_;
    std::set<std::string>  filtered_;

     
    static void ListToSet(std::set<std::string>& target,
                          const std::list<std::string>& source)
    {
      for (std::list<std::string>::const_iterator
             it = source.begin(); it != source.end(); ++it)
      {
        target.insert(*it);
      }
    }


  public:
    CandidateResources(ResourceFinder& finder) : 
      finder_(finder),
      index_(finder.context_.GetIndex()),
      level_(ResourceType_Patient), 
      isFilterApplied_(false)
    {
    }

    ResourceType GetLevel() const
    {
      return level_;
    }

    void GoDown()
    {
      assert(level_ != ResourceType_Instance);

      if (isFilterApplied_)
      {
        std::set<std::string> tmp = filtered_;

        filtered_.clear();

        for (std::set<std::string>::const_iterator 
               it = tmp.begin(); it != tmp.end(); ++it)
        {
          std::list<std::string> children;
          try
          {
            index_.GetChildren(children, *it);
            ListToSet(filtered_, children);
          }
          catch (OrthancException&)
          {
            // The resource was removed in the meantime
          }
        }
      }

      switch (level_)
      {
        case ResourceType_Patient:
          level_ = ResourceType_Study;
          break;

        case ResourceType_Study:
          level_ = ResourceType_Series;
          break;

        case ResourceType_Series:
          level_ = ResourceType_Instance;
          break;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }
    }


    void Flatten(std::list<std::string>& resources) const
    {
      resources.clear();

      if (isFilterApplied_)
      {
        for (std::set<std::string>::const_iterator 
               it = filtered_.begin(); it != filtered_.end(); ++it)
        {
          resources.push_back(*it);
        }
      }
      else
      {
        index_.GetAllUuids(resources, level_);
      }
    }

    
    void RestrictIdentifier(const IQuery& query,
                            const DicomTag& tag)
    {
      assert((level_ == ResourceType_Patient && tag == DICOM_TAG_PATIENT_ID) ||
             (level_ == ResourceType_Study && tag == DICOM_TAG_STUDY_INSTANCE_UID) ||
             (level_ == ResourceType_Study && tag == DICOM_TAG_ACCESSION_NUMBER) ||
             (level_ == ResourceType_Series && tag == DICOM_TAG_SERIES_INSTANCE_UID) ||
             (level_ == ResourceType_Instance && tag == DICOM_TAG_SOP_INSTANCE_UID));

      std::string value;
      if (!query.RestrictIdentifier(value, tag))
      {
        return;
      }

      LOG(INFO) << "Lookup for identifier tag "
                << FromDcmtkBridge::GetName(tag) << " (value: " << value << ")";

      std::list<std::string> resources;
      index_.LookupIdentifier(resources, tag, value, level_);

      if (isFilterApplied_)
      {
        std::set<std::string>  s;
        ListToSet(s, resources);

        std::set<std::string> tmp = filtered_;
        filtered_.clear();

        for (std::set<std::string>::const_iterator 
               it = tmp.begin(); it != tmp.end(); ++it)
        {
          if (s.find(*it) != s.end())
          {
            filtered_.insert(*it);
          }
        }
      }
      else
      {
        assert(filtered_.empty());
        isFilterApplied_ = true;
        ListToSet(filtered_, resources);
      }
    }


    void RestrictMainDicomTags(const IQuery& query)
    {
      if (!query.HasMainDicomTagsFilter(level_))
      {
        return;
      }

      std::list<std::string> resources;
      Flatten(resources);

      isFilterApplied_ = true;
      filtered_.clear();

      for (std::list<std::string>::const_iterator
             it = resources.begin(); it != resources.end(); ++it)
      {
        DicomMap mainTags;
        if (index_.GetMainDicomTags(mainTags, *it, level_))
        {
          if (query.FilterMainDicomTags(*it, level_, mainTags))
          {
            filtered_.insert(*it);
          }
        }
      }
    }
  };


  ResourceFinder::ResourceFinder(ServerContext& context) : 
    context_(context),
    maxResults_(0)
  {
  }


  void ResourceFinder::ApplyAtLevel(CandidateResources& candidates,
                                    const IQuery& query,
                                    ResourceType level)
  {
    if (level != ResourceType_Patient)
    {
      candidates.GoDown();
    }

    switch (level)
    {
      case ResourceType_Patient:
      {
        candidates.RestrictIdentifier(query, DICOM_TAG_PATIENT_ID);
        break;
      }

      case ResourceType_Study:
      {
        candidates.RestrictIdentifier(query, DICOM_TAG_STUDY_INSTANCE_UID);
        candidates.RestrictIdentifier(query, DICOM_TAG_ACCESSION_NUMBER);
        break;
      }

      case ResourceType_Series:
      {
        candidates.RestrictIdentifier(query, DICOM_TAG_SERIES_INSTANCE_UID);
        break;
      }

      case ResourceType_Instance:
      {
        candidates.RestrictIdentifier(query, DICOM_TAG_SOP_INSTANCE_UID);
        break;
      }

      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    candidates.RestrictMainDicomTags(query);
  }



  static bool LookupOneInstance(std::string& result,
                                ServerIndex& index,
                                const std::string& id,
                                ResourceType type)
  {
    if (type == ResourceType_Instance)
    {
      result = id;
      return true;
    }

    std::string childId;
    
    {
      std::list<std::string> children;
      index.GetChildInstances(children, id);

      if (children.empty())
      {
        return false;
      }

      childId = children.front();
    }

    return LookupOneInstance(result, index, childId, GetChildResourceType(type));
  }


  bool ResourceFinder::Apply(std::list<std::string>& result,
                             const IQuery& query)
  {
    CandidateResources candidates(*this);

    ApplyAtLevel(candidates, query, ResourceType_Patient);

    const ResourceType level = query.GetLevel();

    if (level == ResourceType_Study ||
        level == ResourceType_Series ||
        level == ResourceType_Instance)
    {
      ApplyAtLevel(candidates, query, ResourceType_Study);
    }
        
    if (level == ResourceType_Series ||
        level == ResourceType_Instance)
    {
      ApplyAtLevel(candidates, query, ResourceType_Series);
    }
        
    if (level == ResourceType_Instance)
    {
      ApplyAtLevel(candidates, query, ResourceType_Instance);
    }

    if (!query.HasInstanceFilter())
    {
      candidates.Flatten(result);

      if (maxResults_ != 0 &&
          result.size() >= maxResults_)
      {
        result.resize(maxResults_);
        return false;
      }
      else
      {
        return true;
      }
    }
    else
    {
      std::list<std::string> tmp;
      candidates.Flatten(tmp);
      
      result.clear();
      for (std::list<std::string>::const_iterator 
             resource = tmp.begin(); resource != tmp.end(); ++resource)
      {
        try
        {
          std::string instance;
          if (LookupOneInstance(instance, context_.GetIndex(), *resource, level))
          {
            Json::Value content;
            context_.ReadJson(content, instance);
            if (query.FilterInstance(*resource, content))
            {
              result.push_back(*resource);

              if (maxResults_ != 0 &&
                  result.size() >= maxResults_)
              {
                // Too many results, stop before recording this new match
                return false;
              }
            }
          }
        }
        catch (OrthancException&)
        {
          // This resource has been deleted since the search was started
        }
      }      
    }

    return true;  // All the matching resources have been returned
  }
}
