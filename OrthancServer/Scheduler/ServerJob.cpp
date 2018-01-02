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
#include "ServerJob.h"

#include "../../Core/OrthancException.h"
#include "../../Core/SystemToolbox.h"
#include "../../Core/Toolbox.h"

namespace Orthanc
{
  void ServerJob::CheckOrdering()
  {
    std::map<ServerCommandInstance*, unsigned int> index;

    unsigned int count = 0;
    for (std::list<ServerCommandInstance*>::const_iterator
           it = filters_.begin(); it != filters_.end(); ++it)
    {
      index[*it] = count++;
    }

    for (std::list<ServerCommandInstance*>::const_iterator
           it = filters_.begin(); it != filters_.end(); ++it)
    {
      const std::list<ServerCommandInstance*>& nextCommands = (*it)->GetNextCommands();

      for (std::list<ServerCommandInstance*>::const_iterator
             next = nextCommands.begin(); next != nextCommands.end(); ++next)
      {
        if (index.find(*next) == index.end() ||
            index[*next] <= index[*it])
        {
          // You must reorder your calls to "ServerJob::AddCommand"
          throw OrthancException(ErrorCode_BadJobOrdering);
        }
      }
    }
  }


  size_t ServerJob::Submit(SharedMessageQueue& target,
                           ServerCommandInstance::IListener& listener)
  {
    if (submitted_)
    {
      // This job has already been submitted
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    CheckOrdering();

    size_t size = filters_.size();

    for (std::list<ServerCommandInstance*>::iterator 
           it = filters_.begin(); it != filters_.end(); ++it)
    {
      target.Enqueue(*it);
    }

    filters_.clear();
    submitted_ = true;

    return size;
  }


  ServerJob::ServerJob() :
    jobId_(SystemToolbox::GenerateUuid()),
    submitted_(false),
    description_("no description")
  {
  }


  ServerJob::~ServerJob()
  {
    for (std::list<ServerCommandInstance*>::iterator
           it = filters_.begin(); it != filters_.end(); ++it)
    {
      delete *it;
    }

    for (std::list<IDynamicObject*>::iterator
           it = payloads_.begin(); it != payloads_.end(); ++it)
    {
      delete *it;
    }
  }


  ServerCommandInstance& ServerJob::AddCommand(IServerCommand* filter)
  {
    if (submitted_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    filters_.push_back(new ServerCommandInstance(filter, jobId_));
      
    return *filters_.back();
  }


  IDynamicObject& ServerJob::AddPayload(IDynamicObject* payload)
  {
    if (submitted_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    payloads_.push_back(payload);
      
    return *filters_.back();
  }

}
