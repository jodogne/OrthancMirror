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
#include "PluginMemoryBuffer32.h"

#include "../../../OrthancFramework/Sources/OrthancException.h"
#include "../../../OrthancFramework/Sources/Toolbox.h"


namespace Orthanc
{
  void PluginMemoryBuffer32::Clear()
  {
    if (buffer_.size != 0)
    {
      ::free(buffer_.data);
    }
  }


  void PluginMemoryBuffer32::SanityCheck() const
  {
    if ((buffer_.data == NULL && buffer_.size != 0) ||
        (buffer_.data != NULL && buffer_.size == 0))
    {
      throw OrthancException(ErrorCode_Plugin);
    }
  }


  PluginMemoryBuffer32::PluginMemoryBuffer32()
  {
    buffer_.size = 0;
    buffer_.data = NULL;
  }


  void PluginMemoryBuffer32::MoveToString(std::string& target)
  {
    SanityCheck();

    target.resize(buffer_.size);

    if (buffer_.size != 0)
    {
      memcpy(&target[0], buffer_.data, buffer_.size);
    }

    Clear();
  }


  const void* PluginMemoryBuffer32::GetData() const
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


  size_t PluginMemoryBuffer32::GetSize() const
  {
    SanityCheck();
    return buffer_.size;
  }


  void PluginMemoryBuffer32::ToJsonObject(Json::Value& target) const
  {
    SanityCheck();

    if (!Toolbox::ReadJson(target, buffer_.data, buffer_.size) ||
        target.type() != Json::objectValue)
    {
      throw OrthancException(ErrorCode_Plugin, "The plugin has not provided a valid JSON object");
    }
  }
}
