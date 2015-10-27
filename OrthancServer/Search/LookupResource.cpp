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


#include "../PrecompiledHeadersServer.h"
#include "LookupResource.h"

#include "../../Core/OrthancException.h"

namespace Orthanc
{
  LookupResource::Level::Level(ResourceType level)
  {
    const DicomTag* tags = NULL;
    size_t size;
    
    LookupIdentifierQuery::LoadIdentifiers(tags, size, level);
    
    for (size_t i = 0; i < size; i++)
    {
      identifiers_.insert(tags[i]);
    }
    
    DicomMap::LoadMainDicomTags(tags, size, level);
    
    for (size_t i = 0; i < size; i++)
    {
      if (identifiers_.find(tags[i]) == identifiers_.end())
      {
        mainTags_.insert(tags[i]);
      }
    }    
  }

  LookupResource::Level::~Level()
  {
    for (Constraints::iterator it = mainTagsConstraints_.begin();
         it != mainTagsConstraints_.end(); ++it)
    {
      delete *it;
    }

    for (Constraints::iterator it = identifiersConstraints_.begin();
         it != identifiersConstraints_.end(); ++it)
    {
      delete *it;
    }
  }

  bool LookupResource::Level::Add(std::auto_ptr<IFindConstraint>& constraint)
  {
    if (identifiers_.find(constraint->GetTag()) != identifiers_.end())
    {
      identifiersConstraints_.push_back(constraint.release());
      return true;
    }
    else if (mainTags_.find(constraint->GetTag()) != mainTags_.end())
    {
      mainTagsConstraints_.push_back(constraint.release());
      return true;
    }
    else
    {
      return false;
    }
  }


  LookupResource::LookupResource(ResourceType level) : level_(level)
  {
    switch (level)
    {
      case ResourceType_Patient:
        levels_[ResourceType_Patient] = new Level(ResourceType_Patient);
        break;

      case ResourceType_Study:
        levels_[ResourceType_Study] = new Level(ResourceType_Study);
        // Do not add "break" here

      case ResourceType_Series:
        levels_[ResourceType_Series] = new Level(ResourceType_Series);
        // Do not add "break" here

      case ResourceType_Instance:
        levels_[ResourceType_Instance] = new Level(ResourceType_Instance);
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }
  }


  LookupResource::~LookupResource()
  {
    for (Levels::iterator it = levels_.begin();
         it != levels_.end(); ++it)
    {
      delete it->second;
    }

    for (Constraints::iterator it = unoptimizedConstraints_.begin();
         it != unoptimizedConstraints_.end(); ++it)
    {
      delete *it;
    }    
  }



  bool LookupResource::AddInternal(ResourceType level,
                                   std::auto_ptr<IFindConstraint>& constraint)
  {
    Levels::iterator it = levels_.find(level);
    if (it != levels_.end())
    {
      if (it->second->Add(constraint))
      {
        return true;
      }
    }

    return false;
  }


  void LookupResource::Add(IFindConstraint* constraint)
  {
    std::auto_ptr<IFindConstraint> c(constraint);

    if (!AddInternal(ResourceType_Patient, c) &&
        !AddInternal(ResourceType_Study, c) &&
        !AddInternal(ResourceType_Series, c) &&
        !AddInternal(ResourceType_Instance, c))
    {
      unoptimizedConstraints_.push_back(c.release());
    }
  }


  static int64_t ChooseOneInstance(IDatabaseWrapper& database,
                                   int64_t parent,
                                   ResourceType type)
  {
    for (;;)
    {
      if (type == ResourceType_Instance)
      {
        return parent;
      }

      std::list<int64_t>  children;
      database.GetChildrenInternalId(children, parent);

      if (children.empty())
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      parent = children.front();
      type = GetChildResourceType(type);
    }
  }

}
