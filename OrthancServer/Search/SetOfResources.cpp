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
#include "SetOfResources.h"

#include "../../Core/OrthancException.h"


namespace Orthanc
{
  void SetOfResources::Intersect(const std::list<int64_t>& resources)
  {
    if (resources_.get() == NULL)
    {
      resources_.reset(new Resources);

      for (std::list<int64_t>::const_iterator
             it = resources.begin(); it != resources.end(); ++it)
      {
        resources_->insert(*it);
      }
    }
    else
    {
      std::auto_ptr<Resources> filtered(new Resources);

      for (std::list<int64_t>::const_iterator
             it = resources.begin(); it != resources.end(); ++it)
      {
        if (resources_->find(*it) != resources_->end())
        {
          filtered->insert(*it);
        }
      }

      resources_ = filtered;
    }
  }


  void SetOfResources::GoDown()
  {
    if (level_ == ResourceType_Instance)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    if (resources_.get() != NULL)
    {
      std::auto_ptr<Resources> children(new Resources);

      for (Resources::const_iterator it = resources_->begin(); 
           it != resources_->end(); ++it)
      {
        std::list<int64_t> tmp;
        database_.GetChildrenInternalId(tmp, *it);

        for (std::list<int64_t>::const_iterator
               child = tmp.begin(); child != tmp.end(); ++child)
        {
          children->insert(*child);
        }
      }

      resources_ = children;
    }

    switch (level_)
    {
      case ResourceType_Patient:
        level_ = ResourceType_Study;
        break;

      case ResourceType_Study:
        level_ = ResourceType_Series;
        break;

      case ResourceType_Series:
        level_ = ResourceType_Instance;
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }
  }


  void SetOfResources::Flatten(std::list<std::string>& result)
  {
    result.clear();
      
    if (resources_.get() == NULL)
    {
      // All the resources of this level are part of the filter
      database_.GetAllPublicIds(result, level_);
    }
    else
    {
      for (Resources::const_iterator it = resources_->begin(); 
           it != resources_->end(); ++it)
      {
        result.push_back(database_.GetPublicId(*it));
      }
    }
  }


  void SetOfResources::Flatten(std::list<int64_t>& result)
  {
    result.clear();
      
    if (resources_.get() == NULL)
    {
      // All the resources of this level are part of the filter
      database_.GetAllInternalIds(result, level_);
    }
    else
    {
      for (Resources::const_iterator it = resources_->begin(); 
           it != resources_->end(); ++it)
      {
        result.push_back(*it);
      }
    }
  }
}
