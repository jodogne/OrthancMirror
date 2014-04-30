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

#include "../../Core/IDynamicObject.h"
#include "IServerFilter.h"

namespace Orthanc
{
  class ServerFilterInstance : public IDynamicObject
  {
    friend class ServerScheduler;

  public:
    class IListener
    {
    public:
      virtual ~IListener()
      {
      }

      virtual void SignalSuccess(const std::string& jobId) = 0;

      virtual void SignalFailure(const std::string& jobId) = 0;
    };

  private:
    typedef IServerFilter::ListOfStrings  ListOfStrings;

    IServerFilter *filter_;
    std::string jobId_;
    ListOfStrings inputs_;
    std::list<ServerFilterInstance*> next_;

    bool Execute(IListener& listener);

  public:
    ServerFilterInstance(IServerFilter *filter,
                         const std::string& jobId);

    virtual ~ServerFilterInstance();

    const std::string& GetJobId() const
    {
      return jobId_;
    }

    void AddInput(const std::string& input)
    {
      inputs_.push_back(input);
    }

    void ConnectNext(ServerFilterInstance& filter)
    {
      next_.push_back(&filter);
    }

    const std::list<ServerFilterInstance*>& GetNextFilters() const
    {
      return next_;
    }

    IServerFilter& GetFilter() const
    {
      return *filter_;
    }
  };
}
