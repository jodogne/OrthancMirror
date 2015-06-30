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


#pragma once

#include "IServerListener.h"
#include "../Core/Lua/LuaContext.h"
#include "Scheduler/IServerCommand.h"

namespace Orthanc
{
  class ServerContext;

  class LuaScripting : public IServerListener
  {
  private:
    void ApplyOnStoredInstance(const std::string& instanceId,
                               const Json::Value& simplifiedDicom,
                               const Json::Value& metadata,
                               const std::string& remoteAet,
                               const std::string& calledAet);

    IServerCommand* ParseOperation(const std::string& operation,
                                   const Json::Value& parameters);

    void InitializeJob();

    void SubmitJob(const std::string& description);

    void OnStableResource(const ServerIndexChange& change);

    boost::mutex    mutex_;
    LuaContext      lua_;
    ServerContext&  context_;

  public:
    class Locker : public boost::noncopyable
    {
    private:
      LuaScripting& that_;

    public:
      Locker(LuaScripting& that) : that_(that)
      {
        that.mutex_.lock();
      }

      ~Locker()
      {
        that_.mutex_.unlock();
      }

      LuaContext& GetLua()
      {
        return that_.lua_;
      }
    };

    LuaScripting(ServerContext& context);

    virtual void SignalStoredInstance(const std::string& publicId,
                                      DicomInstanceToStore& instance,
                                      const Json::Value& simplifiedTags);

    virtual void SignalChange(const ServerIndexChange& change);

    virtual bool FilterIncomingInstance(const Json::Value& simplified,
                                        const std::string& remoteAet);
  };
}
