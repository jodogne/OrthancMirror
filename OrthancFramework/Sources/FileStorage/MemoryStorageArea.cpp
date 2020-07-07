/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 **/


#include "../PrecompiledHeaders.h"
#include "MemoryStorageArea.h"

#include "../OrthancException.h"
#include "../Logging.h"

namespace Orthanc
{
  MemoryStorageArea::~MemoryStorageArea()
  {
    for (Content::iterator it = content_.begin(); it != content_.end(); ++it)
    {
      if (it->second != NULL)
      {
        delete it->second;
      }
    }
  }
    
  void MemoryStorageArea::Create(const std::string& uuid,
                                 const void* content,
                                 size_t size,
                                 FileContentType type)
  {
    LOG(INFO) << "Creating attachment \"" << uuid << "\" of \"" << static_cast<int>(type)
              << "\" type (size: " << (size / (1024 * 1024) + 1) << "MB)";

    boost::mutex::scoped_lock lock(mutex_);

    if (size != 0 &&
        content == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
    else if (content_.find(uuid) != content_.end())
    {
      throw OrthancException(ErrorCode_InternalError);
    }
    else
    {
      content_[uuid] = new std::string(reinterpret_cast<const char*>(content), size);
    }
  }

  
  void MemoryStorageArea::Read(std::string& content,
                               const std::string& uuid,
                               FileContentType type)
  {
    LOG(INFO) << "Reading attachment \"" << uuid << "\" of \""
              << static_cast<int>(type) << "\" content type";

    boost::mutex::scoped_lock lock(mutex_);

    Content::const_iterator found = content_.find(uuid);

    if (found == content_.end())
    {
      throw OrthancException(ErrorCode_InexistentFile);
    }
    else if (found->second == NULL)
    {
      throw OrthancException(ErrorCode_InternalError);
    }
    else
    {
      content.assign(*found->second);
    }
  }
      

  void MemoryStorageArea::Remove(const std::string& uuid,
                                 FileContentType type)
  {
    LOG(INFO) << "Deleting attachment \"" << uuid << "\" of type " << static_cast<int>(type);

    boost::mutex::scoped_lock lock(mutex_);

    Content::iterator found = content_.find(uuid);
    
    if (found == content_.end())
    {
      // Ignore second removal
    }
    else if (found->second == NULL)
    {
      throw OrthancException(ErrorCode_InternalError);
    }
    else
    {
      delete found->second;
      content_.erase(found);
    }
  }
}
