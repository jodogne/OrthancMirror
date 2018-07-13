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
#include "../ServerJobs/ArchiveJob.h"

namespace Orthanc
{
  static bool AddResourcesOfInterest(ArchiveJob& job,
                                     RestApiPostCall& call)
  {
    Json::Value resources;
    if (call.ParseJsonRequest(resources) &&
        resources.type() == Json::arrayValue)
    {
      for (Json::Value::ArrayIndex i = 0; i < resources.size(); i++)
      {
        if (resources[i].type() != Json::stringValue)
        {
          return false;   // Bad request
        }

        job.AddResource(resources[i].asString());
      }

      return true;
    }
    else
    {
      return false;
    }
  }


  static void SubmitJob(RestApiCall& call,
                        boost::shared_ptr<TemporaryFile>& tmp,
                        ServerContext& context,
                        std::auto_ptr<ArchiveJob>& job,
                        const std::string& filename)
  {
    if (job.get() == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }

    job->SetDescription("REST API");

    if (context.GetJobsEngine().GetRegistry().SubmitAndWait(job.release(), 0 /* TODO priority */))
    {
      // The archive is now created: Prepare the sending of the ZIP file
      FilesystemHttpSender sender(tmp->GetPath());
      sender.SetContentType("application/zip");
      sender.SetContentFilename(filename);

      // Send the ZIP
      call.GetOutput().AnswerStream(sender);
    }
    else
    {
      call.GetOutput().SignalError(HttpStatus_500_InternalServerError);
    }      
  }

  
  static void CreateBatchArchive(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    boost::shared_ptr<TemporaryFile> tmp(new TemporaryFile);
    std::auto_ptr<ArchiveJob> job(new ArchiveJob(tmp, context, false, false));

    if (AddResourcesOfInterest(*job, call))
    {
      SubmitJob(call, tmp, context, job, "Archive.zip");
    }
  }  

  
  template <bool Extended>
  static void CreateBatchMedia(RestApiPostCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    boost::shared_ptr<TemporaryFile> tmp(new TemporaryFile);
    std::auto_ptr<ArchiveJob> job(new ArchiveJob(tmp, context, true, Extended));

    if (AddResourcesOfInterest(*job, call))
    {
      SubmitJob(call, tmp, context, job, "Archive.zip");
    }
  }
  

  static void CreateArchive(RestApiGetCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string id = call.GetUriComponent("id", "");

    boost::shared_ptr<TemporaryFile> tmp(new TemporaryFile);
    std::auto_ptr<ArchiveJob> job(new ArchiveJob(tmp, context, false, false));
    job->AddResource(id);

    SubmitJob(call, tmp, context, job, id + ".zip");
  }


  static void CreateMedia(RestApiGetCall& call)
  {
    ServerContext& context = OrthancRestApi::GetContext(call);

    std::string id = call.GetUriComponent("id", "");

    boost::shared_ptr<TemporaryFile> tmp(new TemporaryFile);
    std::auto_ptr<ArchiveJob> job(new ArchiveJob(tmp, context, true, call.HasArgument("extended")));
    job->AddResource(id);

    SubmitJob(call, tmp, context, job, id + ".zip");
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
