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
#include "LuaScripting.h"

#include "OrthancConfiguration.h"
#include "OrthancRestApi/OrthancRestApi.h"
#include "ServerContext.h"

#include "../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"
#include "../../OrthancFramework/Sources/HttpServer/StringHttpOutput.h"
#include "../../OrthancFramework/Sources/Logging.h"
#include "../../OrthancFramework/Sources/Lua/LuaFunctionCall.h"

#include <OrthancServerResources.h>


namespace Orthanc
{
  class LuaScripting::IEvent : public IDynamicObject
  {
  public:
    virtual void Apply(LuaScripting& lock) = 0;
  };


  class LuaScripting::OnStoredInstanceEvent : public LuaScripting::IEvent
  {
  private:
    std::string    instanceId_;
    Json::Value    simplifiedTags_;
    Json::Value    metadata_;
    Json::Value    origin_;

  public:
    OnStoredInstanceEvent(const std::string& instanceId,
                          const Json::Value& simplifiedTags,
                          const Json::Value& metadata,
                          const DicomInstanceToStore& instance) :
      instanceId_(instanceId),
      simplifiedTags_(simplifiedTags),
      metadata_(metadata)
    {
      instance.GetOrigin().Format(origin_);
    }

    virtual void Apply(LuaScripting& that) ORTHANC_OVERRIDE
    {
      static const char* NAME = "OnStoredInstance";

      LuaScripting::Lock lock(that);

      if (lock.GetLua().IsExistingFunction(NAME))
      {
        that.InitializeJob();

        LuaFunctionCall call(lock.GetLua(), NAME);
        call.PushString(instanceId_);
        call.PushJson(simplifiedTags_);
        call.PushJson(metadata_);
        call.PushJson(origin_);
        call.Execute();

        that.SubmitJob();
      }
    }
  };


  class LuaScripting::ExecuteEvent : public LuaScripting::IEvent
  {
  private:
    std::string    command_;

  public:
    explicit ExecuteEvent(const std::string& command) :
      command_(command)
    {
    }

    virtual void Apply(LuaScripting& that) ORTHANC_OVERRIDE
    {
      LuaScripting::Lock lock(that);

      if (lock.GetLua().IsExistingFunction(command_.c_str()))
      {
        LuaFunctionCall call(lock.GetLua(), command_.c_str());
        call.Execute();
      }
    }
  };


  class LuaScripting::StableResourceEvent : public LuaScripting::IEvent
  {
  private:
    ServerIndexChange  change_;

    class GetInfoOperations : public ServerIndex::IReadOnlyOperations
    {
    private:
      const ServerIndexChange&            change_;
      bool                                ok_;
      DicomMap                            tags_;
      std::map<MetadataType, std::string> metadata_;      

    public:
      explicit GetInfoOperations(const ServerIndexChange& change) :
        change_(change),
        ok_(false)
      {
      }
      
      virtual void Apply(ServerIndex::ReadOnlyTransaction& transaction) ORTHANC_OVERRIDE
      {
        int64_t internalId;
        ResourceType level;
        if (transaction.LookupResource(internalId, level, change_.GetPublicId()) &&
            level == change_.GetResourceType())
        {
          transaction.GetMainDicomTags(tags_, internalId);
          transaction.GetAllMetadata(metadata_, internalId);
          ok_ = true;
        }
      }

      void CallLua(LuaScripting& that,
                   const char* name) const
      {
        if (ok_)
        {
          Json::Value formattedMetadata = Json::objectValue;

          for (std::map<MetadataType, std::string>::const_iterator 
                 it = metadata_.begin(); it != metadata_.end(); ++it)
          {
            std::string key = EnumerationToString(it->first);
            formattedMetadata[key] = it->second;
          }      

          {
            LuaScripting::Lock lock(that);

            if (lock.GetLua().IsExistingFunction(name))
            {
              that.InitializeJob();

              Json::Value json = Json::objectValue;

              if (change_.GetResourceType() == ResourceType_Study)
              {
                DicomMap t;
                tags_.ExtractStudyInformation(t);  // Discard patient-related tags
                FromDcmtkBridge::ToJson(json, t, DicomToJsonFormat_Human);
              }
              else
              {
                FromDcmtkBridge::ToJson(json, tags_, DicomToJsonFormat_Human);
              }

              LuaFunctionCall call(lock.GetLua(), name);
              call.PushString(change_.GetPublicId());
              call.PushJson(json);
              call.PushJson(formattedMetadata);
              call.Execute();

              that.SubmitJob();
            }
          }
        }
      }
    };
    

  public:
    explicit StableResourceEvent(const ServerIndexChange& change) :
      change_(change)
    {
    }

    virtual void Apply(LuaScripting& that) ORTHANC_OVERRIDE
    {
      const char* name;

      switch (change_.GetChangeType())
      {
        case ChangeType_StablePatient:
          name = "OnStablePatient";
          break;

        case ChangeType_StableStudy:
          name = "OnStableStudy";
          break;

        case ChangeType_StableSeries:
          name = "OnStableSeries";
          break;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }

      {
        // Avoid unnecessary calls to the database if there's no Lua callback
        LuaScripting::Lock lock(that);

        if (!lock.GetLua().IsExistingFunction(name))
        {
          return;
        }
      }
      
      GetInfoOperations operations(change_);
      that.context_.GetIndex().Apply(operations);
      operations.CallLua(that, name);
    }
  };


  class LuaScripting::JobEvent : public LuaScripting::IEvent
  {
  public:
    enum Type
    {
      Type_Failure,
      Type_Submitted,
      Type_Success
    };
    
  private:
    Type         type_;
    std::string  jobId_;

  public:
    JobEvent(Type type,
             const std::string& jobId) :
      type_(type),
      jobId_(jobId)
    {
    }

    virtual void Apply(LuaScripting& that) ORTHANC_OVERRIDE
    {
      std::string functionName;
      
      switch (type_)
      {
        case Type_Failure:
          functionName = "OnJobFailure";
          break;

        case Type_Submitted:
          functionName = "OnJobSubmitted";
          break;

        case Type_Success:
          functionName = "OnJobSuccess";
          break;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }

      {
        LuaScripting::Lock lock(that);

        if (lock.GetLua().IsExistingFunction(functionName.c_str()))
        {
          LuaFunctionCall call(lock.GetLua(), functionName.c_str());
          call.PushString(jobId_);
          call.Execute();
        }
      }
    }
  };


  class LuaScripting::DeleteEvent : public LuaScripting::IEvent
  {
  private:
    ResourceType  level_;
    std::string   publicId_;

  public:
    DeleteEvent(ResourceType level,
                const std::string& publicId) :
      level_(level),
      publicId_(publicId)
    {
    }

    virtual void Apply(LuaScripting& that) ORTHANC_OVERRIDE
    {
      std::string functionName;
      
      switch (level_)
      {
        case ResourceType_Patient:
          functionName = "OnDeletedPatient";
          break;

        case ResourceType_Study:
          functionName = "OnDeletedStudy";
          break;

        case ResourceType_Series:
          functionName = "OnDeletedSeries";
          break;

        case ResourceType_Instance:
          functionName = "OnDeletedInstance";
          break;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }

      {
        LuaScripting::Lock lock(that);

        if (lock.GetLua().IsExistingFunction(functionName.c_str()))
        {
          LuaFunctionCall call(lock.GetLua(), functionName.c_str());
          call.PushString(publicId_);
          call.Execute();
        }
      }
    }
  };


  class LuaScripting::UpdateEvent : public LuaScripting::IEvent
  {
  private:
    ResourceType  level_;
    std::string   publicId_;

  public:
    UpdateEvent(ResourceType level,
                const std::string& publicId) :
      level_(level),
      publicId_(publicId)
    {
    }

    virtual void Apply(LuaScripting& that) ORTHANC_OVERRIDE
    {
      std::string functionName;
      
      switch (level_)
      {
        case ResourceType_Patient:
          functionName = "OnUpdatedPatient";
          break;

        case ResourceType_Study:
          functionName = "OnUpdatedStudy";
          break;

        case ResourceType_Series:
          functionName = "OnUpdatedSeries";
          break;

        case ResourceType_Instance:
          functionName = "OnUpdatedInstance";
          break;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }

      {
        LuaScripting::Lock lock(that);

        if (lock.GetLua().IsExistingFunction(functionName.c_str()))
        {
          LuaFunctionCall call(lock.GetLua(), functionName.c_str());
          call.PushString(publicId_);
          call.Execute();
        }
      }
    }
  };


  ServerContext* LuaScripting::GetServerContext(lua_State *state)
  {
    const void* value = LuaContext::GetGlobalVariable(state, "_ServerContext");
    return const_cast<ServerContext*>(reinterpret_cast<const ServerContext*>(value));
  }


  // Syntax in Lua: RestApiGet(uri, builtin)
  int LuaScripting::RestApiGet(lua_State *state)
  {
    ServerContext* serverContext = GetServerContext(state);
    if (serverContext == NULL)
    {
      LOG(ERROR) << "Lua: The Orthanc API is unavailable";
      lua_pushnil(state);
      return 1;
    }

    // Check the types of the arguments
    int nArgs = lua_gettop(state);
    if (nArgs < 1 || nArgs > 3 || 
        !lua_isstring(state, 1) ||                 // URI
        (nArgs >= 2 && !lua_isboolean(state, 2)))  // Restrict to built-in API?
    {
      LOG(ERROR) << "Lua: Bad parameters to RestApiGet()";
      lua_pushnil(state);
      return 1;
    }

    const char* uri = lua_tostring(state, 1);
    bool builtin = (nArgs == 2 ? lua_toboolean(state, 2) != 0 : false);

    std::map<std::string, std::string> headers;
    LuaContext::GetDictionaryArgument(headers, state, 3, true /* HTTP header key to lower case */);

    try
    {
      std::string result;
      if (IHttpHandler::SimpleGet(result, NULL, serverContext->GetHttpHandler().RestrictToOrthancRestApi(builtin), 
                                  RequestOrigin_Lua, uri, headers) == HttpStatus_200_Ok)
      {
        lua_pushlstring(state, result.c_str(), result.size());
        return 1;
      }
    }
    catch (OrthancException& e)
    {
      LOG(ERROR) << "Lua: " << e.What();
    }

    LOG(ERROR) << "Lua: Error in RestApiGet() for URI: " << uri;
    lua_pushnil(state);
    return 1;
  }


  int LuaScripting::RestApiPostOrPut(lua_State *state,
                                     bool isPost)
  {
    ServerContext* serverContext = GetServerContext(state);
    if (serverContext == NULL)
    {
      LOG(ERROR) << "Lua: The Orthanc API is unavailable";
      lua_pushnil(state);
      return 1;
    }

    // Check the types of the arguments
    int nArgs = lua_gettop(state);
    if (nArgs < 2 || nArgs > 4 || 
        !lua_isstring(state, 1) ||                 // URI
        !lua_isstring(state, 2) ||                 // Body
        (nArgs >= 3 && !lua_isboolean(state, 3)))  // Restrict to built-in API?
    {
      LOG(ERROR) << "Lua: Bad parameters to " << (isPost ? "RestApiPost()" : "RestApiPut()");
      lua_pushnil(state);
      return 1;
    }

    const char* uri = lua_tostring(state, 1);
    size_t bodySize = 0;
    const char* bodyData = lua_tolstring(state, 2, &bodySize);
    bool builtin = (nArgs == 3 ? lua_toboolean(state, 3) != 0 : false);

    std::map<std::string, std::string> headers;
    LuaContext::GetDictionaryArgument(headers, state, 4, true /* HTTP header key to lower case */);
        
    try
    {
      std::string result;
      if (isPost ?
          IHttpHandler::SimplePost(result, NULL,
                                   serverContext->GetHttpHandler().RestrictToOrthancRestApi(builtin), 
                                   RequestOrigin_Lua, uri, bodyData, bodySize, headers) == HttpStatus_200_Ok :
          IHttpHandler::SimplePut(result, NULL,
                                  serverContext->GetHttpHandler().RestrictToOrthancRestApi(builtin), 
                                  RequestOrigin_Lua, uri, bodyData, bodySize, headers) == HttpStatus_200_Ok)
      {
        lua_pushlstring(state, result.c_str(), result.size());
        return 1;
      }
    }
    catch (OrthancException& e)
    {
      LOG(ERROR) << "Lua: " << e.What();
    }

    LOG(ERROR) << "Lua: Error in " << (isPost ? "RestApiPost()" : "RestApiPut()") << " for URI: " << uri;
    lua_pushnil(state);
    return 1;
  }


  // Syntax in Lua: RestApiPost(uri, body, builtin)
  int LuaScripting::RestApiPost(lua_State *state)
  {
    return RestApiPostOrPut(state, true);
  }


  // Syntax in Lua: RestApiPut(uri, body, builtin)
  int LuaScripting::RestApiPut(lua_State *state)
  {
    return RestApiPostOrPut(state, false);
  }


  // Syntax in Lua: RestApiDelete(uri, builtin)
  int LuaScripting::RestApiDelete(lua_State *state)
  {
    ServerContext* serverContext = GetServerContext(state);
    if (serverContext == NULL)
    {
      LOG(ERROR) << "Lua: The Orthanc API is unavailable";
      lua_pushnil(state);
      return 1;
    }

    // Check the types of the arguments
    int nArgs = lua_gettop(state);
    if (nArgs < 1 || nArgs > 3 ||
        !lua_isstring(state, 1) ||                 // URI
        (nArgs >= 2 && !lua_isboolean(state, 2)))  // Restrict to built-in API?
    {
      LOG(ERROR) << "Lua: Bad parameters to RestApiDelete()";
      lua_pushnil(state);
      return 1;
    }

    const char* uri = lua_tostring(state, 1);
    bool builtin = (nArgs == 2 ? lua_toboolean(state, 2) != 0 : false);

    std::map<std::string, std::string> headers;
    LuaContext::GetDictionaryArgument(headers, state, 3, true /* HTTP header key to lower case */);
    
    try
    {
      if (IHttpHandler::SimpleDelete(NULL, serverContext->GetHttpHandler().RestrictToOrthancRestApi(builtin), 
                                     RequestOrigin_Lua, uri, headers) == HttpStatus_200_Ok)
      {
        lua_pushboolean(state, 1);
        return 1;
      }
    }
    catch (OrthancException& e)
    {
      LOG(ERROR) << "Lua: " << e.What();
    }

    LOG(ERROR) << "Lua: Error in RestApiDelete() for URI: " << uri;
    lua_pushnil(state);

    return 1;
  }


  // Syntax in Lua: GetOrthancConfiguration()
  int LuaScripting::GetOrthancConfiguration(lua_State *state)
  {
    Json::Value configuration;

    {
      OrthancConfiguration::ReaderLock lock;
      configuration = lock.GetJson();
    }

    LuaContext::GetLuaContext(state).PushJson(configuration);

    return 1;
  }


  size_t LuaScripting::ParseOperation(LuaJobManager::Lock& lock,
                                      const std::string& operation,
                                      const Json::Value& parameters)
  {
    if (operation == "delete")
    {
      LOG(INFO) << "Lua script to delete resource " << parameters["Resource"].asString();
      return lock.AddDeleteResourceOperation(context_);
    }

    if (operation == "store-scu")
    {
      std::string localAet;
      if (parameters.isMember("LocalAet"))
      {
        localAet = parameters["LocalAet"].asString();
      }
      else
      {
        localAet = context_.GetDefaultLocalApplicationEntityTitle();
      }

      std::string name = parameters["Modality"].asString();
      RemoteModalityParameters modality;

      {
        OrthancConfiguration::ReaderLock configLock;
        modality = configLock.GetConfiguration().GetModalityUsingSymbolicName(name);
      }

      // This is not a C-MOVE: No need to call "StoreScuCommand::SetMoveOriginator()"
      return lock.AddStoreScuOperation(context_, localAet, modality);
    }

    if (operation == "store-peer")
    {
      OrthancConfiguration::ReaderLock configLock;
      std::string name = parameters["Peer"].asString();

      WebServiceParameters peer;
      if (configLock.GetConfiguration().LookupOrthancPeer(peer, name))
      {
        return lock.AddStorePeerOperation(peer);
      }
      else
      {
        throw OrthancException(ErrorCode_UnknownResource,
                               "No peer with symbolic name: " + name);
      }
    }

    if (operation == "modify")
    {
      std::unique_ptr<DicomModification> modification(new DicomModification);
      modification->ParseModifyRequest(parameters);

      return lock.AddModifyInstanceOperation(context_, modification.release());
    }

    if (operation == "call-system")
    {
      LOG(INFO) << "Lua script to call system command on " << parameters["Resource"].asString();

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

      std::string command = parameters["Command"].asString();
      std::vector<std::string> postArgs;

      return lock.AddSystemCallOperation(command, args, postArgs);
    }

    throw OrthancException(ErrorCode_ParameterOutOfRange);
  }


  void LuaScripting::InitializeJob()
  {
    lua_.Execute("_InitializeJob()");
  }


  void LuaScripting::SubmitJob()
  {
    Json::Value operations;
    LuaFunctionCall call2(lua_, "_AccessJob");
    call2.ExecuteToJson(operations, false);
     
    if (operations.type() != Json::arrayValue)
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    LuaJobManager::Lock lock(jobManager_, context_.GetJobsEngine());

    bool isFirst = true;
    size_t previous = 0;  // Dummy initialization to avoid warning

    for (Json::Value::ArrayIndex i = 0; i < operations.size(); ++i)
    {
      if (operations[i].type() != Json::objectValue ||
          !operations[i].isMember("Operation"))
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      const Json::Value& parameters = operations[i];
      if (!parameters.isMember("Resource"))
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      std::string operation = parameters["Operation"].asString();
      size_t index = ParseOperation(lock, operation, operations[i]);
        
      std::string resource = parameters["Resource"].asString();
      if (!resource.empty())
      {
        lock.AddDicomInstanceInput(index, context_, resource);
      }
      else if (!isFirst)
      {
        lock.Connect(previous, index);
      }

      isFirst = false;
      previous = index;
    }
  }


  LuaScripting::LuaScripting(ServerContext& context) : 
    context_(context),
    state_(State_Setup),
    heartBeatPeriod_(0)
  {
    lua_.SetGlobalVariable("_ServerContext", &context);
    lua_.RegisterFunction("RestApiGet", RestApiGet);
    lua_.RegisterFunction("RestApiPost", RestApiPost);
    lua_.RegisterFunction("RestApiPut", RestApiPut);
    lua_.RegisterFunction("RestApiDelete", RestApiDelete);
    lua_.RegisterFunction("GetOrthancConfiguration", GetOrthancConfiguration);

    LOG(INFO) << "Initializing Lua for the event handler";
    LoadGlobalConfiguration();
  }


  LuaScripting::~LuaScripting()
  {
    if (state_ == State_Running)
    {
      LOG(ERROR) << "INTERNAL ERROR: LuaScripting::Stop() should be invoked manually to avoid mess in the destruction order!";
      Stop();
    }
  }

  void LuaScripting::HeartBeatThread(LuaScripting* that)
  {
    static const boost::posix_time::time_duration PERIODICITY =
      boost::posix_time::seconds(that->heartBeatPeriod_);
    
    unsigned int sleepDelay = 100;
    
    boost::posix_time::ptime next =
      boost::posix_time::microsec_clock::universal_time() + PERIODICITY;
    
    while (that->state_ != State_Done)
    {
      boost::this_thread::sleep(boost::posix_time::milliseconds(sleepDelay));

      if (that->state_ != State_Done &&
          boost::posix_time::microsec_clock::universal_time() >= next)
      {
        LuaScripting::Lock lock(*that);

        if (lock.GetLua().IsExistingFunction("OnHeartBeat"))
        {
          LuaFunctionCall call(lock.GetLua(), "OnHeartBeat");
          call.Execute();
        }

        next = boost::posix_time::microsec_clock::universal_time() + PERIODICITY;
      }
    }

  }

  void LuaScripting::EventThread(LuaScripting* that)
  {
    for (;;)
    {
      std::unique_ptr<IDynamicObject> event(that->pendingEvents_.Dequeue(100));

      if (event.get() == NULL)
      {
        // The event queue is empty, check whether we should stop
        boost::recursive_mutex::scoped_lock lock(that->mutex_);

        if (that->state_ != State_Running)
        {
          return;
        }
      }
      else
      {
        try
        {
          dynamic_cast<IEvent&>(*event).Apply(*that);
        }
        catch (OrthancException& e)
        {
          LOG(ERROR) << "Error while processing Lua events: " << e.What();
        }
      }

      that->jobManager_.GetDicomConnectionManager().CloseIfInactive();
    }
  }


  void LuaScripting::Start()
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);

    if (state_ != State_Setup ||
        eventThread_.joinable()  /* already started */)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      LOG(INFO) << "Starting the Lua engine";
      eventThread_ = boost::thread(EventThread, this);
      
      LuaScripting::Lock lock(*this);

      if (heartBeatPeriod_ > 0 && lock.GetLua().IsExistingFunction("OnHeartBeat"))
      {
        LOG(INFO) << "Starting the Lua HeartBeat thread with a period of " << heartBeatPeriod_ << " seconds";
        heartBeatThread_ = boost::thread(HeartBeatThread, this);
      }
      state_ = State_Running;
    }
  }


  void LuaScripting::Stop()
  {
    {
      boost::recursive_mutex::scoped_lock lock(mutex_);

      if (state_ != State_Running)
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }

      state_ = State_Done;
    }

    jobManager_.AwakeTrailingSleep();

    if (eventThread_.joinable())
    {
      LOG(INFO) << "Stopping the Lua engine";
      eventThread_.join();
      if (heartBeatThread_.joinable())
      {
        heartBeatThread_.join();
      }
      LOG(INFO) << "The Lua engine has stopped";
    }
  }


  void LuaScripting::SignalStoredInstance(const std::string& publicId,
                                          const DicomInstanceToStore& instance,
                                          const Json::Value& simplifiedTags)
  {
    Json::Value metadata = Json::objectValue;

    for (ServerIndex::MetadataMap::const_iterator 
           it = instance.GetMetadata().begin(); 
         it != instance.GetMetadata().end(); ++it)
    {
      if (it->first.first == ResourceType_Instance)
      {
        metadata[EnumerationToString(it->first.second)] = it->second;
      }
    }

    pendingEvents_.Enqueue(new OnStoredInstanceEvent(publicId, simplifiedTags, metadata, instance));
  }


  void LuaScripting::SignalChange(const ServerIndexChange& change)
  {
    if (change.GetChangeType() == ChangeType_StablePatient ||
        change.GetChangeType() == ChangeType_StableStudy ||
        change.GetChangeType() == ChangeType_StableSeries)
    {
      pendingEvents_.Enqueue(new StableResourceEvent(change));
    }
    else if (change.GetChangeType() == ChangeType_Deleted)
    {
      pendingEvents_.Enqueue(new DeleteEvent(change.GetResourceType(), change.GetPublicId()));
    }
    else if (change.GetChangeType() == ChangeType_UpdatedAttachment ||
             change.GetChangeType() == ChangeType_UpdatedMetadata)
    {
      pendingEvents_.Enqueue(new UpdateEvent(change.GetResourceType(), change.GetPublicId()));
    }
  }


  bool LuaScripting::FilterIncomingInstance(const DicomInstanceToStore& instance,
                                            const Json::Value& simplified)
  {
    static const char* NAME = "ReceivedInstanceFilter";

    boost::recursive_mutex::scoped_lock lock(mutex_);

    if (lua_.IsExistingFunction(NAME))
    {
      LuaFunctionCall call(lua_, NAME);
      call.PushJson(simplified);

      Json::Value origin;
      instance.GetOrigin().Format(origin);
      call.PushJson(origin);

      Json::Value info = Json::objectValue;
      info["HasPixelData"] = instance.HasPixelData();

      DicomTransferSyntax s;
      if (instance.LookupTransferSyntax(s))
      {
        info["TransferSyntaxUID"] = GetTransferSyntaxUid(s);
      }

      call.PushJson(info);

      if (!call.ExecutePredicate())
      {
        return false;
      }
    }

    return true;
  }

  bool LuaScripting::FilterIncomingCStoreInstance(uint16_t& dimseStatus,
                                                  const DicomInstanceToStore& instance,
                                                  const Json::Value& simplified)
  {
    static const char* NAME = "ReceivedCStoreInstanceFilter";

    boost::recursive_mutex::scoped_lock lock(mutex_);

    if (lua_.IsExistingFunction(NAME))
    {
      LuaFunctionCall call(lua_, NAME);
      call.PushJson(simplified);

      Json::Value origin;
      instance.GetOrigin().Format(origin);
      call.PushJson(origin);

      Json::Value info = Json::objectValue;
      info["HasPixelData"] = instance.HasPixelData();

      DicomTransferSyntax s;
      if (instance.LookupTransferSyntax(s))
      {
        info["TransferSyntaxUID"] = GetTransferSyntaxUid(s);
      }

      call.PushJson(info);

      int result;
      call.ExecuteToInt(result);
      return static_cast<uint16_t>(result);
    }

    return true;
  }


  void LuaScripting::Execute(const std::string& command)
  {
    pendingEvents_.Enqueue(new ExecuteEvent(command));
  }


  void LuaScripting::LoadGlobalConfiguration()
  {
    OrthancConfiguration::ReaderLock configLock;

    {
      std::string command;
      Orthanc::ServerResources::GetFileResource(command, Orthanc::ServerResources::LUA_TOOLBOX);
      lua_.Execute(command);
    }    

    std::list<std::string> luaScripts;
    configLock.GetConfiguration().GetListOfStringsParameter(luaScripts, "LuaScripts");
    heartBeatPeriod_ = configLock.GetConfiguration().GetIntegerParameter("LuaHeartBeatPeriod", 0);

    LuaScripting::Lock lock(*this);

    for (std::list<std::string>::const_iterator
           it = luaScripts.begin(); it != luaScripts.end(); ++it)
    {
      std::string path = configLock.GetConfiguration().InterpretStringParameterAsPath(*it);
      LOG(INFO) << "Installing the Lua scripts from: " << path;
      std::string script;
      SystemToolbox::ReadFile(script, path);

      lock.GetLua().Execute(script);
    }
  }

  
  void LuaScripting::SignalJobSubmitted(const std::string& jobId)
  {
    pendingEvents_.Enqueue(new JobEvent(JobEvent::Type_Submitted, jobId));
  }
  

  void LuaScripting::SignalJobSuccess(const std::string& jobId)
  {
    pendingEvents_.Enqueue(new JobEvent(JobEvent::Type_Success, jobId));
  }
  

  void LuaScripting::SignalJobFailure(const std::string& jobId)
  {
    pendingEvents_.Enqueue(new JobEvent(JobEvent::Type_Failure, jobId));
  }
}
