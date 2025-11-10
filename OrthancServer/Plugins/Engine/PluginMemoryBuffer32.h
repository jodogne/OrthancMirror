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


#pragma once

#if ORTHANC_ENABLE_PLUGINS != 1
#  error The plugin support is disabled
#endif

#include "../../../OrthancFramework/Sources/MallocMemoryBuffer.h"
#include "../Include/orthanc/OrthancCPlugin.h"

#include <json/value.h>

namespace Orthanc
{
  class PluginMemoryBuffer32 : public IMemoryBuffer
  {
  private:
    OrthancPluginMemoryBuffer  buffer_;

    void SanityCheck() const;

  public:
    PluginMemoryBuffer32();

    virtual ~PluginMemoryBuffer32() ORTHANC_OVERRIDE
    {
      Clear();
    }

    virtual void MoveToString(std::string& target) ORTHANC_OVERRIDE;

    virtual const void* GetData() const ORTHANC_OVERRIDE;

    virtual size_t GetSize() const ORTHANC_OVERRIDE;

    OrthancPluginMemoryBuffer* GetObject()
    {
      return &buffer_;
    }

    void Release(OrthancPluginMemoryBuffer* target);

    void Release(OrthancPluginMemoryBuffer64* target);

    void Clear();

    void Resize(size_t size);

    void Assign(const void* data,
                size_t size);

    void Assign(const std::string& data);

    void ToJsonObject(Json::Value& target) const;
  };
}
