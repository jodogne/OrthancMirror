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
#include "HttpContentNegociation.h"

#include "../Logging.h"
#include "../OrthancException.h"
#include "../Toolbox.h"

#include <boost/lexical_cast.hpp>

namespace Orthanc
{
  HttpContentNegociation::Handler::Handler(const std::string& type,
                                           const std::string& subtype,
                                           IHandler& handler) :
    type_(type),
    subtype_(subtype),
    handler_(handler)
  {
  }


  bool HttpContentNegociation::Handler::IsMatch(const std::string& type,
                                                const std::string& subtype) const
  {
    if (type == "*" && subtype == "*")
    {
      return true;
    }
        
    if (subtype == "*" && type == type_)
    {
      return true;
    }

    return type == type_ && subtype == subtype_;
  }


  struct HttpContentNegociation::Reference : public boost::noncopyable
  {
    const Handler&  handler_;
    uint8_t         level_;
    float           quality_;

    Reference(const Handler& handler,
              const std::string& type,
              const std::string& subtype,
              float quality) :
      handler_(handler),
      quality_(quality)
    {
      if (type == "*" && subtype == "*")
      {
        level_ = 0;
      }
      else if (subtype == "*")
      {
        level_ = 1;
      }
      else
      {
        level_ = 2;
      }
    }
      
    bool operator< (const Reference& other) const
    {
      if (level_ < other.level_)
      {
        return true;
      }

      if (level_ > other.level_)
      {
        return false;
      }

      return quality_ < other.quality_;
    }
  };


  bool HttpContentNegociation::SplitPair(std::string& first /* out */,
                                         std::string& second /* out */,
                                         const std::string& source,
                                         char separator)
  {
    size_t pos = source.find(separator);

    if (pos == std::string::npos)
    {
      return false;
    }
    else
    {
      first = Toolbox::StripSpaces(source.substr(0, pos));
      second = Toolbox::StripSpaces(source.substr(pos + 1));
      return true;      
    }
  }


  float HttpContentNegociation::GetQuality(const Tokens& parameters)
  {
    for (size_t i = 1; i < parameters.size(); i++)
    {
      std::string key, value;
      if (SplitPair(key, value, parameters[i], '=') &&
          key == "q")
      {
        float quality;
        bool ok = false;

        try
        {
          quality = boost::lexical_cast<float>(value);
          ok = (quality >= 0.0f && quality <= 1.0f);
        }
        catch (boost::bad_lexical_cast&)
        {
        }

        if (ok)
        {
          return quality;
        }
        else
        {
          LOG(ERROR) << "Quality parameter out of range in a HTTP request (must be between 0 and 1): " << value;
          throw OrthancException(ErrorCode_BadRequest);
        }
      }
    }

    return 1.0f;  // Default quality
  }


  void HttpContentNegociation::SelectBestMatch(std::auto_ptr<Reference>& best,
                                               const Handler& handler,
                                               const std::string& type,
                                               const std::string& subtype,
                                               float quality)
  {
    std::auto_ptr<Reference> match(new Reference(handler, type, subtype, quality));

    if (best.get() == NULL ||
        *best < *match)
    {
      best = match;
    }
  }


  void HttpContentNegociation::Register(const std::string& mime,
                                        IHandler& handler)
  {
    std::string type, subtype;

    if (SplitPair(type, subtype, mime, '/') &&
        type != "*" &&
        subtype != "*")
    {
      handlers_.push_back(Handler(type, subtype, handler));
    }
    else
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }

    
  bool HttpContentNegociation::Apply(const HttpHeaders& headers)
  {
    HttpHeaders::const_iterator accept = headers.find("accept");
    if (accept != headers.end())
    {
      return Apply(accept->second);
    }
    else
    {
      return Apply("*/*");
    }
  }


  bool HttpContentNegociation::Apply(const std::string& accept)
  {
    // http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.1
    // https://en.wikipedia.org/wiki/Content_negotiation
    // http://www.newmediacampaigns.com/blog/browser-rest-http-accept-headers

    Tokens mediaRanges;
    Toolbox::TokenizeString(mediaRanges, accept, ',');

    std::auto_ptr<Reference> bestMatch;

    for (Tokens::const_iterator it = mediaRanges.begin();
         it != mediaRanges.end(); ++it)
    {
      Tokens parameters;
      Toolbox::TokenizeString(parameters, *it, ';');

      if (parameters.size() > 0)
      {
        float quality = GetQuality(parameters);

        std::string type, subtype;
        if (SplitPair(type, subtype, parameters[0], '/'))
        {
          for (Handlers::const_iterator it2 = handlers_.begin();
               it2 != handlers_.end(); ++it2)
          {
            if (it2->IsMatch(type, subtype))
            {
              SelectBestMatch(bestMatch, *it2, type, subtype, quality);
            }
          }
        }
      }
    }

    if (bestMatch.get() == NULL)  // No match was found
    {
      return false;
    }
    else
    {
      bestMatch->handler_.Call();
      return true;
    }
  }
}
