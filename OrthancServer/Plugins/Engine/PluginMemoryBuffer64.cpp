/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "../../Sources/PrecompiledHeadersServer.h"
#include "PluginMemoryBuffer64.h"

#include "../../../OrthancFramework/Sources/OrthancException.h"


namespace Orthanc
{
  void PluginMemoryBuffer64::Clear()
  {
    if (buffer_.size != 0)
    {
      ::free(buffer_.data);
    }

    buffer_.data = NULL;
    buffer_.size = 0;
  }


  void PluginMemoryBuffer64::SanityCheck() const
  {
    if ((buffer_.data == NULL && buffer_.size != 0) ||
        (buffer_.data != NULL && buffer_.size == 0))
    {
      throw OrthancException(ErrorCode_Plugin);
    }
  }


  PluginMemoryBuffer64::PluginMemoryBuffer64()
  {
    buffer_.size = 0;
    buffer_.data = NULL;
  }


  void PluginMemoryBuffer64::MoveToString(std::string& target)
  {
    SanityCheck();

    target.resize(buffer_.size);

    if (buffer_.size != 0)
    {
      memcpy(&target[0], buffer_.data, buffer_.size);
    }

    Clear();
  }


  const void* PluginMemoryBuffer64::GetData() const
  {
    SanityCheck();

    if (buffer_.size == 0)
    {
      return NULL;
    }
    else
    {
      return buffer_.data;
    }
  }


  size_t PluginMemoryBuffer64::GetSize() const
  {
    SanityCheck();
    return buffer_.size;
  }


  void PluginMemoryBuffer64::Release(OrthancPluginMemoryBuffer64* target)
  {
    SanityCheck();

    if (target == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }

    target->data = buffer_.data;
    target->size = buffer_.size;

    buffer_.data = NULL;
    buffer_.size = 0;
  }


  void PluginMemoryBuffer64::Resize(size_t size)
  {
    Clear();

    if (size == 0)
    {
      buffer_.data = NULL;
    }
    else
    {
      buffer_.data = ::malloc(size);

      if (buffer_.data == NULL)
      {
        throw OrthancException(ErrorCode_NotEnoughMemory);
      }
    }

    buffer_.size = size;
  }


  void PluginMemoryBuffer64::Assign(const void* data,
                                    size_t size)
  {
    Resize(size);

    if (size != 0)
    {
      memcpy(buffer_.data, data, size);
    }
  }


  void PluginMemoryBuffer64::Assign(const std::string& data)
  {
    if (data.empty())
    {
      Assign(NULL, 0);
    }
    else
    {
      Assign(data.c_str(), data.size());
    }
  }
}
