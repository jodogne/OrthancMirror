/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "PrecompiledHeadersServer.h"
#include "EmbeddedResourceHttpHandler.h"

#include "../../OrthancFramework/Sources/HttpServer/HttpOutput.h"
#include "../../OrthancFramework/Sources/Logging.h"
#include "../../OrthancFramework/Sources/OrthancException.h"
#include "../../OrthancFramework/Sources/SystemToolbox.h"


namespace Orthanc
{
  EmbeddedResourceHttpHandler::EmbeddedResourceHttpHandler(
    const std::string& baseUri,
    ServerResources::DirectoryResourceId resourceId)
  {
    Toolbox::SplitUriComponents(baseUri_, baseUri);
    resourceId_ = resourceId;
  }


  bool EmbeddedResourceHttpHandler::Handle(
    HttpOutput& output,
    RequestOrigin /*origin*/,
    const char* /*remoteIp*/,
    const char* /*username*/,
    HttpMethod method,
    const UriComponents& uri,
    const HttpToolbox::Arguments& headers,
    const HttpToolbox::GetArguments& arguments,
    const void* /*bodyData*/,
    size_t /*bodySize*/)
  {
    if (!Toolbox::IsChildUri(baseUri_, uri))
    {
      // This URI is not served by this handler
      return false;
    }

    if (method != HttpMethod_Get)
    {
      output.SendMethodNotAllowed("GET");
      return true;
    }

    std::string resourcePath = Toolbox::FlattenUri(uri, baseUri_.size());
    MimeType contentType = SystemToolbox::AutodetectMimeType(resourcePath);

    try
    {
      const void* buffer = ServerResources::GetDirectoryResourceBuffer(resourceId_, resourcePath.c_str());
      size_t size = ServerResources::GetDirectoryResourceSize(resourceId_, resourcePath.c_str());

      output.SetContentType(contentType);
      output.Answer(buffer, size);
    }
    catch (OrthancException&)
    {
      LOG(WARNING) << "Unable to find HTTP resource: " << resourcePath;
      output.SendStatus(HttpStatus_404_NotFound);
    }

    return true;
  } 
}
