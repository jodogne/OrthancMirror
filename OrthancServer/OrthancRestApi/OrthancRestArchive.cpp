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

#include "../../Core/HttpServer/FilesystemHttpSender.h"
#include "../../Core/OrthancException.h"
#include "../../Core/SerializationToolbox.h"
#include "../OrthancConfiguration.h"
#include "../ServerContext.h"
#include "../ServerJobs/ArchiveJob.h"


namespace Orthanc
{
  static const char* const KEY_RESOURCES = "Resources";
  static const char* const KEY_EXTENDED = "Extended";
  
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


  static void GetJobParameters(bool& synchronous,         /* out */
                               bool& extended,            /* out */
                               int& priority,             /* out */
                               const Json::Value& body,   /* in */
                               const bool defaultExtended /* in */)
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

  
  template <bool IS_MEDIA,
            bool DEFAULT_IS_EXTENDED  /* only makes sense for media (i.e. not ZIP archives) */ >
  static void CreateBatch(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    Json::Value body;
    if (call.ParseJsonRequest(body))
    {
      bool synchronous, extended;
      int priority;
      GetJobParameters(synchronous, extended, priority, body, DEFAULT_IS_EXTENDED);
      
      std::unique_ptr<ArchiveJob> job(new ArchiveJob(context, IS_MEDIA, extended));
      AddResourcesOfInterest(*job, body);
      SubmitJob(call.GetOutput(), context, job, priority, synchronous, "Archive.zip");
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "Expected a list of resources to archive in the body");
    }
  }
  

  template <bool IS_MEDIA,
            bool DEFAULT_IS_EXTENDED  /* only makes sense for media (i.e. not ZIP archives) */ >
  static void CreateSingleGet(RestApiGetCall& call)
  {
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

    SubmitJob(call.GetOutput(), context, job, 0 /* priority */,
              true /* synchronous */, id + ".zip");
  }


  template <bool IS_MEDIA,
            bool DEFAULT_IS_EXTENDED  /* only makes sense for media (i.e. not ZIP archives) */ >
  static void CreateSinglePost(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string id = call.GetUriComponent("id", "");

    Json::Value body;
    if (call.ParseJsonRequest(body))
    {
      bool synchronous, extended;
      int priority;
      GetJobParameters(synchronous, extended, priority, body, DEFAULT_IS_EXTENDED);
      
      std::unique_ptr<ArchiveJob> job(new ArchiveJob(context, IS_MEDIA, extended));
      job->AddResource(id);
      SubmitJob(call.GetOutput(), context, job, priority, synchronous, id + ".zip");
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat);
    }
  }

    
  void OrthancRestApi::RegisterArchive()
  {
    Register("/patients/{id}/archive",
             CreateSingleGet<false /* ZIP */, false /* extended makes no sense in ZIP */>);
    Register("/studies/{id}/archive",
             CreateSingleGet<false /* ZIP */, false /* extended makes no sense in ZIP */>);
    Register("/series/{id}/archive",
             CreateSingleGet<false /* ZIP */, false /* extended makes no sense in ZIP */>);

    Register("/patients/{id}/archive",
             CreateSinglePost<false /* ZIP */, false /* extended makes no sense in ZIP */>);
    Register("/studies/{id}/archive",
             CreateSinglePost<false /* ZIP */, false /* extended makes no sense in ZIP */>);
    Register("/series/{id}/archive",
             CreateSinglePost<false /* ZIP */, false /* extended makes no sense in ZIP */>);

    Register("/patients/{id}/media",
             CreateSingleGet<true /* media */, false /* not extended by default */>);
    Register("/studies/{id}/media",
             CreateSingleGet<true /* media */, false /* not extended by default */>);
    Register("/series/{id}/media",
             CreateSingleGet<true /* media */, false /* not extended by default */>);

    Register("/patients/{id}/media",
             CreateSinglePost<true /* media */, false /* not extended by default */>);
    Register("/studies/{id}/media",
             CreateSinglePost<true /* media */, false /* not extended by default */>);
    Register("/series/{id}/media",
             CreateSinglePost<true /* media */, false /* not extended by default */>);

    Register("/tools/create-archive",
             CreateBatch<false /* ZIP */,  false /* extended makes no sense in ZIP */>);
    Register("/tools/create-media",
             CreateBatch<true /* media */, false /* not extended by default */>);
    Register("/tools/create-media-extended",
             CreateBatch<true /* media */, true /* extended by default */>);
  }
}
