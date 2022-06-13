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


#pragma once

#include "DicomInstanceToStore.h"
#include "ServerIndexChange.h"
#include "ServerJobs/LuaJobManager.h"

#include "../../OrthancFramework/Sources/MultiThreading/SharedMessageQueue.h"
#include "../../OrthancFramework/Sources/Lua/LuaContext.h"

namespace Orthanc
{
  class ServerContext;

  class LuaScripting : public boost::noncopyable
  {
  private:
    enum State
    {
      State_Setup,
      State_Running,
      State_Done
    };
    
    class ExecuteEvent;
    class IEvent;
    class OnStoredInstanceEvent;
    class StableResourceEvent;
    class JobEvent;
    class DeleteEvent;
    class UpdateEvent;

    static ServerContext* GetServerContext(lua_State *state);

    static int RestApiPostOrPut(lua_State *state,
                                bool isPost);
    static int RestApiGet(lua_State *state);
    static int RestApiPost(lua_State *state);
    static int RestApiPut(lua_State *state);
    static int RestApiDelete(lua_State *state);
    static int GetOrthancConfiguration(lua_State *state);

    size_t ParseOperation(LuaJobManager::Lock& lock,
                          const std::string& operation,
                          const Json::Value& parameters);

    void InitializeJob();

    void SubmitJob();

    boost::recursive_mutex   mutex_;
    LuaContext               lua_;
    ServerContext&           context_;
    LuaJobManager            jobManager_;
    State                    state_;
    boost::thread            eventThread_;
    boost::thread            heartBeatThread_;
    unsigned int             heartBeatPeriod_;
    SharedMessageQueue       pendingEvents_;

    static void EventThread(LuaScripting* that);

    static void HeartBeatThread(LuaScripting* that);

    void LoadGlobalConfiguration();

  public:
    class Lock : public boost::noncopyable
    {
    private:
      LuaScripting&                        that_;
      boost::recursive_mutex::scoped_lock  lock_;

    public:
      explicit Lock(LuaScripting& that) : 
        that_(that), 
        lock_(that.mutex_)
      {
      }

      LuaContext& GetLua()
      {
        return that_.lua_;
      }
    };

    explicit LuaScripting(ServerContext& context);

    ~LuaScripting();

    void Start();

    void Stop();
    
    void SignalStoredInstance(const std::string& publicId,
                              const DicomInstanceToStore& instance,
                              const Json::Value& simplifiedTags);

    void SignalChange(const ServerIndexChange& change);

    bool FilterIncomingInstance(const DicomInstanceToStore& instance,
                                const Json::Value& simplifiedTags);

    bool FilterIncomingCStoreInstance(uint16_t& dimseStatus,
                                      const DicomInstanceToStore& instance,
                                      const Json::Value& simplified);

    void Execute(const std::string& command);

    void SignalJobSubmitted(const std::string& jobId);

    void SignalJobSuccess(const std::string& jobId);

    void SignalJobFailure(const std::string& jobId);

    TimeoutDicomConnectionManager& GetDicomConnectionManager()
    {
      return jobManager_.GetDicomConnectionManager();
    }
  };
}
