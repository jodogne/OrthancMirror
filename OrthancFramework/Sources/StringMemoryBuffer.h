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


#pragma once

#include "IMemoryBuffer.h"
#include "Compatibility.h"

namespace Orthanc
{
  class StringMemoryBuffer : public IMemoryBuffer
  {
  private:
    std::string   buffer_;

  public:
    void Copy(const std::string& buffer)
    {
      buffer_ = buffer;
    }

    void Swap(std::string& buffer)
    {
      buffer_.swap(buffer);
    }

    virtual void MoveToString(std::string& target) ORTHANC_OVERRIDE;

    virtual const void* GetData() const ORTHANC_OVERRIDE
    {
      return (buffer_.empty() ? NULL : buffer_.c_str());
    }

    virtual size_t GetSize() const ORTHANC_OVERRIDE
    {
      return buffer_.size();
    }

    static IMemoryBuffer* CreateFromSwap(std::string& buffer);

    static IMemoryBuffer* CreateFromCopy(const std::string& buffer);
  };
}
