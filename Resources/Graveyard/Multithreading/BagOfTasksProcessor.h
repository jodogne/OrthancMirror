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

#include "BagOfTasks.h"
#include "SharedMessageQueue.h"

#include <stdint.h>
#include <map>

namespace Orthanc
{
  class BagOfTasksProcessor : public boost::noncopyable
  {
  private:
    enum BagStatus
    {
      BagStatus_Running,
      BagStatus_Canceled,
      BagStatus_Failed
    };


    struct Bag
    {
      size_t    size_;
      size_t    done_;
      BagStatus status_;

      Bag() :
        size_(0),
        done_(0),
        status_(BagStatus_Failed)
      {
      }

      explicit Bag(size_t size) : 
        size_(size),
        done_(0),
        status_(BagStatus_Running)
      {
      }
    };

    class Task;


    typedef std::map<uint64_t, Bag>   Bags;
    typedef std::map<uint64_t, bool>  ExitStatus;

    SharedMessageQueue  queue_;

    boost::mutex  mutex_;
    uint64_t  countBags_;
    Bags bags_;
    std::vector<boost::thread*>   threads_;
    ExitStatus  exitStatus_;
    bool continue_;

    boost::condition_variable  bagFinished_;

    static void Worker(BagOfTasksProcessor* that);

    void Cancel(int64_t bag);

    bool Join(int64_t bag);

    float GetProgress(int64_t bag);

    void SignalProgress(Task& task,
                        Bag& bag);

  public:
    class Handle : public boost::noncopyable
    {
      friend class BagOfTasksProcessor;

    private:
      BagOfTasksProcessor&  that_;
      uint64_t              bag_;
      bool                  hasJoined_;
      bool                  status_;
 
      Handle(BagOfTasksProcessor&  that,
             uint64_t bag,
             bool empty) : 
        that_(that),
        bag_(bag),
        hasJoined_(empty)
      {
      }

    public:
      ~Handle()
      {
        Join();
      }

      void Cancel()
      {
        that_.Cancel(bag_);
      }

      bool Join();

      float GetProgress()
      {
        return that_.GetProgress(bag_);
      }
    };
  

    explicit BagOfTasksProcessor(size_t countThreads);

    ~BagOfTasksProcessor();

    Handle* Submit(BagOfTasks& tasks);
  };
}
