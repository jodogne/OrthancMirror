/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2014 Medical Physics Department, CHU of Liege,
 * Belgium
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

#include "OrthancFindRequestHandler.h"

#include <glog/logging.h>
#include <boost/regex.hpp> 

#include "../Core/DicomFormat/DicomArray.h"
#include "ServerToolbox.h"
#include "OrthancInitialization.h"

namespace Orthanc
{
  static bool IsWildcard(const std::string& constraint)
  {
    return (constraint.find('-') != std::string::npos ||
            constraint.find('*') != std::string::npos ||
            constraint.find('\\') != std::string::npos ||
            constraint.find('?') != std::string::npos);
  }

  static bool ApplyRangeConstraint(const std::string& value,
                                   const std::string& constraint)
  {
    size_t separator = constraint.find('-');
    std::string lower, upper, v;
    Toolbox::ToLowerCase(lower, constraint.substr(0, separator));
    Toolbox::ToLowerCase(upper, constraint.substr(separator + 1));
    Toolbox::ToLowerCase(v, value);

    if (lower.size() == 0 && upper.size() == 0)
    {
      return false;
    }

    if (lower.size() == 0)
    {
      return v <= upper;
    }

    if (upper.size() == 0)
    {
      return v >= lower;
    }
    
    return (v >= lower && v <= upper);
  }


  static bool ApplyListConstraint(const std::string& value,
                                  const std::string& constraint)
  {
    std::string v1;
    Toolbox::ToLowerCase(v1, value);

    std::vector<std::string> items;
    Toolbox::TokenizeString(items, constraint, '\\');

    for (size_t i = 0; i < items.size(); i++)
    {
      std::string lower;
      Toolbox::ToLowerCase(lower, items[i]);
      if (lower == v1)
      {
        return true;
      }
    }

    return false;
  }


  static bool Matches(const std::string& value,
                      const std::string& constraint)
  {
    // http://www.itk.org/Wiki/DICOM_QueryRetrieve_Explained
    // http://dicomiseasy.blogspot.be/2012/01/dicom-queryretrieve-part-i.html  

    if (constraint.find('-') != std::string::npos)
    {
      return ApplyRangeConstraint(value, constraint);
    }
    
    if (constraint.find('\\') != std::string::npos)
    {
      return ApplyListConstraint(value, constraint);
    }

    if (constraint.find('*') != std::string::npos ||
        constraint.find('?') != std::string::npos)
    {
      // TODO - Cache the constructed regular expression
      boost::regex pattern(Toolbox::WildcardToRegularExpression(constraint),
                           boost::regex::icase /* case insensitive search */);
      return boost::regex_match(value, pattern);
    }
    else
    {
      std::string v, c;
      Toolbox::ToLowerCase(v, value);
      Toolbox::ToLowerCase(c, constraint);
      return v == c;
    }
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


  static bool Matches(const Json::Value& resource,
                      const DicomArray& query)
  {
    for (size_t i = 0; i < query.GetSize(); i++)
    {
      if (query.GetElement(i).GetValue().IsNull() ||
          query.GetElement(i).GetTag() == DICOM_TAG_QUERY_RETRIEVE_LEVEL ||
          query.GetElement(i).GetTag() == DICOM_TAG_SPECIFIC_CHARACTER_SET ||
          query.GetElement(i).GetTag() == DICOM_TAG_MODALITIES_IN_STUDY)
      {
        continue;
      }

      std::string tag = query.GetElement(i).GetTag().Format();
      std::string value;
      if (resource.isMember(tag))
      {
        value = resource.get(tag, Json::arrayValue).get("Value", "").asString();
      }

      if (!Matches(value, query.GetElement(i).GetValue().AsString()))
      {
        return false;
      }
    }

    return true;
  }


  static void AddAnswer(DicomFindAnswers& answers,
                        const Json::Value& resource,
                        const DicomArray& query)
  {
    DicomMap result;

    for (size_t i = 0; i < query.GetSize(); i++)
    {
      if (query.GetElement(i).GetTag() != DICOM_TAG_QUERY_RETRIEVE_LEVEL &&
          query.GetElement(i).GetTag() != DICOM_TAG_SPECIFIC_CHARACTER_SET)
      {
        std::string tag = query.GetElement(i).GetTag().Format();
        std::string value;
        if (resource.isMember(tag))
        {
          value = resource.get(tag, Json::arrayValue).get("Value", "").asString();
          result.SetValue(query.GetElement(i).GetTag(), value);
        }
        else
        {
          result.SetValue(query.GetElement(i).GetTag(), "");
        }
      }
    }

    answers.Add(result);
  }


  static bool ApplyModalitiesInStudyFilter(std::list<std::string>& filteredStudies,
                                           const std::list<std::string>& studies,
                                           const DicomMap& input,
                                           ServerIndex& index)
  {
    filteredStudies.clear();

    const DicomValue& v = input.GetValue(DICOM_TAG_MODALITIES_IN_STUDY);
    if (v.IsNull())
    {
      return false;
    }

    // Move the allowed modalities into a "std::set"
    std::vector<std::string>  tmp;
    Toolbox::TokenizeString(tmp, v.AsString(), '\\'); 

    std::set<std::string> modalities;
    for (size_t i = 0; i < tmp.size(); i++)
    {
      modalities.insert(tmp[i]);
    }

    // Loop over the studies
    for (std::list<std::string>::const_iterator 
           it = studies.begin(); it != studies.end(); ++it)
    {
      try
      {
        // We are considering a single study. Check whether one of
        // its child series matches one of the modalities.
        Json::Value study;
        if (index.LookupResource(study, *it, ResourceType_Study))
        {
          // Loop over the series of the considered study.
          for (Json::Value::ArrayIndex j = 0; j < study["Series"].size(); j++)   // (*)
          {
            Json::Value series;
            if (index.LookupResource(series, study["Series"][j].asString(), ResourceType_Series))
            {
              // Get the modality of this series
              if (series["MainDicomTags"].isMember("Modality"))
              {
                std::string modality = series["MainDicomTags"]["Modality"].asString();
                if (modalities.find(modality) != modalities.end())
                {
                  // This series of the considered study matches one
                  // of the required modalities. Take the study into
                  // consideration for future filtering.
                  filteredStudies.push_back(*it);

                  // We have finished considering this study. Break the study loop at (*).
                  break;
                }
              }
            }
          }
        }
      }
      catch (OrthancException&)
      {
        // This resource has probably been deleted during the find request
      }
    }

    return true;
  }


  namespace
  {
    class CandidateResources
    {
    private:
      ServerIndex&  index_;
      ModalityManufacturer manufacturer_;
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

      void ApplyExactFilter(const DicomTag& tag, const std::string& value)
      {
        LOG(INFO) << "Applying exact filter on tag "
                  << FromDcmtkBridge::GetName(tag) << " (value: " << value << ")";

        std::list<std::string> resources;
        index_.LookupTagValue(resources, tag, value, level_);

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

    public:
      CandidateResources(ServerIndex& index,
                         ModalityManufacturer manufacturer) : 
        index_(index), 
        manufacturer_(manufacturer),
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
          Json::Value tmp;
          index_.GetAllUuids(tmp, level_);
          for (Json::Value::ArrayIndex i = 0; i < tmp.size(); i++)
          {
            resources.push_back(tmp[i].asString());
          }
        }
      }

      void ApplyFilter(const DicomTag& tag, const DicomMap& query)
      {
        if (query.HasTag(tag))
        {
          const DicomValue& value = query.GetValue(tag);
          if (!value.IsNull())
          {
            std::string value = query.GetValue(tag).AsString();
            if (!IsWildcard(value))
            {
              ApplyExactFilter(tag, value);
            }
          }
        }
      }
    };
  }


  void OrthancFindRequestHandler::Handle(DicomFindAnswers& answers,
                                         const DicomMap& input,
                                         const std::string& callingAETitle)
  {
    /**
     * Retrieve the manufacturer of this modality.
     **/

    ModalityManufacturer manufacturer;

    {
      std::string symbolicName, address;
      int port;

      if (!LookupDicomModalityUsingAETitle(callingAETitle, symbolicName, address, port, manufacturer))
      {
        throw OrthancException("Unknown modality");
      }
    }


    /**
     * Retrieve the query level.
     **/

    const DicomValue* levelTmp = input.TestAndGetValue(DICOM_TAG_QUERY_RETRIEVE_LEVEL);
    if (levelTmp == NULL) 
    {
      throw OrthancException(ErrorCode_BadRequest);
    }

    ResourceType level = StringToResourceType(levelTmp->AsString().c_str());

    if (level != ResourceType_Patient &&
        level != ResourceType_Study &&
        level != ResourceType_Series &&
        level != ResourceType_Instance)
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }


    DicomArray query(input);
    LOG(INFO) << "DICOM C-Find request at level: " << EnumerationToString(level);

    for (size_t i = 0; i < query.GetSize(); i++)
    {
      if (!query.GetElement(i).GetValue().IsNull())
      {
        LOG(INFO) << "  " << query.GetElement(i).GetTag()
                  << "  " << FromDcmtkBridge::GetName(query.GetElement(i).GetTag())
                  << " = " << query.GetElement(i).GetValue().AsString();
      }
    }


    /**
     * Retrieve the candidate resources for this query level. Whenever
     * possible, we avoid returning ALL the resources for this query
     * level, as it would imply reading the JSON file on the harddisk
     * for each of them.
     **/

    CandidateResources candidates(context_.GetIndex(), manufacturer);

    for (;;)
    {
      switch (candidates.GetLevel())
      {
        case ResourceType_Patient:
          candidates.ApplyFilter(DICOM_TAG_PATIENT_ID, input);
          break;

        case ResourceType_Study:
          candidates.ApplyFilter(DICOM_TAG_STUDY_INSTANCE_UID, input);
          candidates.ApplyFilter(DICOM_TAG_ACCESSION_NUMBER, input);
          break;

        case ResourceType_Series:
          candidates.ApplyFilter(DICOM_TAG_SERIES_INSTANCE_UID, input);
          break;

        case ResourceType_Instance:
          candidates.ApplyFilter(DICOM_TAG_SOP_INSTANCE_UID, input);
          break;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }      

      if (candidates.GetLevel() == level)
      {
        break;
      }

      candidates.GoDown();
    }

    std::list<std::string>  resources;
    candidates.Flatten(resources);

    LOG(INFO) << "Number of candidate resources after exact filtering: " << resources.size();

    /**
     * Apply filtering on modalities for studies, if asked (this is an
     * extension to standard DICOM)
     * http://www.medicalconnections.co.uk/kb/Filtering_on_and_Retrieving_the_Modality_in_a_C_FIND
     **/

    if (level == ResourceType_Study &&
        input.HasTag(DICOM_TAG_MODALITIES_IN_STUDY))
    {
      std::list<std::string> filtered;
      if (ApplyModalitiesInStudyFilter(filtered, resources, input, context_.GetIndex()))
      {
        resources = filtered;
      }
    }


    /**
     * Loop over all the resources for this query level.
     **/

    for (std::list<std::string>::const_iterator 
           resource = resources.begin(); resource != resources.end(); ++resource)
    {
      try
      {
        std::string instance;
        if (LookupOneInstance(instance, context_.GetIndex(), *resource, level))
        {
          Json::Value info;
          context_.ReadJson(info, instance);
        
          if (Matches(info, query))
          {
            AddAnswer(answers, info, query);
          }
        }
      }
      catch (OrthancException&)
      {
        // This resource has probably been deleted during the find request
      }
    }
  }
}



/**
 * TODO : Case-insensitive match for PN value representation (Patient
 * Name). Case-senstive match for all the other value representations.
 *
 * Reference: DICOM PS 3.4
 *   - C.2.2.2.1 ("Single Value Matching") 
 *   - C.2.2.2.4 ("Wild Card Matching")
 * http://medical.nema.org/Dicom/2011/11_04pu.pdf (
 **/
