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
#include "HttpToolbox.h"

#include <string.h>

#if (ORTHANC_ENABLE_MONGOOSE == 1 || ORTHANC_ENABLE_CIVETWEB == 1)
#  include "IHttpHandler.h"
#endif


namespace Orthanc
{
  static void SplitGETNameValue(HttpToolbox::GetArguments& result,
                                const char* start,
                                const char* end)
  {
    std::string name, value;
    
    const char* equal = strchr(start, '=');
    if (equal == NULL || equal >= end)
    {
      name = std::string(start, end - start);
      //value = "";
    }
    else
    {
      name = std::string(start, equal - start);
      value = std::string(equal + 1, end);
    }

    Toolbox::UrlDecode(name);
    Toolbox::UrlDecode(value);

    result.push_back(std::make_pair(name, value));
  }


  void HttpToolbox::ParseGetArguments(GetArguments& result, 
                                      const char* query)
  {
    const char* pos = query;

    while (pos != NULL)
    {
      const char* ampersand = strchr(pos, '&');
      if (ampersand)
      {
        SplitGETNameValue(result, pos, ampersand);
        pos = ampersand + 1;
      }
      else
      {
        // No more ampersand, this is the last argument
        SplitGETNameValue(result, pos, pos + strlen(pos));
        pos = NULL;
      }
    }
  }


  void  HttpToolbox::ParseGetQuery(UriComponents& uri,
                                   GetArguments& getArguments, 
                                   const char* query)
  {
    const char *questionMark = ::strchr(query, '?');
    if (questionMark == NULL)
    {
      // No question mark in the string
      Toolbox::SplitUriComponents(uri, query);
      getArguments.clear();
    }
    else
    {
      Toolbox::SplitUriComponents(uri, std::string(query, questionMark));
      HttpToolbox::ParseGetArguments(getArguments, questionMark + 1);
    }    
  }

 
  std::string HttpToolbox::GetArgument(const Arguments& getArguments,
                                       const std::string& name,
                                       const std::string& defaultValue)
  {
    Arguments::const_iterator it = getArguments.find(name);
    if (it == getArguments.end())
    {
      return defaultValue;
    }
    else
    {
      return it->second;
    }
  }


  std::string HttpToolbox::GetArgument(const GetArguments& getArguments,
                                       const std::string& name,
                                       const std::string& defaultValue)
  {
    for (size_t i = 0; i < getArguments.size(); i++)
    {
      if (getArguments[i].first == name)
      {
        return getArguments[i].second;
      }
    }

    return defaultValue;
  }



  void HttpToolbox::ParseCookies(Arguments& result, 
                                 const Arguments& httpHeaders)
  {
    result.clear();

    Arguments::const_iterator it = httpHeaders.find("cookie");
    if (it != httpHeaders.end())
    {
      const std::string& cookies = it->second;

      size_t pos = 0;
      while (pos != std::string::npos)
      {
        size_t nextSemicolon = cookies.find(";", pos);
        std::string cookie;

        if (nextSemicolon == std::string::npos)
        {
          cookie = cookies.substr(pos);
          pos = std::string::npos;
        }
        else
        {
          cookie = cookies.substr(pos, nextSemicolon - pos);
          pos = nextSemicolon + 1;
        }

        size_t equal = cookie.find("=");
        if (equal != std::string::npos)
        {
          std::string name = Toolbox::StripSpaces(cookie.substr(0, equal));
          std::string value = Toolbox::StripSpaces(cookie.substr(equal + 1));
          result[name] = value;
        }
      }
    }
  }


  void HttpToolbox::CompileGetArguments(Arguments& compiled,
                                        const GetArguments& source)
  {
    compiled.clear();

    for (size_t i = 0; i < source.size(); i++)
    {
      compiled[source[i].first] = source[i].second;
    }
  }



#if (ORTHANC_ENABLE_MONGOOSE == 1 || ORTHANC_ENABLE_CIVETWEB == 1)
  bool HttpToolbox::SimpleGet(std::string& result,
                              IHttpHandler& handler,
                              RequestOrigin origin,
                              const std::string& uri,
                              const Arguments& httpHeaders)
  {
    return IHttpHandler::SimpleGet(result, handler, origin, uri, httpHeaders);
  }

  bool HttpToolbox::SimplePost(std::string& result,
                               IHttpHandler& handler,
                               RequestOrigin origin,
                               const std::string& uri,
                               const void* bodyData,
                               size_t bodySize,
                               const Arguments& httpHeaders)
  {
    return IHttpHandler::SimplePost(result, handler, origin, uri, bodyData, bodySize, httpHeaders);
  }

  bool HttpToolbox::SimplePut(std::string& result,
                              IHttpHandler& handler,
                              RequestOrigin origin,
                              const std::string& uri,
                              const void* bodyData,
                              size_t bodySize,
                              const Arguments& httpHeaders)
  {
    return IHttpHandler::SimplePut(result, handler, origin, uri, bodyData, bodySize, httpHeaders);
  }

  bool HttpToolbox::SimpleDelete(IHttpHandler& handler,
                                 RequestOrigin origin,
                                 const std::string& uri,
                                 const Arguments& httpHeaders)
  {
    return IHttpHandler::SimpleDelete(handler, origin, uri, httpHeaders);
  }
#endif
}
