/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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


namespace Orthanc
{
  static void SplitGETNameValue(IHttpHandler::GetArguments& result,
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


  void HttpToolbox::ParseGetArguments(IHttpHandler::GetArguments& result, 
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
                                   IHttpHandler::GetArguments& getArguments, 
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

 
  std::string HttpToolbox::GetArgument(const IHttpHandler::Arguments& getArguments,
                                       const std::string& name,
                                       const std::string& defaultValue)
  {
    IHttpHandler::Arguments::const_iterator it = getArguments.find(name);
    if (it == getArguments.end())
    {
      return defaultValue;
    }
    else
    {
      return it->second;
    }
  }


  std::string HttpToolbox::GetArgument(const IHttpHandler::GetArguments& getArguments,
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



  void HttpToolbox::ParseCookies(IHttpHandler::Arguments& result, 
                                 const IHttpHandler::Arguments& httpHeaders)
  {
    result.clear();

    IHttpHandler::Arguments::const_iterator it = httpHeaders.find("cookie");
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


  void HttpToolbox::CompileGetArguments(IHttpHandler::Arguments& compiled,
                                        const IHttpHandler::GetArguments& source)
  {
    compiled.clear();

    for (size_t i = 0; i < source.size(); i++)
    {
      compiled[source[i].first] = source[i].second;
    }
  }
}
