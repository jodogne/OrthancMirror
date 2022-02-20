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

#include "IWebDavBucket.h"

#include <boost/thread/recursive_mutex.hpp>

namespace Orthanc
{
  class WebDavStorage : public IWebDavBucket
  {
  private:
    class StorageFile;
    class StorageFolder;
    
    StorageFolder* LookupParentFolder(const std::vector<std::string>& path);

    boost::shared_ptr<StorageFolder>  root_;  // PImpl
    boost::recursive_mutex            mutex_;
    bool                              isMemory_;

  public:
    explicit WebDavStorage(bool isMemory);
  
    virtual bool IsExistingFolder(const std::vector<std::string>& path) ORTHANC_OVERRIDE;

    virtual bool ListCollection(Collection& collection,
                                const std::vector<std::string>& path) ORTHANC_OVERRIDE;

    virtual bool GetFileContent(MimeType& mime,
                                std::string& content,
                                boost::posix_time::ptime& modificationTime, 
                                const std::vector<std::string>& path) ORTHANC_OVERRIDE;
  
    virtual bool StoreFile(const std::string& content,
                           const std::vector<std::string>& path) ORTHANC_OVERRIDE;

    virtual bool CreateFolder(const std::vector<std::string>& path) ORTHANC_OVERRIDE;

    virtual bool DeleteItem(const std::vector<std::string>& path) ORTHANC_OVERRIDE;

    virtual void Start() ORTHANC_OVERRIDE
    {
    }

    virtual void Stop() ORTHANC_OVERRIDE
    {
    }

    void RemoveEmptyFolders();
  };
}
