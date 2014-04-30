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


#include "ServerJob.h"

#include "../../Core/OrthancException.h"
#include "../../Core/Toolbox.h"
#include "../../Core/Uuid.h"

namespace Orthanc
{
  void ServerJob::CheckOrdering()
  {
    std::map<ServerFilterInstance*, unsigned int> index;

    unsigned int count = 0;
    for (std::list<ServerFilterInstance*>::const_iterator
           it = filters_.begin(); it != filters_.end(); it++)
    {
      index[*it] = count++;
    }

    for (std::list<ServerFilterInstance*>::const_iterator
           it = filters_.begin(); it != filters_.end(); it++)
    {
      const std::list<ServerFilterInstance*>& nextFilters = (*it)->GetNextFilters();

      for (std::list<ServerFilterInstance*>::const_iterator
             next = nextFilters.begin(); next != nextFilters.end(); next++)
      {
        if (index.find(*next) == index.end() ||
            index[*next] <= index[*it])
        {
          // You must reorder your calls to "ServerJob::AddFilter"
          throw OrthancException("Bad ordering of filters in a job");
        }
      }
    }
  }


  size_t ServerJob::Submit(SharedMessageQueue& target,
                           ServerFilterInstance::IListener& listener)
  {
    if (submitted_)
    {
      // This job has already been submitted
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    CheckOrdering();

    size_t size = filters_.size();

    for (std::list<ServerFilterInstance*>::iterator 
           it = filters_.begin(); it != filters_.end(); it++)
    {
      target.Enqueue(*it);
    }

    filters_.clear();
    submitted_ = true;

    return size;
  }


  ServerJob::ServerJob()
  {
    jobId_ = Toolbox::GenerateUuid();      
    submitted_ = false;
    description_ = "no description";
  }


  ServerJob::~ServerJob()
  {
    for (std::list<ServerFilterInstance*>::iterator
           it = filters_.begin(); it != filters_.end(); it++)
    {
      delete *it;
    }
  }


  ServerFilterInstance& ServerJob::AddFilter(IServerFilter* filter)
  {
    if (submitted_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    filters_.push_back(new ServerFilterInstance(filter, jobId_));
      
    return *filters_.back();
  }
}
