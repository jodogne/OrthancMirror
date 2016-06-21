/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
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
#include "../Core/Lua/LuaFunctionCall.h"
#include "../Core/Logging.h"
#include "FromDcmtkBridge.h"
#include "OrthancInitialization.h"
#include "Search/LookupResource.h"
#include "ServerToolbox.h"

#include <boost/regex.hpp> 


namespace Orthanc
{
  static void GetChildren(std::list<std::string>& target,
                          ServerIndex& index,
                          const std::list<std::string>& source)
  {
    target.clear();

    for (std::list<std::string>::const_iterator
           it = source.begin(); it != source.end(); ++it)
    {
      std::list<std::string> tmp;
      index.GetChildren(tmp, *it);
      target.splice(target.end(), tmp);
    }
  }


  static void StoreSetOfStrings(DicomMap& result,
                                const DicomTag& tag,
                                const std::set<std::string>& values)
  {
    bool isFirst = true;

    std::string s;
    for (std::set<std::string>::const_iterator
           it = values.begin(); it != values.end(); ++it)
    {
      if (isFirst)
      {
        isFirst = false;
      }
      else
      {
        s += "\\";
      }

      s += *it;
    }

    result.SetValue(tag, s, false);
  }


  static void ExtractTagFromMainDicomTags(std::set<std::string>& target,
                                          ServerIndex& index,
                                          const DicomTag& tag,
                                          const std::list<std::string>& resources,
                                          ResourceType level)
  {
    for (std::list<std::string>::const_iterator
           it = resources.begin(); it != resources.end(); ++it)
    {
      DicomMap tags;
      if (index.GetMainDicomTags(tags, *it, level, level) &&
          tags.HasTag(tag))
      {
        target.insert(tags.GetValue(tag).GetContent());
      }
    }
  }


  static void ExtractTagFromInstances(std::set<std::string>& target,
                                      ServerContext& context,
                                      const DicomTag& tag,
                                      const std::list<std::string>& instances)
  {
    // WARNING: This function is slow, as it reads the JSON file
    // summarizing each instance of interest from the hard drive.

    std::string formatted = tag.Format();

    for (std::list<std::string>::const_iterator
           it = instances.begin(); it != instances.end(); ++it)
    {
      Json::Value dicom;
      context.ReadJson(dicom, *it);

      if (dicom.isMember(formatted))
      {
        const Json::Value& source = dicom[formatted];

        if (source.type() == Json::objectValue &&
            source.isMember("Type") &&
            source.isMember("Value") &&
            source["Type"].asString() == "String" &&
            source["Value"].type() == Json::stringValue)
        {
          target.insert(source["Value"].asString());
        }
      }
    }
  }


  static void ComputePatientCounters(DicomMap& result,
                                     ServerIndex& index,
                                     const std::string& patient,
                                     const DicomMap& query)
  {
    std::list<std::string> studies;
    index.GetChildren(studies, patient);

    if (query.HasTag(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_STUDIES))
    {
      result.SetValue(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_STUDIES,
                      boost::lexical_cast<std::string>(studies.size()), false);
    }

    if (!query.HasTag(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_SERIES) &&
        !query.HasTag(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_INSTANCES))
    {
      return;
    }

    std::list<std::string> series;
    GetChildren(series, index, studies);
    studies.clear();  // This information is useless below
    
    if (query.HasTag(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_SERIES))
    {
      result.SetValue(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_SERIES,
                      boost::lexical_cast<std::string>(series.size()), false);
    }

    if (!query.HasTag(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_INSTANCES))
    {
      return;
    }

    std::list<std::string> instances;
    GetChildren(instances, index, series);

    if (query.HasTag(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_INSTANCES))
    {
      result.SetValue(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_INSTANCES,
                      boost::lexical_cast<std::string>(instances.size()), false);
    }
  }


  static void ComputeStudyCounters(DicomMap& result,
                                   ServerContext& context,
                                   const std::string& study,
                                   const DicomMap& query)
  {
    ServerIndex& index = context.GetIndex();

    std::list<std::string> series;
    index.GetChildren(series, study);
    
    if (query.HasTag(DICOM_TAG_NUMBER_OF_STUDY_RELATED_SERIES))
    {
      result.SetValue(DICOM_TAG_NUMBER_OF_STUDY_RELATED_SERIES,
                      boost::lexical_cast<std::string>(series.size()), false);
    }

    if (query.HasTag(DICOM_TAG_MODALITIES_IN_STUDY))
    {
      std::set<std::string> values;
      ExtractTagFromMainDicomTags(values, index, DICOM_TAG_MODALITY, series, ResourceType_Series);
      StoreSetOfStrings(result, DICOM_TAG_MODALITIES_IN_STUDY, values);
    }

    if (!query.HasTag(DICOM_TAG_NUMBER_OF_STUDY_RELATED_INSTANCES) &&
        !query.HasTag(DICOM_TAG_SOP_CLASSES_IN_STUDY))
    {
      return;
    }

    std::list<std::string> instances;
    GetChildren(instances, index, series);

    if (query.HasTag(DICOM_TAG_NUMBER_OF_STUDY_RELATED_INSTANCES))
    {
      result.SetValue(DICOM_TAG_NUMBER_OF_STUDY_RELATED_INSTANCES,
                      boost::lexical_cast<std::string>(instances.size()), false);
    }

    if (query.HasTag(DICOM_TAG_SOP_CLASSES_IN_STUDY))
    {
      if (Configuration::GetGlobalBoolParameter("AllowFindSopClassesInStudy", false))
      {
        std::set<std::string> values;
        ExtractTagFromInstances(values, context, DICOM_TAG_SOP_CLASS_UID, instances);
        StoreSetOfStrings(result, DICOM_TAG_SOP_CLASSES_IN_STUDY, values);
      }
      else
      {
        result.SetValue(DICOM_TAG_SOP_CLASSES_IN_STUDY, "", false);
        LOG(WARNING) << "The handling of \"SOP Classes in Study\" (0008,0062) "
                     << "in C-FIND requests is disabled";
      }
    }
  }


  static void ComputeSeriesCounters(DicomMap& result,
                                    ServerIndex& index,
                                    const std::string& series,
                                    const DicomMap& query)
  {
    std::list<std::string> instances;
    index.GetChildren(instances, series);

    if (query.HasTag(DICOM_TAG_NUMBER_OF_SERIES_RELATED_INSTANCES))
    {
      result.SetValue(DICOM_TAG_NUMBER_OF_SERIES_RELATED_INSTANCES,
                      boost::lexical_cast<std::string>(instances.size()), false);
    }
  }


  static DicomMap* ComputeCounters(ServerContext& context,
                                   const std::string& instanceId,
                                   ResourceType level,
                                   const DicomMap& query)
  {
    switch (level)
    {
      case ResourceType_Patient:
        if (!query.HasTag(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_STUDIES) &&
            !query.HasTag(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_SERIES) &&
            !query.HasTag(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_INSTANCES))
        {
          return NULL;
        }

        break;

      case ResourceType_Study:
        if (!query.HasTag(DICOM_TAG_NUMBER_OF_STUDY_RELATED_SERIES) &&
            !query.HasTag(DICOM_TAG_NUMBER_OF_STUDY_RELATED_INSTANCES) &&
            !query.HasTag(DICOM_TAG_SOP_CLASSES_IN_STUDY) &&
            !query.HasTag(DICOM_TAG_MODALITIES_IN_STUDY))
        {
          return NULL;
        }

        break;

      case ResourceType_Series:
        if (!query.HasTag(DICOM_TAG_NUMBER_OF_SERIES_RELATED_INSTANCES))
        {
          return NULL;
        }

        break;

      default:
        return NULL;
    }

    std::string parent;
    if (!context.GetIndex().LookupParent(parent, instanceId, level))
    {
      throw OrthancException(ErrorCode_UnknownResource);  // The resource was deleted in between
    }

    std::auto_ptr<DicomMap> result(new DicomMap);

    switch (level)
    {
      case ResourceType_Patient:
        ComputePatientCounters(*result, context.GetIndex(), parent, query);
        break;

      case ResourceType_Study:
        ComputeStudyCounters(*result, context, parent, query);
        break;

      case ResourceType_Series:
        ComputeSeriesCounters(*result, context.GetIndex(), parent, query);
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    return result.release();
  }


  static void AddAnswer(DicomFindAnswers& answers,
                        const Json::Value& resource,
                        const DicomArray& query,
                        const std::list<DicomTag>& sequencesToReturn,
                        const DicomMap* counters)
  {
    DicomMap result;

    for (size_t i = 0; i < query.GetSize(); i++)
    {
      if (query.GetElement(i).GetTag() == DICOM_TAG_QUERY_RETRIEVE_LEVEL)
      {
        // Fix issue 30 on Google Code (QR response missing "Query/Retrieve Level" (008,0052))
        result.SetValue(query.GetElement(i).GetTag(), query.GetElement(i).GetValue());
      }
      else if (query.GetElement(i).GetTag() == DICOM_TAG_SPECIFIC_CHARACTER_SET)
      {
        // Do not include the encoding, this is handled by class ParsedDicomFile
      }
      else
      {
        std::string tag = query.GetElement(i).GetTag().Format();
        std::string value;
        if (resource.isMember(tag))
        {
          value = resource.get(tag, Json::arrayValue).get("Value", "").asString();
          result.SetValue(query.GetElement(i).GetTag(), value, false);
        }
        else
        {
          result.SetValue(query.GetElement(i).GetTag(), "", false);
        }
      }
    }

    if (counters != NULL)
    {
      DicomArray tmp(*counters);
      for (size_t i = 0; i < tmp.GetSize(); i++)
      {
        result.SetValue(tmp.GetElement(i).GetTag(), tmp.GetElement(i).GetValue().GetContent(), false);
      }
    }

    if (result.GetSize() == 0 &&
        sequencesToReturn.empty())
    {
      LOG(WARNING) << "The C-FIND request does not return any DICOM tag";
    }
    else if (sequencesToReturn.empty())
    {
      answers.Add(result);
    }
    else
    {
      ParsedDicomFile dicom(result);

      for (std::list<DicomTag>::const_iterator tag = sequencesToReturn.begin();
           tag != sequencesToReturn.end(); ++tag)
      {
        const Json::Value& source = resource[tag->Format()];

        if (source.type() == Json::objectValue &&
            source.isMember("Type") &&
            source.isMember("Value") &&
            source["Type"].asString() == "Sequence" &&
            source["Value"].type() == Json::arrayValue)
        {
          Json::Value content = Json::arrayValue;

          for (Json::Value::ArrayIndex i = 0; i < source["Value"].size(); i++)
          {
            Json::Value item;
            Toolbox::SimplifyTags(item, source["Value"][i], DicomToJsonFormat_Short);
            content.append(item);
          }

          dicom.Replace(*tag, content, false, DicomReplaceMode_InsertIfAbsent);
        }
      }

      answers.Add(dicom);
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


  bool OrthancFindRequestHandler::ApplyLuaFilter(DicomMap& target,
                                                 const DicomMap& source,
                                                 const std::string& remoteIp,
                                                 const std::string& remoteAet,
                                                 const std::string& calledAet)
  {
    Json::Value output;

    {
      LuaScripting::Locker locker(context_.GetLua());
      static const char* NAME = "IncomingFindRequestFilter";
      
      if (!locker.GetLua().IsExistingFunction(NAME))
      {
        return false;
      }

      Json::Value tmp = Json::objectValue;
      DicomArray a(source);

      for (size_t i = 0; i < a.GetSize(); i++)
      {
        const DicomValue& v = a.GetElement(i).GetValue();
        std::string s = (v.IsNull() || v.IsBinary()) ? "" : v.GetContent();
        tmp[a.GetElement(i).GetTag().Format()] = s;
      }

      Json::Value origin = Json::objectValue;
      origin["RemoteIp"] = remoteIp;
      origin["RemoteAet"] = remoteAet;
      origin["CalledAet"] = calledAet;

      LuaFunctionCall call(locker.GetLua(), NAME);
      call.PushJson(tmp);
      call.PushJson(origin);

      call.ExecuteToJson(output, true);
    }

    // The Lua context is released at this point

    if (output.type() != Json::objectValue)
    {
      LOG(ERROR) << "Lua: IncomingFindRequestFilter must return a table";
      throw OrthancException(ErrorCode_LuaBadOutput);
    }

    Json::Value::Members members = output.getMemberNames();

    for (size_t i = 0; i < members.size(); i++)
    {
      if (output[members[i]].type() != Json::stringValue)
      {
        LOG(ERROR) << "Lua: IncomingFindRequestFilter must return a table mapping names of DICOM tags to strings";
        throw OrthancException(ErrorCode_LuaBadOutput);
      }

      DicomTag tag(FromDcmtkBridge::ParseTag(members[i]));
      target.SetValue(tag, output[members[i]].asString(), false);
    }

    return true;
  }


  void OrthancFindRequestHandler::Handle(DicomFindAnswers& answers,
                                         const DicomMap& input,
                                         const std::list<DicomTag>& sequencesToReturn,
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
     * Possibly apply the user-supplied Lua filter.
     **/

    DicomMap lua;
    const DicomMap* filteredInput = &input;

    if (ApplyLuaFilter(lua, input, remoteIp, remoteAet, calledAet))
    {
      filteredInput = &lua;
    }


    /**
     * Retrieve the query level.
     **/

    assert(filteredInput != NULL);
    const DicomValue* levelTmp = filteredInput->TestAndGetValue(DICOM_TAG_QUERY_RETRIEVE_LEVEL);
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


    DicomArray query(*filteredInput);
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

    for (std::list<DicomTag>::const_iterator it = sequencesToReturn.begin();
         it != sequencesToReturn.end(); ++it)
    {
      LOG(INFO) << "  (" << it->Format()
                << ")  " << FromDcmtkBridge::GetName(*it)
                << " : sequence tag whose content will be copied";
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
        ValueRepresentation vr = FromDcmtkBridge::LookupValueRepresentation(tag);

        // DICOM specifies that searches must be case sensitive, except
        // for tags with a PN value representation
        bool sensitive = true;
        if (vr == ValueRepresentation_PersonName)
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
          std::auto_ptr<DicomMap> counters(ComputeCounters(context_, instances[i], level, input));
          AddAnswer(answers, dicom, query, sequencesToReturn, counters.get());
        }
      }
    }

    LOG(INFO) << "Number of matching resources: " << answers.GetSize();

    answers.SetComplete(complete);
  }
}
