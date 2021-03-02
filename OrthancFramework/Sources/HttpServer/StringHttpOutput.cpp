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
#include "StringHttpOutput.h"

#include "../OrthancException.h"

namespace Orthanc
{
  void StringHttpOutput::OnHttpStatusReceived(HttpStatus status)
  {
    switch (status)
    {
      case HttpStatus_200_Ok:
        found_ = true;
        break;

      case HttpStatus_404_NotFound:
        found_ = false;
        break;

      default:
        throw OrthancException(ErrorCode_BadRequest);
    }
  }

  void StringHttpOutput::Send(bool isHeader, const void* buffer, size_t length)
  {
    if (!isHeader)
    {
      buffer_.AddChunk(buffer, length);
    }
  }

  void StringHttpOutput::GetOutput(std::string& output)
  {
    if (found_)
    {
      buffer_.Flatten(output);
    }
    else
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }
  }
}
