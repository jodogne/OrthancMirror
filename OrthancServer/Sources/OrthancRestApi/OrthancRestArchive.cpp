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

#include "../../../OrthancFramework/Sources/Compression/ZipWriter.h"
#include "../../../OrthancFramework/Sources/HttpServer/FilesystemHttpSender.h"
#include "../../../OrthancFramework/Sources/Logging.h"
#include "../../../OrthancFramework/Sources/OrthancException.h"
#include "../../../OrthancFramework/Sources/SerializationToolbox.h"
#include "../OrthancConfiguration.h"
#include "../ServerContext.h"
#include "../ServerJobs/ArchiveJob.h"

#include <boost/filesystem/fstream.hpp>


namespace Orthanc
{
  static const char* const KEY_RESOURCES = "Resources";
  static const char* const KEY_EXTENDED = "Extended";
  static const char* const KEY_TRANSCODE = "Transcode";

  static const char* const CONFIG_LOADER_THREADS = "ZipLoaderThreads";

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
                               unsigned int& loaderThreads,  /* out */
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

    {
      OrthancConfiguration::ReaderLock lock;
      loaderThreads = lock.GetConfiguration().GetUnsignedIntegerParameter(CONFIG_LOADER_THREADS, 0);  // New in Orthanc 1.10.0
    }
   
  }


  namespace
  {
    class SynchronousZipChunk : public IDynamicObject
    {
    private:
      std::string  chunk_;
      bool         done_;

      explicit SynchronousZipChunk(bool done) :
        done_(done)
      {
      }

    public:
      static SynchronousZipChunk* CreateDone()
      {
        return new SynchronousZipChunk(true);
      }

      static SynchronousZipChunk* CreateChunk(const std::string& chunk)
      {
        std::unique_ptr<SynchronousZipChunk> item(new SynchronousZipChunk(false));
        item->chunk_ = chunk;
        return item.release();
      }

      bool IsDone() const
      {
        return done_;
      }

      void SwapString(std::string& target)
      {
        if (done_)
        {
          throw OrthancException(ErrorCode_BadSequenceOfCalls);
        }
        else
        {
          target.swap(chunk_);
        }
      }
    };

    
    class SynchronousZipStream : public ZipWriter::IOutputStream
    {
    private:
      boost::shared_ptr<SharedMessageQueue>  queue_;
      uint64_t                               archiveSize_;

    public:
      explicit SynchronousZipStream(const boost::shared_ptr<SharedMessageQueue>& queue) :
        queue_(queue),
        archiveSize_(0)
      {
      }

      virtual uint64_t GetArchiveSize() const ORTHANC_OVERRIDE
      {
        return archiveSize_;
      }

      virtual void Write(const std::string& chunk) ORTHANC_OVERRIDE
      {
        if (queue_.unique())
        {
          throw OrthancException(ErrorCode_NetworkProtocol,
                                 "HTTP client has disconnected while creating an archive in synchronous mode");
        }
        else
        {
          queue_->Enqueue(SynchronousZipChunk::CreateChunk(chunk));
          archiveSize_ += chunk.size();
        }
      }

      virtual void Close() ORTHANC_OVERRIDE
      {
        queue_->Enqueue(SynchronousZipChunk::CreateDone());
      }
    };


    class SynchronousZipSender : public IHttpStreamAnswer
    {
    private:
      ServerContext&                         context_;
      std::string                            jobId_;
      boost::shared_ptr<SharedMessageQueue>  queue_;
      std::string                            filename_;
      bool                                   done_;
      std::string                            chunk_;

    public:
      SynchronousZipSender(ServerContext& context,
                           const std::string& jobId,
                           const boost::shared_ptr<SharedMessageQueue>& queue,
                           const std::string& filename) :
        context_(context),
        jobId_(jobId),
        queue_(queue),
        filename_(filename),
        done_(false)
      {
      }

      virtual HttpCompression SetupHttpCompression(bool gzipAllowed,
                                                   bool deflateAllowed) ORTHANC_OVERRIDE
      {
        // This function is not called by HttpOutput::AnswerWithoutBuffering()
        throw OrthancException(ErrorCode_InternalError);
      }

      virtual bool HasContentFilename(std::string& filename) ORTHANC_OVERRIDE
      {
        filename = filename_;
        return true;
      }

      virtual std::string GetContentType() ORTHANC_OVERRIDE
      {
        return EnumerationToString(MimeType_Zip);
      }

      virtual uint64_t GetContentLength() ORTHANC_OVERRIDE
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      virtual bool ReadNextChunk() ORTHANC_OVERRIDE
      {
        for (;;)
        {
          std::unique_ptr<IDynamicObject> obj(queue_->Dequeue(100));
        
          if (obj.get() == NULL)
          {
            // Check that the job is still active, which indicates
            // that more data might still be returned
            JobState state;
            if (context_.GetJobsEngine().GetRegistry().GetState(state, jobId_) &&
                (state == JobState_Pending ||
                 state == JobState_Running ||
                 state == JobState_Success))
            {
              continue;
            }
            else
            {
              return false;
            }
          }
          else
          {
            SynchronousZipChunk& item = dynamic_cast<SynchronousZipChunk&>(*obj);
            if (item.IsDone())
            {
              done_ = true;
            }
            else
            {
              item.SwapString(chunk_);
              done_ = false;
            }

            return !done_;
          }
        }
      }

      virtual const char* GetChunkContent() ORTHANC_OVERRIDE
      {
        if (done_)
        {
          throw OrthancException(ErrorCode_InternalError);
        }
        else
        {
          return (chunk_.empty() ? NULL : chunk_.c_str());
        }
      }
      
      virtual size_t GetChunkSize() ORTHANC_OVERRIDE
      {
        if (done_)
        {
          throw OrthancException(ErrorCode_InternalError);
        }
        else
        {
          return chunk_.size();
        }
      }
    };

    
    class SynchronousTemporaryStream : public ZipWriter::IOutputStream
    {
    private:
      boost::shared_ptr<TemporaryFile>  temp_;
      boost::filesystem::ofstream       file_;
      uint64_t                          archiveSize_;

    public:
      explicit SynchronousTemporaryStream(const boost::shared_ptr<TemporaryFile>& temp) :
        temp_(temp),
        archiveSize_(0)
      {
        file_.open(temp_->GetPath(), std::ofstream::out | std::ofstream::binary);
        if (!file_.good())
        {
          throw OrthancException(ErrorCode_CannotWriteFile);
        }
      }
      
      virtual uint64_t GetArchiveSize() const ORTHANC_OVERRIDE
      {
        return archiveSize_;
      }

      virtual void Write(const std::string& chunk) ORTHANC_OVERRIDE
      {
        if (!chunk.empty())
        {
          try
          {
            file_.write(chunk.c_str(), chunk.size());
            
            if (!file_.good())
            {
              file_.close();
              throw OrthancException(ErrorCode_CannotWriteFile);
            }
          }
          catch (boost::filesystem::filesystem_error&)
          {
            throw OrthancException(ErrorCode_CannotWriteFile);
          }
          catch (...)  // To catch "std::system_error&" in C++11
          {
            throw OrthancException(ErrorCode_CannotWriteFile);
          }
        }
        
        archiveSize_ += chunk.size();
      }

      virtual void Close() ORTHANC_OVERRIDE
      {
        try
        {
          file_.close();
        }
        catch (boost::filesystem::filesystem_error&)
        {
          throw OrthancException(ErrorCode_CannotWriteFile);
        }
        catch (...)  // To catch "std::system_error&" in C++11
        {
          throw OrthancException(ErrorCode_CannotWriteFile);
        }
      }
    };
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
      bool streaming;
      
      {
        OrthancConfiguration::ReaderLock lock;
        streaming = lock.GetConfiguration().GetBooleanParameter("SynchronousZipStream", true);  // New in Orthanc 1.9.4
      }

      if (streaming)
      {
        LOG(INFO) << "Streaming a ZIP archive";
        boost::shared_ptr<SharedMessageQueue> queue(new SharedMessageQueue);

        job->AcquireSynchronousTarget(new SynchronousZipStream(queue));

        std::string jobId;
        context.GetJobsEngine().GetRegistry().Submit(jobId, job.release(), priority);

        SynchronousZipSender sender(context, jobId, queue, filename);
        output.AnswerWithoutBuffering(sender);

        // If we reach this line, this means that
        // "SynchronousZipSender::ReadNextChunk()" has returned "false"
      }
      else
      {
        // This was the only behavior in Orthanc <= 1.9.3
        LOG(INFO) << "Not streaming a ZIP archive (use of a temporary file)";
        boost::shared_ptr<TemporaryFile> tmp;

        {
          OrthancConfiguration::ReaderLock lock;
          tmp.reset(lock.GetConfiguration().CreateTemporaryFile());
        }

        job->AcquireSynchronousTarget(new SynchronousTemporaryStream(tmp));

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
                       "contain the ZIP file. This is the default, easy behavior. However, if global configuration option "
                       "\"SynchronousZipStream\" is set to \"false\", asynchronous transfers should be preferred for "
                       "large amount of data, as the creation of the temporary file might lead to network timeouts.", false)
      .SetRequestField("Asynchronous", RestApiCallDocumentation::Type_Boolean,
                       "If `true`, create the archive in asynchronous mode, which means that a job is submitted to create "
                       "the archive in background.", false)
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
      unsigned int loaderThreads;
      GetJobParameters(synchronous, extended, transcode, transferSyntax,
                       priority, loaderThreads, body, DEFAULT_IS_EXTENDED);
      
      std::unique_ptr<ArchiveJob> job(new ArchiveJob(context, IS_MEDIA, extended));
      AddResourcesOfInterest(*job, body);

      if (transcode)
      {
        job->SetTranscode(transferSyntax);
      }
      
      job->SetLoaderThreads(loaderThreads);

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
    static const char* const TRANSCODE = "transcode";
    static const char* const FILENAME = "filename";

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
        .SetHttpGetArgument(FILENAME, RestApiCallDocumentation::Type_String,
                            "Filename to set in the \"Content-Disposition\" HTTP header "
                            "(including file extension)", false)
        .SetHttpGetArgument(TRANSCODE, RestApiCallDocumentation::Type_String,
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

    const std::string id = call.GetUriComponent("id", "");
    const std::string filename = call.GetArgument(FILENAME, id + ".zip");  // New in Orthanc 1.11.0

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

    if (call.HasArgument(TRANSCODE))
    {
      job->SetTranscode(GetTransferSyntax(call.GetArgument(TRANSCODE, "")));
    }

    {
      OrthancConfiguration::ReaderLock lock;
      unsigned int loaderThreads = lock.GetConfiguration().GetUnsignedIntegerParameter(CONFIG_LOADER_THREADS, 0);  // New in Orthanc 1.10.0
      job->SetLoaderThreads(loaderThreads);
    }

    SubmitJob(call.GetOutput(), context, job, 0 /* priority */,
              true /* synchronous */, filename);
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
      unsigned int loaderThreads;
      GetJobParameters(synchronous, extended, transcode, transferSyntax,
                       priority, loaderThreads, body, false /* by default, not extented */);
      
      std::unique_ptr<ArchiveJob> job(new ArchiveJob(context, IS_MEDIA, extended));
      job->AddResource(id);

      if (transcode)
      {
        job->SetTranscode(transferSyntax);
      }

      job->SetLoaderThreads(loaderThreads);

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
