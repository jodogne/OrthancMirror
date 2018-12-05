/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
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
#include "../../Core/SerializationToolbox.h"
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
                               const Json::Value& body,   /* in */
                               const bool defaultExtended /* in */)
  {
    synchronous = OrthancRestApi::IsSynchronousJobRequest
      (true /* synchronous by default */, body);

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
                        std::auto_ptr<ArchiveJob>& job,
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
      boost::shared_ptr<TemporaryFile> tmp(new TemporaryFile);
      job->SetSynchronousTarget(tmp);
    
      Json::Value publicContent;
      if (context.GetJobsEngine().GetRegistry().SubmitAndWait
          (publicContent, job.release(), 0 /* TODO priority */))
      {
        // The archive is now created: Prepare the sending of the ZIP file
        FilesystemHttpSender sender(tmp->GetPath());
        sender.SetContentType(MimeType_Gzip);
        sender.SetContentFilename(filename);

        // Send the ZIP
        output.AnswerStream(sender);
      }
      else
      {
        output.SignalError(HttpStatus_500_InternalServerError);
      }
    }
    else
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }
  }

  
  static void CreateBatchArchive(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    Json::Value body;
    if (call.ParseJsonRequest(body))
    {
      bool synchronous, extended;
      GetJobParameters(synchronous, extended, body, false /* by default, not extended */);
      
      std::auto_ptr<ArchiveJob> job(new ArchiveJob(context, false, extended));
      AddResourcesOfInterest(*job, body);
      SubmitJob(call.GetOutput(), context, job, synchronous, "Archive.zip");
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "Expected a list of resources to archive in the body");
    }
  }  

  
  template <bool DEFAULT_EXTENDED>
  static void CreateBatchMedia(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    Json::Value body;
    if (call.ParseJsonRequest(body))
    {
      bool synchronous, extended;
      GetJobParameters(synchronous, extended, body, DEFAULT_EXTENDED);
      
      std::auto_ptr<ArchiveJob> job(new ArchiveJob(context, true, extended));
      AddResourcesOfInterest(*job, body);
      SubmitJob(call.GetOutput(), context, job, synchronous, "Archive.zip");
    }
    else
    {
      throw OrthancException(ErrorCode_BadFileFormat,
                             "Expected a list of resources to archive in the body");
    }
  }
  

  static void CreateArchive(RestApiGetCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string id = call.GetUriComponent("id", "");

    std::auto_ptr<ArchiveJob> job(new ArchiveJob(context, false, false));
    job->AddResource(id);

    SubmitJob(call.GetOutput(), context, job, true /* synchronous */, id + ".zip");
  }


  static void CreateMedia(RestApiGetCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string id = call.GetUriComponent("id", "");

    std::auto_ptr<ArchiveJob> job(new ArchiveJob(context, true, call.HasArgument("extended")));
    job->AddResource(id);

    SubmitJob(call.GetOutput(), context, job, true /* synchronous */, id + ".zip");
  }


  void OrthancRestApi::RegisterArchive()
  {
    Register("/patients/{id}/archive", CreateArchive);
    Register("/studies/{id}/archive", CreateArchive);
    Register("/series/{id}/archive", CreateArchive);

    Register("/patients/{id}/media", CreateMedia);
    Register("/studies/{id}/media", CreateMedia);
    Register("/series/{id}/media", CreateMedia);

    Register("/tools/create-archive", CreateBatchArchive);
    Register("/tools/create-media", CreateBatchMedia<false>);
    Register("/tools/create-media-extended", CreateBatchMedia<true>);
  }
}
