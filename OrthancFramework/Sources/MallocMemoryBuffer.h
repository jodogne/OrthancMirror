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
  class MallocMemoryBuffer : public IMemoryBuffer
  {
  public:
    typedef void (*FreeFunction) (void* buffer);
    
  private:
    void*         buffer_;
    size_t        size_;
    FreeFunction  free_;

  public:
    MallocMemoryBuffer();

    virtual ~MallocMemoryBuffer()
    {
      Clear();
    }

    void Clear();

    void Assign(void* buffer,
                size_t size,
                FreeFunction freeFunction);
    
    virtual void MoveToString(std::string& target) ORTHANC_OVERRIDE;

    virtual const void* GetData() const ORTHANC_OVERRIDE
    {
      return buffer_;
    }

    virtual size_t GetSize() const ORTHANC_OVERRIDE
    {
      return size_;
    }
  };
}
