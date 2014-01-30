/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2013 Medical Physics Department, CHU of Liege,
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

  static std::string ToLowerCase(const std::string& s)
  {
    std::string result = s;
    Toolbox::ToLowerCase(result);
    return result;
  }

  static bool ApplyRangeConstraint(const std::string& value,
                                   const std::string& constraint)
  {
    size_t separator = constraint.find('-');
    std::string lower = ToLowerCase(constraint.substr(0, separator));
    std::string upper = ToLowerCase(constraint.substr(separator + 1));
    std::string v = ToLowerCase(value);

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
    std::string v1 = ToLowerCase(value);

    std::vector<std::string> items;
    Toolbox::TokenizeString(items, constraint, '\\');

    for (size_t i = 0; i < items.size(); i++)
    {
      Toolbox::ToLowerCase(items[i]);
      if (items[i] == v1)
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
      return ToLowerCase(value) == ToLowerCase(constraint);
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


  static bool LookupCandidateResourcesInternal(/* out */ std::list<std::string>& resources,
                                               /* in */  ServerIndex& index,
                                               /* in */  ResourceType level,
                                               /* in */  const DicomMap& query,
                                               /* in */  DicomTag tag)
  {
    if (query.HasTag(tag))
    {
      const DicomValue& value = query.GetValue(tag);
      if (!value.IsNull())
      {
        std::string str = query.GetValue(tag).AsString();
        if (!IsWildcard(str))
        {
          index.LookupTagValue(resources, tag, str/*, level*/);
          return true;
        }
      }
    }

    return false;
  }


  static void LookupCandidateResources(/* out */ std::list<std::string>& resources,
                                       /* in */  ServerIndex& index,
                                       /* in */  ResourceType level,
                                       /* in */  const DicomMap& query,
                                       /* in */  ModalityManufacturer manufacturer)
  {
    // TODO : Speed up using full querying against the MainDicomTags.

    resources.clear();

    bool done = false;

    switch (level)
    {
      case ResourceType_Patient:
        done = LookupCandidateResourcesInternal(resources, index, level, query, DICOM_TAG_PATIENT_ID);
        break;

      case ResourceType_Study:
        done = LookupCandidateResourcesInternal(resources, index, level, query, DICOM_TAG_STUDY_INSTANCE_UID);
        break;

      case ResourceType_Series:
        done = LookupCandidateResourcesInternal(resources, index, level, query, DICOM_TAG_SERIES_INSTANCE_UID);
        break;

      case ResourceType_Instance:
        if (manufacturer == ModalityManufacturer_MedInria)
        {
          std::list<std::string> series;

          if (LookupCandidateResourcesInternal(series, index, ResourceType_Series, query, DICOM_TAG_SERIES_INSTANCE_UID) &&
              series.size() == 1)
          {
            index.GetChildInstances(resources, series.front());
            done = true;
          }          
        }
        else
        {
          done = LookupCandidateResourcesInternal(resources, index, level, query, DICOM_TAG_SOP_INSTANCE_UID);
        }

        break;

      default:
        break;
    }

    if (!done)
    {
      Json::Value allResources;
      index.GetAllUuids(allResources, level);
      assert(allResources.type() == Json::arrayValue);

      for (Json::Value::ArrayIndex i = 0; i < allResources.size(); i++)
      {
        resources.push_back(allResources[i].asString());
      }
    }
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

    switch (manufacturer)
    {
      case ModalityManufacturer_MedInria:
        // MedInria makes FIND requests at the instance level before starting MOVE
        break;

      default:
        if (level != ResourceType_Patient &&
            level != ResourceType_Study &&
            level != ResourceType_Series)
        {
          throw OrthancException(ErrorCode_NotImplemented);
        }
    }


    /**
     * Retrieve the candidate resources for this query level. Whenever
     * possible, we avoid returning ALL the resources for this query
     * level, as it would imply reading the JSON file on the harddisk
     * for each of them.
     **/

    std::list<std::string>  resources;
    LookupCandidateResources(resources, context_.GetIndex(), level, input, manufacturer);


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

    DicomArray query(input);
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
