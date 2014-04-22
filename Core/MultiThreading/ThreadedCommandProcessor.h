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


#pragma once

#include "../ICommand.h"

#include "SharedMessageQueue.h"

namespace Orthanc
{
  class ThreadedCommandProcessor
  {
  public:
    class IListener
    {
    public:
      virtual ~IListener()
      {
      }

      virtual void SignalProgress(unsigned int current,
                                  unsigned int total) = 0;

      virtual void SignalSuccess(unsigned int total) = 0;

      virtual void SignalFailure() = 0;

      virtual void SignalCancel() = 0;
    };

  private:
    SharedMessageQueue  queue_;
    bool done_;
    bool cancel_;
    std::vector<boost::thread*>  threads_;
    IListener* listener_;

    boost::mutex mutex_;
    bool success_;
    unsigned int remainingCommands_, totalCommands_;
    boost::condition_variable processedCommand_;

    static void Processor(ThreadedCommandProcessor* that);

  public:
    ThreadedCommandProcessor(unsigned int numThreads);

    ~ThreadedCommandProcessor();

    // This takes the ownership of the command
    void Post(ICommand* command);

    bool Join();

    void Cancel();

    void SetListener(IListener& listener);

    IListener& GetListener() const
    {
      return *listener_;
    }
  };
}
