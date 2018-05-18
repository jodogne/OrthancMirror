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


#include "../PrecompiledHeadersServer.h"
#include "LuaJobManager.h"


namespace Orthanc
{
  void LuaJobManager::ConnectionTimeoutThread(LuaJobManager* manager)
  {
    while (manager->continue_)
    {
      manager->connectionManager_.CheckTimeout();
      boost::this_thread::sleep(boost::posix_time::milliseconds(100));
    }
  }

    
  void LuaJobManager::SignalDone(const SequenceOfOperationsJob& job)
  {
    boost::mutex::scoped_lock lock(mutex_);

    if (&job == currentJob_)
    {
      currentId_.clear();
      currentJob_ = NULL;
    }
  }


  LuaJobManager::LuaJobManager(JobsEngine&  engine) :
    engine_(engine),
    currentJob_(NULL),
    maxOperations_(1000),
    priority_(0),
    continue_(true)
  {
    connectionTimeoutThread_ = boost::thread(ConnectionTimeoutThread, this);
  }


  LuaJobManager::~LuaJobManager()
  {
    continue_ = false;

    if (connectionTimeoutThread_.joinable())
    {
      connectionTimeoutThread_.join();
    }
  }


  void LuaJobManager::SetMaxOperationsPerJob(size_t count)
  {
    boost::mutex::scoped_lock lock(mutex_);
    maxOperations_ = count;
  }


  void LuaJobManager::SetPriority(int priority)
  {
    boost::mutex::scoped_lock lock(mutex_);
    priority_ = priority;
  }


  void LuaJobManager::SetTrailingOperationTimeout(unsigned int timeout)
  {
    boost::mutex::scoped_lock lock(mutex_);
    trailingTimeout_ = timeout;
  }


  LuaJobManager::Lock* LuaJobManager::Modify()
  {
    boost::mutex::scoped_lock lock(mutex_);

    if (currentJob_ != NULL)
    {
      std::auto_ptr<Lock> result(new Lock(*currentJob_));

      if (!result->IsDone() &&
          result->GetOperationsCount() < maxOperations_)
      {
        return result.release();
      }
    }

    // Need to create a new job, as the previous one is either
    // finished, or is getting too long
    currentJob_ = new SequenceOfOperationsJob;

    engine_.GetRegistry().Submit(currentId_, currentJob_, priority_);

    std::auto_ptr<Lock> result(new Lock(*currentJob_));
    result->SetTrailingOperationTimeout(trailingTimeout_);

    return result.release();
  }
}
