/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include "../../PrecompiledHeadersServer.h"
#include "SetOfResources.h"

#include "../../../../OrthancFramework/Sources/OrthancException.h"


namespace Orthanc
{
  namespace Compatibility
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
        std::unique_ptr<Resources> filtered(new Resources);

        for (std::list<int64_t>::const_iterator
               it = resources.begin(); it != resources.end(); ++it)
        {
          if (resources_->find(*it) != resources_->end())
          {
            filtered->insert(*it);
          }
        }

#if __cplusplus < 201103L
        resources_.reset(filtered.release());
#else
        resources_ = std::move(filtered);
#endif
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
        std::unique_ptr<Resources> children(new Resources);

        for (Resources::const_iterator it = resources_->begin(); 
             it != resources_->end(); ++it)
        {
          std::list<int64_t> tmp;
          transaction_.GetChildrenInternalId(tmp, *it);

          for (std::list<int64_t>::const_iterator
                 child = tmp.begin(); child != tmp.end(); ++child)
          {
            children->insert(*child);
          }
        }

#if __cplusplus < 201103L
        resources_.reset(children.release());
#else
        resources_ = std::move(children);
#endif
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
        transaction_.GetAllPublicIds(result, level_);
      }
      else
      {
        for (Resources::const_iterator it = resources_->begin(); 
             it != resources_->end(); ++it)
        {
          result.push_back(transaction_.GetPublicId(*it));
        }
      }
    }


    void SetOfResources::Flatten(ILookupResources& compatibility,
                                 std::list<int64_t>& result)
    {
      result.clear();
      
      if (resources_.get() == NULL)
      {
        // All the resources of this level are part of the filter
        compatibility.GetAllInternalIds(result, level_);
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
}
