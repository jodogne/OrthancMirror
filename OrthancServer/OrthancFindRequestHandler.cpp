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

namespace Orthanc
{
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

      if (children.size() == 0)
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


  void OrthancFindRequestHandler::Handle(const DicomMap& input,
                                         DicomFindAnswers& answers)
  {
    LOG(WARNING) << "Find-SCU request received";

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
        level != ResourceType_Series)
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }


    /**
     * Retrieve all the resources for this query level.
     **/

    Json::Value resources;
    context_.GetIndex().GetAllUuids(resources, level);
    assert(resources.type() == Json::arrayValue);

    // TODO : Speed up using MainDicomTags (to avoid looping over ALL
    // the resources and reading the JSON file for each of them)



    /**
     * Apply filtering on modalities for studies, if asked (this is an
     * extension to standard DICOM)
     * http://www.medicalconnections.co.uk/kb/Filtering_on_and_Retrieving_the_Modality_in_a_C_FIND
     **/

    if (level == ResourceType_Study &&
        input.HasTag(DICOM_TAG_MODALITIES_IN_STUDY))
    {
      const DicomValue& v = input.GetValue(DICOM_TAG_MODALITIES_IN_STUDY);
      if (!v.IsNull())
      {
        // Move the allowed modalities into a "std::set"
        std::vector<std::string>  tmp;
        Toolbox::TokenizeString(tmp, v.AsString(), '\\'); 

        std::set<std::string> modalities;
        for (size_t i = 0; i < tmp.size(); i++)
        {
          modalities.insert(tmp[i]);
        }

        // Loop over the studies
        Json::Value studies = resources;
        resources = Json::arrayValue;

        for (Json::Value::ArrayIndex i = 0; i < studies.size(); i++)
        {
          // We are considering a single study. Check whether one of
          // its child series matches one of the modalities.
          Json::Value study;
          if (context_.GetIndex().LookupResource(study, studies[i].asString(), ResourceType_Study))
          {
            // Loop over the series of the considered study.
            for (Json::Value::ArrayIndex j = 0; j < study["Series"].size(); j++)   // (*)
            {
              Json::Value series;
              if (context_.GetIndex().LookupResource(series, study["Series"][j].asString(), ResourceType_Series))
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
                    resources.append(studies[i]);

                    // We have finished considering this study. Break the study loop at (*).
                    break;
                  }
                }
              }
            }
          }
        }
      }
    }


    /**
     * Loop over all the resources for this query level.
     **/

    DicomArray query(input);
    for (Json::Value::ArrayIndex i = 0; i < resources.size(); i++)
    {
      try
      {
        std::string instance;
        if (LookupOneInstance(instance, context_.GetIndex(), resources[i].asString(), level))
        {
          Json::Value resource;
          context_.ReadJson(resource, instance);
        
          if (Matches(resource, query))
          {
            AddAnswer(answers, resource, query);
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
