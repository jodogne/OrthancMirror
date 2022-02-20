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
#include "HttpFileSender.h"

#include "../OrthancException.h"
#include "../Toolbox.h"
#include "../SystemToolbox.h"

#include <boost/lexical_cast.hpp>

namespace Orthanc
{
  void HttpFileSender::SetContentType(MimeType contentType)
  {
    contentType_ = EnumerationToString(contentType);
  }

  void HttpFileSender::SetContentType(const std::string &contentType)
  {
    contentType_ = contentType;
  }

  const std::string &HttpFileSender::GetContentType() const
  {
    return contentType_;
  }

  void HttpFileSender::SetContentFilename(const std::string& filename)
  {
    filename_ = filename;

    if (contentType_.empty())
    {
      contentType_ = SystemToolbox::AutodetectMimeType(filename);
    }
  }

  const std::string &HttpFileSender::GetContentFilename() const
  {
    return filename_;
  }

  HttpCompression HttpFileSender::SetupHttpCompression(bool, bool)
  {
    return HttpCompression_None;
  }


  bool HttpFileSender::HasContentFilename(std::string& filename)
  {
    if (filename_.empty())
    {
      return false;
    }
    else
    {
      filename = filename_;
      return true;
    }
  }
    
  std::string HttpFileSender::GetContentType()
  {
    if (contentType_.empty())
    {
      return MIME_BINARY;
    }
    else
    {
      return contentType_;
    }
  }
}
