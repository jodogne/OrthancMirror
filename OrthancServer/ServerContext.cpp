/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2014 Medical Physics Department, CHU of Liege,
 * Belgium
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
#include "ServerContext.h"

#include "../Core/HttpServer/FilesystemHttpSender.h"
#include "../Core/Lua/LuaFunctionCall.h"
#include "FromDcmtkBridge.h"
#include "ServerToolbox.h"
#include "OrthancInitialization.h"

#include <glog/logging.h>
#include <EmbeddedResources.h>
#include <dcmtk/dcmdata/dcfilefo.h>


#include "Scheduler/CallSystemCommand.h"
#include "Scheduler/DeleteInstanceCommand.h"
#include "Scheduler/ModifyInstanceCommand.h"
#include "Scheduler/StoreScuCommand.h"
#include "Scheduler/StorePeerCommand.h"
#include "OrthancRestApi/OrthancRestApi.h"
#include "../Plugins/Engine/OrthancPlugins.h"


#define ENABLE_DICOM_CACHE  1

static const char* RECEIVED_INSTANCE_FILTER = "ReceivedInstanceFilter";
static const char* ON_STORED_INSTANCE = "OnStoredInstance";

static const size_t DICOM_CACHE_SIZE = 2;

/**
 * IMPORTANT: We make the assumption that the same instance of
 * FileStorage can be accessed from multiple threads. This seems OK
 * since the filesystem implements the required locking mechanisms,
 * but maybe a read-writer lock on the "FileStorage" could be
 * useful. Conversely, "ServerIndex" already implements mutex-based
 * locking.
 **/

namespace Orthanc
{
  ServerContext::ServerContext(IDatabaseWrapper& database) :
    index_(*this, database),
    compressionEnabled_(false),
    provider_(*this),
    dicomCache_(provider_, DICOM_CACHE_SIZE),
    scheduler_(Configuration::GetGlobalIntegerParameter("LimitJobs", 10)),
    plugins_(NULL),
    pluginsManager_(NULL)
  {
    scu_.SetLocalApplicationEntityTitle(Configuration::GetGlobalStringParameter("DicomAet", "ORTHANC"));
    //scu_.SetMillisecondsBeforeClose(1);  // The connection is always released

    lua_.Execute(Orthanc::EmbeddedResources::LUA_TOOLBOX);
  }

  void ServerContext::SetCompressionEnabled(bool enabled)
  {
    if (enabled)
      LOG(WARNING) << "Disk compression is enabled";
    else
      LOG(WARNING) << "Disk compression is disabled";

    compressionEnabled_ = enabled;
  }

  void ServerContext::RemoveFile(const std::string& fileUuid,
                                 FileContentType type)
  {
    accessor_.Remove(fileUuid, type);
  }


  bool ServerContext::ApplyReceivedInstanceFilter(const Json::Value& simplified,
                                                  const std::string& remoteAet)
  {
    LuaContextLocker locker(*this);

    if (locker.GetLua().IsExistingFunction(RECEIVED_INSTANCE_FILTER))
    {
      LuaFunctionCall call(locker.GetLua(), RECEIVED_INSTANCE_FILTER);
      call.PushJson(simplified);
      call.PushString(remoteAet);

      if (!call.ExecutePredicate())
      {
        return false;
      }
    }

    return true;
  }


  static IServerCommand* ParseOperation(ServerContext& context,
                                        const std::string& operation,
                                        const Json::Value& parameters)
  {
    if (operation == "delete")
    {
      LOG(INFO) << "Lua script to delete instance " << parameters["Instance"].asString();
      return new DeleteInstanceCommand(context);
    }

    if (operation == "store-scu")
    {
      std::string modality = parameters["Modality"].asString();
      LOG(INFO) << "Lua script to send instance " << parameters["Instance"].asString()
                << " to modality " << modality << " using Store-SCU";
      return new StoreScuCommand(context, Configuration::GetModalityUsingSymbolicName(modality), true);
    }

    if (operation == "store-peer")
    {
      std::string peer = parameters["Peer"].asString();
      LOG(INFO) << "Lua script to send instance " << parameters["Instance"].asString()
                << " to peer " << peer << " using HTTP";

      OrthancPeerParameters parameters;
      Configuration::GetOrthancPeer(parameters, peer);
      return new StorePeerCommand(context, parameters, true);
    }

    if (operation == "modify")
    {
      LOG(INFO) << "Lua script to modify instance " << parameters["Instance"].asString();
      std::auto_ptr<ModifyInstanceCommand> command(new ModifyInstanceCommand(context));
      OrthancRestApi::ParseModifyRequest(command->GetModification(), parameters);
      return command.release();
    }

    if (operation == "call-system")
    {
      LOG(INFO) << "Lua script to call system command on " << parameters["Instance"].asString();

      const Json::Value& argsIn = parameters["Arguments"];
      if (argsIn.type() != Json::arrayValue)
      {
        throw OrthancException(ErrorCode_BadParameterType);
      }

      std::vector<std::string> args;
      args.reserve(argsIn.size());
      for (Json::Value::ArrayIndex i = 0; i < argsIn.size(); ++i)
      {
        // http://jsoncpp.sourceforge.net/namespace_json.html#7d654b75c16a57007925868e38212b4e
        switch (argsIn[i].type())
        {
          case Json::stringValue:
            args.push_back(argsIn[i].asString());
            break;

          case Json::intValue:
            args.push_back(boost::lexical_cast<std::string>(argsIn[i].asInt()));
            break;

          case Json::uintValue:
            args.push_back(boost::lexical_cast<std::string>(argsIn[i].asUInt()));
            break;

          case Json::realValue:
            args.push_back(boost::lexical_cast<std::string>(argsIn[i].asFloat()));
            break;

          default:
            throw OrthancException(ErrorCode_BadParameterType);
        }
      }

      return new CallSystemCommand(context, parameters["Command"].asString(), args);
    }

    throw OrthancException(ErrorCode_ParameterOutOfRange);
  }


  void ServerContext::ApplyLuaOnStoredInstance(const std::string& instanceId,
                                               const Json::Value& simplifiedDicom,
                                               const Json::Value& metadata)
  {
    LuaContextLocker locker(*this);

    if (locker.GetLua().IsExistingFunction(ON_STORED_INSTANCE))
    {
      locker.GetLua().Execute("_InitializeJob()");

      LuaFunctionCall call(locker.GetLua(), ON_STORED_INSTANCE);
      call.PushString(instanceId);
      call.PushJson(simplifiedDicom);
      call.PushJson(metadata);
      call.Execute();

      Json::Value operations;
      LuaFunctionCall call2(locker.GetLua(), "_AccessJob");
      call2.ExecuteToJson(operations);
     
      if (operations.type() != Json::arrayValue)
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      ServerJob job;
      ServerCommandInstance* previousCommand = NULL;

      for (Json::Value::ArrayIndex i = 0; i < operations.size(); ++i)
      {
        if (operations[i].type() != Json::objectValue ||
            !operations[i].isMember("Operation"))
        {
          throw OrthancException(ErrorCode_InternalError);
        }

        const Json::Value& parameters = operations[i];
        std::string operation = parameters["Operation"].asString();

        ServerCommandInstance& command = job.AddCommand(ParseOperation(*this, operation, operations[i]));
        
        if (!parameters.isMember("Instance"))
        {
          throw OrthancException(ErrorCode_InternalError);
        }

        std::string instance = parameters["Instance"].asString();
        if (instance.empty())
        {
          previousCommand->ConnectOutput(command);
        }
        else 
        {
          command.AddInput(instance);
        }

        previousCommand = &command;
      }

      job.SetDescription(std::string("Lua script: ") + ON_STORED_INSTANCE);
      scheduler_.Submit(job);
    }
  }


  StoreStatus ServerContext::Store(std::string& resultPublicId,
                                   DicomInstanceToStore& dicom)
  {
    try
    {
      DicomInstanceHasher hasher(dicom.GetSummary());
      resultPublicId = hasher.HashInstance();

      Json::Value simplified;
      SimplifyTags(simplified, dicom.GetJson());

      // Test if the instance must be filtered out
      if (!ApplyReceivedInstanceFilter(simplified, dicom.GetRemoteAet()))
      {
        LOG(INFO) << "An incoming instance has been discarded by the filter";
        return StoreStatus_FilteredOut;
      }

      if (compressionEnabled_)
      {
        accessor_.SetCompressionForNextOperations(CompressionType_Zlib);
      }
      else
      {
        accessor_.SetCompressionForNextOperations(CompressionType_None);
      }      

      FileInfo dicomInfo = accessor_.Write(dicom.GetBufferData(), dicom.GetBufferSize(), FileContentType_Dicom);
      FileInfo jsonInfo = accessor_.Write(dicom.GetJson().toStyledString(), FileContentType_DicomAsJson);

      ServerIndex::Attachments attachments;
      attachments.push_back(dicomInfo);
      attachments.push_back(jsonInfo);

      typedef std::map<MetadataType, std::string>  InstanceMetadata;
      InstanceMetadata  instanceMetadata;
      StoreStatus status = index_.Store(instanceMetadata, dicom.GetSummary(), attachments, 
                                        dicom.GetRemoteAet(), dicom.GetMetadata());

      dicom.GetMetadata().clear();

      for (InstanceMetadata::const_iterator it = instanceMetadata.begin();
           it != instanceMetadata.end(); it++)
      {
        dicom.GetMetadata().insert(std::make_pair(std::make_pair(ResourceType_Instance, it->first),
                                                  it->second));
      }
            
      if (status != StoreStatus_Success)
      {
        accessor_.Remove(dicomInfo.GetUuid(), FileContentType_Dicom);
        accessor_.Remove(jsonInfo.GetUuid(), FileContentType_DicomAsJson);
      }

      switch (status)
      {
        case StoreStatus_Success:
          LOG(INFO) << "New instance stored";
          break;

        case StoreStatus_AlreadyStored:
          LOG(INFO) << "Already stored";
          break;

        case StoreStatus_Failure:
          LOG(ERROR) << "Store failure";
          break;

        default:
          // This should never happen
          break;
      }

      if (status == StoreStatus_Success ||
          status == StoreStatus_AlreadyStored)
      {
        Json::Value metadata = Json::objectValue;
        for (std::map<MetadataType, std::string>::const_iterator 
               it = instanceMetadata.begin(); 
             it != instanceMetadata.end(); ++it)
        {
          metadata[EnumerationToString(it->first)] = it->second;
        }

        try
        {
          ApplyLuaOnStoredInstance(resultPublicId, simplified, metadata);
        }
        catch (OrthancException& e)
        {
          LOG(ERROR) << "Error in OnStoredInstance callback (Lua): " << e.What();
        }

        if (plugins_ != NULL)
        {
          try
          {
            plugins_->SignalStoredInstance(dicom, resultPublicId);
          }
          catch (OrthancException& e)
          {
            LOG(ERROR) << "Error in OnStoredInstance callback (plugins): " << e.What();
          }
        }
      }

      return status;
    }
    catch (OrthancException& e)
    {
      if (e.GetErrorCode() == ErrorCode_InexistentTag)
      {
        LogMissingRequiredTag(dicom.GetSummary());
      }

      throw;
    }
  }



  void ServerContext::AnswerAttachment(RestApiOutput& output,
                                       const std::string& instancePublicId,
                                       FileContentType content)
  {
    FileInfo attachment;
    if (!index_.LookupAttachment(attachment, instancePublicId, content))
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    accessor_.SetCompressionForNextOperations(attachment.GetCompressionType());

    std::auto_ptr<HttpFileSender> sender(accessor_.ConstructHttpFileSender(attachment.GetUuid(), attachment.GetContentType()));
    sender->SetContentType(GetMimeType(content));
    sender->SetDownloadFilename(instancePublicId + ".dcm");
    output.AnswerFile(*sender);
  }


  void ServerContext::ReadJson(Json::Value& result,
                               const std::string& instancePublicId)
  {
    std::string s;
    ReadFile(s, instancePublicId, FileContentType_DicomAsJson);

    Json::Reader reader;
    if (!reader.parse(s, result))
    {
      throw OrthancException("Corrupted JSON file");
    }
  }


  void ServerContext::ReadFile(std::string& result,
                               const std::string& instancePublicId,
                               FileContentType content,
                               bool uncompressIfNeeded)
  {
    FileInfo attachment;
    if (!index_.LookupAttachment(attachment, instancePublicId, content))
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    if (uncompressIfNeeded)
    {
      accessor_.SetCompressionForNextOperations(attachment.GetCompressionType());
    }
    else
    {
      accessor_.SetCompressionForNextOperations(CompressionType_None);
    }

    accessor_.Read(result, attachment.GetUuid(), attachment.GetContentType());
  }


  IDynamicObject* ServerContext::DicomCacheProvider::Provide(const std::string& instancePublicId)
  {
    std::string content;
    context_.ReadFile(content, instancePublicId, FileContentType_Dicom);
    return new ParsedDicomFile(content);
  }


  ServerContext::DicomCacheLocker::DicomCacheLocker(ServerContext& that,
                                                    const std::string& instancePublicId) : 
    that_(that),
    lock_(that_.dicomCacheMutex_)
  {
#if ENABLE_DICOM_CACHE == 0
    static std::auto_ptr<IDynamicObject> p;
    p.reset(provider_.Provide(instancePublicId));
    dicom_ = dynamic_cast<ParsedDicomFile*>(p.get());
#else
    dicom_ = &dynamic_cast<ParsedDicomFile&>(that_.dicomCache_.Access(instancePublicId));
#endif
  }


  ServerContext::DicomCacheLocker::~DicomCacheLocker()
  {
  }


  void ServerContext::SetStoreMD5ForAttachments(bool storeMD5)
  {
    LOG(INFO) << "Storing MD5 for attachments: " << (storeMD5 ? "yes" : "no");
    accessor_.SetStoreMD5(storeMD5);
  }


  bool ServerContext::AddAttachment(const std::string& resourceId,
                                    FileContentType attachmentType,
                                    const void* data,
                                    size_t size)
  {
    LOG(INFO) << "Adding attachment " << EnumerationToString(attachmentType) << " to resource " << resourceId;
    
    if (compressionEnabled_)
    {
      accessor_.SetCompressionForNextOperations(CompressionType_Zlib);
    }
    else
    {
      accessor_.SetCompressionForNextOperations(CompressionType_None);
    }      

    FileInfo info = accessor_.Write(data, size, attachmentType);
    StoreStatus status = index_.AddAttachment(info, resourceId);

    if (status != StoreStatus_Success)
    {
      accessor_.Remove(info.GetUuid(), info.GetContentType());
      return false;
    }
    else
    {
      return true;
    }
  }


  bool ServerContext::DeleteResource(Json::Value& target,
                                     const std::string& uuid,
                                     ResourceType expectedType)
  {
    return index_.DeleteResource(target, uuid, expectedType);
  }


  void ServerContext::SignalChange(const ServerIndexChange& change)
  {
    if (plugins_ != NULL)
    {
      try
      {
        plugins_->SignalChange(change);
      }
      catch (OrthancException& e)
      {
        LOG(ERROR) << "Error in OnChangeCallback (plugins): " << e.What();
      }
    }
  }


  bool ServerContext::HasPlugins() const
  {
    return (pluginsManager_ && plugins_);
  }


  const PluginsManager& ServerContext::GetPluginsManager() const
  {
    if (HasPlugins())
    {
      return *pluginsManager_;
    }
    else
    {
      throw OrthancException(ErrorCode_InternalError);
    }
  }


  const OrthancPlugins& ServerContext::GetOrthancPlugins() const
  {
    if (HasPlugins())
    {
      return *plugins_;
    }
    else
    {
      throw OrthancException(ErrorCode_InternalError);
    }
  }
}
