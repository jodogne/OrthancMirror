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

#include "../../../OrthancFramework/Sources/DicomNetworking/TimeoutDicomConnectionManager.h"
#include "../../../OrthancFramework/Sources/DicomParsing/DicomModification.h"
#include "../../../OrthancFramework/Sources/JobsEngine/JobsEngine.h"
#include "../../../OrthancFramework/Sources/JobsEngine/Operations/SequenceOfOperationsJob.h"
#include "../../../OrthancFramework/Sources/WebServiceParameters.h"

namespace Orthanc
{
  class ServerContext;
  
  class LuaJobManager : private SequenceOfOperationsJob::IObserver
  {
  private:
    boost::mutex              mutex_;
    std::string               currentId_;
    SequenceOfOperationsJob*  currentJob_;
    size_t                    maxOperations_;
    int                       priority_;
    unsigned int              trailingTimeout_;
    TimeoutDicomConnectionManager  connectionManager_;

    virtual void SignalDone(const SequenceOfOperationsJob& job) ORTHANC_OVERRIDE;

  public:
    LuaJobManager();

    void SetMaxOperationsPerJob(size_t count);

    void SetPriority(int priority);

    void SetTrailingOperationTimeout(unsigned int timeout);

    void AwakeTrailingSleep();

    TimeoutDicomConnectionManager& GetDicomConnectionManager()
    {
      return connectionManager_;
    }

    class Lock : public boost::noncopyable
    {
    private:
      LuaJobManager&                                  that_;
      boost::mutex::scoped_lock                       lock_;
      JobsEngine&                                     engine_;
      std::unique_ptr<SequenceOfOperationsJob::Lock>  jobLock_;
      bool                                            isNewJob_;

    public:
      Lock(LuaJobManager& that,
           JobsEngine& engine);

      ~Lock();

      size_t AddLogOperation();

      size_t AddDeleteResourceOperation(ServerContext& context);

      size_t AddStoreScuOperation(ServerContext& context,
                                  const std::string& localAet,
                                  const RemoteModalityParameters& modality);

      size_t AddStorePeerOperation(const WebServiceParameters& peer);

      size_t AddSystemCallOperation(const std::string& command);

      size_t AddSystemCallOperation(const std::string& command,
                                    const std::vector<std::string>& preArguments,
                                    const std::vector<std::string>& postArguments);

      size_t AddModifyInstanceOperation(ServerContext& context,
                                        DicomModification* modification);

      void AddNullInput(size_t operation); 

      void AddStringInput(size_t operation,
                          const std::string& content);

      void AddDicomInstanceInput(size_t operation,
                                 ServerContext& context,
                                 const std::string& instanceId);

      void Connect(size_t operation1,
                   size_t operation2);
    };
  };
}
