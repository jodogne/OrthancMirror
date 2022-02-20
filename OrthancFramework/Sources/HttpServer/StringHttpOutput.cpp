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
#include "StringHttpOutput.h"

#include "../OrthancException.h"
#include "../Toolbox.h"

namespace Orthanc
{
  StringHttpOutput::StringHttpOutput() :
    status_(HttpStatus_404_NotFound),
    validBody_(true),
    validHeaders_(true)
  {
  }


  void StringHttpOutput::Send(bool isHeader, const void* buffer, size_t length)
  {
    if (isHeader)
    {
      if (validHeaders_)
      {
        headers_.AddChunk(buffer, length);
      }
      else
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
    }
    else
    {
      if (validBody_)
      {
        body_.AddChunk(buffer, length);
      }
      else
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
    }
  }

  
  void StringHttpOutput::GetBody(std::string& output)
  {
    if (!validBody_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else if (status_ == HttpStatus_200_Ok)
    {
      body_.Flatten(output);
      validBody_ = false;
    }
    else
    {
      throw OrthancException(ErrorCode_UnknownResource);
    }
  }


  void StringHttpOutput::GetHeaders(std::map<std::string, std::string>& target,
                                    bool keyToLowerCase)
  {
    if (!validHeaders_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      std::string s;
      headers_.Flatten(s);
      validHeaders_ = false;

      std::vector<std::string> lines;
      Orthanc::Toolbox::TokenizeString(lines, s, '\n');

      target.clear();

      for (size_t i = 1 /* skip the HTTP status line */; i < lines.size(); i++)
      {
        size_t colon = lines[i].find(':');
        if (colon != std::string::npos)
        {
          std::string key = lines[i].substr(0, colon);

          if (keyToLowerCase)
          {
            Toolbox::ToLowerCase(key);
          }
          
          const std::string value = lines[i].substr(colon + 1);
          target[Toolbox::StripSpaces(key)] = Toolbox::StripSpaces(value);
        }
      }
    }
  }
}
