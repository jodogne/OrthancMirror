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


#include "../PrecompiledHeaders.h"
#include "BagOfRunnablesBySteps.h"

#include "../Logging.h"

#include <stack>
#include <boost/thread.hpp>

namespace Orthanc
{
  struct BagOfRunnablesBySteps::PImpl
  {
    bool continue_;
    bool stopFinishListener_;

    boost::mutex mutex_;
    boost::condition_variable oneThreadIsStopped_;
    boost::condition_variable oneThreadIsJoined_;

    // The list of threads that are waiting to be joined.
    typedef std::stack<IRunnableBySteps*>  StoppedThreads;
    StoppedThreads  stoppedThreads_;

    // The set of active runnables, i.e. the runnables that have not
    // finished their job yet, plus the runnables that have not been
    // joined yet.
    typedef std::map<IRunnableBySteps*, boost::thread*>  ActiveThreads;
    ActiveThreads  activeThreads_;

    // The thread that joins the runnables after they stop
    std::auto_ptr<boost::thread> finishListener_;
  };



  void BagOfRunnablesBySteps::RunnableThread(BagOfRunnablesBySteps* bag,
                                             IRunnableBySteps* runnable)
  {
    while (bag->pimpl_->continue_)
    {
      if (!runnable->Step())
      {
        break;
      }
    }

    {
      // Register this runnable as having stopped
      boost::mutex::scoped_lock lock(bag->pimpl_->mutex_);
      bag->pimpl_->stoppedThreads_.push(runnable);
      bag->pimpl_->oneThreadIsStopped_.notify_one();
    }
  }

  
  void BagOfRunnablesBySteps::FinishListener(BagOfRunnablesBySteps* bag)
  {
    boost::mutex::scoped_lock lock(bag->pimpl_->mutex_);

    while (!bag->pimpl_->stopFinishListener_)
    {
      while (!bag->pimpl_->stoppedThreads_.empty())
      {
        std::auto_ptr<IRunnableBySteps> r(bag->pimpl_->stoppedThreads_.top());
        bag->pimpl_->stoppedThreads_.pop();

        assert(r.get() != NULL);
        assert(bag->pimpl_->activeThreads_.find(r.get()) != bag->pimpl_->activeThreads_.end());

        std::auto_ptr<boost::thread> t(bag->pimpl_->activeThreads_[r.get()]);
        bag->pimpl_->activeThreads_.erase(r.get());

        assert(t.get() != NULL);
        assert(bag->pimpl_->activeThreads_.find(r.get()) == bag->pimpl_->activeThreads_.end());

        if (t->joinable())
        {
          t->join();
        }

        bag->pimpl_->oneThreadIsJoined_.notify_one();
      }

      bag->pimpl_->oneThreadIsStopped_.wait(lock);
    }
  }


  BagOfRunnablesBySteps::BagOfRunnablesBySteps() : pimpl_(new PImpl)
  {
    pimpl_->continue_ = true;
    pimpl_->stopFinishListener_ = false;

    // Everyting is set up, the finish listener can be started
    pimpl_->finishListener_.reset(new boost::thread(FinishListener, this));
  }


  BagOfRunnablesBySteps::~BagOfRunnablesBySteps()
  {
    if (!pimpl_->stopFinishListener_)
    {
      LOG(ERROR) << "INTERNAL ERROR: BagOfRunnablesBySteps::Finalize() should be invoked manually to avoid mess in the destruction order!";
      Finalize();
    }
  }


  void BagOfRunnablesBySteps::Add(IRunnableBySteps* runnable)
  {
    // Make sure the runnable is deleted is something goes wrong
    std::auto_ptr<IRunnableBySteps> runnableRabi(runnable);

    boost::mutex::scoped_lock lock(pimpl_->mutex_);
    boost::thread* t(new boost::thread(RunnableThread, this, runnable));

    pimpl_->activeThreads_.insert(std::make_pair(runnableRabi.release(), t));
  }


  void BagOfRunnablesBySteps::StopAll()
  {
    boost::mutex::scoped_lock lock(pimpl_->mutex_);
    pimpl_->continue_ = false;

    while (pimpl_->activeThreads_.size() > 0)
    {
      pimpl_->oneThreadIsJoined_.wait(lock);
    }

    pimpl_->continue_ = true;
  }



  void BagOfRunnablesBySteps::Finalize()
  {
    if (!pimpl_->stopFinishListener_)
    {
      StopAll();

      // Stop the finish listener
      pimpl_->stopFinishListener_ = true;
      pimpl_->oneThreadIsStopped_.notify_one();  // Awakens the listener

      if (pimpl_->finishListener_->joinable())
      {
        pimpl_->finishListener_->join();
      }
    }
  }

}
