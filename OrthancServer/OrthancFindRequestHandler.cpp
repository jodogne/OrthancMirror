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
#include "OrthancFindRequestHandler.h"

#include "../Core/Logging.h"
#include "../Core/DicomFormat/DicomArray.h"
#include "ServerToolbox.h"
#include "OrthancInitialization.h"
#include "FromDcmtkBridge.h"

#include "ResourceFinder.h"
#include "DicomFindQuery.h"

#include <boost/regex.hpp> 


namespace Orthanc
{
  static void AddAnswer(DicomFindAnswers& answers,
                        const Json::Value& resource,
                        const DicomArray& query)
  {
    DicomMap result;

    for (size_t i = 0; i < query.GetSize(); i++)
    {
      // Fix issue 30 (QR response missing "Query/Retrieve Level" (008,0052))
      if (query.GetElement(i).GetTag() == DICOM_TAG_QUERY_RETRIEVE_LEVEL)
      {
        result.SetValue(query.GetElement(i).GetTag(), query.GetElement(i).GetValue());
      }
      else if (query.GetElement(i).GetTag() == DICOM_TAG_SPECIFIC_CHARACTER_SET)
      {
      }
      else
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

    if (result.GetSize() == 0)
    {
      LOG(WARNING) << "The C-FIND request does not return any DICOM tag";
    }
    else
    {
      answers.Add(result);
    }
  }


  namespace
  {
    class CFindQuery : public DicomFindQuery
    {
    private:
      DicomFindAnswers&      answers_;
      ServerIndex&           index_;
      const DicomArray&      query_;
      bool                   hasModalitiesInStudy_;
      std::set<std::string>  modalitiesInStudy_;

    public:
      CFindQuery(DicomFindAnswers& answers,
                 ServerIndex& index,
                 const DicomArray& query) :
        answers_(answers),
        index_(index),
        query_(query),
        hasModalitiesInStudy_(false)
      {
      }

      void SetModalitiesInStudy(const std::string& value)
      {
        hasModalitiesInStudy_ = true;
        
        std::vector<std::string>  tmp;
        Toolbox::TokenizeString(tmp, value, '\\'); 

        for (size_t i = 0; i < tmp.size(); i++)
        {
          modalitiesInStudy_.insert(tmp[i]);
        }
      }

      virtual bool HasMainDicomTagsFilter(ResourceType level) const
      {
        if (DicomFindQuery::HasMainDicomTagsFilter(level))
        {
          return true;
        }

        return (level == ResourceType_Study &&
                hasModalitiesInStudy_);
      }

      virtual bool FilterMainDicomTags(const std::string& resourceId,
                                       ResourceType level,
                                       const DicomMap& mainTags) const
      {
        if (!DicomFindQuery::FilterMainDicomTags(resourceId, level, mainTags))
        {
          return false;
        }

        if (level != ResourceType_Study ||
            !hasModalitiesInStudy_)
        {
          return true;
        }

        try
        {
          // We are considering a single study, and the
          // "MODALITIES_IN_STUDY" tag is set in the C-Find. Check
          // whether one of its child series matches one of the
          // modalities.

          Json::Value study;
          if (index_.LookupResource(study, resourceId, ResourceType_Study))
          {
            // Loop over the series of the considered study.
            for (Json::Value::ArrayIndex j = 0; j < study["Series"].size(); j++)
            {
              Json::Value series;
              if (index_.LookupResource(series, study["Series"][j].asString(), ResourceType_Series))
              {
                // Get the modality of this series
                if (series["MainDicomTags"].isMember("Modality"))
                {
                  std::string modality = series["MainDicomTags"]["Modality"].asString();
                  if (modalitiesInStudy_.find(modality) != modalitiesInStudy_.end())
                  {
                    // This series of the considered study matches one
                    // of the required modalities. Take the study into
                    // consideration for future filtering.
                    return true;
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

        return false;
      }

      virtual bool HasInstanceFilter() const
      {
        return true;
      }

      virtual bool FilterInstance(const std::string& instanceId,
                                  const Json::Value& content) const
      {
        bool ok = DicomFindQuery::FilterInstance(instanceId, content);

        if (ok)
        {
          // Add this resource to the answers
          AddAnswer(answers_, content, query_);
        }

        return ok;
      }
    };
  }



  bool OrthancFindRequestHandler::Handle(DicomFindAnswers& answers,
                                         const DicomMap& input,
                                         const std::string& callingAETitle)
  {
    /**
     * Ensure that the calling modality is known to Orthanc.
     **/

    RemoteModalityParameters modality;

    if (!Configuration::LookupDicomModalityUsingAETitle(modality, callingAETitle))
    {
      throw OrthancException("Unknown modality");
    }

    // ModalityManufacturer manufacturer = modality.GetManufacturer();

    bool caseSensitivePN = Configuration::GetGlobalBoolParameter("CaseSensitivePN", false);


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
     * Build up the query object.
     **/

    CFindQuery findQuery(answers, context_.GetIndex(), query);
    findQuery.SetLevel(level);
        
    for (size_t i = 0; i < query.GetSize(); i++)
    {
      const DicomTag tag = query.GetElement(i).GetTag();

      if (query.GetElement(i).GetValue().IsNull() ||
          tag == DICOM_TAG_QUERY_RETRIEVE_LEVEL ||
          tag == DICOM_TAG_SPECIFIC_CHARACTER_SET)
      {
        continue;
      }

      std::string value = query.GetElement(i).GetValue().AsString();
      if (value.size() == 0)
      {
        // An empty string corresponds to a "*" wildcard constraint, so we ignore it
        continue;
      }

      if (tag == DICOM_TAG_MODALITIES_IN_STUDY)
      {
        findQuery.SetModalitiesInStudy(value);
      }
      else
      {
        findQuery.SetConstraint(tag, value, caseSensitivePN);
      }
    }


    /**
     * Run the query.
     **/

    ResourceFinder finder(context_);

    switch (level)
    {
      case ResourceType_Patient:
      case ResourceType_Study:
      case ResourceType_Series:
        finder.SetMaxResults(maxResults_);
        break;

      case ResourceType_Instance:
        finder.SetMaxResults(maxInstances_);
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    std::list<std::string> tmp;
    bool finished = finder.Apply(tmp, findQuery);

    LOG(INFO) << "Number of matching resources: " << tmp.size();

    return finished;
  }
}
