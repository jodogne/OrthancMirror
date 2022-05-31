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


#include "../PrecompiledHeadersServer.h"
#include "OrthancRestApi.h"

#include "../../../OrthancFramework/Sources/Cache/SharedArchive.h"
#include "../../../OrthancFramework/Sources/DicomNetworking/DicomAssociation.h"
#include "../../../OrthancFramework/Sources/DicomNetworking/DicomControlUserConnection.h"
#include "../../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"
#include "../../../OrthancFramework/Sources/Logging.h"
#include "../../../OrthancFramework/Sources/SerializationToolbox.h"

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
  static const char* const KEY_LOCAL_AET = "LocalAet";
  static const char* const KEY_NORMALIZE = "Normalize";
  static const char* const KEY_QUERY = "Query";
  static const char* const KEY_RESOURCES = "Resources";
  static const char* const KEY_TARGET_AET = "TargetAet";
  static const char* const KEY_TIMEOUT = "Timeout";
  static const char* const KEY_CHECK_FIND = "CheckFind";
  static const char* const SOP_CLASS_UID = "SOPClassUID";
  static const char* const SOP_INSTANCE_UID = "SOPInstanceUID";
  
  static RemoteModalityParameters MyGetModalityUsingSymbolicName(const std::string& name)
  {
    OrthancConfiguration::ReaderLock lock;
    return lock.GetConfiguration().GetModalityUsingSymbolicName(name);
  }


  static void InjectAssociationTimeout(DicomAssociationParameters& params,
                                       const Json::Value& body)
  {
    if (body.type() == Json::objectValue &&
        body.isMember(KEY_TIMEOUT))
    {
      // New in Orthanc 1.7.0
      params.SetTimeout(SerializationToolbox::ReadUnsignedInteger(body, KEY_TIMEOUT));
    }
  }

  static DicomAssociationParameters GetAssociationParameters(RestApiPostCall& call,
                                                             const Json::Value& body)
  {   
    const std::string& localAet =
      OrthancRestApi::GetContext(call).GetDefaultLocalApplicationEntityTitle();
    const RemoteModalityParameters remote =
      MyGetModalityUsingSymbolicName(call.GetUriComponent("id", ""));

    DicomAssociationParameters params(localAet, remote);
    InjectAssociationTimeout(params, body);
    
    return params;
  }


  static DicomAssociationParameters GetAssociationParameters(RestApiPostCall& call)
  {
    Json::Value body;

    if (!call.ParseJsonRequest(body))
    {
      throw OrthancException(ErrorCode_BadFileFormat, "Cannot parse the JSON body");
    }
      
    return GetAssociationParameters(call, body);
  }
  

  static void DocumentModalityParametersShared(RestApiCall& call,
                                               bool includePermissions)
  {
    call.GetDocumentation()
      .SetRequestField("AET", RestApiCallDocumentation::Type_String,
                       "AET of the remote DICOM modality", true)
      .SetRequestField("Host", RestApiCallDocumentation::Type_String,
                       "Host address of the remote DICOM modality (typically, an IP address)", true)
      .SetRequestField("Port", RestApiCallDocumentation::Type_Number,
                       "TCP port of the remote DICOM modality", true)
      .SetRequestField("Manufacturer", RestApiCallDocumentation::Type_String, "Manufacturer of the remote DICOM "
                       "modality (check configuration option `DicomModalities` for possible values", false)
      .SetRequestField("UseDicomTls", RestApiCallDocumentation::Type_Boolean, "Whether to use DICOM TLS "
                       "in the SCU connection initiated by Orthanc (new in Orthanc 1.9.0)", false);

    if (includePermissions)
    {
      call.GetDocumentation()
        .SetRequestField("AllowEcho", RestApiCallDocumentation::Type_Boolean,
                         "Whether to accept C-ECHO SCU commands issued by the remote modality", false)
        .SetRequestField("AllowStore", RestApiCallDocumentation::Type_Boolean,
                         "Whether to accept C-STORE SCU commands issued by the remote modality", false)
        .SetRequestField("AllowFind", RestApiCallDocumentation::Type_Boolean,
                         "Whether to accept C-FIND SCU commands issued by the remote modality", false)
        .SetRequestField("AllowFindWorklist", RestApiCallDocumentation::Type_Boolean,
                         "Whether to accept C-FIND SCU commands for worklists issued by the remote modality", false)
        .SetRequestField("AllowMove", RestApiCallDocumentation::Type_Boolean,
                         "Whether to accept C-MOVE SCU commands issued by the remote modality", false)
        .SetRequestField("AllowGet", RestApiCallDocumentation::Type_Boolean,
                         "Whether to accept C-GET SCU commands issued by the remote modality", false)
        .SetRequestField("AllowStorageCommitment", RestApiCallDocumentation::Type_Boolean,
                         "Whether to accept storage commitment requests issued by the remote modality", false)
        .SetRequestField("AllowTranscoding", RestApiCallDocumentation::Type_Boolean,
                         "Whether to allow transcoding for operations initiated by this modality. "
                         "This option applies to Orthanc C-GET SCP and to Orthanc C-STORE SCU. "
                         "It only has an effect if the global option `EnableTranscoding` is set to `true`.", false);
    }
  }


  /***************************************************************************
   * DICOM C-Echo SCU
   ***************************************************************************/

  static void ExecuteEcho(RestApiOutput& output,
                          const DicomAssociationParameters& parameters,
                          const Json::Value& body)
  {
    DicomControlUserConnection connection(parameters);

    if (connection.Echo())
    {
      bool find = false;
      
      if (body.type() == Json::objectValue &&
          body.isMember(KEY_CHECK_FIND))
      {
        find = SerializationToolbox::ReadBoolean(body, KEY_CHECK_FIND);
      }
      else
      {
        OrthancConfiguration::ReaderLock lock;
        find = lock.GetConfiguration().GetBooleanParameter("DicomEchoChecksFind", false);
      }

      if (find)
      {
        // Issue a C-FIND request at the study level about a random Study Instance UID
        const std::string studyInstanceUid = FromDcmtkBridge::GenerateUniqueIdentifier(ResourceType_Study);
        
        DicomMap query;
        query.SetValue(DICOM_TAG_STUDY_INSTANCE_UID, studyInstanceUid, false);

        DicomFindAnswers answers(false /* not a worklist */);

        // The following line throws an exception if the remote modality doesn't support C-FIND
        connection.Find(answers, ResourceType_Study, query, false /* normalize */);
      }

      // Echo has succeeded
      output.AnswerBuffer("{}", MimeType_Json);
    }
    else
    {
      // Echo has failed
      output.SignalError(HttpStatus_500_InternalServerError);
    }
  }


  static void DocumentEchoShared(RestApiPostCall& call)
  {
    call.GetDocumentation()
      .SetRequestField(KEY_TIMEOUT, RestApiCallDocumentation::Type_Number,
                       "Timeout for the C-ECHO command, in seconds", false)
      .SetRequestField(KEY_CHECK_FIND, RestApiCallDocumentation::Type_Boolean,
                       "Issue a dummy C-FIND command after the C-GET SCU, in order to check whether the remote "
                       "modality knows about Orthanc. This field defaults to the value of the `DicomEchoChecksFind` "
                       "configuration option. New in Orthanc 1.8.1.", false);
  }
  
  
  static void DicomEcho(RestApiPostCall& call)
  {
    if (call.IsDocumentation())
    {
      DocumentEchoShared(call);
      call.GetDocumentation()
        .SetTag("Networking")
        .SetSummary("Trigger C-ECHO SCU")
        .SetDescription("Trigger C-ECHO SCU command against the DICOM modality whose identifier is provided in URL: "
                        "https://book.orthanc-server.com/users/rest.html#performing-c-echo")
        .SetUriArgument("id", "Identifier of the modality of interest");
      return;
    }

    Json::Value body = Json::objectValue;

    if (call.GetBodySize() == 0 /* allow empty body, was disallowed in Orthanc 1.7.0->1.8.1 */ ||
        call.ParseJsonRequest(body))
    {
      const DicomAssociationParameters parameters = GetAssociationParameters(call, body);
      ExecuteEcho(call.GetOutput(), parameters, body);
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat, "Cannot parse the JSON body");
    }
  }
  

  static void DicomEchoTool(RestApiPostCall& call)
  {
    if (call.IsDocumentation())
    {
      DocumentEchoShared(call);
      DocumentModalityParametersShared(call, false);
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("Trigger C-ECHO SCU")
        .SetDescription("Trigger C-ECHO SCU command against a DICOM modality described in the POST body, "
                        "without having to register the modality in some `/modalities/{id}` (new in Orthanc 1.8.1)");
      return;
    }

    Json::Value body;
    if (call.ParseJsonRequest(body))
    {
      RemoteModalityParameters modality;
      modality.Unserialize(body);

      const std::string& localAet =
        OrthancRestApi::GetContext(call).GetDefaultLocalApplicationEntityTitle();
      
      DicomAssociationParameters params(localAet, modality);
      InjectAssociationTimeout(params, body);

      ExecuteEcho(call.GetOutput(), params, body);
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat, "Cannot parse the JSON body");
    }
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
                          DicomControlUserConnection& connection,
                          const DicomMap& fields)
  {
    // Only keep the filters from "fields" that are related to the patient
    DicomMap s;
    fields.ExtractPatientInformation(s);
    connection.Find(result, ResourceType_Patient, s, true /* normalize */);
  }


  static void FindStudy(DicomFindAnswers& result,
                        DicomControlUserConnection& connection,
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
                         DicomControlUserConnection& connection,
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
                           DicomControlUserConnection& connection,
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
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetDeprecated()
        .SetTag("Networking")
        .SetSummary("C-FIND SCU for patients")
        .SetDescription("Trigger C-FIND SCU command against the DICOM modality whose identifier is provided in URL, "
                        "in order to find a patient. Deprecated in favor of `/modalities/{id}/query`.")
        .AddRequestType(MimeType_Json, "Associative array containing the query on the values of the DICOM tags")
        .AddAnswerType(MimeType_Json, "JSON array describing the DICOM tags of the matching patients")
        .SetUriArgument("id", "Identifier of the modality of interest");
      return;
    }

    LOG(WARNING) << "This URI is deprecated: " << call.FlattenUri();

    DicomMap fields;
    DicomMap::SetupFindPatientTemplate(fields);
    if (!MergeQueryAndTemplate(fields, call))
    {
      return;
    }

    DicomFindAnswers answers(false);

    {
      DicomControlUserConnection connection(GetAssociationParameters(call));
      FindPatient(answers, connection, fields);
    }

    Json::Value result;
    answers.ToJson(result, DicomToJsonFormat_Human);
    call.GetOutput().AnswerJson(result);
  }

  static void DicomFindStudy(RestApiPostCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetDeprecated()
        .SetTag("Networking")
        .SetSummary("C-FIND SCU for studies")
        .SetDescription("Trigger C-FIND SCU command against the DICOM modality whose identifier is provided in URL, "
                        "in order to find a study. Deprecated in favor of `/modalities/{id}/query`.")
        .AddRequestType(MimeType_Json, "Associative array containing the query on the values of the DICOM tags")
        .AddAnswerType(MimeType_Json, "JSON array describing the DICOM tags of the matching studies")
        .SetUriArgument("id", "Identifier of the modality of interest");
      return;
    }

    LOG(WARNING) << "This URI is deprecated: " << call.FlattenUri();

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
      
    DicomFindAnswers answers(false);

    {
      DicomControlUserConnection connection(GetAssociationParameters(call));
      FindStudy(answers, connection, fields);
    }

    Json::Value result;
    answers.ToJson(result, DicomToJsonFormat_Human);
    call.GetOutput().AnswerJson(result);
  }

  static void DicomFindSeries(RestApiPostCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetDeprecated()
        .SetTag("Networking")
        .SetSummary("C-FIND SCU for series")
        .SetDescription("Trigger C-FIND SCU command against the DICOM modality whose identifier is provided in URL, "
                        "in order to find a series. Deprecated in favor of `/modalities/{id}/query`.")
        .AddRequestType(MimeType_Json, "Associative array containing the query on the values of the DICOM tags")
        .AddAnswerType(MimeType_Json, "JSON array describing the DICOM tags of the matching series")
        .SetUriArgument("id", "Identifier of the modality of interest");
      return;
    }

    LOG(WARNING) << "This URI is deprecated: " << call.FlattenUri();

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
         
    DicomFindAnswers answers(false);

    {
      DicomControlUserConnection connection(GetAssociationParameters(call));
      FindSeries(answers, connection, fields);
    }

    Json::Value result;
    answers.ToJson(result, DicomToJsonFormat_Human);
    call.GetOutput().AnswerJson(result);
  }

  static void DicomFindInstance(RestApiPostCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetDeprecated()
        .SetTag("Networking")
        .SetSummary("C-FIND SCU for instances")
        .SetDescription("Trigger C-FIND SCU command against the DICOM modality whose identifier is provided in URL, "
                        "in order to find an instance. Deprecated in favor of `/modalities/{id}/query`.")
        .AddRequestType(MimeType_Json, "Associative array containing the query on the values of the DICOM tags")
        .AddAnswerType(MimeType_Json, "JSON array describing the DICOM tags of the matching instances")
        .SetUriArgument("id", "Identifier of the modality of interest");
      return;
    }

    LOG(WARNING) << "This URI is deprecated: " << call.FlattenUri();

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
         
    DicomFindAnswers answers(false);

    {
      DicomControlUserConnection connection(GetAssociationParameters(call));
      FindInstance(answers, connection, fields);
    }

    Json::Value result;
    answers.ToJson(result, DicomToJsonFormat_Human);
    call.GetOutput().AnswerJson(result);
  }


  static void CopyTagIfExists(DicomMap& target,
                              const ParsedDicomFile& source,
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
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetDeprecated()
        .SetTag("Networking")
        .SetSummary("Hierarchical C-FIND SCU")
        .SetDescription("Trigger a sequence of C-FIND SCU commands against the DICOM modality whose identifier is provided in URL, "
                        "in order to discover a hierarchy of matching patients/studies/series. "
                        "Deprecated in favor of `/modalities/{id}/query`.")
        .AddRequestType(MimeType_Json, "Associative array containing the query on the values of the DICOM tags")
        .AddAnswerType(MimeType_Json, "JSON array describing the DICOM tags of the matching patients, embedding the "
                       "matching studies, then the matching series.")
        .SetUriArgument("id", "Identifier of the modality of interest");
      return;
    }

    LOG(WARNING) << "This URI is deprecated: " << call.FlattenUri();

    DicomMap m;
    DicomMap::SetupFindPatientTemplate(m);
    if (!MergeQueryAndTemplate(m, call))
    {
      return;
    }
 
    DicomControlUserConnection connection(GetAssociationParameters(call));
    
    DicomFindAnswers patients(false);
    FindPatient(patients, connection, m);

    // Loop over the found patients
    Json::Value result = Json::arrayValue;
    for (size_t i = 0; i < patients.GetSize(); i++)
    {
      Json::Value patient;
      patients.ToJson(patient, i, DicomToJsonFormat_Human);

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
        studies.ToJson(study, j, DicomToJsonFormat_Human);

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
          series.ToJson(series2, k, DicomToJsonFormat_Human);
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
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Networking")
        .SetSummary("Trigger C-FIND SCU")
        .SetDescription("Trigger C-FIND SCU command against the DICOM modality whose identifier is provided in URL: "
                        "https://book.orthanc-server.com/users/rest.html#performing-query-retrieve-c-find-and-find-with-rest")
        .SetUriArgument("id", "Identifier of the modality of interest")
        .SetRequestField(KEY_QUERY, RestApiCallDocumentation::Type_JsonObject,
                         "Associative array containing the filter on the values of the DICOM tags", true)
        .SetRequestField(KEY_LEVEL, RestApiCallDocumentation::Type_String,
                         "Level of the query (`Patient`, `Study`, `Series` or `Instance`)", true)
        .SetRequestField(KEY_NORMALIZE, RestApiCallDocumentation::Type_Boolean,
                         "Whether to normalize the query, i.e. whether to wipe out from the query, the DICOM tags "
                         "that are not applicable for the query-retrieve level of interest", false)
        .SetRequestField(KEY_LOCAL_AET, RestApiCallDocumentation::Type_String,
                         "Local AET that is used for this commands, defaults to `DicomAet` configuration option. "
                         "Ignored if `DicomModalities` already sets `LocalAet` for this modality.", false)
        .SetRequestField(KEY_TIMEOUT, RestApiCallDocumentation::Type_Number,
                         "Timeout for the C-FIND command and subsequent C-MOVE retrievals, in seconds (new in Orthanc 1.9.1)", false)
        .SetAnswerField("ID", RestApiCallDocumentation::Type_JsonObject,
                        "Identifier of the query, to be used with `/queries/{id}`")
        .SetAnswerField("Path", RestApiCallDocumentation::Type_JsonObject,
                        "Root path to the query in the REST API");
      return;
    }

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
    else if (request.isMember(KEY_LOCAL_AET) &&
             request[KEY_LOCAL_AET].type() != Json::stringValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "The field " + std::string(KEY_LOCAL_AET) + " must contain a string");
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

      if (request.isMember(KEY_LOCAL_AET))
      {
        handler->SetLocalAet(request[KEY_LOCAL_AET].asString());
      }

      if (request.isMember(KEY_TIMEOUT))
      {
        // New in Orthanc 1.9.1
        handler->SetTimeout(SerializationToolbox::ReadUnsignedInteger(request, KEY_TIMEOUT));
      }

      AnswerQueryHandler(call, handler);
    }
  }


  static void ListQueries(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Networking")
        .SetSummary("List query/retrieve operations")
        .SetDescription("List the identifiers of all the query/retrieve operations on DICOM modalities, "
                        "as initiated by calls to `/modalities/{id}/query`. The length of this list is bounded "
                        "by the `QueryRetrieveSize` configuration option of Orthanc. "
                        "https://book.orthanc-server.com/users/rest.html#performing-query-retrieve-c-find-and-find-with-rest")
        .AddAnswerType(MimeType_Json, "JSON array containing the identifiers");
      return;
    }

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
      explicit QueryAccessor(RestApiCall& call) :
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

    static void AnswerDicomMap(RestApiGetCall& call,
                               const DicomMap& value,
                               DicomToJsonFormat format)
    {
      Json::Value full = Json::objectValue;
      FromDcmtkBridge::ToJson(full, value, format);
      call.GetOutput().AnswerJson(full);
    }
  }


  static void ListQueryAnswers(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      OrthancRestApi::DocumentDicomFormat(call, DicomToJsonFormat_Full);
      
      call.GetDocumentation()
        .SetTag("Networking")
        .SetSummary("List answers to a query")
        .SetDescription("List the indices of all the available answers resulting from a query/retrieve operation "
                        "on some DICOM modality, whose identifier is provided in the URL")
        .SetUriArgument("id", "Identifier of the query of interest")
        .SetHttpGetArgument("expand", RestApiCallDocumentation::Type_String,
                            "If present, retrieve detailed information about the individual answers", false)        
        .AddAnswerType(MimeType_Json, "JSON array containing the indices of the answers, or detailed information "
                       "about the reported answers (if `expand` argument is provided)");
      return;
    }

    const bool expand = call.HasArgument("expand");
    const DicomToJsonFormat format = OrthancRestApi::GetDicomFormat(call, DicomToJsonFormat_Full);
    
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
        FromDcmtkBridge::ToJson(json, value, format);

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
    if (call.IsDocumentation())
    {
      OrthancRestApi::DocumentDicomFormat(call, DicomToJsonFormat_Full);

      call.GetDocumentation()
        .SetTag("Networking")
        .SetSummary("Get one answer")
        .SetDescription("Get the content (DICOM tags) of one answer associated with the "
                        "query/retrieve operation whose identifier is provided in the URL")
        .SetUriArgument("id", "Identifier of the query of interest")
        .SetUriArgument("index", "Index of the answer")
        .AddAnswerType(MimeType_Json, "JSON object containing the DICOM tags of the answer");
      return;
    }

    size_t index = boost::lexical_cast<size_t>(call.GetUriComponent("index", ""));

    QueryAccessor query(call);

    DicomMap map;
    query.GetHandler().GetAnswer(map, index);

    AnswerDicomMap(call, map, OrthancRestApi::GetDicomFormat(call, DicomToJsonFormat_Full));
  }


  static void SubmitRetrieveJob(RestApiPostCall& call,
                                bool allAnswers,
                                size_t index)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string targetAet;
    int timeout = -1;
    
    Json::Value body;
    if (call.ParseJsonRequest(body))
    {
      targetAet = Toolbox::GetJsonStringField(body, KEY_TARGET_AET, context.GetDefaultLocalApplicationEntityTitle());
      timeout = Toolbox::GetJsonIntegerField(body, KEY_TIMEOUT, -1);
    }
    else
    {
      body = Json::objectValue;
      if (call.GetBodySize() > 0)
      {
        call.BodyToString(targetAet);
      }
      else
      {
        targetAet = context.GetDefaultLocalApplicationEntityTitle();
      }
    }
    
    std::unique_ptr<DicomMoveScuJob> job(new DicomMoveScuJob(context));
    job->SetQueryFormat(OrthancRestApi::GetDicomFormat(body, DicomToJsonFormat_Short));
    
    {
      QueryAccessor query(call);
      job->SetTargetAet(targetAet);
      job->SetLocalAet(query.GetHandler().GetLocalAet());
      job->SetRemoteModality(query.GetHandler().GetRemoteModality());

      if (timeout >= 0)
      {
        // New in Orthanc 1.7.0
        job->SetTimeout(static_cast<uint32_t>(timeout));
      }
      else if (query.GetHandler().HasTimeout())
      {
        // New in Orthanc 1.9.1
        job->SetTimeout(query.GetHandler().GetTimeout());
      }

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


  static void DocumentRetrieveShared(RestApiPostCall& call)
  {
    OrthancRestApi::DocumentSubmitCommandsJob(call);
    OrthancRestApi::DocumentDicomFormat(call, DicomToJsonFormat_Short);

    call.GetDocumentation()
      .SetTag("Networking")
      .SetUriArgument("id", "Identifier of the query of interest")
      .SetRequestField(KEY_TARGET_AET, RestApiCallDocumentation::Type_String,
                       "AET of the target modality. By default, the AET of Orthanc is used, as defined in the "
                       "`DicomAet` configuration option.", false)
      .SetRequestField(KEY_TIMEOUT, RestApiCallDocumentation::Type_Number,
                       "Timeout for the C-MOVE command, in seconds", false)
      .AddRequestType(MimeType_PlainText, "AET of the target modality");
  }
  

  static void RetrieveOneAnswer(RestApiPostCall& call)
  {
    if (call.IsDocumentation())
    {
      DocumentRetrieveShared(call);
      call.GetDocumentation()
        .SetSummary("Retrieve one answer")
        .SetDescription("Start a C-MOVE SCU command as a job, in order to retrieve one answer associated with the "
                        "query/retrieve operation whose identifiers are provided in the URL: "
                        "https://book.orthanc-server.com/users/rest.html#performing-retrieve-c-move")
        .SetUriArgument("index", "Index of the answer");
      return;
    }

    size_t index = boost::lexical_cast<size_t>(call.GetUriComponent("index", ""));
    SubmitRetrieveJob(call, false, index);
  }


  static void RetrieveAllAnswers(RestApiPostCall& call)
  {
    if (call.IsDocumentation())
    {
      DocumentRetrieveShared(call);
      call.GetDocumentation()
        .SetSummary("Retrieve all answers")
        .SetDescription("Start a C-MOVE SCU command as a job, in order to retrieve all the answers associated with the "
                        "query/retrieve operation whose identifier is provided in the URL: "
                        "https://book.orthanc-server.com/users/rest.html#performing-retrieve-c-move");
      return;
    }

    SubmitRetrieveJob(call, true, 0);
  }


  static void GetQueryArguments(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      OrthancRestApi::DocumentDicomFormat(call, DicomToJsonFormat_Full);

      call.GetDocumentation()
        .SetTag("Networking")
        .SetSummary("Get original query arguments")
        .SetDescription("Get the original DICOM filter associated with the query/retrieve operation "
                        "whose identifier is provided in the URL")
        .SetUriArgument("id", "Identifier of the query of interest")
        .AddAnswerType(MimeType_Json, "Content of the original query");
      return;
    }

    QueryAccessor query(call);
    AnswerDicomMap(call, query.GetHandler().GetQuery(), OrthancRestApi::GetDicomFormat(call, DicomToJsonFormat_Full));
  }


  static void GetQueryLevel(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Networking")
        .SetSummary("Get level of original query")
        .SetDescription("Get the query level (value of the `QueryRetrieveLevel` tag) of the query/retrieve operation "
                        "whose identifier is provided in the URL")
        .SetUriArgument("id", "Identifier of the query of interest")
        .AddAnswerType(MimeType_PlainText, "The level");
      return;
    }

    QueryAccessor query(call);
    call.GetOutput().AnswerBuffer(EnumerationToString(query.GetHandler().GetLevel()), MimeType_PlainText);
  }


  static void GetQueryModality(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Networking")
        .SetSummary("Get modality of original query")
        .SetDescription("Get the identifier of the DICOM modality that was targeted by the query/retrieve operation "
                        "whose identifier is provided in the URL")
        .SetUriArgument("id", "Identifier of the query of interest")
        .AddAnswerType(MimeType_PlainText, "The identifier of the DICOM modality");
      return;
    }

    QueryAccessor query(call);
    call.GetOutput().AnswerBuffer(query.GetHandler().GetModalitySymbolicName(), MimeType_PlainText);
  }


  static void DeleteQuery(RestApiDeleteCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Networking")
        .SetSummary("Delete a query")
        .SetDescription("Delete the query/retrieve operation whose identifier is provided in the URL")
        .SetUriArgument("id", "Identifier of the query of interest");
      return;
    }

    ServerContext& context = OrthancRestApi::GetContext(call);
    context.GetQueryRetrieveArchive().Remove(call.GetUriComponent("id", ""));
    call.GetOutput().AnswerBuffer("", MimeType_PlainText);
  }


  static void ListQueryOperations(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Networking")
        .SetSummary("List operations on a query")
        .SetDescription("List the available operations for the query/retrieve operation whose identifier is provided in the URL")
        .SetUriArgument("id", "Identifier of the query of interest")
        .AddAnswerType(MimeType_Json, "JSON array containing the list of operations");
      return;
    }

    // Ensure that the query of interest does exist
    QueryAccessor query(call);  

    RestApi::AutoListChildren(call);
  }


  static void ListQueryAnswerOperations(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Networking")
        .SetSummary("List operations on an answer")
        .SetDescription("List the available operations on an answer associated with the "
                        "query/retrieve operation whose identifier is provided in the URL")
        .SetUriArgument("id", "Identifier of the query of interest")
        .SetUriArgument("index", "Index of the answer")
        .AddAnswerType(MimeType_Json, "JSON array containing the list of operations");
      return;
    }

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
    
    if (call.IsDocumentation())
    {
      const std::string resources = GetResourceTypeText(CHILDREN_LEVEL, true /* plural */, false /* lower case */);      
      call.GetDocumentation()
        .SetTag("Networking")
        .SetSummary("Query the child " + resources + " of an answer")
        .SetDescription("Issue a second DICOM C-FIND operation, in order to query the child " + resources +
                        " associated with one answer to some query/retrieve operation whose identifiers are provided in the URL")
        .SetUriArgument("id", "Identifier of the query of interest")
        .SetUriArgument("index", "Index of the answer")
        .SetRequestField(KEY_QUERY, RestApiCallDocumentation::Type_JsonObject,
                         "Associative array containing the filter on the values of the DICOM tags", true)
        .SetRequestField(KEY_TIMEOUT, RestApiCallDocumentation::Type_Number,
                         "Timeout for the C-FIND command, in seconds (new in Orthanc 1.9.1)", false)
        .SetAnswerField("ID", RestApiCallDocumentation::Type_JsonObject,
                        "Identifier of the query, to be used with `/queries/{id}`")
        .SetAnswerField("Path", RestApiCallDocumentation::Type_JsonObject,
                        "Root path to the query in the REST API");
      return;
    }

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

        // New in Orthanc 1.9.1
        if (request.isMember(KEY_TIMEOUT))
        {
          handler->SetTimeout(SerializationToolbox::ReadUnsignedInteger(request, KEY_TIMEOUT));
        }
        else if (parent.GetHandler().HasTimeout())
        {
          handler->SetTimeout(parent.GetHandler().GetTimeout());
        }

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

        // This switch-case mimics "DicomControlUserConnection::Move()"
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
    static const char* KEY_MOVE_ORIGINATOR_AET = "MoveOriginatorAet";
    static const char* KEY_MOVE_ORIGINATOR_ID = "MoveOriginatorID";
    static const char* KEY_STORAGE_COMMITMENT = "StorageCommitment";
    
    if (call.IsDocumentation())
    {
      OrthancRestApi::DocumentSubmitCommandsJob(call);
      call.GetDocumentation()
        .SetTag("Networking")
        .SetSummary("Trigger C-STORE SCU")
        .SetDescription("Start a C-STORE SCU command as a job, in order to send DICOM resources stored locally "
                        "to some remote DICOM modality whose identifier is provided in the URL: "
                        "https://book.orthanc-server.com/users/rest.html#rest-store-scu")
        .AddRequestType(MimeType_PlainText, "The Orthanc identifier of one resource to be sent")
        .SetRequestField(KEY_RESOURCES, RestApiCallDocumentation::Type_JsonListOfStrings,
                         "List of the Orthanc identifiers of all the DICOM resources to be sent", true)
        .SetRequestField(KEY_LOCAL_AET, RestApiCallDocumentation::Type_String,
                         "Local AET that is used for this commands, defaults to `DicomAet` configuration option. "
                         "Ignored if `DicomModalities` already sets `LocalAet` for this modality.", false)
        .SetRequestField(KEY_MOVE_ORIGINATOR_AET, RestApiCallDocumentation::Type_String,
                         "Move originator AET that is used for this commands, in order to fake a C-MOVE SCU", false)
        .SetRequestField(KEY_MOVE_ORIGINATOR_ID, RestApiCallDocumentation::Type_Number,
                         "Move originator ID that is used for this commands, in order to fake a C-MOVE SCU", false)
        .SetRequestField(KEY_STORAGE_COMMITMENT, RestApiCallDocumentation::Type_Boolean,
                         "Whether to chain C-STORE with DICOM storage commitment to validate the success of the transmission: "
                         "https://book.orthanc-server.com/users/storage-commitment.html#chaining-c-store-with-storage-commitment", false)
        .SetRequestField(KEY_TIMEOUT, RestApiCallDocumentation::Type_Number,
                         "Timeout for the C-STORE command, in seconds", false)
        .SetUriArgument("id", "Identifier of the modality of interest");
      return;
    }

    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string remote = call.GetUriComponent("id", "");

    Json::Value request;
    std::unique_ptr<DicomModalityStoreJob> job(new DicomModalityStoreJob(context));

    GetInstancesToExport(request, *job, remote, call);

    std::string localAet = Toolbox::GetJsonStringField
      (request, KEY_LOCAL_AET, context.GetDefaultLocalApplicationEntityTitle());
    std::string moveOriginatorAET = Toolbox::GetJsonStringField
      (request, KEY_MOVE_ORIGINATOR_AET, context.GetDefaultLocalApplicationEntityTitle());
    int moveOriginatorID = Toolbox::GetJsonIntegerField
      (request, KEY_MOVE_ORIGINATOR_ID, 0 /* By default, not a C-MOVE */);

    job->SetLocalAet(localAet);
    job->SetRemoteModality(MyGetModalityUsingSymbolicName(remote));

    if (moveOriginatorID != 0)
    {
      job->SetMoveOriginator(moveOriginatorAET, moveOriginatorID);
    }

    // New in Orthanc 1.6.0
    if (Toolbox::GetJsonBooleanField(request, KEY_STORAGE_COMMITMENT, false))
    {
      job->EnableStorageCommitment(true);
    }

    // New in Orthanc 1.7.0
    if (request.isMember(KEY_TIMEOUT))
    {
      job->SetTimeout(SerializationToolbox::ReadUnsignedInteger(request, KEY_TIMEOUT));
    }

    OrthancRestApi::GetApi(call).SubmitCommandsJob
      (call, job.release(), true /* synchronous by default */, request);
  }


  static void DicomStoreStraight(RestApiPostCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Networking")
        .SetSummary("Straight C-STORE SCU")
        .SetDescription("Synchronously send the DICOM instance in the POST body to the remote DICOM modality "
                        "whose identifier is provided in URL, without having to first store it locally within Orthanc. "
                        "This is an alternative to command-line tools such as `storescu` from DCMTK or dcm4che.")
        .SetUriArgument("id", "Identifier of the modality of interest")
        .AddRequestType(MimeType_Dicom, "DICOM instance to be sent")
        .SetAnswerField(SOP_CLASS_UID, RestApiCallDocumentation::Type_String,
                        "SOP class UID of the DICOM instance, if the C-STORE SCU has succeeded")
        .SetAnswerField(SOP_INSTANCE_UID, RestApiCallDocumentation::Type_String,
                        "SOP instance UID of the DICOM instance, if the C-STORE SCU has succeeded");
      return;
    }

    Json::Value body = Json::objectValue;  // No body
    DicomStoreUserConnection connection(GetAssociationParameters(call, body));

    std::string sopClassUid, sopInstanceUid;
    connection.Store(sopClassUid, sopInstanceUid, call.GetBodyData(),
                     call.GetBodySize(), false /* Not a C-MOVE */, "", 0);

    Json::Value answer = Json::objectValue;
    answer[SOP_CLASS_UID] = sopClassUid;
    answer[SOP_INSTANCE_UID] = sopInstanceUid;
    
    call.GetOutput().AnswerJson(answer);
  }


  /***************************************************************************
   * DICOM C-Move SCU
   ***************************************************************************/
  
  static void DicomMove(RestApiPostCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Networking")
        .SetSummary("Trigger C-MOVE SCU")
        .SetDescription("Start a C-MOVE SCU command as a job, in order to drive the execution of a sequence of "
                        "C-STORE commands by some remote DICOM modality whose identifier is provided in the URL: "
                        "https://book.orthanc-server.com/users/rest.html#performing-c-move")
        .SetRequestField(KEY_RESOURCES, RestApiCallDocumentation::Type_JsonListOfObjects,
                         "List of queries identifying all the DICOM resources to be sent", true)
        .SetRequestField(KEY_LEVEL, RestApiCallDocumentation::Type_String,
                         "Level of the query (`Patient`, `Study`, `Series` or `Instance`)", true)
        .SetRequestField(KEY_LOCAL_AET, RestApiCallDocumentation::Type_String,
                         "Local AET that is used for this commands, defaults to `DicomAet` configuration option. "
                         "Ignored if `DicomModalities` already sets `LocalAet` for this modality.", false)
        .SetRequestField(KEY_TARGET_AET, RestApiCallDocumentation::Type_String,
                         "Target AET that will be used by the remote DICOM modality as a target for its C-STORE SCU "
                         "commands, defaults to `DicomAet` configuration option in order to do a simple query/retrieve", false)
        .SetRequestField(KEY_TIMEOUT, RestApiCallDocumentation::Type_Number,
                         "Timeout for the C-STORE command, in seconds", false)
        .SetUriArgument("id", "Identifier of the modality of interest");
      return;
    }

    const ServerContext& context = OrthancRestApi::GetContext(call);

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
      (request, KEY_LOCAL_AET, context.GetDefaultLocalApplicationEntityTitle());
    std::string targetAet = Toolbox::GetJsonStringField
      (request, KEY_TARGET_AET, context.GetDefaultLocalApplicationEntityTitle());

    const RemoteModalityParameters source =
      MyGetModalityUsingSymbolicName(call.GetUriComponent("id", ""));

    DicomAssociationParameters params(localAet, source);
    InjectAssociationTimeout(params, request);  // Handles KEY_TIMEOUT

    DicomControlUserConnection connection(params);

    for (Json::Value::ArrayIndex i = 0; i < request[KEY_RESOURCES].size(); i++)
    {
      DicomMap resource;
      FromDcmtkBridge::FromJson(resource, request[KEY_RESOURCES][i], "Resources elements");
      
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
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Networking")
        .SetSummary("List Orthanc peers")
        .SetDescription("List all the Orthanc peers that are known to Orthanc. This corresponds either to the content of the "
                        "`OrthancPeers` configuration option, or to the information stored in the database if "
                        "`OrthancPeersInDatabase` is `true`.")
        .SetHttpGetArgument("expand", RestApiCallDocumentation::Type_String,
                            "If present, retrieve detailed information about the individual Orthanc peers", false)
        .AddAnswerType(MimeType_Json, "JSON array containing either the identifiers of the peers, or detailed information "
                       "about the peers (if `expand` argument is provided)");
      return;
    }

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
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Networking")
        .SetSummary("List operations on peer")
        .SetDescription("List the operations that are available for an Orthanc peer.")
        .SetUriArgument("id", "Identifier of the peer of interest")
        .AddAnswerType(MimeType_Json, "List of the available operations");
      return;
    }

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
    static const char* KEY_TRANSCODE = "Transcode";
    static const char* KEY_COMPRESS = "Compress";

    if (call.IsDocumentation())
    {
      OrthancRestApi::DocumentSubmitCommandsJob(call);
      call.GetDocumentation()
        .SetTag("Networking")
        .SetSummary("Send to Orthanc peer")
        .SetDescription("Send DICOM resources stored locally to some remote Orthanc peer whose identifier is provided in the URL: "
                        "https://book.orthanc-server.com/users/rest.html#sending-one-resource")
        .AddRequestType(MimeType_PlainText, "The Orthanc identifier of one resource to be sent")
        .SetRequestField(KEY_RESOURCES, RestApiCallDocumentation::Type_JsonListOfStrings,
                         "List of the Orthanc identifiers of all the DICOM resources to be sent", true)
        .SetRequestField(KEY_TRANSCODE, RestApiCallDocumentation::Type_String,
                         "Transcode to the provided DICOM transfer syntax before the actual sending", false)
        .SetRequestField(KEY_COMPRESS, RestApiCallDocumentation::Type_Boolean,
                         "Whether to compress the DICOM instances using gzip before the actual sending", false)
        .SetUriArgument("id", "Identifier of the modality of interest");
      return;
    }

    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string remote = call.GetUriComponent("id", "");

    Json::Value request;
    std::unique_ptr<OrthancPeerStoreJob> job(new OrthancPeerStoreJob(context));

    GetInstancesToExport(request, *job, remote, call);

    if (request.type() == Json::objectValue &&
        request.isMember(KEY_TRANSCODE))
    {
      job->SetTranscode(SerializationToolbox::ReadString(request, KEY_TRANSCODE));
    }

    if (request.type() == Json::objectValue &&
        request.isMember(KEY_COMPRESS))
    {
      job->SetCompress(SerializationToolbox::ReadBoolean(request, KEY_COMPRESS));
    }
    
    {
      OrthancConfiguration::ReaderLock lock;
      
      WebServiceParameters peer;
      if (lock.GetConfiguration().LookupOrthancPeer(peer, remote))
      {
        job->SetPeer(peer);    
      }
      else
      {
        throw OrthancException(ErrorCode_UnknownResource,
                               "No peer with symbolic name: " + remote);
      }
    }

    OrthancRestApi::GetApi(call).SubmitCommandsJob
      (call, job.release(), true /* synchronous by default */, request);
  }

  static void PeerSystem(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Networking")
        .SetSummary("Get peer system information")
        .SetDescription("Get system information about some Orthanc peer. This corresponds to doing a `GET` request "
                        "against the `/system` URI of the remote peer. This route can be used to test connectivity.")
        .SetUriArgument("id", "Identifier of the peer of interest")
        .AddAnswerType(MimeType_Json, "System information about the peer");
      return;
    }

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

  static void GetPeerConfiguration(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      Json::Value sample;
      sample["HttpHeaders"] = Json::objectValue;
      sample["Password"] = Json::nullValue;
      sample["Pkcs11"] = false;
      sample["Url"] = "http://127.0.1.1:5000/";
      sample["Username"] = "alice";      
      call.GetDocumentation()
        .SetTag("Networking")
        .SetSummary("Get peer configuration")
        .SetDescription("Get detailed information about the configuration of some Orthanc peer")
        .SetUriArgument("id", "Identifier of the peer of interest")
        .AddAnswerType(MimeType_Json, "Configuration of the peer")
        .SetSample(sample);
      return;
    }

    OrthancConfiguration::ReaderLock lock;
    const std::string peer = call.GetUriComponent("id", "");

    WebServiceParameters info;  
    if (lock.GetConfiguration().LookupOrthancPeer(info, peer))
    {
      Json::Value answer;
      info.FormatPublic(answer);
      call.GetOutput().AnswerJson(answer);
    }
    else
    {
      throw OrthancException(ErrorCode_UnknownResource,
                             "No peer with symbolic name: " + peer);
    }
  }

  static void PeerStoreStraight(RestApiPostCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Networking")
        .SetSummary("Straight store to peer")
        .SetDescription("Synchronously send the DICOM instance in the POST body to the Orthanc peer "
                        "whose identifier is provided in URL, without having to first store it locally within Orthanc. "
                        "This is an alternative to command-line tools such as `curl`.")
        .SetUriArgument("id", "Identifier of the modality of interest")
        .AddRequestType(MimeType_Dicom, "DICOM instance to be sent")
        .SetAnswerField("ID", RestApiCallDocumentation::Type_String,
                        "Orthanc identifier of the DICOM instance in the remote Orthanc peer")
        .SetAnswerField("ParentPatient", RestApiCallDocumentation::Type_String,
                        "Orthanc identifier of the parent patient in the remote Orthanc peer")
        .SetAnswerField("ParentStudy", RestApiCallDocumentation::Type_String,
                        "Orthanc identifier of the parent study in the remote Orthanc peer")
        .SetAnswerField("ParentSeries", RestApiCallDocumentation::Type_String,
                        "Orthanc identifier of the parent series in the remote Orthanc peer")
        .SetAnswerField("Path", RestApiCallDocumentation::Type_String,
                        "Path to the DICOM instance in the remote Orthanc server")
        .SetAnswerField("Status", RestApiCallDocumentation::Type_String,
                        "Status of the store operation");
      return;
    }

    const std::string peer = call.GetUriComponent("id", "");

    WebServiceParameters info;  

    {
      OrthancConfiguration::ReaderLock lock;
      if (!lock.GetConfiguration().LookupOrthancPeer(info, peer))
      {
        throw OrthancException(ErrorCode_UnknownResource, "No peer with symbolic name: " + peer);
      }
    }

    HttpClient client(info, "instances");
    client.SetMethod(HttpMethod_Post);
    client.AddHeader("Expect", "");
    client.SetExternalBody(call.GetBodyData(), call.GetBodySize());

    Json::Value answer;
    if (client.Apply(answer))
    {
      call.GetOutput().AnswerJson(answer);
    }
    else
    {
      throw OrthancException(ErrorCode_NetworkProtocol, "Cannot send DICOM to remote peer: " + peer);
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
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Networking")
        .SetSummary("List DICOM modalities")
        .SetDescription("List all the DICOM modalities that are known to Orthanc. This corresponds either to the content of the "
                        "`DicomModalities` configuration option, or to the information stored in the database if "
                        "`DicomModalitiesInDatabase` is `true`.")
        .SetHttpGetArgument("expand", RestApiCallDocumentation::Type_String,
                            "If present, retrieve detailed information about the individual DICOM modalities", false)
        .AddAnswerType(MimeType_Json, "JSON array containing either the identifiers of the modalities, or detailed information "
                       "about the modalities (if `expand` argument is provided)");
      return;
    }

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
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Networking")
        .SetSummary("List operations on modality")
        .SetDescription("List the operations that are available for a DICOM modality.")
        .SetUriArgument("id", "Identifier of the DICOM modality of interest")
        .AddAnswerType(MimeType_Json, "List of the available operations");
      return;
    }

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
    if (call.IsDocumentation())
    {
      DocumentModalityParametersShared(call, true);
      call.GetDocumentation()
        .SetTag("Networking")
        .SetSummary("Update DICOM modality")
        .SetDescription("Define a new DICOM modality, or update an existing one. This change is permanent iff. "
                        "`DicomModalitiesInDatabase` is `true`, otherwise it is lost at the next restart of Orthanc.")
        .SetUriArgument("id", "Identifier of the new/updated DICOM modality");
      return;
    }

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
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
  }


  static void DeleteModality(RestApiDeleteCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Networking")
        .SetSummary("Delete DICOM modality")
        .SetDescription("Delete one DICOM modality. This change is permanent iff. `DicomModalitiesInDatabase` is `true`, "
                        "otherwise it is lost at the next restart of Orthanc.")
        .SetUriArgument("id", "Identifier of the DICOM modality of interest");
      return;
    }

    ServerContext& context = OrthancRestApi::GetContext(call);

    {
      OrthancConfiguration::WriterLock lock;
      lock.GetConfiguration().RemoveModality(call.GetUriComponent("id", ""));
    }

    context.SignalUpdatedModalities();

    call.GetOutput().AnswerBuffer("", MimeType_PlainText);
  }


  static void GetModalityConfiguration(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      Json::Value sample;
      sample["AET"] = "ORTHANCTEST";
      sample["AllowEcho"] = true;
      sample["AllowEventReport"] = true;
      sample["AllowFind"] = true;
      sample["AllowFindWorklist"] = true;
      sample["AllowGet"] = true;
      sample["AllowMove"] = true;
      sample["AllowNAction"] = true;
      sample["AllowStore"] = true;
      sample["AllowTranscoding"] = true;
      sample["Host"] = "127.0.1.1";
      sample["Manufacturer"] = "Generic";
      sample["Port"] = 5001;
      call.GetDocumentation()
        .SetTag("Networking")
        .SetSummary("Get modality configuration")
        .SetDescription("Get detailed information about the configuration of some DICOM modality")
        .SetUriArgument("id", "Identifier of the modality of interest")
        .AddAnswerType(MimeType_Json, "Configuration of the modality")
        .SetSample(sample);
      return;
    }

    const std::string modality = call.GetUriComponent("id", "");

    Json::Value answer;

    {
      OrthancConfiguration::ReaderLock lock;
      lock.GetConfiguration().GetModalityUsingSymbolicName(modality).Serialize(answer, true /* force advanced format */);
    }
    
    call.GetOutput().AnswerJson(answer);
  }


  static void UpdatePeer(RestApiPutCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Networking")
        .SetSummary("Update Orthanc peer")
        .SetDescription("Define a new Orthanc peer, or update an existing one. This change is permanent iff. "
                        "`OrthancPeersInDatabase` is `true`, otherwise it is lost at the next restart of Orthanc.")
        .SetUriArgument("id", "Identifier of the new/updated Orthanc peer")
        .SetRequestField("URL", RestApiCallDocumentation::Type_String,
                         "URL of the root of the REST API of the remote Orthanc peer, for instance `http://localhost:8042/`", true)
        .SetRequestField("Username", RestApiCallDocumentation::Type_String,
                         "Username for the credentials", false)
        .SetRequestField("Password", RestApiCallDocumentation::Type_String,
                         "Password for the credentials", false)
        .SetRequestField("CertificateFile", RestApiCallDocumentation::Type_String,
                         "SSL certificate for the HTTPS connections", false)
        .SetRequestField("CertificateKeyFile", RestApiCallDocumentation::Type_String,
                         "Key file for the SSL certificate for the HTTPS connections", false)
        .SetRequestField("CertificateKeyPassword", RestApiCallDocumentation::Type_String,
                         "Key password for the SSL certificate for the HTTPS connections", false)
        .SetRequestField("HttpHeaders", RestApiCallDocumentation::Type_JsonObject,
                         "HTTP headers to be used for the connections to the remote peer", false);
      return;
    }

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
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Networking")
        .SetSummary("Delete Orthanc peer")
        .SetDescription("Delete one Orthanc peer. This change is permanent iff. `OrthancPeersInDatabase` is `true`, "
                        "otherwise it is lost at the next restart of Orthanc.")
        .SetUriArgument("id", "Identifier of the Orthanc peer of interest");
      return;
    }

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
    if (call.IsDocumentation())
    {
      OrthancRestApi::DocumentDicomFormat(call, DicomToJsonFormat_Human);

      call.GetDocumentation()
        .SetTag("Networking")
        .SetSummary("C-FIND SCU for worklist")
        .SetDescription("Trigger C-FIND SCU command against the remote worklists of the DICOM modality "
                        "whose identifier is provided in URL")
        .SetUriArgument("id", "Identifier of the modality of interest")
        .SetRequestField(KEY_QUERY, RestApiCallDocumentation::Type_JsonObject,
                         "Associative array containing the filter on the values of the DICOM tags", true)
        .AddAnswerType(MimeType_Json, "JSON array describing the DICOM tags of the matching worklists");
      return;
    }

    Json::Value json;
    if (call.ParseJsonRequest(json))
    {
      std::unique_ptr<ParsedDicomFile> query;
      DicomToJsonFormat format;

      if (json.isMember(KEY_QUERY))
      {
        // New in Orthanc 1.9.5
        query.reset(ParsedDicomFile::CreateFromJson(json[KEY_QUERY], static_cast<DicomFromJsonFlags>(0),
                                                    "" /* no private creator */));
        format = OrthancRestApi::GetDicomFormat(json, DicomToJsonFormat_Human);
      }
      else
      {
        // Compatibility with Orthanc <= 1.9.4
        query.reset(ParsedDicomFile::CreateFromJson(json, static_cast<DicomFromJsonFlags>(0),
                                                    "" /* no private creator */));
        format = DicomToJsonFormat_Human;
      }

      DicomFindAnswers answers(true);

      {
        DicomControlUserConnection connection(GetAssociationParameters(call, json));
        connection.FindWorklist(answers, *query);
      }

      Json::Value result;
      answers.ToJson(result, format);
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

    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Networking")
        .SetSummary("Trigger storage commitment request")
        .SetDescription("Trigger a storage commitment request to some remote DICOM modality whose identifier is provided "
                        "in the URL: https://book.orthanc-server.com/users/storage-commitment.html#storage-commitment-scu")
        .SetRequestField(ORTHANC_RESOURCES, RestApiCallDocumentation::Type_JsonListOfStrings,
                         "List of the Orthanc identifiers of the DICOM resources to be checked by storage commitment", true)
        .SetRequestField(DICOM_INSTANCES, RestApiCallDocumentation::Type_JsonListOfObjects,
                         "List of DICOM resources that are not necessarily stored within Orthanc, but that must "
                         "be checked by storage commitment. This is a list of JSON objects that must contain the "
                         "`SOPClassUID` and `SOPInstanceUID` fields.", true)
        .SetRequestField(KEY_TIMEOUT, RestApiCallDocumentation::Type_Number,
                         "Timeout for the storage commitment command (new in Orthanc 1.9.1)", false)
        .SetAnswerField("ID", RestApiCallDocumentation::Type_JsonObject,
                        "Identifier of the storage commitment report, to be used with `/storage-commitment/{id}`")
        .SetAnswerField("Path", RestApiCallDocumentation::Type_JsonObject,
                        "Root path to the storage commitment report in the REST API")
        .SetUriArgument("id", "Identifier of the modality of interest");
      return;
    }

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
              if (context.LookupOrReconstructMetadata(sopClassUid, *it, ResourceType_Instance, MetadataType_Instance_SopClassUid) &&
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

        DicomAssociationParameters parameters(localAet, remote);
        InjectAssociationTimeout(parameters, json);
        
        std::vector<std::string> a(sopClassUids.begin(), sopClassUids.end());
        std::vector<std::string> b(sopInstanceUids.begin(), sopInstanceUids.end());
        DicomAssociation::RequestStorageCommitment(parameters, transactionUid, a, b);
      }

      Json::Value result = Json::objectValue;
      result["ID"] = transactionUid;
      result["Path"] = "/storage-commitment/" + transactionUid;
      call.GetOutput().AnswerJson(result);
    }
  }


  static void GetStorageCommitmentReport(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Networking")
        .SetSummary("Get storage commitment report")
        .SetDescription("Get the storage commitment report whose identifier is provided in the URL: "
                        "https://book.orthanc-server.com/users/storage-commitment.html#storage-commitment-scu")
        .SetAnswerField("Status", RestApiCallDocumentation::Type_String,
                        "Can be `Success`, `Failure`, or `Pending` (the latter means that no report has been received yet)")
        .SetAnswerField("RemoteAET", RestApiCallDocumentation::Type_String,
                        "AET of the remote DICOM modality")
        .SetAnswerField("Failures", RestApiCallDocumentation::Type_JsonListOfObjects,
                        "List of failures that have been encountered during the storage commitment request")
        .SetAnswerField("Success", RestApiCallDocumentation::Type_JsonListOfObjects,
                        "List of DICOM instances that have been acknowledged by the remote modality, "
                        "each one is reported as a JSON object containing the `SOPClassUID` and "
                        "`SOPInstanceUID` DICOM tags")
        .SetUriArgument("id", "Identifier of the storage commitment report");
      return;
    }

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
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("Networking")
        .SetSummary("Remove after storage commitment")
        .SetDescription("Remove out of Orthanc, the DICOM instances that have been reported to have been properly "
                        "received the storage commitment report whose identifier is provided in the URL. This is "
                        "only possible if the `Status` of the storage commitment report is `Success`. "
                        "https://book.orthanc-server.com/users/storage-commitment.html#removing-the-instances")
        .SetUriArgument("id", "Identifier of the storage commitment report");
      return;
    }

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
            CLOG(INFO, HTTP) << "Storage commitment - Removing SOP instance UID / Orthanc ID: "
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
    Register("/modalities/{id}/store-straight", DicomStoreStraight);  // New in 1.6.1
    Register("/modalities/{id}/move", DicomMove);
    Register("/modalities/{id}/configuration", GetModalityConfiguration);  // New in 1.8.1

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
    Register("/peers/{id}/configuration", GetPeerConfiguration);  // New in 1.8.1
    Register("/peers/{id}/store-straight", PeerStoreStraight);    // New in 1.9.1

    Register("/modalities/{id}/find-worklist", DicomFindWorklist);

    // Storage commitment
    Register("/modalities/{id}/storage-commitment", StorageCommitmentScu);
    Register("/storage-commitment/{id}", GetStorageCommitmentReport);
    Register("/storage-commitment/{id}/remove", RemoveAfterStorageCommitment);

    Register("/tools/dicom-echo", DicomEchoTool);  // New in 1.8.1
  }
}
