/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012 Medical Physics Department, CHU of Liege,
 * Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include "EmbeddedResourceHttpHandler.h"

#include "../OrthancException.h"

#include <stdio.h>


namespace Orthanc
{
  EmbeddedResourceHttpHandler::EmbeddedResourceHttpHandler(
    const std::string& baseUri,
    EmbeddedResources::DirectoryResourceId resourceId)
  {
    Toolbox::SplitUriComponents(baseUri_, baseUri);
    resourceId_ = resourceId;
  }


  bool EmbeddedResourceHttpHandler::IsServedUri(const UriComponents& uri)
  {
    return Toolbox::IsChildUri(baseUri_, uri);
  }


  void EmbeddedResourceHttpHandler::Handle(
    HttpOutput& output,
    const std::string& method,
    const UriComponents& uri,
    const Arguments& headers,
    const Arguments& arguments,
    const std::string&)
  {
    if (method != "GET")
    {
      output.SendMethodNotAllowedError("GET");
      return;
    }

    std::string resourcePath = Toolbox::FlattenUri(uri, baseUri_.size());
    std::string contentType = Toolbox::AutodetectMimeType(resourcePath);

    try
    {
      const void* buffer = EmbeddedResources::GetDirectoryResourceBuffer(resourceId_, resourcePath.c_str());
      size_t size = EmbeddedResources::GetDirectoryResourceSize(resourceId_, resourcePath.c_str());
      output.AnswerBufferWithContentType(buffer, size, contentType);
    }
    catch (OrthancException& e)
    {
      output.SendHeader(Orthanc_HttpStatus_404_NotFound);
    }
  } 
}
