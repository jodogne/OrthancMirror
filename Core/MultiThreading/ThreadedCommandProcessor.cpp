/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2014 Medical Physics Department, CHU of Liege,
 * Belgium
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


#include "ThreadedCommandProcessor.h"

#include "../OrthancException.h"

namespace Orthanc
{
  static const int32_t TIMEOUT = 10;


  void ThreadedCommandProcessor::Processor(ThreadedCommandProcessor* that)
  {
    while (!that->done_)
    {
      std::auto_ptr<IDynamicObject> command(that->queue_.Dequeue(TIMEOUT));

      if (command.get() != NULL)
      {
        bool success = false;

        try
        {
          if (that->success_)
          {
            // No command has failed so far

            if (that->cancel_)
            {
              // The commands have been canceled. Skip the execution
              // of this command, yet mark it as succeeded.
              success = true;
            }
            else
            {
              success = dynamic_cast<ICommand&>(*command).Execute();
            }
          }
          else
          {
            // A command has already failed. Skip the execution of this command.
          }
        }
        catch (OrthancException)
        {
        }

        {
          boost::mutex::scoped_lock lock(that->mutex_);
          assert(that->remainingCommands_ > 0);
          that->remainingCommands_--;

          if (!success)
          {
            if (!that->cancel_ && that->listener_ && that->success_)
            {
              // This is the first command that fails
              that->listener_->SignalFailure();
            }

            that->success_ = false;
          }
          else
          {
            if (!that->cancel_ && that->listener_)
            {
              if (that->remainingCommands_ == 0)
              {
                that->listener_->SignalSuccess(that->totalCommands_);
              }
              else
              {
                that->listener_->SignalProgress(that->totalCommands_ - that->remainingCommands_,
                                                that->totalCommands_);
              }
            }
          }

          that->processedCommand_.notify_all();
        }
      }
    }
  }


  ThreadedCommandProcessor::ThreadedCommandProcessor(unsigned int numThreads)
  {
    if (numThreads < 1)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    listener_ = NULL;
    success_ = true;
    done_ = false;
    cancel_ = false;
    threads_.resize(numThreads);
    remainingCommands_ = 0;
    totalCommands_ = 0;

    for (unsigned int i = 0; i < numThreads; i++)
    {
      threads_[i] = new boost::thread(Processor, this);
    }
  }


  ThreadedCommandProcessor::~ThreadedCommandProcessor()
  {
    done_ = true;
      
    for (unsigned int i = 0; i < threads_.size(); i++)
    {
      boost::thread* t = threads_[i];

      if (t != NULL)
      {
        if (t->joinable())
        {
          t->join();
        }

        delete t;
      }
    }
  }


  void ThreadedCommandProcessor::Post(ICommand* command)
  {
    boost::mutex::scoped_lock lock(mutex_);
    queue_.Enqueue(command);
    remainingCommands_++;
    totalCommands_++;
  }


  bool ThreadedCommandProcessor::Join()
  {
    boost::mutex::scoped_lock lock(mutex_);

    while (!remainingCommands_ == 0)
    {
      processedCommand_.wait(lock);
    }

    if (cancel_ && listener_)
    {
      listener_->SignalCancel();
    }

    // Reset the sequence counters for subsequent commands
    bool hasSucceeded = success_;
    success_ = true;
    totalCommands_ = 0;
    cancel_ = false;

    return hasSucceeded;
  }


  void ThreadedCommandProcessor::Cancel()
  {
    boost::mutex::scoped_lock lock(mutex_);

    cancel_ = true;
  }


  void ThreadedCommandProcessor::SetListener(IListener& listener)
  {
    boost::mutex::scoped_lock lock(mutex_);
    listener_ = &listener;
  }
}
