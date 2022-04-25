/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
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

#include "../../OrthancFramework/Sources/DicomFormat/DicomArray.h"
#include "../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"
#include "../../OrthancFramework/Sources/Logging.h"
#include "../../OrthancFramework/Sources/Lua/LuaFunctionCall.h"
#include "../../OrthancFramework/Sources/MetricsRegistry.h"
#include "OrthancConfiguration.h"
#include "Search/DatabaseLookup.h"
#include "ServerContext.h"
#include "ServerToolbox.h"

#include <boost/regex.hpp> 


namespace Orthanc
{
  static void AddAnswer(DicomFindAnswers& answers,
                        ServerContext& context,
                        const std::string& publicId,
                        const std::string& instanceId,
                        const DicomMap& mainDicomTags,
                        const Json::Value* dicomAsJson,
                        ResourceType level,
                        const DicomArray& query,
                        const std::list<DicomTag>& sequencesToReturn,
                        const std::string& defaultPrivateCreator,
                        const std::map<uint16_t, std::string>& privateCreators,
                        const std::string& retrieveAet)
  {
    ExpandedResource resource;
    std::set<DicomTag> requestedTags;
    
    query.GetTags(requestedTags);
    requestedTags.erase(DICOM_TAG_QUERY_RETRIEVE_LEVEL); // this is not part of the answer

    // reuse ExpandResource to get missing tags and computed tags (ModalitiesInStudy ...).  This code is therefore shared between C-Find, tools/find, list-resources and QIDO-RS
    context.ExpandResource(resource, publicId, mainDicomTags, instanceId, dicomAsJson, level, requestedTags, ExpandResourceDbFlags_IncludeMainDicomTags);

    DicomMap result;

    /**
     * Add the mandatory "Retrieve AE Title (0008,0054)" tag, which was missing in Orthanc <= 1.7.2.
     * http://dicom.nema.org/medical/dicom/current/output/html/part04.html#sect_C.4.1.1.3.2
     * https://groups.google.com/g/orthanc-users/c/-7zNTKR_PMU/m/kfjwzEVNAgAJ
     **/
    result.SetValue(DICOM_TAG_RETRIEVE_AE_TITLE, retrieveAet, false /* not binary */);

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
        const DicomValue* value = resource.tags_.TestAndGetValue(tag);

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

    if (result.GetSize() == 0 &&
        sequencesToReturn.empty())
    {
      CLOG(WARNING, DICOM) << "The C-FIND request does not return any DICOM tag";
    }
    else if (sequencesToReturn.empty())
    {
      answers.Add(result);
    }
    else if (dicomAsJson == NULL)
    {
      CLOG(WARNING, DICOM) << "C-FIND query requesting a sequence, but reading JSON from disk is disabled";
      answers.Add(result);
    }
    else
    {
      ParsedDicomFile dicom(result, GetDefaultDicomEncoding(),
                            true /* be permissive, cf. issue #136 */, defaultPrivateCreator, privateCreators);

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
            Toolbox::SimplifyDicomAsJson(item, source["Value"][i], DicomToJsonFormat_Short);
            content.append(item);
          }

          if (tag->IsPrivate())
          {
            std::map<uint16_t, std::string>::const_iterator found = privateCreators.find(tag->GetGroup());
            
            if (found != privateCreators.end())
            {
              dicom.Replace(*tag, content, false, DicomReplaceMode_InsertIfAbsent, found->second.c_str());
            }
            else
            {
              dicom.Replace(*tag, content, false, DicomReplaceMode_InsertIfAbsent, defaultPrivateCreator);
            }
          }
          else
          {
            dicom.Replace(*tag, content, false, DicomReplaceMode_InsertIfAbsent, "" /* no private creator */);
          }
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
    // https://bugs.orthanc-server.com/show_bug.cgi?id=31
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
      call.ExecuteToDicom(target);

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
    std::string                 defaultPrivateCreator_;       // the private creator to use if the group is not defined in the query itself
    const std::map<uint16_t, std::string>& privateCreators_;  // the private creators defined in the query itself
    std::string                 retrieveAet_;

  public:
    LookupVisitor(DicomFindAnswers&  answers,
                  ServerContext& context,
                  ResourceType level,
                  const DicomMap& query,
                  const std::list<DicomTag>& sequencesToReturn,
                  const std::map<uint16_t, std::string>& privateCreators) :
      answers_(answers),
      context_(context),
      level_(level),
      query_(query),
      queryAsArray_(query),
      sequencesToReturn_(sequencesToReturn),
      privateCreators_(privateCreators)
    {
      answers_.SetComplete(false);

      {
        OrthancConfiguration::ReaderLock lock;
        defaultPrivateCreator_ = lock.GetConfiguration().GetDefaultPrivateCreator();
        retrieveAet_ = lock.GetConfiguration().GetOrthancAET();
      }
    }

    virtual bool IsDicomAsJsonNeeded() const ORTHANC_OVERRIDE
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
      
    virtual void MarkAsComplete() ORTHANC_OVERRIDE
    {
      answers_.SetComplete(true);
    }

    virtual void Visit(const std::string& publicId,
                       const std::string& instanceId,
                       const DicomMap& mainDicomTags,
                       const Json::Value* dicomAsJson) ORTHANC_OVERRIDE
    {
      AddAnswer(answers_, context_, publicId, instanceId, mainDicomTags, dicomAsJson, level_, queryAsArray_, sequencesToReturn_,
                defaultPrivateCreator_, privateCreators_, retrieveAet_);
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
     * Deal with global configuration
     **/

    bool caseSensitivePN;

    {
      OrthancConfiguration::ReaderLock lock;
      caseSensitivePN = lock.GetConfiguration().GetBooleanParameter("CaseSensitivePN", false);

      RemoteModalityParameters remote;
      if (!lock.GetConfiguration().LookupDicomModalityUsingAETitle(remote, remoteAet))
      {
        if (lock.GetConfiguration().GetBooleanParameter("DicomAlwaysAllowFind", false))
        {
          CLOG(INFO, DICOM) << "C-FIND: Allowing SCU request from unknown modality with AET: " << remoteAet;
        }
        else
        {
          // This should never happen, given the test at bottom of
          // "OrthancApplicationEntityFilter::IsAllowedRequest()"
          throw OrthancException(ErrorCode_InexistentItem,
                                 "C-FIND: Rejecting SCU request from unknown modality with AET: " + remoteAet);
        }
      }
    }



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
    CLOG(INFO, DICOM) << "DICOM C-Find request at level: " << EnumerationToString(level);

    for (size_t i = 0; i < query.GetSize(); i++)
    {
      if (!query.GetElement(i).GetValue().IsNull())
      {
        CLOG(INFO, DICOM) << "  (" << query.GetElement(i).GetTag().Format()
                          << ")  " << FromDcmtkBridge::GetTagName(query.GetElement(i))
                          << " = " << context_.GetDeidentifiedContent(query.GetElement(i));
      }
    }

    for (std::list<DicomTag>::const_iterator it = sequencesToReturn.begin();
         it != sequencesToReturn.end(); ++it)
    {
      CLOG(INFO, DICOM) << "  (" << it->Format()
                        << ")  " << FromDcmtkBridge::GetTagName(*it, "")
                        << " : sequence tag whose content will be copied";
    }

    // collect the private creators from the query itself
    std::map<uint16_t, std::string> privateCreators;
    for (size_t i = 0; i < query.GetSize(); i++)
    {
      const DicomElement& element = query.GetElement(i);
      if (element.GetTag().IsPrivate() && element.GetTag().GetElement() == 0x10)
      {
        privateCreators[element.GetTag().GetGroup()] = element.GetValue().GetContent();
      }
    }

    /**
     * Build up the query object.
     **/

    DatabaseLookup lookup;

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
        CLOG(INFO, DICOM) << "Because of a patch for the manufacturer of the remote modality, " 
                          << "ignoring constraint on tag (" << tag.Format() << ") "
                          << FromDcmtkBridge::GetTagName(element);
      }
    }


    /**
     * Run the query.
     **/

    size_t limit = (level == ResourceType_Instance) ? maxInstances_ : maxResults_;


    LookupVisitor visitor(answers, context_, level, *filteredInput, sequencesToReturn, privateCreators);
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
