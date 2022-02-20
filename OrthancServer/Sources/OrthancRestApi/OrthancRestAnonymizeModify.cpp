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

#include "../../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"
#include "../../../OrthancFramework/Sources/Logging.h"
#include "../../../OrthancFramework/Sources/SerializationToolbox.h"
#include "../OrthancConfiguration.h"
#include "../ServerContext.h"
#include "../ServerJobs/MergeStudyJob.h"
#include "../ServerJobs/ResourceModificationJob.h"
#include "../ServerJobs/SplitStudyJob.h"

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/predicate.hpp>

#define INFO_SUBSEQUENCES \
  "Starting with Orthanc 1.9.4, paths to subsequences can be provided using the "\
  "same syntax as the `dcmodify` command-line tool (wildcards are supported as well)."


static const char* const CONTENT = "Content";
static const char* const FORCE = "Force";
static const char* const INSTANCES = "Instances";
static const char* const INTERPRET_BINARY_TAGS = "InterpretBinaryTags";
static const char* const KEEP = "Keep";
static const char* const KEEP_PRIVATE_TAGS = "KeepPrivateTags";
static const char* const KEEP_SOURCE = "KeepSource";
static const char* const LEVEL = "Level";
static const char* const PARENT = "Parent";
static const char* const PRIVATE_CREATOR = "PrivateCreator";
static const char* const REMOVE = "Remove";
static const char* const REPLACE = "Replace";
static const char* const RESOURCES = "Resources";
static const char* const SERIES = "Series";
static const char* const TAGS = "Tags";
static const char* const TRANSCODE = "Transcode";


namespace Orthanc
{
  // Modification of DICOM instances ------------------------------------------

  
  static std::string GeneratePatientName(ServerContext& context)
  {
    uint64_t seq = context.GetIndex().IncrementGlobalSequence(GlobalProperty_AnonymizationSequence, true /* shared */);
    return "Anonymized" + boost::lexical_cast<std::string>(seq);
  }


  static void DocumentKeepSource(RestApiPostCall& call)
  {
    call.GetDocumentation()
      .SetRequestField(KEEP_SOURCE, RestApiCallDocumentation::Type_Boolean,
                       "If set to `false`, instructs Orthanc to the remove original resources. "
                       "By default, the original resources are kept in Orthanc.", false);
  }


  static void DocumentModifyOptions(RestApiPostCall& call)
  {
    // Check out "DicomModification::ParseModifyRequest()"
    call.GetDocumentation()
      .SetRequestField(TRANSCODE, RestApiCallDocumentation::Type_String,
                       "Transcode the DICOM instances to the provided DICOM transfer syntax: "
                       "https://book.orthanc-server.com/faq/transcoding.html", false)
      .SetRequestField(FORCE, RestApiCallDocumentation::Type_Boolean,
                       "Allow the modification of tags related to DICOM identifiers, at the risk of "
                       "breaking the DICOM model of the real world", false)
      .SetRequestField("RemovePrivateTags", RestApiCallDocumentation::Type_Boolean,
                       "Remove the private tags from the DICOM instances (defaults to `false`)", false)
      .SetRequestField(REPLACE, RestApiCallDocumentation::Type_JsonObject,
                       "Associative array to change the value of some DICOM tags in the DICOM instances. " INFO_SUBSEQUENCES, false)
      .SetRequestField(REMOVE, RestApiCallDocumentation::Type_JsonListOfStrings,
                       "List of tags that must be removed from the DICOM instances. " INFO_SUBSEQUENCES, false)
      .SetRequestField(KEEP, RestApiCallDocumentation::Type_JsonListOfStrings,
                       "Keep the original value of the specified tags, to be chosen among the `StudyInstanceUID`, "
                       "`SeriesInstanceUID` and `SOPInstanceUID` tags. Avoid this feature as much as possible, "
                       "as this breaks the DICOM model of the real world.", false)
      .SetRequestField(PRIVATE_CREATOR, RestApiCallDocumentation::Type_String,
                       "The private creator to be used for private tags in `Replace`", false);

    // This was existing, but undocumented in Orthanc <= 1.9.6
    DocumentKeepSource(call);
  }


  static void DocumentAnonymizationOptions(RestApiPostCall& call)
  {
    // Check out "DicomModification::ParseAnonymizationRequest()"
    call.GetDocumentation()
      .SetRequestField(FORCE, RestApiCallDocumentation::Type_Boolean,
                       "Allow the modification of tags related to DICOM identifiers, at the risk of "
                       "breaking the DICOM model of the real world", false)
      .SetRequestField("DicomVersion", RestApiCallDocumentation::Type_String,
                       "Version of the DICOM standard to be used for anonymization. Check out "
                       "configuration option `DeidentifyLogsDicomVersion` for possible values.", false)
      .SetRequestField(KEEP_PRIVATE_TAGS, RestApiCallDocumentation::Type_Boolean,
                       "Keep the private tags from the DICOM instances (defaults to `false`)", false)
      .SetRequestField(REPLACE, RestApiCallDocumentation::Type_JsonObject,
                       "Associative array to change the value of some DICOM tags in the DICOM instances. " INFO_SUBSEQUENCES, false)
      .SetRequestField(REMOVE, RestApiCallDocumentation::Type_JsonListOfStrings,
                       "List of additional tags to be removed from the DICOM instances. " INFO_SUBSEQUENCES, false)
      .SetRequestField(KEEP, RestApiCallDocumentation::Type_JsonListOfStrings,
                       "List of DICOM tags whose value must not be destroyed by the anonymization. " INFO_SUBSEQUENCES, false)
      .SetRequestField(PRIVATE_CREATOR, RestApiCallDocumentation::Type_String,
                       "The private creator to be used for private tags in `Replace`", false);

    // This was existing, but undocumented in Orthanc <= 1.9.6
    DocumentKeepSource(call);
  }


  static void ParseModifyRequest(Json::Value& request,
                                 DicomModification& target,
                                 const RestApiPostCall& call)
  {
    // curl http://localhost:8042/series/95a6e2bf-9296e2cc-bf614e2f-22b391ee-16e010e0/modify -X POST -d '{"Replace":{"InstitutionName":"My own clinic"},"Priority":9}'

    {
      OrthancConfiguration::ReaderLock lock;
      target.SetPrivateCreator(lock.GetConfiguration().GetDefaultPrivateCreator());
    }
    
    if (call.ParseJsonRequest(request))
    {
      target.ParseModifyRequest(request);
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
  }


  static void ParseAnonymizationRequest(Json::Value& request,
                                        DicomModification& target,
                                        RestApiPostCall& call)
  {
    // curl http://localhost:8042/instances/6e67da51-d119d6ae-c5667437-87b9a8a5-0f07c49f/anonymize -X POST -d '{"Replace":{"PatientName":"hello","0010-0020":"world"},"Keep":["StudyDescription", "SeriesDescription"],"KeepPrivateTags": true,"Remove":["Modality"]}' > Anonymized.dcm

    {
      OrthancConfiguration::ReaderLock lock;
      target.SetPrivateCreator(lock.GetConfiguration().GetDefaultPrivateCreator());
    }
    
    if (call.ParseJsonRequest(request) &&
        request.isObject())
    {
      bool patientNameOverridden;
      target.ParseAnonymizationRequest(patientNameOverridden, request);

      if (!patientNameOverridden)
      {
        // Override the random Patient's Name by one that is more
        // user-friendly (provided none was specified by the user)
        target.Replace(DICOM_TAG_PATIENT_NAME, GeneratePatientName(OrthancRestApi::GetContext(call)), true);
      }
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
  }


  static void AnonymizeOrModifyInstance(DicomModification& modification,
                                        RestApiPostCall& call,
                                        bool transcode,
                                        DicomTransferSyntax targetSyntax)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);
    std::string id = call.GetUriComponent("id", "");

    std::unique_ptr<ParsedDicomFile> modified;

    {
      ServerContext::DicomCacheLocker locker(context, id);
      modified.reset(locker.GetDicom().Clone(true));
    }
    
    modification.Apply(*modified);

    if (transcode)
    {
      IDicomTranscoder::DicomImage source;
      source.AcquireParsed(*modified);  // "modified" is invalid below this point
      
      IDicomTranscoder::DicomImage transcoded;

      std::set<DicomTransferSyntax> s;
      s.insert(targetSyntax);
      
      if (context.Transcode(transcoded, source, s, true))
      {      
        call.GetOutput().AnswerBuffer(transcoded.GetBufferData(),
                                      transcoded.GetBufferSize(), MimeType_Dicom);
      }
      else
      {
        throw OrthancException(ErrorCode_InternalError,
                               "Cannot transcode to transfer syntax: " +
                               std::string(GetTransferSyntaxUid(targetSyntax)));
      }
    }
    else
    {
      modified->Answer(call.GetOutput());
    }
  }


  static ResourceType DetectModifyLevel(const DicomModification& modification)
  {
    if (modification.IsReplaced(DICOM_TAG_PATIENT_ID))
    {
      return ResourceType_Patient;
    }
    else if (modification.IsReplaced(DICOM_TAG_STUDY_INSTANCE_UID))
    {
      return ResourceType_Study;
    }
    else if (modification.IsReplaced(DICOM_TAG_SERIES_INSTANCE_UID))
    {
      return ResourceType_Series;
    }
    else
    {
      return ResourceType_Instance;
    }
  }


  static void ModifyInstance(RestApiPostCall& call)
  {
    if (call.IsDocumentation())
    {
      DocumentModifyOptions(call);
      call.GetDocumentation()
        .SetTag("Instances")
        .SetSummary("Modify instance")
        .SetDescription("Download a modified version of the DICOM instance whose Orthanc identifier is provided in the URL: "
                        "https://book.orthanc-server.com/users/anonymization.html#modification-of-a-single-instance")
        .SetUriArgument("id", "Orthanc identifier of the instance of interest")
        .AddAnswerType(MimeType_Dicom, "The modified DICOM instance");
      return;
    }

    DicomModification modification;
    modification.SetAllowManualIdentifiers(true);

    Json::Value request;
    ParseModifyRequest(request, modification, call);

    modification.SetLevel(DetectModifyLevel(modification));

    if (request.isMember(TRANSCODE))
    {
      std::string s = SerializationToolbox::ReadString(request, TRANSCODE);
      
      DicomTransferSyntax syntax;
      if (LookupTransferSyntax(syntax, s))
      {
        AnonymizeOrModifyInstance(modification, call, true, syntax);
      }
      else
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange, "Unknown transfer syntax: " + s);
      }
    }
    else
    {
      AnonymizeOrModifyInstance(modification, call, false /* no transcoding */,
                                DicomTransferSyntax_LittleEndianImplicit /* unused */);
    }
  }


  static void AnonymizeInstance(RestApiPostCall& call)
  {
    if (call.IsDocumentation())
    {
      DocumentAnonymizationOptions(call);
      call.GetDocumentation()
        .SetTag("Instances")
        .SetSummary("Anonymize instance")
        .SetDescription("Download an anonymized version of the DICOM instance whose Orthanc identifier is provided in the URL: "
                        "https://book.orthanc-server.com/users/anonymization.html#anonymization-of-a-single-instance")
        .SetUriArgument("id", "Orthanc identifier of the instance of interest")
        .AddAnswerType(MimeType_Dicom, "The anonymized DICOM instance");
      return;
    }

    DicomModification modification;
    modification.SetAllowManualIdentifiers(true);

    Json::Value request;
    ParseAnonymizationRequest(request, modification, call);

    AnonymizeOrModifyInstance(modification, call, false /* no transcoding */,
                              DicomTransferSyntax_LittleEndianImplicit /* unused */);
  }


  static void SetKeepSource(CleaningInstancesJob& job,
                            const Json::Value& body)
  {
    if (body.isMember(KEEP_SOURCE))
    {
      job.SetKeepSource(SerializationToolbox::ReadBoolean(body, KEEP_SOURCE));
    }
  }


  static void SubmitModificationJob(std::unique_ptr<DicomModification>& modification,
                                    bool isAnonymization,
                                    RestApiPostCall& call,
                                    const Json::Value& body,
                                    ResourceType outputLevel /* unused for multiple resources */,
                                    bool isSingleResource,
                                    const std::set<std::string>& resources)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    std::unique_ptr<ResourceModificationJob> job(new ResourceModificationJob(context));

    if (isSingleResource)  // This notably configures the output format
    {
      job->SetSingleResourceModification(modification.release(), outputLevel, isAnonymization);
    }
    else
    {
      job->SetMultipleResourcesModification(modification.release(), isAnonymization);
    }
    
    job->SetOrigin(call);
    SetKeepSource(*job, body);

    if (body.isMember(TRANSCODE))
    {
      job->SetTranscode(SerializationToolbox::ReadString(body, TRANSCODE));
    }

    for (std::set<std::string>::const_iterator
           it = resources.begin(); it != resources.end(); ++it)
    {
      context.AddChildInstances(*job, *it);
    }
    
    job->AddTrailingStep();

    OrthancRestApi::GetApi(call).SubmitCommandsJob
      (call, job.release(), true /* synchronous by default */, body);
  }


  static void SubmitModificationJob(std::unique_ptr<DicomModification>& modification,
                                    bool isAnonymization,
                                    RestApiPostCall& call,
                                    const Json::Value& body,
                                    ResourceType outputLevel)
  {
    // This was the only flavor in Orthanc <= 1.9.3
    std::set<std::string> resources;
    resources.insert(call.GetUriComponent("id", ""));
    
    SubmitModificationJob(modification, isAnonymization, call, body, outputLevel,
                          true /* single resource */, resources);
  }

  
  static void SubmitBulkJob(std::unique_ptr<DicomModification>& modification,
                            bool isAnonymization,
                            RestApiPostCall& call,
                            const Json::Value& body)
  {
    std::set<std::string> resources;
    SerializationToolbox::ReadSetOfStrings(resources, body, RESOURCES);

    SubmitModificationJob(modification, isAnonymization,
                          call, body, ResourceType_Instance /* arbitrary value, unused */,
                          false /* multiple resources */, resources);
  }


  template <enum ResourceType resourceType>
  static void ModifyResource(RestApiPostCall& call)
  {
    if (call.IsDocumentation())
    {
      OrthancRestApi::DocumentSubmitCommandsJob(call);
      DocumentModifyOptions(call);
      const std::string r = GetResourceTypeText(resourceType, false /* plural */, false /* lower case */);      
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(resourceType, true /* plural */, true /* upper case */))
        .SetSummary("Modify " + r)
        .SetDescription("Start a job that will modify all the DICOM instances within the " + r +
                        " whose identifier is provided in the URL. The modified DICOM instances will be "
                        "stored into a brand new " + r + ", whose Orthanc identifiers will be returned by the job. "
                        "https://book.orthanc-server.com/users/anonymization.html#modification-of-studies-or-series")
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest");
      return;
    }
    
    std::unique_ptr<DicomModification> modification(new DicomModification);

    Json::Value body;
    ParseModifyRequest(body, *modification, call);

    modification->SetLevel(resourceType);
    
    SubmitModificationJob(modification, false /* not an anonymization */,
                          call, body, resourceType);
  }


  // New in Orthanc 1.9.4
  static void BulkModify(RestApiPostCall& call)
  {
    if (call.IsDocumentation())
    {
      OrthancRestApi::DocumentSubmitCommandsJob(call);
      DocumentModifyOptions(call);
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("Modify a set of resources")
        .SetRequestField(RESOURCES, RestApiCallDocumentation::Type_JsonListOfStrings,
                         "List of the Orthanc identifiers of the patients/studies/series/instances of interest.", true)
        .SetRequestField(LEVEL, RestApiCallDocumentation::Type_String,
                         "Level of the modification (`Patient`, `Study`, `Series` or `Instance`). If absent, "
                         "the level defaults to `Instance`, but is set to `Patient` if `PatientID` is modified, "
                         "to `Study` if `StudyInstanceUID` is modified, or to `Series` if `SeriesInstancesUID` "
                         "is modified. (new in Orthanc 1.9.7)", false)
        .SetDescription("Start a job that will modify all the DICOM patients, studies, series or instances "
                        "whose identifiers are provided in the `Resources` field.")
        .AddAnswerType(MimeType_Json, "The list of all the resources that have been altered by this modification");
      return;
    }
    
    std::unique_ptr<DicomModification> modification(new DicomModification);

    Json::Value body;
    ParseModifyRequest(body, *modification, call);

    if (body.isMember(LEVEL))
    {
      // This case was introduced in Orthanc 1.9.7
      modification->SetLevel(StringToResourceType(body[LEVEL].asCString()));
    }
    else
    {
      modification->SetLevel(DetectModifyLevel(*modification));
    }

    SubmitBulkJob(modification, false /* not an anonymization */, call, body);
  }


  template <enum ResourceType resourceType>
  static void AnonymizeResource(RestApiPostCall& call)
  {
    if (call.IsDocumentation())
    {
      OrthancRestApi::DocumentSubmitCommandsJob(call);
      DocumentAnonymizationOptions(call);
      const std::string r = GetResourceTypeText(resourceType, false /* plural */, false /* lower case */);      
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(resourceType, true /* plural */, true /* upper case */))
        .SetSummary("Anonymize " + r)
        .SetDescription("Start a job that will anonymize all the DICOM instances within the " + r +
                        " whose identifier is provided in the URL. The modified DICOM instances will be "
                        "stored into a brand new " + r + ", whose Orthanc identifiers will be returned by the job. "
                        "https://book.orthanc-server.com/users/anonymization.html#anonymization-of-patients-studies-or-series")
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest");
      return;
    }

    std::unique_ptr<DicomModification> modification(new DicomModification);

    Json::Value body;
    ParseAnonymizationRequest(body, *modification, call);

    SubmitModificationJob(modification, true /* anonymization */,
                          call, body, resourceType);
  }


  // New in Orthanc 1.9.4
  static void BulkAnonymize(RestApiPostCall& call)
  {
    if (call.IsDocumentation())
    {
      OrthancRestApi::DocumentSubmitCommandsJob(call);
      DocumentAnonymizationOptions(call);
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("Anonymize a set of resources")
        .SetRequestField(RESOURCES, RestApiCallDocumentation::Type_JsonListOfStrings,
                         "List of the Orthanc identifiers of the patients/studies/series/instances of interest.", true)
        .SetDescription("Start a job that will anonymize all the DICOM patients, studies, series or instances "
                        "whose identifiers are provided in the `Resources` field.")
        .AddAnswerType(MimeType_Json, "The list of all the resources that have been created by this anonymization");
      return;
    }

    std::unique_ptr<DicomModification> modification(new DicomModification);

    Json::Value body;
    ParseAnonymizationRequest(body, *modification, call);

    SubmitBulkJob(modification, true /* anonymization */, call, body);
  }


  static void StoreCreatedInstance(std::string& id /* out */,
                                   RestApiPostCall& call,
                                   ParsedDicomFile& dicom,
                                   bool sendAnswer)
  {
    std::unique_ptr<DicomInstanceToStore> toStore(DicomInstanceToStore::CreateFromParsedDicomFile(dicom));
    toStore->SetOrigin(DicomInstanceOrigin::FromRest(call));

    ServerContext& context = OrthancRestApi::GetContext(call);
    ServerContext::StoreResult result = context.Store(id, *toStore, StoreInstanceMode_Default);

    if (result.GetStatus() == StoreStatus_Failure)
    {
      throw OrthancException(ErrorCode_CannotStoreInstance);
    }

    if (sendAnswer)
    {
      OrthancRestApi::GetApi(call).AnswerStoredInstance(call, *toStore, result.GetStatus(), id);
    }
  }


  static void CreateDicomV1(ParsedDicomFile& dicom,
                            RestApiPostCall& call,
                            const Json::Value& request)
  {
    // curl http://localhost:8042/tools/create-dicom -X POST -d '{"PatientName":"Hello^World"}'
    // curl http://localhost:8042/tools/create-dicom -X POST -d '{"PatientName":"Hello^World","PixelData":"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAAAAAA6mKC9AAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH3gUGDDcB53FulQAAAElJREFUGNNtj0sSAEEEQ1+U+185s1CtmRkblQ9CZldsKHJDk6DLGLJa6chjh0ooQmpjXMM86zPwydGEj6Ed/UGykkEM8X+p3u8/8LcOJIWLGeMAAAAASUVORK5CYII="}'

    assert(request.isObject());
    LOG(WARNING) << "Using a deprecated call to /tools/create-dicom";

    Json::Value::Members members = request.getMemberNames();
    for (size_t i = 0; i < members.size(); i++)
    {
      const std::string& name = members[i];
      if (request[name].type() != Json::stringValue)
      {
        throw OrthancException(ErrorCode_CreateDicomNotString);
      }

      std::string value = request[name].asString();

      DicomTag tag = FromDcmtkBridge::ParseTag(name);
      if (tag == DICOM_TAG_PIXEL_DATA)
      {
        dicom.EmbedContent(value);
      }
      else
      {
        // This is V1, don't try and decode data URI scheme
        dicom.ReplacePlainString(tag, value);
      }
    }
  }


  static void InjectTags(ParsedDicomFile& dicom,
                         const Json::Value& tags,
                         bool decodeBinaryTags,
                         const std::string& privateCreator,
                         bool force)
  {
    if (tags.type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadRequest, "Tags field is not an array");
    }

    // Inject the user-specified tags
    Json::Value::Members members = tags.getMemberNames();
    for (size_t i = 0; i < members.size(); i++)
    {
      const std::string& name = members[i];
      DicomTag tag = FromDcmtkBridge::ParseTag(name);

      if (tag != DICOM_TAG_SPECIFIC_CHARACTER_SET)
      {
        if (!force &&
            tag != DICOM_TAG_PATIENT_ID &&
            tag != DICOM_TAG_ACQUISITION_DATE &&
            tag != DICOM_TAG_ACQUISITION_TIME &&
            tag != DICOM_TAG_CONTENT_DATE &&
            tag != DICOM_TAG_CONTENT_TIME &&
            tag != DICOM_TAG_INSTANCE_CREATION_DATE &&
            tag != DICOM_TAG_INSTANCE_CREATION_TIME &&
            tag != DICOM_TAG_SERIES_DATE &&
            tag != DICOM_TAG_SERIES_TIME &&
            tag != DICOM_TAG_STUDY_DATE &&
            tag != DICOM_TAG_STUDY_TIME &&
            dicom.HasTag(tag))
        {
          throw OrthancException(ErrorCode_CreateDicomOverrideTag, name);
        }

        if (tag == DICOM_TAG_PIXEL_DATA)
        {
          throw OrthancException(ErrorCode_CreateDicomUseContent);
        }
        else
        {
          dicom.Replace(tag, tags[name], decodeBinaryTags, DicomReplaceMode_InsertIfAbsent, privateCreator);
        }
      }
    }
  }


  static void CreateSeries(RestApiPostCall& call,
                           ParsedDicomFile& base /* in */,
                           const Json::Value& content,
                           bool decodeBinaryTags,
                           const std::string& privateCreator,
                           bool force)
  {
    assert(content.isArray());
    assert(content.size() > 0);
    ServerContext& context = OrthancRestApi::GetContext(call);

    base.ReplacePlainString(DICOM_TAG_IMAGES_IN_ACQUISITION, boost::lexical_cast<std::string>(content.size()));
    base.ReplacePlainString(DICOM_TAG_NUMBER_OF_TEMPORAL_POSITIONS, "1");

    std::string someInstance;

    try
    {
      for (Json::ArrayIndex i = 0; i < content.size(); i++)
      {
        std::unique_ptr<ParsedDicomFile> dicom(base.Clone(false));
        const Json::Value* payload = NULL;

        if (content[i].type() == Json::stringValue)
        {
          payload = &content[i];
        }
        else if (content[i].type() == Json::objectValue)
        {
          if (!content[i].isMember(CONTENT))
          {
            throw OrthancException(ErrorCode_CreateDicomNoPayload);
          }

          payload = &content[i][CONTENT];

          if (content[i].isMember(TAGS))
          {
            InjectTags(*dicom, content[i][TAGS], decodeBinaryTags, privateCreator, force);
          }
        }

        if (payload == NULL ||
            payload->type() != Json::stringValue)
        {
          throw OrthancException(ErrorCode_CreateDicomUseDataUriScheme);
        }

        dicom->EmbedContent(payload->asString());
        dicom->ReplacePlainString(DICOM_TAG_INSTANCE_NUMBER, boost::lexical_cast<std::string>(i + 1));
        dicom->ReplacePlainString(DICOM_TAG_IMAGE_INDEX, boost::lexical_cast<std::string>(i + 1));

        StoreCreatedInstance(someInstance, call, *dicom, false);
      }
    }
    catch (OrthancException&)
    {
      // Error: Remove the newly-created series
      
      std::string series;
      if (context.GetIndex().LookupParent(series, someInstance))
      {
        Json::Value dummy;
        context.GetIndex().DeleteResource(dummy, series, ResourceType_Series);
      }

      throw;
    }

    std::string series;
    if (context.GetIndex().LookupParent(series, someInstance))
    {
      OrthancRestApi::GetApi(call).AnswerStoredResource(call, series, ResourceType_Series, StoreStatus_Success);
    }
  }


  static void CreateDicomV2(RestApiPostCall& call,
                            const Json::Value& request)
  {
    static const char* const SPECIFIC_CHARACTER_SET_2 = "SpecificCharacterSet";
    static const char* const TYPE = "Type";
    static const char* const VALUE = "Value";
    
    assert(request.isObject());
    ServerContext& context = OrthancRestApi::GetContext(call);

    if (!request.isMember(TAGS) ||
        request[TAGS].type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_BadRequest);
    }

    ParsedDicomFile dicom(true);

    {
      Encoding encoding;

      if (request[TAGS].isMember(SPECIFIC_CHARACTER_SET_2))
      {
        const char* tmp = request[TAGS][SPECIFIC_CHARACTER_SET_2].asCString();
        if (!GetDicomEncoding(encoding, tmp))
        {
          throw OrthancException(ErrorCode_ParameterOutOfRange,
                                 "Unknown specific character set: " + std::string(tmp));
        }
      }
      else
      {
        encoding = GetDefaultDicomEncoding();
      }

      dicom.SetEncoding(encoding);
    }

    ResourceType parentType = ResourceType_Instance;

    if (request.isMember(PARENT))
    {
      // Locate the parent tags
      std::string parent = request[PARENT].asString();
      if (!context.GetIndex().LookupResourceType(parentType, parent))
      {
        throw OrthancException(ErrorCode_CreateDicomBadParent);
      }

      if (parentType == ResourceType_Instance)
      {
        throw OrthancException(ErrorCode_CreateDicomParentIsInstance);
      }

      // Select one existing child instance of the parent resource, to
      // retrieve all its tags
      Json::Value siblingTags;
      std::string siblingInstanceId;

      {
        // Retrieve all the instances of the parent resource
        std::list<std::string>  siblingInstances;
        context.GetIndex().GetChildInstances(siblingInstances, parent);

        if (siblingInstances.empty())
	{
	  // Error: No instance (should never happen)
          throw OrthancException(ErrorCode_InternalError);
        }

        siblingInstanceId = siblingInstances.front();
        context.ReadDicomAsJson(siblingTags, siblingInstanceId);
      }


      // Choose the same encoding as the parent resource
      {
        static const char* const SPECIFIC_CHARACTER_SET = "0008,0005";

        if (siblingTags.isMember(SPECIFIC_CHARACTER_SET))
        {
          Encoding encoding;

          if (!siblingTags[SPECIFIC_CHARACTER_SET].isMember(VALUE) ||
              siblingTags[SPECIFIC_CHARACTER_SET][VALUE].type() != Json::stringValue ||
              !GetDicomEncoding(encoding, siblingTags[SPECIFIC_CHARACTER_SET][VALUE].asCString()))
          {
            LOG(WARNING) << "Instance with an incorrect Specific Character Set, "
                         << "using the default Orthanc encoding: " << siblingInstanceId;
            encoding = GetDefaultDicomEncoding();
          }

          dicom.SetEncoding(encoding);
        }
      }


      // Retrieve the tags for all the parent modules
      typedef std::set<DicomTag> ModuleTags;
      ModuleTags moduleTags;

      ResourceType type = parentType;
      for (;;)
      {
        DicomTag::AddTagsForModule(moduleTags, GetModule(type));
      
        if (type == ResourceType_Patient)
        {
          break;   // We're done
        }

        // Go up
        std::string tmp;
        if (!context.GetIndex().LookupParent(tmp, parent))
        {
          throw OrthancException(ErrorCode_InternalError);
        }

        parent = tmp;
        type = GetParentResourceType(type);
      }

      for (ModuleTags::const_iterator it = moduleTags.begin();
           it != moduleTags.end(); ++it)
      {
        std::string t = it->Format();
        if (siblingTags.isMember(t))
        {
          const Json::Value& tag = siblingTags[t];
          if (tag[TYPE] == "Null")
          {
            dicom.ReplacePlainString(*it, "");
          }
          else if (tag[TYPE] == "String")
          {
            std::string value = tag[VALUE].asString();  // This is an UTF-8 value (as it comes from JSON)
            dicom.ReplacePlainString(*it, value);
          }
        }
      }
    }


    bool decodeBinaryTags = true;
    if (request.isMember(INTERPRET_BINARY_TAGS))
    {
      const Json::Value& v = request[INTERPRET_BINARY_TAGS];
      if (v.type() != Json::booleanValue)
      {
        throw OrthancException(ErrorCode_BadRequest);
      }

      decodeBinaryTags = v.asBool();
    }


    // New argument in Orthanc 1.6.0
    std::string privateCreator;
    if (request.isMember(PRIVATE_CREATOR))
    {
      const Json::Value& v = request[PRIVATE_CREATOR];
      if (v.type() != Json::stringValue)
      {
        throw OrthancException(ErrorCode_BadRequest);
      }

      privateCreator = v.asString();
    }
    else
    {
      OrthancConfiguration::ReaderLock lock;
      privateCreator = lock.GetConfiguration().GetDefaultPrivateCreator();
    }


    // New in Orthanc 1.9.0
    bool force = false;
    if (request.isMember(FORCE))
    {
      const Json::Value& v = request[FORCE];
      if (v.type() != Json::booleanValue)
      {
        throw OrthancException(ErrorCode_BadRequest);
      }

      force = v.asBool();
    }


    // Inject time-related information
    std::string date, time;
    SystemToolbox::GetNowDicom(date, time, true /* use UTC time (not local time) */);
    dicom.ReplacePlainString(DICOM_TAG_ACQUISITION_DATE, date);
    dicom.ReplacePlainString(DICOM_TAG_ACQUISITION_TIME, time);
    dicom.ReplacePlainString(DICOM_TAG_CONTENT_DATE, date);
    dicom.ReplacePlainString(DICOM_TAG_CONTENT_TIME, time);
    dicom.ReplacePlainString(DICOM_TAG_INSTANCE_CREATION_DATE, date);
    dicom.ReplacePlainString(DICOM_TAG_INSTANCE_CREATION_TIME, time);

    if (parentType == ResourceType_Patient ||
        parentType == ResourceType_Study ||
        parentType == ResourceType_Instance /* no parent */)
    {
      dicom.ReplacePlainString(DICOM_TAG_SERIES_DATE, date);
      dicom.ReplacePlainString(DICOM_TAG_SERIES_TIME, time);
    }

    if (parentType == ResourceType_Patient ||
        parentType == ResourceType_Instance /* no parent */)
    {
      dicom.ReplacePlainString(DICOM_TAG_STUDY_DATE, date);
      dicom.ReplacePlainString(DICOM_TAG_STUDY_TIME, time);
    }


    InjectTags(dicom, request[TAGS], decodeBinaryTags, privateCreator, force);


    // Inject the content (either an image, or a PDF file)
    if (request.isMember(CONTENT))
    {
      const Json::Value& content = request[CONTENT];

      if (content.type() == Json::stringValue)
      {
        dicom.EmbedContent(request[CONTENT].asString());

      }
      else if (content.type() == Json::arrayValue)
      {
        if (content.size() > 0)
        {
          // Let's create a series instead of a single instance
          CreateSeries(call, dicom, content, decodeBinaryTags, privateCreator, force);
          return;
        }
      }
      else
      {
        throw OrthancException(ErrorCode_CreateDicomUseDataUriScheme);
      }
    }

    std::string id;
    StoreCreatedInstance(id, call, dicom, true);
  }


  static void CreateDicom(RestApiPostCall& call)
  {
    if (call.IsDocumentation())
    {
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("Create one DICOM instance")
        .SetDescription("Create one DICOM instance, and store it into Orthanc")
        .SetRequestField(TAGS, RestApiCallDocumentation::Type_JsonObject,
                         "Associative array containing the tags of the new instance to be created", true)
        .SetRequestField(CONTENT, RestApiCallDocumentation::Type_String,
                         "This field can be used to embed an image (pixel data) or a PDF inside the created DICOM instance. "
                         "The PNG image, the JPEG image or the PDF file must be provided using their "
                         "[data URI scheme encoding](https://en.wikipedia.org/wiki/Data_URI_scheme). "
                         "This field can possibly contain a JSON array, in which case a DICOM series is created "
                         "containing one DICOM instance for each item in the `Content` field.", false)
        .SetRequestField(PARENT, RestApiCallDocumentation::Type_String,
                         "If present, the newly created instance will be attached to the parent DICOM resource "
                         "whose Orthanc identifier is contained in this field. The DICOM tags of the parent "
                         "modules in the DICOM hierarchy will be automatically copied to the newly created instance.", false)
        .SetRequestField(INTERPRET_BINARY_TAGS, RestApiCallDocumentation::Type_Boolean,
                         "If some value in the `Tags` associative array is formatted according to some "
                         "[data URI scheme encoding](https://en.wikipedia.org/wiki/Data_URI_scheme), "
                         "whether this value is decoded to a binary value or kept as such (`true` by default)", false)
        .SetRequestField(PRIVATE_CREATOR, RestApiCallDocumentation::Type_String,
                         "The private creator to be used for private tags in `Tags`", false)
        .SetRequestField(FORCE, RestApiCallDocumentation::Type_Boolean,
                         "Avoid the consistency checks for the DICOM tags that enforce the DICOM model of the real-world. "
                         "You can notably use this flag if you need to manually set the tags `StudyInstanceUID`, "
                         "`SeriesInstanceUID`, or `SOPInstanceUID`. Be careful with this feature.", false)
        .SetAnswerField("ID", RestApiCallDocumentation::Type_String, "Orthanc identifier of the newly created instance")
        .SetAnswerField("Path", RestApiCallDocumentation::Type_String, "Path to access the instance in the REST API");
      return;
    }

    Json::Value request;
    if (!call.ParseJsonRequest(request) ||
        !request.isObject())
    {
      throw OrthancException(ErrorCode_BadRequest);
    }

    if (request.isMember(TAGS))
    {
      CreateDicomV2(call, request);
    }
    else
    {
      // Compatibility with Orthanc <= 0.9.3
      ParsedDicomFile dicom(true);
      CreateDicomV1(dicom, call, request);

      std::string id;
      StoreCreatedInstance(id, call, dicom, true);
    }
  }


  static void SplitStudy(RestApiPostCall& call)
  {
    if (call.IsDocumentation())
    {
      OrthancRestApi::DocumentSubmitCommandsJob(call);
      call.GetDocumentation()
        .SetTag("Studies")
        .SetSummary("Split study")
        .SetDescription("Start a new job so as to split the DICOM study whose Orthanc identifier is provided in the URL, "
                        "by taking some of its children series or instances out of it and putting them into a brand new study "
                        "(this new study is created by setting the `StudyInstanceUID` tag to a random identifier): "
                        "https://book.orthanc-server.com/users/anonymization.html#splitting")
        .SetUriArgument("id", "Orthanc identifier of the study of interest")
        .SetRequestField(SERIES, RestApiCallDocumentation::Type_JsonListOfStrings,
                         "The list of series to be separated from the parent study. "
                         "These series must all be children of the same source study, that is specified in the URI.", false)
        .SetRequestField(REPLACE, RestApiCallDocumentation::Type_JsonObject,
                         "Associative array to change the value of some DICOM tags in the new study. "
                         "These tags must be part of the \"Patient Module Attributes\" or the \"General Study "
                         "Module Attributes\", as specified by the DICOM 2011 standard in Tables C.7-1 and C.7-3.", false)
        .SetRequestField(REMOVE, RestApiCallDocumentation::Type_JsonListOfStrings,
                         "List of tags that must be removed in the new study (from the same modules as in the `Replace` option)", false)
        .SetRequestField(KEEP_SOURCE, RestApiCallDocumentation::Type_Boolean,
                         "If set to `true`, instructs Orthanc to keep a copy of the original series/instances in the source study. "
                         "By default, the original series/instances are deleted from Orthanc.", false)
        .SetRequestField(INSTANCES, RestApiCallDocumentation::Type_JsonListOfStrings,
                         "The list of instances to be separated from the parent study. "
                         "These instances must all be children of the same source study, that is specified in the URI.", false);
      return;
    }
    
    ServerContext& context = OrthancRestApi::GetContext(call);

    Json::Value request;
    if (!call.ParseJsonRequest(request))
    {
      // Bad JSON request
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    const std::string study = call.GetUriComponent("id", "");

    std::unique_ptr<SplitStudyJob> job(new SplitStudyJob(context, study));    
    job->SetOrigin(call);

    bool ok = false;
    if (request.isMember(SERIES))
    {
      std::vector<std::string> series;
      SerializationToolbox::ReadArrayOfStrings(series, request, SERIES);

      for (size_t i = 0; i < series.size(); i++)
      {
        job->AddSourceSeries(series[i]);
        ok = true;
      }
    }

    if (request.isMember(INSTANCES))
    {
      std::vector<std::string> instances;
      SerializationToolbox::ReadArrayOfStrings(instances, request, INSTANCES);

      for (size_t i = 0; i < instances.size(); i++)
      {
        job->AddSourceInstance(instances[i]);
        ok = true;
      }
    }

    if (!ok)
    {
      throw OrthancException(ErrorCode_BadRequest, "Both the \"Series\" and the \"Instances\" fields are missing");
    }    
    
    job->AddTrailingStep();

    SetKeepSource(*job, request);

    if (request.isMember(REMOVE))
    {
      if (request[REMOVE].type() != Json::arrayValue)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }

      for (Json::Value::ArrayIndex i = 0; i < request[REMOVE].size(); i++)
      {
        if (request[REMOVE][i].type() != Json::stringValue)
        {
          throw OrthancException(ErrorCode_BadFileFormat);
        }
        else
        {
          job->Remove(FromDcmtkBridge::ParseTag(request[REMOVE][i].asCString()));
        }
      }
    }

    if (request.isMember(REPLACE))
    {
      if (request[REPLACE].type() != Json::objectValue)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }

      Json::Value::Members tags = request[REPLACE].getMemberNames();

      for (size_t i = 0; i < tags.size(); i++)
      {
        const Json::Value& value = request[REPLACE][tags[i]];
        
        if (value.type() != Json::stringValue)
        {
          throw OrthancException(ErrorCode_BadFileFormat);
        }
        else
        {
          job->Replace(FromDcmtkBridge::ParseTag(tags[i]), value.asString());
        }
      }
    }

    OrthancRestApi::GetApi(call).SubmitCommandsJob
      (call, job.release(), true /* synchronous by default */, request);
  }


  static void MergeStudy(RestApiPostCall& call)
  {
    if (call.IsDocumentation())
    {
      OrthancRestApi::DocumentSubmitCommandsJob(call);
      call.GetDocumentation()
        .SetTag("Studies")
        .SetSummary("Merge study")
        .SetDescription("Start a new job so as to move some DICOM resources into the DICOM study whose Orthanc identifier "
                        "is provided in the URL: https://book.orthanc-server.com/users/anonymization.html#merging")
        .SetUriArgument("id", "Orthanc identifier of the study of interest")
        .SetRequestField(RESOURCES, RestApiCallDocumentation::Type_JsonListOfStrings,
                         "The list of DICOM resources (studies, series, and/or instances) to be merged "
                         "into the study of interest (mandatory option)", true)
        .SetRequestField(KEEP_SOURCE, RestApiCallDocumentation::Type_Boolean,
                         "If set to `true`, instructs Orthanc to keep a copy of the original resources in their source study. "
                         "By default, the original resources are deleted from Orthanc.", false);
      return;
    }

    ServerContext& context = OrthancRestApi::GetContext(call);

    Json::Value request;
    if (!call.ParseJsonRequest(request))
    {
      // Bad JSON request
      throw OrthancException(ErrorCode_BadFileFormat);
    }

    const std::string study = call.GetUriComponent("id", "");

    std::unique_ptr<MergeStudyJob> job(new MergeStudyJob(context, study));    
    job->SetOrigin(call);

    std::vector<std::string> resources;
    SerializationToolbox::ReadArrayOfStrings(resources, request, RESOURCES);

    for (size_t i = 0; i < resources.size(); i++)
    {
      job->AddSource(resources[i]);
    }

    job->AddTrailingStep();

    SetKeepSource(*job, request);

    OrthancRestApi::GetApi(call).SubmitCommandsJob
      (call, job.release(), true /* synchronous by default */, request);
  }
  

  void OrthancRestApi::RegisterAnonymizeModify()
  {
    Register("/instances/{id}/modify", ModifyInstance);
    Register("/series/{id}/modify", ModifyResource<ResourceType_Series>);
    Register("/studies/{id}/modify", ModifyResource<ResourceType_Study>);
    Register("/patients/{id}/modify", ModifyResource<ResourceType_Patient>);
    Register("/tools/bulk-modify", BulkModify);

    Register("/instances/{id}/anonymize", AnonymizeInstance);
    Register("/series/{id}/anonymize", AnonymizeResource<ResourceType_Series>);
    Register("/studies/{id}/anonymize", AnonymizeResource<ResourceType_Study>);
    Register("/patients/{id}/anonymize", AnonymizeResource<ResourceType_Patient>);
    Register("/tools/bulk-anonymize", BulkAnonymize);

    Register("/tools/create-dicom", CreateDicom);

    Register("/studies/{id}/split", SplitStudy);
    Register("/studies/{id}/merge", MergeStudy);
  }
}
