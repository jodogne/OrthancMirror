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
#include "EmbeddedResourceHttpHandler.h"

#include "../Logging.h"
#include "../OrthancException.h"
#include "HttpOutput.h"

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


  bool EmbeddedResourceHttpHandler::Handle(
    HttpOutput& output,
    RequestOrigin /*origin*/,
    const char* /*remoteIp*/,
    const char* /*username*/,
    HttpMethod method,
    const UriComponents& uri,
    const Arguments& headers,
    const GetArguments& arguments,
    const char* /*bodyData*/,
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
    std::string contentType = Toolbox::AutodetectMimeType(resourcePath);

    try
    {
      const void* buffer = EmbeddedResources::GetDirectoryResourceBuffer(resourceId_, resourcePath.c_str());
      size_t size = EmbeddedResources::GetDirectoryResourceSize(resourceId_, resourcePath.c_str());

      output.SetContentType(contentType.c_str());
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
