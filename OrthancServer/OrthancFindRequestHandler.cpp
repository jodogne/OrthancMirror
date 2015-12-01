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

#include "../Core/DicomFormat/DicomArray.h"
#include "../Core/Logging.h"
#include "FromDcmtkBridge.h"
#include "OrthancInitialization.h"
#include "Search/LookupResource.h"
#include "ServerToolbox.h"

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



  bool OrthancFindRequestHandler::FilterQueryTag(std::string& value /* can be modified */,
                                                 ResourceType level,
                                                 const DicomTag& tag,
                                                 ModalityManufacturer manufacturer)
  {
    switch (manufacturer)
    {
      case ModalityManufacturer_EFilm2:
        // Following Denis Nesterov's mail on 2015-11-30
        if (tag == DicomTag(0x0008, 0x0000) ||  // "GenericGroupLength"
            tag == DicomTag(0x0010, 0x0000) ||  // "GenericGroupLength"
            tag == DicomTag(0x0020, 0x0000))    // "GenericGroupLength"
        {
          return false;
        }

        break;

      case ModalityManufacturer_Vitrea:
        // Following Denis Nesterov's mail on 2015-11-30
        if (tag == DicomTag(0x5653, 0x0010))  // "PrivateCreator = Vital Images SW 3.4"
        {
          return false;
        }

        break;

      default:
        break;
    }

    return true;
  }


  void OrthancFindRequestHandler::Handle(DicomFindAnswers& answers,
                                         const DicomMap& input,
                                         const std::string& remoteIp,
                                         const std::string& remoteAet,
                                         const std::string& calledAet)
  {
    /**
     * Ensure that the remote modality is known to Orthanc.
     **/

    RemoteModalityParameters modality;

    if (!Configuration::LookupDicomModalityUsingAETitle(modality, remoteAet))
    {
      throw OrthancException(ErrorCode_UnknownModality);
    }

    bool caseSensitivePN = Configuration::GetGlobalBoolParameter("CaseSensitivePN", false);


    /**
     * Retrieve the query level.
     **/

    const DicomValue* levelTmp = input.TestAndGetValue(DICOM_TAG_QUERY_RETRIEVE_LEVEL);
    if (levelTmp == NULL ||
        levelTmp->IsNull() ||
        levelTmp->IsBinary())
    {
      LOG(ERROR) << "C-FIND request without the tag 0008,0052 (QueryRetrieveLevel)";
      throw OrthancException(ErrorCode_BadRequest);
    }

    ResourceType level = StringToResourceType(levelTmp->GetContent().c_str());

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
                  << " = " << query.GetElement(i).GetValue().GetContent();
      }
    }


    /**
     * Build up the query object.
     **/

    LookupResource finder(level);

    for (size_t i = 0; i < query.GetSize(); i++)
    {
      const DicomTag tag = query.GetElement(i).GetTag();

      if (query.GetElement(i).GetValue().IsNull() ||
          tag == DICOM_TAG_QUERY_RETRIEVE_LEVEL ||
          tag == DICOM_TAG_SPECIFIC_CHARACTER_SET)
      {
        continue;
      }

      std::string value = query.GetElement(i).GetValue().GetContent();
      if (value.size() == 0)
      {
        // An empty string corresponds to a "*" wildcard constraint, so we ignore it
        continue;
      }

      if (FilterQueryTag(value, level, tag, modality.GetManufacturer()))
      {
        ValueRepresentation vr = FromDcmtkBridge::GetValueRepresentation(tag);

        // DICOM specifies that searches must be case sensitive, except
        // for tags with a PN value representation
        bool sensitive = true;
        if (vr == ValueRepresentation_PatientName)
        {
          sensitive = caseSensitivePN;
        }

        finder.AddDicomConstraint(tag, value, sensitive);
      }
      else
      {
        LOG(INFO) << "Because of a patch for the manufacturer of the remote modality, " 
                  << "ignoring constraint on tag (" << tag.Format() << ") " << FromDcmtkBridge::GetName(tag);
      }
    }


    /**
     * Run the query.
     **/

    size_t maxResults = (level == ResourceType_Instance) ? maxInstances_ : maxResults_;

    std::vector<std::string> resources, instances;
    context_.GetIndex().FindCandidates(resources, instances, finder);

    assert(resources.size() == instances.size());
    bool complete = true;

    for (size_t i = 0; i < instances.size(); i++)
    {
      Json::Value dicom;
      context_.ReadJson(dicom, instances[i]);
      
      if (finder.IsMatch(dicom))
      {
        if (maxResults != 0 &&
            answers.GetSize() >= maxResults)
        {
          complete = false;
          break;
        }
        else
        {
          AddAnswer(answers, dicom, query);
        }
      }
    }

    LOG(INFO) << "Number of matching resources: " << answers.GetSize();

    answers.SetComplete(complete);
  }
}
