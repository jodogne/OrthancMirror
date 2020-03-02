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


#include "PrecompiledHeadersServer.h"
#include "OrthancFindRequestHandler.h"

#include "../Core/DicomFormat/DicomArray.h"
#include "../Core/DicomParsing/FromDcmtkBridge.h"
#include "../Core/Logging.h"
#include "../Core/Lua/LuaFunctionCall.h"
#include "../Core/MetricsRegistry.h"
#include "OrthancConfiguration.h"
#include "Search/DatabaseLookup.h"
#include "ServerContext.h"
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

      for (std::list<std::string>::const_iterator
             it = series.begin(); it != series.end(); ++it)
      {
        DicomMap tags;
        if (index.GetMainDicomTags(tags, *it, ResourceType_Series, ResourceType_Series))
        {
          const DicomValue* value = tags.TestAndGetValue(DICOM_TAG_MODALITY);

          if (value != NULL &&
              !value->IsNull() &&
              !value->IsBinary())
          {
            values.insert(value->GetContent());
          }
        }
      }

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
      std::set<std::string> values;

      for (std::list<std::string>::const_iterator
             it = instances.begin(); it != instances.end(); ++it)
      {
        std::string value;
        if (context.LookupOrReconstructMetadata(value, *it, MetadataType_Instance_SopClassUid))
        {
          values.insert(value);
        }
      }

      StoreSetOfStrings(result, DICOM_TAG_SOP_CLASSES_IN_STUDY, values);
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

    std::unique_ptr<DicomMap> result(new DicomMap);

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
                        const DicomMap& mainDicomTags,
                        const Json::Value* dicomAsJson,
                        const DicomArray& query,
                        const std::list<DicomTag>& sequencesToReturn,
                        const DicomMap* counters)
  {
    DicomMap match;

    if (dicomAsJson != NULL)
    {
      match.FromDicomAsJson(*dicomAsJson);
    }
    else
    {
      match.Assign(mainDicomTags);
    }
    
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
        const DicomTag& tag = query.GetElement(i).GetTag();
        const DicomValue* value = match.TestAndGetValue(tag);

        if (value != NULL &&
            !value->IsNull() &&
            !value->IsBinary())
        {
          result.SetValue(tag, value->GetContent(), false);
        }
        else
        {
          result.SetValue(tag, "", false);
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
    else if (dicomAsJson == NULL)
    {
      LOG(WARNING) << "C-FIND query requesting a sequence, but reading JSON from disk is disabled";
      answers.Add(result);
    }
    else
    {
      ParsedDicomFile dicom(result, GetDefaultDicomEncoding(), true /* be permissive, cf. issue #136 */);

      for (std::list<DicomTag>::const_iterator tag = sequencesToReturn.begin();
           tag != sequencesToReturn.end(); ++tag)
      {
        assert(dicomAsJson != NULL);
        const Json::Value& source = (*dicomAsJson) [tag->Format()];

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
            ServerToolbox::SimplifyTags(item, source["Value"][i], DicomToJsonFormat_Short);
            content.append(item);
          }

          dicom.Replace(*tag, content, false, DicomReplaceMode_InsertIfAbsent,
                        "" /* no private creator */);
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
    // Whatever the manufacturer, remove the GenericGroupLength tags
    // http://dicom.nema.org/medical/dicom/current/output/chtml/part05/sect_7.2.html
    // https://bitbucket.org/sjodogne/orthanc/issues/31/
    if (tag.GetElement() == 0x0000)
    {
      return false;
    }

    switch (manufacturer)
    {
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
                                                 const std::string& calledAet,
                                                 ModalityManufacturer manufacturer)
  {
    static const char* LUA_CALLBACK = "IncomingFindRequestFilter";
    
    LuaScripting::Lock lock(context_.GetLuaScripting());

    if (!lock.GetLua().IsExistingFunction(LUA_CALLBACK))
    {
      return false;
    }
    else
    {
      Json::Value origin;
      FormatOrigin(origin, remoteIp, remoteAet, calledAet, manufacturer);

      LuaFunctionCall call(lock.GetLua(), LUA_CALLBACK);
      call.PushDicom(source);
      call.PushJson(origin);
      FromDcmtkBridge::ExecuteToDicom(target, call);

      return true;
    }
  }


  OrthancFindRequestHandler::OrthancFindRequestHandler(ServerContext& context) :
    context_(context),
    maxResults_(0),
    maxInstances_(0)
  {
  }


  class OrthancFindRequestHandler::LookupVisitor : public ServerContext::ILookupVisitor
  {
  private:
    DicomFindAnswers&           answers_;
    ServerContext&              context_;
    ResourceType                level_;
    const DicomMap&             query_;
    DicomArray                  queryAsArray_;
    const std::list<DicomTag>&  sequencesToReturn_;

  public:
    LookupVisitor(DicomFindAnswers&  answers,
                  ServerContext& context,
                  ResourceType level,
                  const DicomMap& query,
                  const std::list<DicomTag>& sequencesToReturn) :
      answers_(answers),
      context_(context),
      level_(level),
      query_(query),
      queryAsArray_(query),
      sequencesToReturn_(sequencesToReturn)
    {
      answers_.SetComplete(false);
    }

    virtual bool IsDicomAsJsonNeeded() const
    {
      // Ask the "DICOM-as-JSON" attachment only if sequences are to
      // be returned OR if "query_" contains non-main DICOM tags!

      DicomMap withoutSpecialTags;
      withoutSpecialTags.Assign(query_);

      // Check out "ComputeCounters()"
      withoutSpecialTags.Remove(DICOM_TAG_MODALITIES_IN_STUDY);
      withoutSpecialTags.Remove(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_INSTANCES);
      withoutSpecialTags.Remove(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_SERIES);
      withoutSpecialTags.Remove(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_STUDIES);
      withoutSpecialTags.Remove(DICOM_TAG_NUMBER_OF_SERIES_RELATED_INSTANCES);
      withoutSpecialTags.Remove(DICOM_TAG_NUMBER_OF_STUDY_RELATED_INSTANCES);
      withoutSpecialTags.Remove(DICOM_TAG_NUMBER_OF_STUDY_RELATED_SERIES);
      withoutSpecialTags.Remove(DICOM_TAG_SOP_CLASSES_IN_STUDY);

      // Check out "AddAnswer()"
      withoutSpecialTags.Remove(DICOM_TAG_SPECIFIC_CHARACTER_SET);
      withoutSpecialTags.Remove(DICOM_TAG_QUERY_RETRIEVE_LEVEL);
      
      return (!sequencesToReturn_.empty() ||
              !withoutSpecialTags.HasOnlyMainDicomTags());
    }
      
    virtual void MarkAsComplete()
    {
      answers_.SetComplete(true);
    }

    virtual void Visit(const std::string& publicId,
                       const std::string& instanceId,
                       const DicomMap& mainDicomTags,
                       const Json::Value* dicomAsJson) 
    {
      std::unique_ptr<DicomMap> counters(ComputeCounters(context_, instanceId, level_, query_));

      AddAnswer(answers_, mainDicomTags, dicomAsJson,
                queryAsArray_, sequencesToReturn_, counters.get());
    }
  };


  void OrthancFindRequestHandler::Handle(DicomFindAnswers& answers,
                                         const DicomMap& input,
                                         const std::list<DicomTag>& sequencesToReturn,
                                         const std::string& remoteIp,
                                         const std::string& remoteAet,
                                         const std::string& calledAet,
                                         ModalityManufacturer manufacturer)
  {
    MetricsRegistry::Timer timer(context_.GetMetricsRegistry(), "orthanc_find_scp_duration_ms");

    /**
     * Possibly apply the user-supplied Lua filter.
     **/

    DicomMap lua;
    const DicomMap* filteredInput = &input;

    if (ApplyLuaFilter(lua, input, remoteIp, remoteAet, calledAet, manufacturer))
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
      throw OrthancException(ErrorCode_BadRequest,
                             "C-FIND request without the tag 0008,0052 (QueryRetrieveLevel)");
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
                  << "  " << FromDcmtkBridge::GetTagName(query.GetElement(i))
                  << " = " << query.GetElement(i).GetValue().GetContent();
      }
    }

    for (std::list<DicomTag>::const_iterator it = sequencesToReturn.begin();
         it != sequencesToReturn.end(); ++it)
    {
      LOG(INFO) << "  (" << it->Format()
                << ")  " << FromDcmtkBridge::GetTagName(*it, "")
                << " : sequence tag whose content will be copied";
    }


    /**
     * Build up the query object.
     **/

    DatabaseLookup lookup;

    bool caseSensitivePN;

    {
      OrthancConfiguration::ReaderLock lock;
      caseSensitivePN = lock.GetConfiguration().GetBooleanParameter("CaseSensitivePN", false);
    }

    for (size_t i = 0; i < query.GetSize(); i++)
    {
      const DicomElement& element = query.GetElement(i);
      const DicomTag tag = element.GetTag();

      if (element.GetValue().IsNull() ||
          tag == DICOM_TAG_QUERY_RETRIEVE_LEVEL ||
          tag == DICOM_TAG_SPECIFIC_CHARACTER_SET)
      {
        continue;
      }

      std::string value = element.GetValue().GetContent();
      if (value.size() == 0)
      {
        // An empty string corresponds to an universal constraint, so we ignore it
        continue;
      }

      if (FilterQueryTag(value, level, tag, manufacturer))
      {
        ValueRepresentation vr = FromDcmtkBridge::LookupValueRepresentation(tag);

        // DICOM specifies that searches must be case sensitive, except
        // for tags with a PN value representation
        bool sensitive = true;
        if (vr == ValueRepresentation_PersonName)
        {
          sensitive = caseSensitivePN;
        }

        lookup.AddDicomConstraint(tag, value, sensitive, true /* mandatory */);
      }
      else
      {
        LOG(INFO) << "Because of a patch for the manufacturer of the remote modality, " 
                  << "ignoring constraint on tag (" << tag.Format() << ") " << FromDcmtkBridge::GetTagName(element);
      }
    }


    /**
     * Run the query.
     **/

    size_t limit = (level == ResourceType_Instance) ? maxInstances_ : maxResults_;


    LookupVisitor visitor(answers, context_, level, *filteredInput, sequencesToReturn);
    context_.Apply(visitor, lookup, level, 0 /* "since" is not relevant to C-FIND */, limit);
  }


  void OrthancFindRequestHandler::FormatOrigin(Json::Value& origin,
                                               const std::string& remoteIp,
                                               const std::string& remoteAet,
                                               const std::string& calledAet,
                                               ModalityManufacturer manufacturer)
  {
    origin = Json::objectValue;
    origin["RemoteIp"] = remoteIp;
    origin["RemoteAet"] = remoteAet;
    origin["CalledAet"] = calledAet;
    origin["Manufacturer"] = EnumerationToString(manufacturer);
  }
}
