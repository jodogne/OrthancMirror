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

#include "FromDcmtkBridge.h"

#include <glog/logging.h>
#include <boost/algorithm/string/predicate.hpp>

namespace Orthanc
{
  static bool Compare(const std::string& a,
                      const std::string& b,
                      bool caseSensitive)
  {
    if (caseSensitive)
    {
      return a == b;
    }
    else
    {
      return boost::iequals(a, b);
    }
  }


  class ResourceFinder::CandidateResources
  {
  private:
    typedef std::map<DicomTag, std::string>  Query;

    ServerIndex&  index_;
    ResourceType  level_;
    bool  isFilterApplied_;
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


    void RestrictIdentifier(const DicomTag& tag, 
                            const std::string& value)
    {
      assert((level_ == ResourceType_Patient && tag == DICOM_TAG_PATIENT_ID) ||
             (level_ == ResourceType_Study && tag == DICOM_TAG_STUDY_INSTANCE_UID) ||
             (level_ == ResourceType_Study && tag == DICOM_TAG_ACCESSION_NUMBER) ||
             (level_ == ResourceType_Series && tag == DICOM_TAG_SERIES_INSTANCE_UID) ||
             (level_ == ResourceType_Instance && tag == DICOM_TAG_SOP_INSTANCE_UID));

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


    void RestrictIdentifier(Query& query,
                            const DicomTag& tag)
    {
      Query::iterator it = query.find(tag);
      if (it != query.end())
      {
        RestrictIdentifier(it->first, it->second);
        query.erase(it);
      }
    }


  public:
    CandidateResources(ServerIndex& index) : 
      index_(index), 
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
          index_.GetChildren(children, *it);
          ListToSet(filtered_, children);
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


    void RestrictIdentifier(Query& query)
    {
      switch (level_)
      {
        case ResourceType_Patient:
        {
          RestrictIdentifier(query, DICOM_TAG_PATIENT_ID);
          break;
        }

        case ResourceType_Study:
        {
          RestrictIdentifier(query, DICOM_TAG_STUDY_INSTANCE_UID);
          RestrictIdentifier(query, DICOM_TAG_ACCESSION_NUMBER);
          break;
        }

        case ResourceType_Series:
        {
          RestrictIdentifier(query, DICOM_TAG_SERIES_INSTANCE_UID);
          break;
        }

        case ResourceType_Instance:
        {
          RestrictIdentifier(query, DICOM_TAG_SOP_INSTANCE_UID);
          break;
        }

        default:
          throw OrthancException(ErrorCode_InternalError);
      }
    }


    void RestrictMainDicomTags(const Query& query,
                               bool caseSensitive)
    {
      if (query.size() == 0)
      {
        return;
      }

      std::list<std::string> resources;
      Flatten(resources);

      isFilterApplied_ = true;
      filtered_.clear();

      for (std::list<std::string>::const_iterator
             it = resources.begin(); it != resources.end(); it++)
      {
        DicomMap mainTags;
        if (index_.GetMainDicomTags(mainTags, *it, level_))
        {
          for (Query::const_iterator tag = query.begin(); 
               tag != query.end(); ++tag)
          {
            assert(DicomMap::IsMainDicomTag(tag->first, level_));
            if (tag->first != DICOM_TAG_PATIENT_ID &&
                tag->first != DICOM_TAG_STUDY_INSTANCE_UID &&
                tag->first != DICOM_TAG_ACCESSION_NUMBER &&
                tag->first != DICOM_TAG_SERIES_INSTANCE_UID &&
                tag->first != DICOM_TAG_SOP_INSTANCE_UID)
            {
              LOG(INFO) << "Lookup for main DICOM tag "
                        << FromDcmtkBridge::GetName(tag->first) << " (value: " << tag->second << ")";
                
              const DicomValue* value = mainTags.TestAndGetValue(tag->first);
              if (value != NULL &&
                  Compare(value->AsString(), tag->second, caseSensitive))
              {
                filtered_.insert(*it);
              }
            }
          }            
        }
      }
    }
  };


  ResourceFinder::ResourceFinder(ServerIndex& index) : 
    index_(index),
    level_(ResourceType_Patient),
    caseSensitive_(true)
  {
  }


  void ResourceFinder::AddTag(const std::string& tag,
                              const std::string& value)
  {
    AddTag(FromDcmtkBridge::ParseTag(tag.c_str()), value);
  }


  void ResourceFinder::ExtractTagsForLevel(Query& target,
                                           Query& source,
                                           ResourceType level)
  {
    typedef std::set<DicomTag>  Tags;

    Tags  tags;
    DicomMap::GetMainDicomTags(tags, level);

    target.clear();

    for (Tags::const_iterator tag = tags.begin(); tag != tags.end(); tag++)
    {
      Query::iterator value = source.find(*tag);
      if (value != source.end())
      {
        target.insert(*value);
        source.erase(value);
      }
    }
  }


  void ResourceFinder::ApplyAtLevel(CandidateResources& candidates,
                                    ResourceType level)
  {
    if (level != ResourceType_Patient)
    {
      candidates.GoDown();
    }

    candidates.RestrictIdentifier(query_);

    Query tmp;
    ExtractTagsForLevel(tmp, query_, level);
    candidates.RestrictMainDicomTags(tmp, caseSensitive_);
  }


  void ResourceFinder::Apply(std::list<std::string>& result)
  {
    CandidateResources candidates(index_);

    ApplyAtLevel(candidates, ResourceType_Patient);

    if (level_ == ResourceType_Study ||
        level_ == ResourceType_Series ||
        level_ == ResourceType_Instance)
    {
      ApplyAtLevel(candidates, ResourceType_Study);
    }
        
    if (level_ == ResourceType_Series ||
        level_ == ResourceType_Instance)
    {
      ApplyAtLevel(candidates, ResourceType_Series);
    }
        
    if (level_ == ResourceType_Instance)
    {
      ApplyAtLevel(candidates, ResourceType_Instance);
    }
        
    if (!query_.empty())
    {
      LOG(ERROR) << "Invalid query: Searching against a tag that is not valid for the requested level";
      throw OrthancException(ErrorCode_BadRequest);
    }

    candidates.Flatten(result);
  }
}
