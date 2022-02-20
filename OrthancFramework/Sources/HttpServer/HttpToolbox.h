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

#include "../Compatibility.h"
#include "../OrthancFramework.h"
#include "../Toolbox.h"

#include <boost/noncopyable.hpp>
#include <map>
#include <vector>

namespace Orthanc
{
  class IHttpHandler;
  
  class ORTHANC_PUBLIC HttpToolbox : public boost::noncopyable
  {
  public:
    typedef std::map<std::string, std::string>                  Arguments;
    typedef std::vector< std::pair<std::string, std::string> >  GetArguments;

    static void ParseGetArguments(GetArguments& result, 
                                  const char* query);

    static void ParseGetQuery(UriComponents& uri,
                              GetArguments& getArguments, 
                              const char* query);

    static std::string GetArgument(const Arguments& getArguments,
                                   const std::string& name,
                                   const std::string& defaultValue);

    static std::string GetArgument(const GetArguments& getArguments,
                                   const std::string& name,
                                   const std::string& defaultValue);

    static void ParseCookies(Arguments& result, 
                             const Arguments& httpHeaders);

    static void CompileGetArguments(Arguments& compiled,
                                    const GetArguments& source);

#if (ORTHANC_ENABLE_MONGOOSE == 1 || ORTHANC_ENABLE_CIVETWEB == 1)
    ORTHANC_DEPRECATED(static bool SimpleGet(std::string& result,
                                             IHttpHandler& handler,
                                             RequestOrigin origin,
                                             const std::string& uri,
                                             const Arguments& httpHeaders));

    ORTHANC_DEPRECATED(static bool SimplePost(std::string& result,
                                              IHttpHandler& handler,
                                              RequestOrigin origin,
                                              const std::string& uri,
                                              const void* bodyData,
                                              size_t bodySize,
                                              const Arguments& httpHeaders));

    ORTHANC_DEPRECATED(static bool SimplePut(std::string& result,
                                             IHttpHandler& handler,
                                             RequestOrigin origin,
                                             const std::string& uri,
                                             const void* bodyData,
                                             size_t bodySize,
                                             const Arguments& httpHeaders));

    ORTHANC_DEPRECATED(static bool SimpleDelete(IHttpHandler& handler,
                                                RequestOrigin origin,
                                                const std::string& uri,
                                                const Arguments& httpHeaders));
#endif
  };
}
