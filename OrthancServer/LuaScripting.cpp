/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
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

#include "Scheduler/DeleteInstanceCommand.h"
#include "Scheduler/StoreScuCommand.h"
#include "Scheduler/StorePeerCommand.h"
#include "Scheduler/ModifyInstanceCommand.h"
#include "Scheduler/CallSystemCommand.h"
#include "OrthancRestApi/OrthancRestApi.h"

#include <glog/logging.h>
#include <EmbeddedResources.h>

static const char* RECEIVED_INSTANCE_FILTER = "ReceivedInstanceFilter";
static const char* ON_STORED_INSTANCE = "OnStoredInstance";


namespace Orthanc
{
  IServerCommand* LuaScripting::ParseOperation(const std::string& operation,
                                               const Json::Value& parameters)
  {
    if (operation == "delete")
    {
      LOG(INFO) << "Lua script to delete instance " << parameters["Instance"].asString();
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
      LOG(INFO) << "Lua script to send instance " << parameters["Instance"].asString()
                << " to modality " << modality << " using Store-SCU";
      return new StoreScuCommand(context_, localAet,
                                 Configuration::GetModalityUsingSymbolicName(modality), true);
    }

    if (operation == "store-peer")
    {
      std::string peer = parameters["Peer"].asString();
      LOG(INFO) << "Lua script to send instance " << parameters["Instance"].asString()
                << " to peer " << peer << " using HTTP";

      OrthancPeerParameters parameters;
      Configuration::GetOrthancPeer(parameters, peer);
      return new StorePeerCommand(context_, parameters, true);
    }

    if (operation == "modify")
    {
      LOG(INFO) << "Lua script to modify instance " << parameters["Instance"].asString();
      DicomModification modification;
      OrthancRestApi::ParseModifyRequest(modification, parameters);

      std::auto_ptr<ModifyInstanceCommand> command(new ModifyInstanceCommand(context_, modification));
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

      return new CallSystemCommand(context_, parameters["Command"].asString(), args);
    }

    throw OrthancException(ErrorCode_ParameterOutOfRange);
  }


  LuaScripting::LuaScripting(ServerContext& context) : context_(context)
  {
    lua_.Execute(Orthanc::EmbeddedResources::LUA_TOOLBOX);
    lua_.SetHttpProxy(Configuration::GetGlobalStringParameter("HttpProxy", ""));
  }


  void LuaScripting::ApplyOnStoredInstance(const std::string& instanceId,
                                           const Json::Value& simplifiedTags,
                                           const Json::Value& metadata,
                                           const std::string& remoteAet,
                                           const std::string& calledAet)
  {
    if (lua_.IsExistingFunction(ON_STORED_INSTANCE))
    {
      lua_.Execute("_InitializeJob()");

      LuaFunctionCall call(lua_, ON_STORED_INSTANCE);
      call.PushString(instanceId);
      call.PushJson(simplifiedTags);
      call.PushJson(metadata);
      call.PushJson(remoteAet);
      call.PushJson(calledAet);
      call.Execute();

      Json::Value operations;
      LuaFunctionCall call2(lua_, "_AccessJob");
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

        ServerCommandInstance& command = job.AddCommand(ParseOperation(operation, operations[i]));
        
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
      context_.GetScheduler().Submit(job);
    }
  }


  void LuaScripting::SignalStoredInstance(const std::string& publicId,
                                          DicomInstanceToStore& instance,
                                          const Json::Value& simplifiedTags)
  {
    boost::mutex::scoped_lock lock(mutex_);

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

    ApplyOnStoredInstance(publicId, simplifiedTags, metadata, 
                          instance.GetRemoteAet(), instance.GetCalledAet());
  }


  void LuaScripting::SignalChange(const ServerIndexChange& change)
  {
    boost::mutex::scoped_lock lock(mutex_);

    // TODO
  }


  bool LuaScripting::FilterIncomingInstance(const Json::Value& simplified,
                                            const std::string& remoteAet)
  {
    boost::mutex::scoped_lock lock(mutex_);

    if (lua_.IsExistingFunction(RECEIVED_INSTANCE_FILTER))
    {
      LuaFunctionCall call(lua_, RECEIVED_INSTANCE_FILTER);
      call.PushJson(simplified);
      call.PushString(remoteAet);

      if (!call.ExecutePredicate())
      {
        return false;
      }
    }

    return true;
  }
}
