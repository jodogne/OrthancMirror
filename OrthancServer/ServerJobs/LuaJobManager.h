/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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

#include "../../Core/DicomParsing/DicomModification.h"
#include "../../Core/JobsEngine/JobsEngine.h"
#include "../../Core/JobsEngine/Operations/SequenceOfOperationsJob.h"
#include "../../Core/WebServiceParameters.h"

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
    unsigned int              dicomTimeout_;

    virtual void SignalDone(const SequenceOfOperationsJob& job);

  public:
    LuaJobManager();

    void SetMaxOperationsPerJob(size_t count);

    void SetPriority(int priority);

    void SetTrailingOperationTimeout(unsigned int timeout);

    void AwakeTrailingSleep();

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

      size_t AddStoreScuOperation(const std::string& localAet,
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
