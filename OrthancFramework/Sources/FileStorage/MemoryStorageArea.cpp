/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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

#include "../Logging.h"
#include "../OrthancException.h"
#include "../StringMemoryBuffer.h"

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

  
  IMemoryBuffer* MemoryStorageArea::Read(const std::string& uuid,
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
      return StringMemoryBuffer::CreateFromCopy(*found->second);
    }
  }
      

  IMemoryBuffer* MemoryStorageArea::ReadRange(const std::string& uuid,
                                              FileContentType type,
                                              uint64_t start /* inclusive */,
                                              uint64_t end /* exclusive */)
  {
    LOG(INFO) << "Reading attachment \"" << uuid << "\" of \""
              << static_cast<int>(type) << "\" content type "
              << "(range from " << start << " to " << end << ")";

    if (start > end)
    {
      throw OrthancException(ErrorCode_BadRange);
    }
    else if (start == end)
    {
      return new StringMemoryBuffer;
    }
    else
    {
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
      else if (end > found->second->size())
      {
        throw OrthancException(ErrorCode_BadRange);
      }
      else
      {
        std::string range;
        range.resize(end - start);
        assert(!range.empty());

        memcpy(&range[0], &found->second[start], range.size());
        
        return StringMemoryBuffer::CreateFromSwap(range);
      }
    }
  }


  bool MemoryStorageArea::HasReadRange() const
  {
    return true;
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
