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


#include "../PrecompiledHeaders.h"
#include "IHttpHandler.h"

#include "HttpOutput.h"
#include "HttpToolbox.h"
#include "StringHttpOutput.h"

static const char* LOCALHOST = "127.0.0.1";


namespace Orthanc
{
  HttpStatus IHttpHandler::SimpleGet(std::string& answerBody,
                                     HttpToolbox::Arguments* answerHeaders,
                                     IHttpHandler& handler,
                                     RequestOrigin origin,
                                     const std::string& uri,
                                     const HttpToolbox::Arguments& httpHeaders)
  {
    UriComponents curi;
    HttpToolbox::GetArguments getArguments;
    HttpToolbox::ParseGetQuery(curi, getArguments, uri.c_str());

    StringHttpOutput stream;
    HttpOutput http(stream, false /* no keep alive */);

    if (handler.Handle(http, origin, LOCALHOST, "", HttpMethod_Get, curi, 
                       httpHeaders, getArguments, NULL /* no body for GET */, 0))
    {
      stream.GetBody(answerBody);

      if (answerHeaders != NULL)
      {
        stream.GetHeaders(*answerHeaders, true /* convert key to lower case */);
      }
      
      return stream.GetStatus();
    }
    else
    {
      return HttpStatus_404_NotFound;
    }
  }


  static HttpStatus SimplePostOrPut(std::string& answerBody,
                                    HttpToolbox::Arguments* answerHeaders,
                                    IHttpHandler& handler,
                                    RequestOrigin origin,
                                    HttpMethod method,
                                    const std::string& uri,
                                    const void* bodyData,
                                    size_t bodySize,
                                    const HttpToolbox::Arguments& httpHeaders)
  {
    HttpToolbox::GetArguments getArguments;  // No GET argument for POST/PUT

    UriComponents curi;
    Toolbox::SplitUriComponents(curi, uri);

    StringHttpOutput stream;
    HttpOutput http(stream, false /* no keep alive */);

    if (handler.Handle(http, origin, LOCALHOST, "", method, curi, 
                       httpHeaders, getArguments, bodyData, bodySize))
    {
      stream.GetBody(answerBody);

      if (answerHeaders != NULL)
      {
        stream.GetHeaders(*answerHeaders, true /* convert key to lower case */);
      }
      
      return stream.GetStatus();
    }
    else
    {
      return HttpStatus_404_NotFound;
    }
  }


  HttpStatus IHttpHandler::SimplePost(std::string& answerBody,
                                      HttpToolbox::Arguments* answerHeaders,
                                      IHttpHandler& handler,
                                      RequestOrigin origin,
                                      const std::string& uri,
                                      const void* bodyData,
                                      size_t bodySize,
                                      const HttpToolbox::Arguments& httpHeaders)
  {
    return SimplePostOrPut(answerBody, answerHeaders, handler, origin, HttpMethod_Post, uri, bodyData, bodySize, httpHeaders);
  }


  HttpStatus IHttpHandler::SimplePut(std::string& answerBody,
                                     HttpToolbox::Arguments* answerHeaders,
                                     IHttpHandler& handler,
                                     RequestOrigin origin,
                                     const std::string& uri,
                                     const void* bodyData,
                                     size_t bodySize,
                                     const HttpToolbox::Arguments& httpHeaders)
  {
    return SimplePostOrPut(answerBody, answerHeaders, handler, origin, HttpMethod_Put, uri, bodyData, bodySize, httpHeaders);
  }


  HttpStatus IHttpHandler::SimpleDelete(HttpToolbox::Arguments* answerHeaders,
                                        IHttpHandler& handler,
                                        RequestOrigin origin,
                                        const std::string& uri,
                                        const HttpToolbox::Arguments& httpHeaders)
  {
    UriComponents curi;
    Toolbox::SplitUriComponents(curi, uri);

    HttpToolbox::GetArguments getArguments;  // No GET argument for DELETE

    StringHttpOutput stream;
    HttpOutput http(stream, false /* no keep alive */);

    if (handler.Handle(http, origin, LOCALHOST, "", HttpMethod_Delete, curi, 
                       httpHeaders, getArguments, NULL /* no body for DELETE */, 0))
    {
      if (answerHeaders != NULL)
      {
        stream.GetHeaders(*answerHeaders, true /* convert key to lower case */);
      }
      
      return stream.GetStatus();
    }
    else
    {
      return HttpStatus_404_NotFound;
    }
  }
}
