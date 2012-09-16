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


#include "HttpHandler.h"

#include <string.h>

namespace Orthanc
{
  static void SplitGETNameValue(HttpHandler::Arguments& result,
                                const char* start,
                                const char* end)
  {
    const char* equal = strchr(start, '=');
    if (equal == NULL || equal >= end)
    {
      result.insert(std::make_pair(std::string(start, end - start), ""));
    }
    else
    {
      result.insert(std::make_pair(std::string(start, equal - start),
                                   std::string(equal + 1, end)));
    }
  }


  void HttpHandler::ParseGetQuery(HttpHandler::Arguments& result, const char* query)
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



  std::string HttpHandler::GetArgument(const Arguments& arguments,
                                       const std::string& name,
                                       const std::string& defaultValue)
  {
    Arguments::const_iterator it = arguments.find(name);
    if (it == arguments.end())
    {
      return defaultValue;
    }
    else
    {
      return it->second;
    }
  }
}
