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


#pragma once

#include "ServerJob.h"

#include "../../Core/MultiThreading/Semaphore.h"

namespace Orthanc
{
  class ServerScheduler : public ServerCommandInstance::IListener
  {
  private:
    struct JobInfo
    {
      bool watched_;
      bool cancel_;
      size_t size_;
      size_t success_;
      size_t failures_;
      std::string description_;
    };

    enum JobStatus
    {
      JobStatus_Running = 1,
      JobStatus_Success = 2,
      JobStatus_Failure = 3
    };

    typedef IServerCommand::ListOfStrings  ListOfStrings;
    typedef std::map<std::string, JobInfo> Jobs;

    boost::mutex mutex_;
    boost::condition_variable watchedJobFinished_;
    Jobs jobs_;
    SharedMessageQueue queue_;
    bool finish_;
    boost::thread worker_;
    std::map<std::string, JobStatus> watchedJobStatus_;
    Semaphore availableJob_;

    JobInfo& GetJobInfo(const std::string& jobId);

    virtual void SignalSuccess(const std::string& jobId);

    virtual void SignalFailure(const std::string& jobId);

    static void Worker(ServerScheduler* that);

    void SubmitInternal(ServerJob& job,
                        bool watched);

  public:
    explicit ServerScheduler(unsigned int maxjobs);

    ~ServerScheduler();

    void Stop();

    void Submit(ServerJob& job);

    bool SubmitAndWait(ListOfStrings& outputs,
                       ServerJob& job);

    bool SubmitAndWait(ServerJob& job);

    bool IsRunning(const std::string& jobId);

    void Cancel(const std::string& jobId);

    // Returns a number between 0 and 1
    float GetProgress(const std::string& jobId);

    bool IsRunning(const ServerJob& job)
    {
      return IsRunning(job.GetId());
    }

    void Cancel(const ServerJob& job) 
    {
      Cancel(job.GetId());
    }

    float GetProgress(const ServerJob& job) 
    {
      return GetProgress(job.GetId());
    }

    void GetListOfJobs(ListOfStrings& jobs);
  };
}
