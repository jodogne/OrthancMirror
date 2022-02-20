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

#include "IHttpHandler.h"

#include <boost/shared_ptr.hpp>

namespace Orthanc
{
  class FilesystemHttpHandler : public IHttpHandler
  {
  private:
    // PImpl idiom to avoid the inclusion of boost::filesystem
    // throughout the software
    struct PImpl;
    boost::shared_ptr<PImpl> pimpl_;

    bool listDirectoryContent_;

  public:
    FilesystemHttpHandler(const std::string& baseUri,
                          const std::string& root);

    virtual bool CreateChunkedRequestReader(std::unique_ptr<IChunkedRequestReader>& target,
                                            RequestOrigin origin,
                                            const char* remoteIp,
                                            const char* username,
                                            HttpMethod method,
                                            const UriComponents& uri,
                                            const HttpToolbox::Arguments& headers) ORTHANC_OVERRIDE
    {
      return false;
    }

    virtual bool Handle(HttpOutput& output,
                        RequestOrigin origin,
                        const char* remoteIp,
                        const char* username,
                        HttpMethod method,
                        const UriComponents& uri,
                        const HttpToolbox::Arguments& headers,
                        const HttpToolbox::GetArguments& arguments,
                        const void* /*bodyData*/,
                        size_t /*bodySize*/) ORTHANC_OVERRIDE;

    bool IsListDirectoryContent() const
    {
      return listDirectoryContent_;
    }

    void SetListDirectoryContent(bool enabled)
    {
      listDirectoryContent_ = enabled;
    }
  };
}
