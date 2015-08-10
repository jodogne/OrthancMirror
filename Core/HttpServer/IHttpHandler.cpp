/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
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
#include "IHttpHandler.h"

#include "../Toolbox.h"

namespace Orthanc
{
  void IHttpHandler::GetAcceptedCompressions(std::set<HttpCompression>& result,
                                             const Arguments& headers)
  {
    // Look if the client wishes HTTP compression
    // https://en.wikipedia.org/wiki/HTTP_compression
    Arguments::const_iterator it = headers.find("accept-encoding");
    if (it != headers.end())
    {
      std::vector<std::string> encodings;
      Toolbox::TokenizeString(encodings, it->second, ',');
      for (size_t i = 0; i < encodings.size(); i++)
      {
        std::string s = Toolbox::StripSpaces(encodings[i]);

        if (s == "deflate")
        {
          result.insert(HttpCompression_Deflate);
        }
        else if (s == "gzip")
        {
          result.insert(HttpCompression_Gzip);
        }
      }
    }
  }


  HttpCompression IHttpHandler::GetPreferredCompression(const Arguments& headers,
                                                        size_t bodySize)
  {
#if 0
    // TODO
    if (bodySize < 1024)
    {
      // Do not compress small answers
      return HttpCompression_None;
    }
#endif

    HttpCompression result = HttpCompression_None;

    Arguments::const_iterator it = headers.find("accept-encoding");
    if (it != headers.end())
    {
      std::vector<std::string> encodings;
      Toolbox::TokenizeString(encodings, it->second, ',');
      for (size_t i = 0; i < encodings.size(); i++)
      {
        std::string s = Toolbox::StripSpaces(encodings[i]);

        if (s == "deflate" &&
            result != HttpCompression_Gzip)  // Always prefer "gzip" over "deflate"
        {
          result = HttpCompression_Deflate;
        }
        else if (s == "gzip")
        {
          result = HttpCompression_Gzip;
        }
      }
    }

    return result;
  }
}
