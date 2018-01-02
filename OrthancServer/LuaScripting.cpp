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


#include "PrecompiledHeadersServer.h"
#include "LuaScripting.h"

#include "ServerContext.h"
#include "OrthancInitialization.h"
#include "../Core/Lua/LuaFunctionCall.h"
#include "../Core/HttpServer/StringHttpOutput.h"
#include "../Core/Logging.h"

#include "Scheduler/DeleteInstanceCommand.h"
#include "Scheduler/StoreScuCommand.h"
#include "Scheduler/StorePeerCommand.h"
#include "Scheduler/ModifyInstanceCommand.h"
#include "Scheduler/CallSystemCommand.h"
#include "OrthancRestApi/OrthancRestApi.h"

#include <EmbeddedResources.h>


namespace Orthanc
{
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
    if ((nArgs != 1 && nArgs != 2) || 
        !lua_isstring(state, 1) ||                 // URI
        (nArgs == 2 && !lua_isboolean(state, 2)))  // Restrict to built-in API?
    {
      LOG(ERROR) << "Lua: Bad parameters to RestApiGet()";
      lua_pushnil(state);
      return 1;
    }

    const char* uri = lua_tostring(state, 1);
    bool builtin = (nArgs == 2 ? lua_toboolean(state, 2) != 0 : false);

    try
    {
      std::string result;
      if (HttpToolbox::SimpleGet(result, serverContext->GetHttpHandler().RestrictToOrthancRestApi(builtin), 
                                 RequestOrigin_Lua, uri))
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
    if ((nArgs != 2 && nArgs != 3) || 
        !lua_isstring(state, 1) ||                 // URI
        !lua_isstring(state, 2) ||                 // Body
        (nArgs == 3 && !lua_isboolean(state, 3)))  // Restrict to built-in API?
    {
      LOG(ERROR) << "Lua: Bad parameters to " << (isPost ? "RestApiPost()" : "RestApiPut()");
      lua_pushnil(state);
      return 1;
    }

    const char* uri = lua_tostring(state, 1);
    size_t bodySize = 0;
    const char* bodyData = lua_tolstring(state, 2, &bodySize);
    bool builtin = (nArgs == 3 ? lua_toboolean(state, 3) != 0 : false);

    try
    {
      std::string result;
      if (isPost ?
          HttpToolbox::SimplePost(result, serverContext->GetHttpHandler().RestrictToOrthancRestApi(builtin), 
                                  RequestOrigin_Lua, uri, bodyData, bodySize) :
          HttpToolbox::SimplePut(result, serverContext->GetHttpHandler().RestrictToOrthancRestApi(builtin), 
                                 RequestOrigin_Lua, uri, bodyData, bodySize))
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
    if ((nArgs != 1 && nArgs != 2) || 
        !lua_isstring(state, 1) ||                 // URI
        (nArgs == 2 && !lua_isboolean(state, 2)))  // Restrict to built-in API?
    {
      LOG(ERROR) << "Lua: Bad parameters to RestApiDelete()";
      lua_pushnil(state);
      return 1;
    }

    const char* uri = lua_tostring(state, 1);
    bool builtin = (nArgs == 2 ? lua_toboolean(state, 2) != 0 : false);

    try
    {
      if (HttpToolbox::SimpleDelete(serverContext->GetHttpHandler().RestrictToOrthancRestApi(builtin), 
                                    RequestOrigin_Lua, uri))
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
    Configuration::GetConfiguration(configuration);

    LuaContext::GetLuaContext(state).PushJson(configuration);

    return 1;
  }


  IServerCommand* LuaScripting::ParseOperation(const std::string& operation,
                                               const Json::Value& parameters)
  {
    if (operation == "delete")
    {
      LOG(INFO) << "Lua script to delete resource " << parameters["Resource"].asString();
      return new DeleteInstanceCommand(context_);
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

      std::string modality = parameters["Modality"].asString();
      LOG(INFO) << "Lua script to send resource " << parameters["Resource"].asString()
                << " to modality " << modality << " using Store-SCU";

      // This is not a C-MOVE: No need to call "StoreScuCommand::SetMoveOriginator()"
      return new StoreScuCommand(context_, localAet,
                                 Configuration::GetModalityUsingSymbolicName(modality), true);
    }

    if (operation == "store-peer")
    {
      std::string peer = parameters["Peer"].asString();
      LOG(INFO) << "Lua script to send resource " << parameters["Resource"].asString()
                << " to peer " << peer << " using HTTP";

      WebServiceParameters parameters;
      Configuration::GetOrthancPeer(parameters, peer);
      return new StorePeerCommand(context_, parameters, true);
    }

    if (operation == "modify")
    {
      LOG(INFO) << "Lua script to modify resource " << parameters["Resource"].asString();
      std::auto_ptr<DicomModification> modification(new DicomModification);
      OrthancRestApi::ParseModifyRequest(*modification, parameters);

      std::auto_ptr<ModifyInstanceCommand> command
        (new ModifyInstanceCommand(context_, RequestOrigin_Lua, modification.release()));

      return command.release();
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

      return new CallSystemCommand(context_, parameters["Command"].asString(), args);
    }

    throw OrthancException(ErrorCode_ParameterOutOfRange);
  }


  void LuaScripting::InitializeJob()
  {
    lua_.Execute("_InitializeJob()");
  }


  void LuaScripting::SubmitJob(const std::string& description)
  {
    Json::Value operations;
    LuaFunctionCall call2(lua_, "_AccessJob");
    call2.ExecuteToJson(operations, false);
     
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

      ServerCommandInstance& command = job.AddCommand(ParseOperation(operation, operations[i]));
        
      if (!parameters.isMember("Resource"))
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      std::string resource = parameters["Resource"].asString();
      if (resource.empty())
      {
        previousCommand->ConnectOutput(command);
      }
      else 
      {
        command.AddInput(resource);
      }

      previousCommand = &command;
    }

    job.SetDescription(description);
    context_.GetScheduler().Submit(job);
  }


  LuaScripting::LuaScripting(ServerContext& context) : context_(context)
  {
    lua_.SetGlobalVariable("_ServerContext", &context);
    lua_.RegisterFunction("RestApiGet", RestApiGet);
    lua_.RegisterFunction("RestApiPost", RestApiPost);
    lua_.RegisterFunction("RestApiPut", RestApiPut);
    lua_.RegisterFunction("RestApiDelete", RestApiDelete);
    lua_.RegisterFunction("GetOrthancConfiguration", GetOrthancConfiguration);

    lua_.Execute(Orthanc::EmbeddedResources::LUA_TOOLBOX);
  }


  void LuaScripting::ApplyOnStoredInstance(const std::string& instanceId,
                                           const Json::Value& simplifiedTags,
                                           const Json::Value& metadata,
                                           const DicomInstanceToStore& instance)
  {
    static const char* NAME = "OnStoredInstance";

    if (lua_.IsExistingFunction(NAME))
    {
      InitializeJob();

      LuaFunctionCall call(lua_, NAME);
      call.PushString(instanceId);
      call.PushJson(simplifiedTags);
      call.PushJson(metadata);

      Json::Value origin;
      instance.GetOriginInformation(origin);
      call.PushJson(origin);

      call.Execute();

      SubmitJob(std::string("Lua script: ") + NAME);
    }
  }


  void LuaScripting::SignalStoredInstance(const std::string& publicId,
                                          DicomInstanceToStore& instance,
                                          const Json::Value& simplifiedTags)
  {
    boost::recursive_mutex::scoped_lock lock(mutex_);

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

    ApplyOnStoredInstance(publicId, simplifiedTags, metadata, instance);
  }


  
  void LuaScripting::OnStableResource(const ServerIndexChange& change)
  {
    const char* name;

    switch (change.GetChangeType())
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


    Json::Value tags, metadata;
    if (context_.GetIndex().LookupResource(tags, change.GetPublicId(), change.GetResourceType()) &&
        context_.GetIndex().GetMetadata(metadata, change.GetPublicId()))
    {
      boost::recursive_mutex::scoped_lock lock(mutex_);

      if (lua_.IsExistingFunction(name))
      {
        InitializeJob();

        LuaFunctionCall call(lua_, name);
        call.PushString(change.GetPublicId());
        call.PushJson(tags["MainDicomTags"]);
        call.PushJson(metadata);
        call.Execute();

        SubmitJob(std::string("Lua script: ") + name);
      }
    }
  }



  void LuaScripting::SignalChange(const ServerIndexChange& change)
  {
    if (change.GetChangeType() == ChangeType_StablePatient ||
        change.GetChangeType() == ChangeType_StableStudy ||
        change.GetChangeType() == ChangeType_StableSeries)
    {
      OnStableResource(change);
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
      instance.GetOriginInformation(origin);
      call.PushJson(origin);

      if (!call.ExecutePredicate())
      {
        return false;
      }
    }

    return true;
  }


  void LuaScripting::Execute(const std::string& command)
  {
    LuaScripting::Locker locker(*this);
      
    if (locker.GetLua().IsExistingFunction(command.c_str()))
    {
      LuaFunctionCall call(locker.GetLua(), command.c_str());
      call.Execute();
    }
  }
}
