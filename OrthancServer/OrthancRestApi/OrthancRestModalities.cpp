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


#include "../PrecompiledHeadersServer.h"
#include "OrthancRestApi.h"

#include "../../Core/Cache/SharedArchive.h"
#include "../../Core/DicomParsing/FromDcmtkBridge.h"
#include "../../Core/Logging.h"
#include "../../Core/SerializationToolbox.h"

#include "../OrthancConfiguration.h"
#include "../QueryRetrieveHandler.h"
#include "../ServerContext.h"
#include "../ServerJobs/DicomModalityStoreJob.h"
#include "../ServerJobs/DicomMoveScuJob.h"
#include "../ServerJobs/OrthancPeerStoreJob.h"
#include "../ServerToolbox.h"
#include "../StorageCommitmentReports.h"


namespace Orthanc
{
  static const char* const KEY_LEVEL = "Level";
  static const char* const KEY_QUERY = "Query";
  static const char* const KEY_NORMALIZE = "Normalize";
  static const char* const KEY_RESOURCES = "Resources";

  
  static RemoteModalityParameters MyGetModalityUsingSymbolicName(const std::string& name)
  {
    OrthancConfiguration::ReaderLock lock;
    return lock.GetConfiguration().GetModalityUsingSymbolicName(name);
  }


  /***************************************************************************
   * DICOM C-Echo SCU
   ***************************************************************************/

  static void DicomEcho(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    const std::string& localAet = context.GetDefaultLocalApplicationEntityTitle();
    RemoteModalityParameters remote =
      MyGetModalityUsingSymbolicName(call.GetUriComponent("id", ""));

    try
    {
      DicomUserConnection connection(localAet, remote);
      connection.Open();
      
      if (connection.Echo())
      {
        // Echo has succeeded
        call.GetOutput().AnswerBuffer("{}", MimeType_Json);
        return;
      }
    }
    catch (OrthancException&)
    {
    }

    // Echo has failed
    call.GetOutput().SignalError(HttpStatus_500_InternalServerError);
  }



  /***************************************************************************
   * DICOM C-Find SCU => DEPRECATED!
   ***************************************************************************/

  static bool MergeQueryAndTemplate(DicomMap& result,
                                    const RestApiCall& call)
  {
    Json::Value query;

    if (!call.ParseJsonRequest(query) ||
        query.type() != Json::objectValue)
    {
      return false;
    }

    Json::Value::Members members = query.getMemberNames();
    for (size_t i = 0; i < members.size(); i++)
    {
      DicomTag t = FromDcmtkBridge::ParseTag(members[i]);
      result.SetValue(t, query[members[i]].asString(), false);
    }

    return true;
  }


  static void FindPatient(DicomFindAnswers& result,
                          DicomUserConnection& connection,
                          const DicomMap& fields)
  {
    // Only keep the filters from "fields" that are related to the patient
    DicomMap s;
    fields.ExtractPatientInformation(s);
    connection.Find(result, ResourceType_Patient, s, true /* normalize */);
  }


  static void FindStudy(DicomFindAnswers& result,
                        DicomUserConnection& connection,
                        const DicomMap& fields)
  {
    // Only keep the filters from "fields" that are related to the study
    DicomMap s;
    fields.ExtractStudyInformation(s);

    s.CopyTagIfExists(fields, DICOM_TAG_PATIENT_ID);
    s.CopyTagIfExists(fields, DICOM_TAG_ACCESSION_NUMBER);
    s.CopyTagIfExists(fields, DICOM_TAG_MODALITIES_IN_STUDY);

    connection.Find(result, ResourceType_Study, s, true /* normalize */);
  }

  static void FindSeries(DicomFindAnswers& result,
                         DicomUserConnection& connection,
                         const DicomMap& fields)
  {
    // Only keep the filters from "fields" that are related to the series
    DicomMap s;
    fields.ExtractSeriesInformation(s);

    s.CopyTagIfExists(fields, DICOM_TAG_PATIENT_ID);
    s.CopyTagIfExists(fields, DICOM_TAG_ACCESSION_NUMBER);
    s.CopyTagIfExists(fields, DICOM_TAG_STUDY_INSTANCE_UID);

    connection.Find(result, ResourceType_Series, s, true /* normalize */);
  }

  static void FindInstance(DicomFindAnswers& result,
                           DicomUserConnection& connection,
                           const DicomMap& fields)
  {
    // Only keep the filters from "fields" that are related to the instance
    DicomMap s;
    fields.ExtractInstanceInformation(s);

    s.CopyTagIfExists(fields, DICOM_TAG_PATIENT_ID);
    s.CopyTagIfExists(fields, DICOM_TAG_ACCESSION_NUMBER);
    s.CopyTagIfExists(fields, DICOM_TAG_STUDY_INSTANCE_UID);
    s.CopyTagIfExists(fields, DICOM_TAG_SERIES_INSTANCE_UID);

    connection.Find(result, ResourceType_Instance, s, true /* normalize */);
  }


  static void DicomFindPatient(RestApiPostCall& call)
  {
    LOG(WARNING) << "This URI is deprecated: " << call.FlattenUri();
    ServerContext& context = OrthancRestApi::GetContext(call);

    DicomMap fields;
    DicomMap::SetupFindPatientTemplate(fields);
    if (!MergeQueryAndTemplate(fields, call))
    {
      return;
    }

    const std::string& localAet = context.GetDefaultLocalApplicationEntityTitle();
    RemoteModalityParameters remote =
      MyGetModalityUsingSymbolicName(call.GetUriComponent("id", ""));
    
    DicomFindAnswers answers(false);

    {
      DicomUserConnection connection(localAet, remote);
      connection.Open();
      FindPatient(answers, connection, fields);
    }

    Json::Value result;
    answers.ToJson(result, true);
    call.GetOutput().AnswerJson(result);
  }

  static void DicomFindStudy(RestApiPostCall& call)
  {
    LOG(WARNING) << "This URI is deprecated: " << call.FlattenUri();
    ServerContext& context = OrthancRestApi::GetContext(call);

    DicomMap fields;
    DicomMap::SetupFindStudyTemplate(fields);
    if (!MergeQueryAndTemplate(fields, call))
    {
      return;
    }

    if (fields.GetValue(DICOM_TAG_ACCESSION_NUMBER).GetContent().size() <= 2 &&
        fields.GetValue(DICOM_TAG_PATIENT_ID).GetContent().size() <= 2)
    {
      return;
    }        
      
    const std::string& localAet = context.GetDefaultLocalApplicationEntityTitle();
    RemoteModalityParameters remote =
      MyGetModalityUsingSymbolicName(call.GetUriComponent("id", ""));

    DicomFindAnswers answers(false);

    {
      DicomUserConnection connection(localAet, remote);
      connection.Open();
      FindStudy(answers, connection, fields);
    }

    Json::Value result;
    answers.ToJson(result, true);
    call.GetOutput().AnswerJson(result);
  }

  static void DicomFindSeries(RestApiPostCall& call)
  {
    LOG(WARNING) << "This URI is deprecated: " << call.FlattenUri();
    ServerContext& context = OrthancRestApi::GetContext(call);

    DicomMap fields;
    DicomMap::SetupFindSeriesTemplate(fields);
    if (!MergeQueryAndTemplate(fields, call))
    {
      return;
    }

    if ((fields.GetValue(DICOM_TAG_ACCESSION_NUMBER).GetContent().size() <= 2 &&
         fields.GetValue(DICOM_TAG_PATIENT_ID).GetContent().size() <= 2) ||
        fields.GetValue(DICOM_TAG_STUDY_INSTANCE_UID).GetContent().size() <= 2)
    {
      return;
    }        
         
    const std::string& localAet = context.GetDefaultLocalApplicationEntityTitle();
    RemoteModalityParameters remote =
      MyGetModalityUsingSymbolicName(call.GetUriComponent("id", ""));

    DicomFindAnswers answers(false);

    {
      DicomUserConnection connection(localAet, remote);
      connection.Open();
      FindSeries(answers, connection, fields);
    }

    Json::Value result;
    answers.ToJson(result, true);
    call.GetOutput().AnswerJson(result);
  }

  static void DicomFindInstance(RestApiPostCall& call)
  {
    LOG(WARNING) << "This URI is deprecated: " << call.FlattenUri();
    ServerContext& context = OrthancRestApi::GetContext(call);

    DicomMap fields;
    DicomMap::SetupFindInstanceTemplate(fields);
    if (!MergeQueryAndTemplate(fields, call))
    {
      return;
    }

    if ((fields.GetValue(DICOM_TAG_ACCESSION_NUMBER).GetContent().size() <= 2 &&
         fields.GetValue(DICOM_TAG_PATIENT_ID).GetContent().size() <= 2) ||
        fields.GetValue(DICOM_TAG_STUDY_INSTANCE_UID).GetContent().size() <= 2 ||
        fields.GetValue(DICOM_TAG_SERIES_INSTANCE_UID).GetContent().size() <= 2)
    {
      return;
    }        
         
    const std::string& localAet = context.GetDefaultLocalApplicationEntityTitle();
    RemoteModalityParameters remote =
      MyGetModalityUsingSymbolicName(call.GetUriComponent("id", ""));

    DicomFindAnswers answers(false);

    {
      DicomUserConnection connection(localAet, remote);
      connection.Open();
      FindInstance(answers, connection, fields);
    }

    Json::Value result;
    answers.ToJson(result, true);
    call.GetOutput().AnswerJson(result);
  }


  static void CopyTagIfExists(DicomMap& target,
                              ParsedDicomFile& source,
                              const DicomTag& tag)
  {
    std::string tmp;
    if (source.GetTagValue(tmp, tag))
    {
      target.SetValue(tag, tmp, false);
    }
  }


  static void DicomFind(RestApiPostCall& call)
  {
    LOG(WARNING) << "This URI is deprecated: " << call.FlattenUri();
    ServerContext& context = OrthancRestApi::GetContext(call);

    DicomMap m;
    DicomMap::SetupFindPatientTemplate(m);
    if (!MergeQueryAndTemplate(m, call))
    {
      return;
    }
 
    const std::string& localAet = context.GetDefaultLocalApplicationEntityTitle();
    RemoteModalityParameters remote =
      MyGetModalityUsingSymbolicName(call.GetUriComponent("id", ""));

    DicomUserConnection connection(localAet, remote);
    connection.Open();
    
    DicomFindAnswers patients(false);
    FindPatient(patients, connection, m);

    // Loop over the found patients
    Json::Value result = Json::arrayValue;
    for (size_t i = 0; i < patients.GetSize(); i++)
    {
      Json::Value patient;
      patients.ToJson(patient, i, true);

      DicomMap::SetupFindStudyTemplate(m);
      if (!MergeQueryAndTemplate(m, call))
      {
        return;
      }

      CopyTagIfExists(m, patients.GetAnswer(i), DICOM_TAG_PATIENT_ID);

      DicomFindAnswers studies(false);
      FindStudy(studies, connection, m);

      patient["Studies"] = Json::arrayValue;
      
      // Loop over the found studies
      for (size_t j = 0; j < studies.GetSize(); j++)
      {
        Json::Value study;
        studies.ToJson(study, j, true);

        DicomMap::SetupFindSeriesTemplate(m);
        if (!MergeQueryAndTemplate(m, call))
        {
          return;
        }

        CopyTagIfExists(m, studies.GetAnswer(j), DICOM_TAG_PATIENT_ID);
        CopyTagIfExists(m, studies.GetAnswer(j), DICOM_TAG_STUDY_INSTANCE_UID);

        DicomFindAnswers series(false);
        FindSeries(series, connection, m);

        // Loop over the found series
        study["Series"] = Json::arrayValue;
        for (size_t k = 0; k < series.GetSize(); k++)
        {
          Json::Value series2;
          series.ToJson(series2, k, true);
          study["Series"].append(series2);
        }

        patient["Studies"].append(study);
      }

      result.append(patient);
    }
    
    call.GetOutput().AnswerJson(result);
  }



  /***************************************************************************
   * DICOM C-Find and C-Move SCU => Recommended since Orthanc 0.9.0
   ***************************************************************************/

  static void AnswerQueryHandler(RestApiPostCall& call,
                                 std::unique_ptr<QueryRetrieveHandler>& handler)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    if (handler.get() == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }

    handler->Run();
    
    std::string s = context.GetQueryRetrieveArchive().Add(handler.release());
    Json::Value result = Json::objectValue;
    result["ID"] = s;
    result["Path"] = "/queries/" + s;
    
    call.GetOutput().AnswerJson(result);
  }

  
  static void DicomQuery(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);
    Json::Value request;

    if (!call.ParseJsonRequest(request) ||
        request.type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat, "Must provide a JSON object");
    }
    else if (!request.isMember(KEY_LEVEL) ||
             request[KEY_LEVEL].type() != Json::stringValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "The JSON body must contain field " + std::string(KEY_LEVEL));
    }
    else if (request.isMember(KEY_NORMALIZE) &&
             request[KEY_NORMALIZE].type() != Json::booleanValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "The field " + std::string(KEY_NORMALIZE) + " must contain a Boolean");
    }
    else if (request.isMember(KEY_QUERY) &&
             request[KEY_QUERY].type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "The field " + std::string(KEY_QUERY) + " must contain a JSON object");
    }
    else
    {
      std::unique_ptr<QueryRetrieveHandler>  handler(new QueryRetrieveHandler(context));
      
      handler->SetModality(call.GetUriComponent("id", ""));
      handler->SetLevel(StringToResourceType(request[KEY_LEVEL].asCString()));

      if (request.isMember(KEY_QUERY))
      {
        std::map<DicomTag, std::string> query;
        SerializationToolbox::ReadMapOfTags(query, request, KEY_QUERY);

        for (std::map<DicomTag, std::string>::const_iterator
               it = query.begin(); it != query.end(); ++it)
        {
          handler->SetQuery(it->first, it->second);
        }
      }

      if (request.isMember(KEY_NORMALIZE))
      {
        handler->SetFindNormalized(request[KEY_NORMALIZE].asBool());
      }

      AnswerQueryHandler(call, handler);
    }
  }


  static void ListQueries(RestApiGetCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    std::list<std::string> queries;
    context.GetQueryRetrieveArchive().List(queries);

    Json::Value result = Json::arrayValue;
    for (std::list<std::string>::const_iterator
           it = queries.begin(); it != queries.end(); ++it)
    {
      result.append(*it);
    }

    call.GetOutput().AnswerJson(result);
  }


  namespace
  {
    class QueryAccessor
    {
    private:
      ServerContext&            context_;
      SharedArchive::Accessor   accessor_;
      QueryRetrieveHandler*     handler_;

    public:
      QueryAccessor(RestApiCall& call) :
        context_(OrthancRestApi::GetContext(call)),
        accessor_(context_.GetQueryRetrieveArchive(), call.GetUriComponent("id", "")),
        handler_(NULL)
      {
        if (accessor_.IsValid())
        {
          handler_ = &dynamic_cast<QueryRetrieveHandler&>(accessor_.GetItem());
        }
        else
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }
      }                     

      QueryRetrieveHandler& GetHandler() const
      {
        assert(handler_ != NULL);
        return *handler_;
      }
    };

    static void AnswerDicomMap(RestApiCall& call,
                               const DicomMap& value,
                               bool simplify)
    {
      Json::Value full = Json::objectValue;
      FromDcmtkBridge::ToJson(full, value, simplify);
      call.GetOutput().AnswerJson(full);
    }
  }


  static void ListQueryAnswers(RestApiGetCall& call)
  {
    const bool expand = call.HasArgument("expand");
    const bool simplify = call.HasArgument("simplify");
    
    QueryAccessor query(call);
    size_t count = query.GetHandler().GetAnswersCount();

    Json::Value result = Json::arrayValue;
    for (size_t i = 0; i < count; i++)
    {
      if (expand)
      {
        // New in Orthanc 1.5.0
        DicomMap value;
        query.GetHandler().GetAnswer(value, i);
        
        Json::Value json = Json::objectValue;
        FromDcmtkBridge::ToJson(json, value, simplify);

        result.append(json);
      }
      else
      {
        result.append(boost::lexical_cast<std::string>(i));
      }
    }

    call.GetOutput().AnswerJson(result);
  }


  static void GetQueryOneAnswer(RestApiGetCall& call)
  {
    size_t index = boost::lexical_cast<size_t>(call.GetUriComponent("index", ""));

    QueryAccessor query(call);

    DicomMap map;
    query.GetHandler().GetAnswer(map, index);

    AnswerDicomMap(call, map, call.HasArgument("simplify"));
  }


  static void SubmitRetrieveJob(RestApiPostCall& call,
                                bool allAnswers,
                                size_t index)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string targetAet;
    
    Json::Value body;
    if (call.ParseJsonRequest(body))
    {
      targetAet = SerializationToolbox::ReadString(body, "TargetAet");
    }
    else
    {
      body = Json::objectValue;
      call.BodyToString(targetAet);
    }
    
    std::unique_ptr<DicomMoveScuJob> job(new DicomMoveScuJob(context));
    
    {
      QueryAccessor query(call);
      job->SetTargetAet(targetAet);
      job->SetLocalAet(query.GetHandler().GetLocalAet());
      job->SetRemoteModality(query.GetHandler().GetRemoteModality());

      LOG(WARNING) << "Driving C-Move SCU on remote modality "
                   << query.GetHandler().GetRemoteModality().GetApplicationEntityTitle()
                   << " to target modality " << targetAet;

      if (allAnswers)
      {
        for (size_t i = 0; i < query.GetHandler().GetAnswersCount(); i++)
        {
          job->AddFindAnswer(query.GetHandler(), i);
        }
      }
      else
      {
        job->AddFindAnswer(query.GetHandler(), index);
      }
    }

    OrthancRestApi::GetApi(call).SubmitCommandsJob
      (call, job.release(), true /* synchronous by default */, body);
  }
  

  static void RetrieveOneAnswer(RestApiPostCall& call)
  {
    size_t index = boost::lexical_cast<size_t>(call.GetUriComponent("index", ""));
    SubmitRetrieveJob(call, false, index);
  }


  static void RetrieveAllAnswers(RestApiPostCall& call)
  {
    SubmitRetrieveJob(call, true, 0);
  }


  static void GetQueryArguments(RestApiGetCall& call)
  {
    QueryAccessor query(call);
    AnswerDicomMap(call, query.GetHandler().GetQuery(), call.HasArgument("simplify"));
  }


  static void GetQueryLevel(RestApiGetCall& call)
  {
    QueryAccessor query(call);
    call.GetOutput().AnswerBuffer(EnumerationToString(query.GetHandler().GetLevel()), MimeType_PlainText);
  }


  static void GetQueryModality(RestApiGetCall& call)
  {
    QueryAccessor query(call);
    call.GetOutput().AnswerBuffer(query.GetHandler().GetModalitySymbolicName(), MimeType_PlainText);
  }


  static void DeleteQuery(RestApiDeleteCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);
    context.GetQueryRetrieveArchive().Remove(call.GetUriComponent("id", ""));
    call.GetOutput().AnswerBuffer("", MimeType_PlainText);
  }


  static void ListQueryOperations(RestApiGetCall& call)
  {
    // Ensure that the query of interest does exist
    QueryAccessor query(call);  

    RestApi::AutoListChildren(call);
  }


  static void ListQueryAnswerOperations(RestApiGetCall& call)
  {
    // Ensure that the query of interest does exist
    QueryAccessor query(call);

    // Ensure that the answer of interest does exist
    size_t index = boost::lexical_cast<size_t>(call.GetUriComponent("index", ""));

    DicomMap map;
    query.GetHandler().GetAnswer(map, index);

    Json::Value answer = Json::arrayValue;
    answer.append("content");
    answer.append("retrieve");

    switch (query.GetHandler().GetLevel())
    {
      case ResourceType_Patient:
        answer.append("query-study");

      case ResourceType_Study:
        answer.append("query-series");

      case ResourceType_Series:
        answer.append("query-instances");
        break;

      default:
        break;
    }
    
    call.GetOutput().AnswerJson(answer);
  }


  template <ResourceType CHILDREN_LEVEL>
  static void QueryAnswerChildren(RestApiPostCall& call)
  {
    // New in Orthanc 1.5.0
    assert(CHILDREN_LEVEL == ResourceType_Study ||
           CHILDREN_LEVEL == ResourceType_Series ||
           CHILDREN_LEVEL == ResourceType_Instance);
    
    ServerContext& context = OrthancRestApi::GetContext(call);

    std::unique_ptr<QueryRetrieveHandler>  handler(new QueryRetrieveHandler(context));
      
    {
      const QueryAccessor parent(call);
      const ResourceType level = parent.GetHandler().GetLevel();
    
      const size_t index = boost::lexical_cast<size_t>(call.GetUriComponent("index", ""));

      Json::Value request;

      if (index >= parent.GetHandler().GetAnswersCount())
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
      else if (CHILDREN_LEVEL == ResourceType_Study &&
               level != ResourceType_Patient)
      {
        throw OrthancException(ErrorCode_UnknownResource);
      }
      else if (CHILDREN_LEVEL == ResourceType_Series &&
               level != ResourceType_Patient &&
               level != ResourceType_Study)
      {
        throw OrthancException(ErrorCode_UnknownResource);
      }      
      else if (CHILDREN_LEVEL == ResourceType_Instance &&
               level != ResourceType_Patient &&
               level != ResourceType_Study &&
               level != ResourceType_Series)
      {
        throw OrthancException(ErrorCode_UnknownResource);
      }
      else if (!call.ParseJsonRequest(request))
      {
        throw OrthancException(ErrorCode_BadFileFormat, "Must provide a JSON object");
      }
      else
      {
        handler->SetFindNormalized(parent.GetHandler().IsFindNormalized());
        handler->SetModality(parent.GetHandler().GetModalitySymbolicName());
        handler->SetLevel(CHILDREN_LEVEL);

        if (request.isMember(KEY_QUERY))
        {
          std::map<DicomTag, std::string> query;
          SerializationToolbox::ReadMapOfTags(query, request, KEY_QUERY);

          for (std::map<DicomTag, std::string>::const_iterator
                 it = query.begin(); it != query.end(); ++it)
          {
            handler->SetQuery(it->first, it->second);
          }
        }

        DicomMap answer;
        parent.GetHandler().GetAnswer(answer, index);

        // This switch-case mimics "DicomUserConnection::Move()"
        switch (parent.GetHandler().GetLevel())
        {
          case ResourceType_Patient:
            handler->CopyStringTag(answer, DICOM_TAG_PATIENT_ID);
            break;

          case ResourceType_Study:
            handler->CopyStringTag(answer, DICOM_TAG_STUDY_INSTANCE_UID);
            break;

          case ResourceType_Series:
            handler->CopyStringTag(answer, DICOM_TAG_STUDY_INSTANCE_UID);
            handler->CopyStringTag(answer, DICOM_TAG_SERIES_INSTANCE_UID);
            break;

          case ResourceType_Instance:
            handler->CopyStringTag(answer, DICOM_TAG_STUDY_INSTANCE_UID);
            handler->CopyStringTag(answer, DICOM_TAG_SERIES_INSTANCE_UID);
            handler->CopyStringTag(answer, DICOM_TAG_SOP_INSTANCE_UID);
            break;

          default:
            throw OrthancException(ErrorCode_InternalError);
        }
      }
    }
      
    AnswerQueryHandler(call, handler);
  }
  


  /***************************************************************************
   * DICOM C-Store SCU
   ***************************************************************************/

  static void GetInstancesToExport(Json::Value& otherArguments,
                                   SetOfInstancesJob& job,
                                   const std::string& remote,
                                   RestApiPostCall& call)
  {
    otherArguments = Json::objectValue;
    ServerContext& context = OrthancRestApi::GetContext(call);

    Json::Value request;
    if (Toolbox::IsSHA1(call.GetBodyData(), call.GetBodySize()))
    {
      std::string s;
      call.BodyToString(s);

      // This is for compatibility with Orthanc <= 0.5.1.
      request = Json::arrayValue;
      request.append(Toolbox::StripSpaces(s));
    }
    else if (!call.ParseJsonRequest(request))
    {
      // Bad JSON request
      throw OrthancException(ErrorCode_BadFileFormat, "Must provide a JSON value");
    }

    if (request.isString())
    {
      std::string item = request.asString();
      request = Json::arrayValue;
      request.append(item);
    }
    else if (!request.isArray() &&
             !request.isObject())
    {
      throw OrthancException(ErrorCode_BadFileFormat, "Must provide a JSON object, or a JSON array of strings");
    }

    const Json::Value* resources;
    if (request.isArray())
    {
      resources = &request;
    }
    else
    {
      if (request.type() != Json::objectValue ||
          !request.isMember(KEY_RESOURCES))
      {
        throw OrthancException(ErrorCode_BadFileFormat,
                               "Missing field in JSON: \"" + std::string(KEY_RESOURCES) + "\"");
      }

      resources = &request[KEY_RESOURCES];
      if (!resources->isArray())
      {
        throw OrthancException(ErrorCode_BadFileFormat,
                               "JSON field \"" + std::string(KEY_RESOURCES) + "\" must contain an array");
      }

      // Copy the remaining arguments
      Json::Value::Members members = request.getMemberNames();
      for (Json::Value::ArrayIndex i = 0; i < members.size(); i++)
      {
        otherArguments[members[i]] = request[members[i]];
      }
    }

    bool logExportedResources;

    {
      OrthancConfiguration::ReaderLock lock;
      logExportedResources = lock.GetConfiguration().GetBooleanParameter("LogExportedResources", false);
    }

    for (Json::Value::ArrayIndex i = 0; i < resources->size(); i++)
    {
      if (!(*resources) [i].isString())
      {
        throw OrthancException(ErrorCode_BadFileFormat,
                               "Resources to be exported must be specified as a JSON array of strings");
      }

      std::string stripped = Toolbox::StripSpaces((*resources) [i].asString());
      if (!Toolbox::IsSHA1(stripped))
      {
        throw OrthancException(ErrorCode_BadFileFormat,
                               "This string is not a valid Orthanc identifier: " + stripped);
      }

      job.AddParentResource(stripped);  // New in Orthanc 1.5.7
      
      context.AddChildInstances(job, stripped);

      if (logExportedResources)
      {
        context.GetIndex().LogExportedResource(stripped, remote);
      }
    }
  }


  static void DicomStore(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string remote = call.GetUriComponent("id", "");

    Json::Value request;
    std::unique_ptr<DicomModalityStoreJob> job(new DicomModalityStoreJob(context));

    GetInstancesToExport(request, *job, remote, call);

    std::string localAet = Toolbox::GetJsonStringField
      (request, "LocalAet", context.GetDefaultLocalApplicationEntityTitle());
    std::string moveOriginatorAET = Toolbox::GetJsonStringField
      (request, "MoveOriginatorAet", context.GetDefaultLocalApplicationEntityTitle());
    int moveOriginatorID = Toolbox::GetJsonIntegerField
      (request, "MoveOriginatorID", 0 /* By default, not a C-MOVE */);

    job->SetLocalAet(localAet);
    job->SetRemoteModality(MyGetModalityUsingSymbolicName(remote));

    if (moveOriginatorID != 0)
    {
      job->SetMoveOriginator(moveOriginatorAET, moveOriginatorID);
    }

    // New in Orthanc 1.6.0
    if (Toolbox::GetJsonBooleanField(request, "StorageCommitment", false))
    {
      job->EnableStorageCommitment(true);
    }

    OrthancRestApi::GetApi(call).SubmitCommandsJob
      (call, job.release(), true /* synchronous by default */, request);
  }


  /***************************************************************************
   * DICOM C-Move SCU
   ***************************************************************************/
  
  static void DicomMove(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    Json::Value request;

    if (!call.ParseJsonRequest(request) ||
        request.type() != Json::objectValue ||
        !request.isMember(KEY_RESOURCES) ||
        !request.isMember(KEY_LEVEL) ||
        request[KEY_RESOURCES].type() != Json::arrayValue ||
        request[KEY_LEVEL].type() != Json::stringValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat, "Must provide a JSON body containing fields " +
                             std::string(KEY_RESOURCES) + " and " + std::string(KEY_LEVEL));
    }

    ResourceType level = StringToResourceType(request[KEY_LEVEL].asCString());
    
    std::string localAet = Toolbox::GetJsonStringField
      (request, "LocalAet", context.GetDefaultLocalApplicationEntityTitle());
    std::string targetAet = Toolbox::GetJsonStringField
      (request, "TargetAet", context.GetDefaultLocalApplicationEntityTitle());

    const RemoteModalityParameters source =
      MyGetModalityUsingSymbolicName(call.GetUriComponent("id", ""));

    DicomUserConnection connection(localAet, source);
    connection.Open();
    
    for (Json::Value::ArrayIndex i = 0; i < request[KEY_RESOURCES].size(); i++)
    {
      DicomMap resource;
      FromDcmtkBridge::FromJson(resource, request[KEY_RESOURCES][i]);
      
      connection.Move(targetAet, level, resource);
    }

    // Move has succeeded
    call.GetOutput().AnswerBuffer("{}", MimeType_Json);
  }



  /***************************************************************************
   * Orthanc Peers => Store client
   ***************************************************************************/

  static bool IsExistingPeer(const OrthancRestApi::SetOfStrings& peers,
                             const std::string& id)
  {
    return peers.find(id) != peers.end();
  }

  static void ListPeers(RestApiGetCall& call)
  {
    OrthancConfiguration::ReaderLock lock;

    OrthancRestApi::SetOfStrings peers;
    lock.GetConfiguration().GetListOfOrthancPeers(peers);

    if (call.HasArgument("expand"))
    {
      Json::Value result = Json::objectValue;
      for (OrthancRestApi::SetOfStrings::const_iterator
             it = peers.begin(); it != peers.end(); ++it)
      {
        WebServiceParameters peer;
        
        if (lock.GetConfiguration().LookupOrthancPeer(peer, *it))
        {
          Json::Value info;
          peer.FormatPublic(info);
          result[*it] = info;
        }
      }
      call.GetOutput().AnswerJson(result);
    }
    else // if expand is not present, keep backward compatibility and return an array of peers
    {
      Json::Value result = Json::arrayValue;
      for (OrthancRestApi::SetOfStrings::const_iterator
             it = peers.begin(); it != peers.end(); ++it)
      {
        result.append(*it);
      }

      call.GetOutput().AnswerJson(result);
    }
  }

  static void ListPeerOperations(RestApiGetCall& call)
  {
    OrthancConfiguration::ReaderLock lock;

    OrthancRestApi::SetOfStrings peers;
    lock.GetConfiguration().GetListOfOrthancPeers(peers);

    std::string id = call.GetUriComponent("id", "");
    if (IsExistingPeer(peers, id))
    {
      RestApi::AutoListChildren(call);
    }
  }

  static void PeerStore(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string remote = call.GetUriComponent("id", "");

    Json::Value request;
    std::unique_ptr<OrthancPeerStoreJob> job(new OrthancPeerStoreJob(context));

    GetInstancesToExport(request, *job, remote, call);
    
    OrthancConfiguration::ReaderLock lock;

    WebServiceParameters peer;
    if (lock.GetConfiguration().LookupOrthancPeer(peer, remote))
    {
      job->SetPeer(peer);    
      OrthancRestApi::GetApi(call).SubmitCommandsJob
        (call, job.release(), true /* synchronous by default */, request);
    }
    else
    {
      throw OrthancException(ErrorCode_UnknownResource,
                             "No peer with symbolic name: " + remote);
    }
  }

  static void PeerSystem(RestApiGetCall& call)
  {
    std::string remote = call.GetUriComponent("id", "");

    OrthancConfiguration::ReaderLock lock;

    WebServiceParameters peer;
    if (lock.GetConfiguration().LookupOrthancPeer(peer, remote))
    {
      HttpClient client(peer, "system");
      std::string answer;

      client.SetMethod(HttpMethod_Get);

      if (!client.Apply(answer))
      {
        LOG(ERROR) << "Unable to get the system info from remote Orthanc peer: " << peer.GetUrl();
        call.GetOutput().SignalError(client.GetLastStatus());
        return;
      }

      call.GetOutput().AnswerBuffer(answer, MimeType_Json);
    }
    else
    {
      throw OrthancException(ErrorCode_UnknownResource,
                             "No peer with symbolic name: " + remote);
    }
  }

  // DICOM bridge -------------------------------------------------------------

  static bool IsExistingModality(const OrthancRestApi::SetOfStrings& modalities,
                                 const std::string& id)
  {
    return modalities.find(id) != modalities.end();
  }

  static void ListModalities(RestApiGetCall& call)
  {
    OrthancConfiguration::ReaderLock lock;

    OrthancRestApi::SetOfStrings modalities;
    lock.GetConfiguration().GetListOfDicomModalities(modalities);

    if (call.HasArgument("expand"))
    {
      Json::Value result = Json::objectValue;
      for (OrthancRestApi::SetOfStrings::const_iterator
             it = modalities.begin(); it != modalities.end(); ++it)
      {
        const RemoteModalityParameters& remote = lock.GetConfiguration().GetModalityUsingSymbolicName(*it);
        
        Json::Value info;
        remote.Serialize(info, true /* force advanced format */);
        result[*it] = info;
      }
      call.GetOutput().AnswerJson(result);
    }
    else // if expand is not present, keep backward compatibility and return an array of modalities ids
    {
      Json::Value result = Json::arrayValue;
      for (OrthancRestApi::SetOfStrings::const_iterator
             it = modalities.begin(); it != modalities.end(); ++it)
      {
        result.append(*it);
      }
      call.GetOutput().AnswerJson(result);
    }
  }


  static void ListModalityOperations(RestApiGetCall& call)
  {
    OrthancConfiguration::ReaderLock lock;

    OrthancRestApi::SetOfStrings modalities;
    lock.GetConfiguration().GetListOfDicomModalities(modalities);

    std::string id = call.GetUriComponent("id", "");
    if (IsExistingModality(modalities, id))
    {
      RestApi::AutoListChildren(call);
    }
  }


  static void UpdateModality(RestApiPutCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    Json::Value json;
    if (call.ParseJsonRequest(json))
    {
      RemoteModalityParameters modality;
      modality.Unserialize(json);

      {
        OrthancConfiguration::WriterLock lock;
        lock.GetConfiguration().UpdateModality(call.GetUriComponent("id", ""), modality);
      }

      context.SignalUpdatedModalities();

      call.GetOutput().AnswerBuffer("", MimeType_PlainText);
    }
  }


  static void DeleteModality(RestApiDeleteCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    {
      OrthancConfiguration::WriterLock lock;
      lock.GetConfiguration().RemoveModality(call.GetUriComponent("id", ""));
    }

    context.SignalUpdatedModalities();

    call.GetOutput().AnswerBuffer("", MimeType_PlainText);
  }


  static void UpdatePeer(RestApiPutCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    Json::Value json;
    if (call.ParseJsonRequest(json))
    {
      WebServiceParameters peer;
      peer.Unserialize(json);

      {
        OrthancConfiguration::WriterLock lock;
        lock.GetConfiguration().UpdatePeer(call.GetUriComponent("id", ""), peer);
      }

      context.SignalUpdatedPeers();

      call.GetOutput().AnswerBuffer("", MimeType_PlainText);
    }
  }


  static void DeletePeer(RestApiDeleteCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    {
      OrthancConfiguration::WriterLock lock;
      lock.GetConfiguration().RemovePeer(call.GetUriComponent("id", ""));
    }

    context.SignalUpdatedPeers();

    call.GetOutput().AnswerBuffer("", MimeType_PlainText);
  }


  static void DicomFindWorklist(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    Json::Value json;
    if (call.ParseJsonRequest(json))
    {
      const std::string& localAet = context.GetDefaultLocalApplicationEntityTitle();
      const RemoteModalityParameters remote =
        MyGetModalityUsingSymbolicName(call.GetUriComponent("id", ""));

      std::unique_ptr<ParsedDicomFile> query
        (ParsedDicomFile::CreateFromJson(json, static_cast<DicomFromJsonFlags>(0),
                                         "" /* no private creator */));

      DicomFindAnswers answers(true);

      {
        DicomUserConnection connection(localAet, remote);
        connection.Open();
        connection.FindWorklist(answers, *query);
      }

      Json::Value result;
      answers.ToJson(result, true);
      call.GetOutput().AnswerJson(result);
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat, "Must provide a JSON object");
    }
  }


  // Storage commitment SCU ---------------------------------------------------

  static void StorageCommitmentScu(RestApiPostCall& call)
  {
    static const char* const ORTHANC_RESOURCES = "Resources";
    static const char* const DICOM_INSTANCES = "DicomInstances";
    static const char* const SOP_CLASS_UID = "SOPClassUID";
    static const char* const SOP_INSTANCE_UID = "SOPInstanceUID";

    ServerContext& context = OrthancRestApi::GetContext(call);

    Json::Value json;
    if (!call.ParseJsonRequest(json) ||
        json.type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "Must provide a JSON object with a list of resources");
    }
    else if (!json.isMember(ORTHANC_RESOURCES) &&
             !json.isMember(DICOM_INSTANCES))
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "Empty storage commitment request, one of these fields is mandatory: \"" +
                             std::string(ORTHANC_RESOURCES) + "\" or \"" + std::string(DICOM_INSTANCES) + "\"");
    }
    else
    {
      std::list<std::string> sopClassUids, sopInstanceUids;

      if (json.isMember(ORTHANC_RESOURCES))
      {
        const Json::Value& resources = json[ORTHANC_RESOURCES];
          
        if (resources.type() != Json::arrayValue)
        {
          throw OrthancException(ErrorCode_BadFileFormat,
                                 "The \"" + std::string(ORTHANC_RESOURCES) +
                                 "\" field must provide an array of Orthanc resources");
        }
        else
        {
          for (Json::Value::ArrayIndex i = 0; i < resources.size(); i++)
          {
            if (resources[i].type() != Json::stringValue)
            {
              throw OrthancException(ErrorCode_BadFileFormat,
                                     "The \"" + std::string(ORTHANC_RESOURCES) +
                                     "\" field must provide an array of strings, found: " + resources[i].toStyledString());
            }

            std::list<std::string> instances;
            context.GetIndex().GetChildInstances(instances, resources[i].asString());
            
            for (std::list<std::string>::const_iterator
                   it = instances.begin(); it != instances.end(); ++it)
            {
              std::string sopClassUid, sopInstanceUid;
              DicomMap tags;
              if (context.LookupOrReconstructMetadata(sopClassUid, *it, MetadataType_Instance_SopClassUid) &&
                  context.GetIndex().GetAllMainDicomTags(tags, *it) &&
                  tags.LookupStringValue(sopInstanceUid, DICOM_TAG_SOP_INSTANCE_UID, false))
              {
                sopClassUids.push_back(sopClassUid);
                sopInstanceUids.push_back(sopInstanceUid);
              }
              else
              {
                throw OrthancException(ErrorCode_InternalError,
                                       "Cannot retrieve SOP Class/Instance UID of Orthanc instance: " + *it);
              }
            }
          }
        }
      }

      if (json.isMember(DICOM_INSTANCES))
      {
        const Json::Value& instances = json[DICOM_INSTANCES];
          
        if (instances.type() != Json::arrayValue)
        {
          throw OrthancException(ErrorCode_BadFileFormat,
                                 "The \"" + std::string(DICOM_INSTANCES) +
                                 "\" field must provide an array of DICOM instances");
        }
        else
        {
          for (Json::Value::ArrayIndex i = 0; i < instances.size(); i++)
          {
            if (instances[i].type() == Json::arrayValue)
            {
              if (instances[i].size() != 2 ||
                  instances[i][0].type() != Json::stringValue ||
                  instances[i][1].type() != Json::stringValue)
              {
                throw OrthancException(ErrorCode_BadFileFormat,
                                       "An instance entry must provide an array with 2 strings: "
                                       "SOP Class UID and SOP Instance UID");
              }
              else
              {
                sopClassUids.push_back(instances[i][0].asString());
                sopInstanceUids.push_back(instances[i][1].asString());
              }
            }
            else if (instances[i].type() == Json::objectValue)
            {
              if (!instances[i].isMember(SOP_CLASS_UID) ||
                  !instances[i].isMember(SOP_INSTANCE_UID) ||
                  instances[i][SOP_CLASS_UID].type() != Json::stringValue ||
                  instances[i][SOP_INSTANCE_UID].type() != Json::stringValue)
              {
                throw OrthancException(ErrorCode_BadFileFormat,
                                       "An instance entry must provide an object with 2 string fiels: "
                                       "\"" + std::string(SOP_CLASS_UID) + "\" and \"" +
                                       std::string(SOP_INSTANCE_UID));
              }
              else
              {
                sopClassUids.push_back(instances[i][SOP_CLASS_UID].asString());
                sopInstanceUids.push_back(instances[i][SOP_INSTANCE_UID].asString());
              }
            }
            else
            {
              throw OrthancException(ErrorCode_BadFileFormat,
                                     "JSON array or object is expected to specify one "
                                     "instance to be queried, found: " + instances[i].toStyledString());
            }
          }
        }
      }

      if (sopClassUids.size() != sopInstanceUids.size())
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      const std::string transactionUid = Toolbox::GenerateDicomPrivateUniqueIdentifier();

      if (sopClassUids.empty())
      {
        LOG(WARNING) << "Issuing an outgoing storage commitment request that is empty: " << transactionUid;
      }

      {
        const RemoteModalityParameters remote =
          MyGetModalityUsingSymbolicName(call.GetUriComponent("id", ""));

        const std::string& remoteAet = remote.GetApplicationEntityTitle();
        const std::string& localAet = context.GetDefaultLocalApplicationEntityTitle();
        
        // Create a "pending" storage commitment report BEFORE the
        // actual SCU call in order to avoid race conditions
        context.GetStorageCommitmentReports().Store(
          transactionUid, new StorageCommitmentReports::Report(remoteAet));

        DicomUserConnection scu(localAet, remote);

        std::vector<std::string> a(sopClassUids.begin(), sopClassUids.end());
        std::vector<std::string> b(sopInstanceUids.begin(), sopInstanceUids.end());
        scu.RequestStorageCommitment(transactionUid, a, b);
      }

      Json::Value result = Json::objectValue;
      result["ID"] = transactionUid;
      result["Path"] = "/storage-commitment/" + transactionUid;
      call.GetOutput().AnswerJson(result);
    }
  }


  static void GetStorageCommitmentReport(RestApiGetCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    const std::string& transactionUid = call.GetUriComponent("id", "");

    {
      StorageCommitmentReports::Accessor accessor(
        context.GetStorageCommitmentReports(), transactionUid);

      if (accessor.IsValid())
      {
        Json::Value json;
        accessor.GetReport().Format(json);
        call.GetOutput().AnswerJson(json);
      }
      else
      {
        throw OrthancException(ErrorCode_InexistentItem,
                               "No storage commitment transaction with UID: " + transactionUid);
      }
    }
  }
  

  static void RemoveAfterStorageCommitment(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    const std::string& transactionUid = call.GetUriComponent("id", "");

    {
      StorageCommitmentReports::Accessor accessor(
        context.GetStorageCommitmentReports(), transactionUid);

      if (!accessor.IsValid())
      {
        throw OrthancException(ErrorCode_InexistentItem,
                               "No storage commitment transaction with UID: " + transactionUid);
      }
      else if (accessor.GetReport().GetStatus() != StorageCommitmentReports::Report::Status_Success)
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls,
                               "Cannot remove DICOM instances after failure "
                               "in storage commitment transaction: " + transactionUid);
      }
      else
      {
        std::vector<std::string> sopInstanceUids;
        accessor.GetReport().GetSuccessSopInstanceUids(sopInstanceUids);

        for (size_t i = 0; i < sopInstanceUids.size(); i++)
        {
          std::vector<std::string> orthancId;
          context.GetIndex().LookupIdentifierExact(
            orthancId, ResourceType_Instance, DICOM_TAG_SOP_INSTANCE_UID, sopInstanceUids[i]);

          for (size_t j = 0; j < orthancId.size(); j++)
          {
            LOG(INFO) << "Storage commitment - Removing SOP instance UID / Orthanc ID: "
                      << sopInstanceUids[i] << " / " << orthancId[j];

            Json::Value tmp;
            context.GetIndex().DeleteResource(tmp, orthancId[j], ResourceType_Instance);
          }
        }
          
        call.GetOutput().AnswerBuffer("{}", MimeType_Json);
      }
    }
  }
  

  void OrthancRestApi::RegisterModalities()
  {
    Register("/modalities", ListModalities);
    Register("/modalities/{id}", ListModalityOperations);
    Register("/modalities/{id}", UpdateModality);
    Register("/modalities/{id}", DeleteModality);
    Register("/modalities/{id}/echo", DicomEcho);
    Register("/modalities/{id}/find-patient", DicomFindPatient);
    Register("/modalities/{id}/find-study", DicomFindStudy);
    Register("/modalities/{id}/find-series", DicomFindSeries);
    Register("/modalities/{id}/find-instance", DicomFindInstance);
    Register("/modalities/{id}/find", DicomFind);
    Register("/modalities/{id}/store", DicomStore);
    Register("/modalities/{id}/move", DicomMove);

    // For Query/Retrieve
    Register("/modalities/{id}/query", DicomQuery);
    Register("/queries", ListQueries);
    Register("/queries/{id}", DeleteQuery);
    Register("/queries/{id}", ListQueryOperations);
    Register("/queries/{id}/answers", ListQueryAnswers);
    Register("/queries/{id}/answers/{index}", ListQueryAnswerOperations);
    Register("/queries/{id}/answers/{index}/content", GetQueryOneAnswer);
    Register("/queries/{id}/answers/{index}/retrieve", RetrieveOneAnswer);
    Register("/queries/{id}/answers/{index}/query-instances",
             QueryAnswerChildren<ResourceType_Instance>);
    Register("/queries/{id}/answers/{index}/query-series",
             QueryAnswerChildren<ResourceType_Series>);
    Register("/queries/{id}/answers/{index}/query-studies",
             QueryAnswerChildren<ResourceType_Study>);
    Register("/queries/{id}/level", GetQueryLevel);
    Register("/queries/{id}/modality", GetQueryModality);
    Register("/queries/{id}/query", GetQueryArguments);
    Register("/queries/{id}/retrieve", RetrieveAllAnswers);

    Register("/peers", ListPeers);
    Register("/peers/{id}", ListPeerOperations);
    Register("/peers/{id}", UpdatePeer);
    Register("/peers/{id}", DeletePeer);
    Register("/peers/{id}/store", PeerStore);
    Register("/peers/{id}/system", PeerSystem);

    Register("/modalities/{id}/find-worklist", DicomFindWorklist);

    // Storage commitment
    Register("/modalities/{id}/storage-commitment", StorageCommitmentScu);
    Register("/storage-commitment/{id}", GetStorageCommitmentReport);
    Register("/storage-commitment/{id}/remove", RemoveAfterStorageCommitment);
  }
}
