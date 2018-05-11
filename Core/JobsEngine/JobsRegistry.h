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

#if !defined(ORTHANC_SANDBOXED)
#  error The macro ORTHANC_SANDBOXED must be defined
#endif

#if ORTHANC_SANDBOXED == 1
#  error The job engine cannot be used in sandboxed environments
#endif

#include "JobInfo.h"

#include <list>
#include <set>
#include <queue>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>

namespace Orthanc
{
  // This class handles the state machine of the jobs engine
  class JobsRegistry : public boost::noncopyable
  {
  private:
    class JobHandler;

    struct PriorityComparator
    {
      bool operator() (JobHandler*& a,
                       JobHandler*& b) const;
    };

    typedef std::map<std::string, JobHandler*>              JobsIndex;
    typedef std::list<JobHandler*>                          CompletedJobs;
    typedef std::set<JobHandler*>                           RetryJobs;
    typedef std::priority_queue<JobHandler*, 
                                std::vector<JobHandler*>,   // Could be a "std::deque"
                                PriorityComparator>         PendingJobs;

    boost::mutex               mutex_;
    JobsIndex                  jobsIndex_;
    PendingJobs                pendingJobs_;
    CompletedJobs              completedJobs_;
    RetryJobs                  retryJobs_;

    boost::condition_variable  pendingJobAvailable_;
    boost::condition_variable  someJobComplete_;
    size_t                     maxCompletedJobs_;


#ifndef NDEBUG
    bool IsPendingJob(const JobHandler& job) const;

    bool IsCompletedJob(JobHandler& job) const;
    
    bool IsRetryJob(JobHandler& job) const;
#endif

    void CheckInvariants() const;

    void ForgetOldCompletedJobs();

    void SetCompletedJob(JobHandler& job,
                         bool success);
    
    void MarkRunningAsCompleted(JobHandler& job,
                                bool success);

    void MarkRunningAsRetry(JobHandler& job,
                            unsigned int timeout);
    
    void MarkRunningAsPaused(JobHandler& job);
    
    bool GetStateInternal(JobState& state,
                          const std::string& id);

    void RemovePendingJob(const std::string& id);
      
    void RemoveRetryJob(JobHandler* handler);
      
  public:
    JobsRegistry() :
      maxCompletedJobs_(10)
    {
    }


    ~JobsRegistry();

    void SetMaxCompletedJobs(size_t i);
    
    void ListJobs(std::set<std::string>& target);

    bool GetJobInfo(JobInfo& target,
                    const std::string& id);
    
    void Submit(std::string& id,
                IJob* job,        // Takes ownership
                int priority);
    
    void Submit(IJob* job,        // Takes ownership
                int priority);

    bool SubmitAndWait(IJob* job,        // Takes ownership
                       int priority);
    
    bool SetPriority(const std::string& id,
                     int priority);

    bool Pause(const std::string& id);
    
    bool Resume(const std::string& id);

    bool Resubmit(const std::string& id);

    bool Cancel(const std::string& id);
    
    void ScheduleRetries();
    
    bool GetState(JobState& state,
                  const std::string& id);

    class RunningJob : public boost::noncopyable
    {
    private:
      JobsRegistry&  registry_;
      JobHandler*    handler_;  // Can only be accessed if the
                                // registry mutex is locked!
      IJob*          job_;  // Will by design be in mutual exclusion,
                            // because only one RunningJob can be
                            // executed at a time on a JobHandler

      std::string    id_;
      int            priority_;
      JobState       targetState_;
      unsigned int   targetRetryTimeout_;
      bool           canceled_;
      
    public:
      RunningJob(JobsRegistry& registry,
                 unsigned int timeout);

      ~RunningJob();

      bool IsValid() const;

      const std::string& GetId() const;

      int GetPriority() const;

      IJob& GetJob();

      bool IsPauseScheduled();

      bool IsCancelScheduled();

      void MarkSuccess();

      void MarkFailure();

      void MarkPause();

      void MarkCanceled();

      void MarkRetry(unsigned int timeout);

      void UpdateStatus(ErrorCode code);
    };
  };
}
