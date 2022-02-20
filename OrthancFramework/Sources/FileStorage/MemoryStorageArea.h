/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include "IStorageArea.h"

#include "../Compatibility.h"  // For ORTHANC_OVERRIDE

#include <boost/thread/mutex.hpp>
#include <map>

namespace Orthanc
{
  class MemoryStorageArea : public IStorageArea
  {
  private:
    typedef std::map<std::string, std::string*>  Content;
    
    boost::mutex  mutex_;
    Content       content_;
    
  public:
    virtual ~MemoryStorageArea();
    
    virtual void Create(const std::string& uuid,
                        const void* content,
                        size_t size,
                        FileContentType type) ORTHANC_OVERRIDE;

    virtual IMemoryBuffer* Read(const std::string& uuid,
                                FileContentType type) ORTHANC_OVERRIDE;

    virtual IMemoryBuffer* ReadRange(const std::string& uuid,
                                     FileContentType type,
                                     uint64_t start /* inclusive */,
                                     uint64_t end /* exclusive */) ORTHANC_OVERRIDE;
    
    virtual bool HasReadRange() const ORTHANC_OVERRIDE;

    virtual void Remove(const std::string& uuid,
                        FileContentType type) ORTHANC_OVERRIDE;
  };
}
