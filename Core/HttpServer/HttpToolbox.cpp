/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * In addition, as a special exception, the copyright holders of this
 * program give permission to link the code of its release with the
 * OpenSSL project's "OpenSSL" library (or with modified versions of it
 * that use the same license as the "OpenSSL" library), and distribute
 * the linked executables. You must obey the GNU General Public License
 * in all respects for all of the code used other than "OpenSSL". If you
 * modify file(s) with this exception, you may extend this exception to
 * your version of the file(s), but you are not obligated to do so. If
 * you do not wish to do so, delete this exception statement from your
 * version. If you delete this exception statement from all source files
 * in the program, then also delete it here.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include "../PrecompiledHeaders.h"
#include "HttpToolbox.h"

#include <stdio.h>
#include <string.h>
#include <iostream>

#include "HttpOutput.h"
#include "StringHttpOutput.h"


static const char* LOCALHOST = "127.0.0.1";



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


  bool HttpToolbox::SimpleGet(std::string& result,
                              IHttpHandler& handler,
                              RequestOrigin origin,
                              const std::string& uri,
                              const IHttpHandler::Arguments& httpHeaders)
  {
    UriComponents curi;
    IHttpHandler::GetArguments getArguments;
    ParseGetQuery(curi, getArguments, uri.c_str());

    StringHttpOutput stream;
    HttpOutput http(stream, false /* no keep alive */);

    if (handler.Handle(http, origin, LOCALHOST, "", HttpMethod_Get, curi, 
                       httpHeaders, getArguments, NULL /* no body for GET */, 0))
    {
      stream.GetOutput(result);
      return true;
    }
    else
    {
      return false;
    }
  }


  bool HttpToolbox::SimpleGet(std::string& result,
                              IHttpHandler& handler,
                              RequestOrigin origin,
                              const std::string& uri)
  {
    IHttpHandler::Arguments headers;  // No HTTP header
    return SimpleGet(result, handler, origin, uri, headers);
  }


  static bool SimplePostOrPut(std::string& result,
                              IHttpHandler& handler,
                              RequestOrigin origin,
                              HttpMethod method,
                              const std::string& uri,
                              const char* bodyData,
                              size_t bodySize)
  {
    IHttpHandler::Arguments headers;  // No HTTP header
    IHttpHandler::GetArguments getArguments;  // No GET argument for POST/PUT

    UriComponents curi;
    Toolbox::SplitUriComponents(curi, uri);

    StringHttpOutput stream;
    HttpOutput http(stream, false /* no keep alive */);

    if (handler.Handle(http, origin, LOCALHOST, "", method, curi, 
                       headers, getArguments, bodyData, bodySize))
    {
      stream.GetOutput(result);
      return true;
    }
    else
    {
      return false;
    }
  }


  bool HttpToolbox::SimplePost(std::string& result,
                               IHttpHandler& handler,
                               RequestOrigin origin,
                               const std::string& uri,
                               const char* bodyData,
                               size_t bodySize)
  {
    return SimplePostOrPut(result, handler, origin, HttpMethod_Post, uri, bodyData, bodySize);
  }


  bool HttpToolbox::SimplePut(std::string& result,
                              IHttpHandler& handler,
                              RequestOrigin origin,
                              const std::string& uri,
                              const char* bodyData,
                              size_t bodySize)
  {
    return SimplePostOrPut(result, handler, origin, HttpMethod_Put, uri, bodyData, bodySize);
  }


  bool HttpToolbox::SimpleDelete(IHttpHandler& handler,
                                 RequestOrigin origin,
                                 const std::string& uri)
  {
    UriComponents curi;
    Toolbox::SplitUriComponents(curi, uri);

    IHttpHandler::Arguments headers;  // No HTTP header
    IHttpHandler::GetArguments getArguments;  // No GET argument for DELETE

    StringHttpOutput stream;
    HttpOutput http(stream, false /* no keep alive */);

    return handler.Handle(http, origin, LOCALHOST, "", HttpMethod_Delete, curi, 
                          headers, getArguments, NULL /* no body for DELETE */, 0);
  }
}
