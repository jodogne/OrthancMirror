/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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

#include "../../../OrthancFramework/Sources/HttpServer/FilesystemHttpSender.h"
#include "../../../OrthancFramework/Sources/OrthancException.h"
#include "../../../OrthancFramework/Sources/SerializationToolbox.h"
#include "../OrthancConfiguration.h"
#include "../ServerContext.h"
#include "../ServerJobs/ArchiveJob.h"


namespace Orthanc
{
  static const char* const KEY_RESOURCES = "Resources";
  static const char* const KEY_EXTENDED = "Extended";
  static const char* const KEY_TRANSCODE = "Transcode";
  
  static void AddResourcesOfInterestFromArray(ArchiveJob& job,
                                              const Json::Value& resources)
  {
    if (resources.type() != Json::arrayValue)
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "Expected a list of strings (Orthanc identifiers)");
    }
    
    for (Json::Value::ArrayIndex i = 0; i < resources.size(); i++)
    {
      if (resources[i].type() != Json::stringValue)
      {
        throw OrthancException(ErrorCode_BadFileFormat,
                               "Expected a list of strings (Orthanc identifiers)");
      }
      else
      {
        job.AddResource(resources[i].asString());
      }
    }
  }

  
  static void AddResourcesOfInterest(ArchiveJob& job         /* inout */,
                                     const Json::Value& body /* in */)
  {
    if (body.type() == Json::arrayValue)
    {
      AddResourcesOfInterestFromArray(job, body);
    }
    else if (body.type() == Json::objectValue)
    {
      if (body.isMember(KEY_RESOURCES))
      {
        AddResourcesOfInterestFromArray(job, body[KEY_RESOURCES]);
      }
      else
      {
        throw OrthancException(ErrorCode_BadFileFormat,
                               "Missing field " + std::string(KEY_RESOURCES) +
                               " in the JSON body");
      }
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
  }


  static DicomTransferSyntax GetTransferSyntax(const std::string& value)
  {
    DicomTransferSyntax syntax;
    if (LookupTransferSyntax(syntax, value))
    {
      return syntax;
    }
    else
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange,
                             "Unknown transfer syntax: " + value);
    }
  }
  
  
  static void GetJobParameters(bool& synchronous,            /* out */
                               bool& extended,               /* out */
                               bool& transcode,              /* out */
                               DicomTransferSyntax& syntax,  /* out */
                               int& priority,                /* out */
                               const Json::Value& body,      /* in */
                               const bool defaultExtended    /* in */)
  {
    synchronous = OrthancRestApi::IsSynchronousJobRequest
      (true /* synchronous by default */, body);

    priority = OrthancRestApi::GetJobRequestPriority(body);

    if (body.type() == Json::objectValue &&
        body.isMember(KEY_EXTENDED))
    {
      extended = SerializationToolbox::ReadBoolean(body, KEY_EXTENDED);
    }
    else
    {
      extended = defaultExtended;
    }

    if (body.type() == Json::objectValue &&
        body.isMember(KEY_TRANSCODE))
    {
      transcode = true;
      syntax = GetTransferSyntax(SerializationToolbox::ReadString(body, KEY_TRANSCODE));
    }
    else
    {
      transcode = false;
    }
  }


  static void SubmitJob(RestApiOutput& output,
                        ServerContext& context,
                        std::unique_ptr<ArchiveJob>& job,
                        int priority,
                        bool synchronous,
                        const std::string& filename)
  {
    if (job.get() == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }

    job->SetDescription("REST API");

    if (synchronous)
    {
      boost::shared_ptr<TemporaryFile> tmp;

      {
        OrthancConfiguration::ReaderLock lock;
        tmp.reset(lock.GetConfiguration().CreateTemporaryFile());
      }

      job->SetSynchronousTarget(tmp);
    
      Json::Value publicContent;
      context.GetJobsEngine().GetRegistry().SubmitAndWait
        (publicContent, job.release(), priority);
      
      {
        // The archive is now created: Prepare the sending of the ZIP file
        FilesystemHttpSender sender(tmp->GetPath(), MimeType_Zip);
        sender.SetContentFilename(filename);

        // Send the ZIP
        output.AnswerStream(sender);
      }
    }
    else
    {
      OrthancRestApi::SubmitGenericJob(output, context, job.release(), false, priority);
    }
  }


  static void DocumentPostArguments(RestApiPostCall& call,
                                    bool isMedia,
                                    bool defaultExtended)
  {
    call.GetDocumentation()
      .SetRequestField("Synchronous", RestApiCallDocumentation::Type_Boolean,
                     "If `true`, create the archive in synchronous mode, which means that the HTTP answer will directly "
                     "contain the ZIP file. This is the default, easy behavior, but it is *not* be desirable to archive "
                     "large amount of data, as it might lead to network timeouts.", false)
      .SetRequestField("Asynchronous", RestApiCallDocumentation::Type_Boolean,
                       "If `true`, create the archive in asynchronous mode, which means that a job is submitted to create "
                       "the archive in background. Prefer this flavor wherever possible.", false)
      .SetRequestField(KEY_TRANSCODE, RestApiCallDocumentation::Type_String,
                       "If present, the DICOM files in the archive will be transcoded to the provided "
                       "transfer syntax: https://book.orthanc-server.com/faq/transcoding.html", false)
      .SetRequestField("Priority", RestApiCallDocumentation::Type_Number,
                       "In asynchronous mode, the priority of the job. The lower the value, the higher the priority.", false)
      .AddAnswerType(MimeType_Zip, "In synchronous mode, the ZIP file containing the archive")
      .AddAnswerType(MimeType_Json, "In asynchronous mode, information about the job that has been submitted to "
                     "generate the archive: https://book.orthanc-server.com/users/advanced-rest.html#jobs")
      .SetAnswerField("ID", RestApiCallDocumentation::Type_String, "Identifier of the job")
      .SetAnswerField("Path", RestApiCallDocumentation::Type_String, "Path to access the job in the REST API");

    if (isMedia)
    {
      call.GetDocumentation().SetRequestField(
        KEY_EXTENDED, RestApiCallDocumentation::Type_Boolean, "If `true`, will include additional "
        "tags such as `SeriesDescription`, leading to a so-called *extended DICOMDIR*. Default value is " +
        std::string(defaultExtended ? "`true`" : "`false`") + ".", false);
    }
  }

  
  template <bool IS_MEDIA,
            bool DEFAULT_IS_EXTENDED  /* only makes sense for media (i.e. not ZIP archives) */ >
  static void CreateBatch(RestApiPostCall& call)
  {
    if (call.IsDocumentation())
    {
      DocumentPostArguments(call, IS_MEDIA, DEFAULT_IS_EXTENDED);
      std::string m = (IS_MEDIA ? "DICOMDIR media" : "ZIP archive");
      call.GetDocumentation()
        .SetTag("System")
        .SetSummary("Create " + m)
        .SetDescription("Create a " + m + " containing the DICOM resources (patients, studies, series, or instances) "
                        "whose Orthanc identifiers are provided in the body")
        .SetRequestField("Resources", RestApiCallDocumentation::Type_JsonListOfStrings,
                         "The list of Orthanc identifiers of interest.", false);
      return;
    }

    ServerContext& context = OrthancRestApi::GetContext(call);

    Json::Value body;
    if (call.ParseJsonRequest(body))
    {
      bool synchronous, extended, transcode;
      DicomTransferSyntax transferSyntax;
      int priority;
      GetJobParameters(synchronous, extended, transcode, transferSyntax,
                       priority, body, DEFAULT_IS_EXTENDED);
      
      std::unique_ptr<ArchiveJob> job(new ArchiveJob(context, IS_MEDIA, extended));
      AddResourcesOfInterest(*job, body);

      if (transcode)
      {
        job->SetTranscode(transferSyntax);
      }
      
      SubmitJob(call.GetOutput(), context, job, priority, synchronous, "Archive.zip");
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "Expected a list of resources to archive in the body");
    }
  }
  

  template <bool IS_MEDIA>
  static void CreateSingleGet(RestApiGetCall& call)
  {
    if (call.IsDocumentation())
    {
      ResourceType t = StringToResourceType(call.GetFullUri()[0].c_str());
      std::string r = GetResourceTypeText(t, false /* plural */, false /* upper case */);
      std::string m = (IS_MEDIA ? "DICOMDIR media" : "ZIP archive");
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(t, true /* plural */, true /* upper case */))
        .SetSummary("Create " + m)
        .SetDescription("Synchronously create a " + m + " containing the DICOM " + r +
                        " whose Orthanc identifier is provided in the URL. This flavor is synchronous, "
                        "which might *not* be desirable to archive large amount of data, as it might "
                        "lead to network timeouts. Prefer the asynchronous version using `POST` method.")
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest")
        .SetHttpGetArgument("transcode", RestApiCallDocumentation::Type_String,
                            "If present, the DICOM files in the archive will be transcoded to the provided "
                            "transfer syntax: https://book.orthanc-server.com/faq/transcoding.html", false)
        .AddAnswerType(MimeType_Zip, "ZIP file containing the archive");
      if (IS_MEDIA)
      {
        call.GetDocumentation().SetHttpGetArgument(
          "extended", RestApiCallDocumentation::Type_String,
          "If present, will include additional tags such as `SeriesDescription`, leading to a so-called *extended DICOMDIR*", false);
      }
      return;
    }

    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string id = call.GetUriComponent("id", "");

    bool extended;
    if (IS_MEDIA)
    {
      extended = call.HasArgument("extended");
    }
    else
    {
      extended = false;
    }

    std::unique_ptr<ArchiveJob> job(new ArchiveJob(context, IS_MEDIA, extended));
    job->AddResource(id);

    static const char* const TRANSCODE = "transcode";
    if (call.HasArgument(TRANSCODE))
    {
      job->SetTranscode(GetTransferSyntax(call.GetArgument(TRANSCODE, "")));
    }

    SubmitJob(call.GetOutput(), context, job, 0 /* priority */,
              true /* synchronous */, id + ".zip");
  }


  template <bool IS_MEDIA>
  static void CreateSinglePost(RestApiPostCall& call)
  {
    if (call.IsDocumentation())
    {
      DocumentPostArguments(call, IS_MEDIA, false /* not extended by default */);
      ResourceType t = StringToResourceType(call.GetFullUri()[0].c_str());
      std::string r = GetResourceTypeText(t, false /* plural */, false /* upper case */);
      std::string m = (IS_MEDIA ? "DICOMDIR media" : "ZIP archive");
      call.GetDocumentation()
        .SetTag(GetResourceTypeText(t, true /* plural */, true /* upper case */))
        .SetSummary("Create " + m)
        .SetDescription("Create a " + m + " containing the DICOM " + r +
                        " whose Orthanc identifier is provided in the URL")
        .SetUriArgument("id", "Orthanc identifier of the " + r + " of interest");
      return;
    }

    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string id = call.GetUriComponent("id", "");

    Json::Value body;
    if (call.ParseJsonRequest(body))
    {
      bool synchronous, extended, transcode;
      DicomTransferSyntax transferSyntax;
      int priority;
      GetJobParameters(synchronous, extended, transcode, transferSyntax,
                       priority, body, false /* by default, not extented */);
      
      std::unique_ptr<ArchiveJob> job(new ArchiveJob(context, IS_MEDIA, extended));
      job->AddResource(id);

      if (transcode)
      {
        job->SetTranscode(transferSyntax);
      }

      SubmitJob(call.GetOutput(), context, job, priority, synchronous, id + ".zip");
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
  }

    
  void OrthancRestApi::RegisterArchive()
  {
    Register("/patients/{id}/archive", CreateSingleGet<false /* ZIP */>);
    Register("/patients/{id}/archive", CreateSinglePost<false /* ZIP */>);
    Register("/patients/{id}/media",   CreateSingleGet<true /* media */>);
    Register("/patients/{id}/media",   CreateSinglePost<true /* media */>);
    Register("/series/{id}/archive",   CreateSingleGet<false /* ZIP */>);
    Register("/series/{id}/archive",   CreateSinglePost<false /* ZIP */>);
    Register("/series/{id}/media",     CreateSingleGet<true /* media */>);
    Register("/series/{id}/media",     CreateSinglePost<true /* media */>);
    Register("/studies/{id}/archive",  CreateSingleGet<false /* ZIP */>);
    Register("/studies/{id}/archive",  CreateSinglePost<false /* ZIP */>);
    Register("/studies/{id}/media",    CreateSingleGet<true /* media */>);
    Register("/studies/{id}/media",    CreateSinglePost<true /* media */>);

    Register("/tools/create-archive",
             CreateBatch<false /* ZIP */,  false /* extended makes no sense in ZIP */>);
    Register("/tools/create-media",
             CreateBatch<true /* media */, false /* not extended by default */>);
    Register("/tools/create-media-extended",
             CreateBatch<true /* media */, true /* extended by default */>);
  }
}
