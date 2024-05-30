/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2024 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2024 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
    else if (subtype == "*" && type == type_)
    {
      return true;
    }
    else
    {
      return type == type_ && subtype == subtype_;
    }
  }


  class HttpContentNegociation::Reference : public boost::noncopyable
  {
  private:
    const Handler&  handler_;
    uint8_t         level_;
    float           quality_;
    Dictionary      parameters_;

    static float GetQuality(const Dictionary& parameters)
    {
      Dictionary::const_iterator found = parameters.find("q");

      if (found != parameters.end())
      {
        float quality;
        bool ok = false;

        try
        {
          quality = boost::lexical_cast<float>(found->second);
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
          throw OrthancException(
            ErrorCode_BadRequest,
            "Quality parameter out of range in a HTTP request (must be between 0 and 1): " + found->second);
        }
      }
      else
      {
        return 1.0f;  // Default quality
      }
    }

  public:
    Reference(const Handler& handler,
              const std::string& type,
              const std::string& subtype,
              const Dictionary& parameters) :
      handler_(handler),
      quality_(GetQuality(parameters)),
      parameters_(parameters)
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

    void Call() const
    {
      handler_.Call(parameters_);
    }
      
    bool operator< (const Reference& other) const
    {
      if (level_ < other.level_)
      {
        return true;
      }
      else if (level_ > other.level_)
      {
        return false;
      }
      else
      {
        return quality_ < other.quality_;
      }
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


  void HttpContentNegociation::SelectBestMatch(std::unique_ptr<Reference>& target,
                                               const Handler& handler,
                                               const std::string& type,
                                               const std::string& subtype,
                                               const Dictionary& parameters)
  {
    std::unique_ptr<Reference> match(new Reference(handler, type, subtype, parameters));

    if (target.get() == NULL ||
        *target < *match)
    {
#if __cplusplus < 201103L
      target.reset(match.release());
#else
      target = std::move(match);
#endif
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

    
  bool HttpContentNegociation::Apply(const Dictionary& headers)
  {
    Dictionary::const_iterator accept = headers.find("accept");
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

    std::unique_ptr<Reference> bestMatch;

    for (Tokens::const_iterator it = mediaRanges.begin();
         it != mediaRanges.end(); ++it)
    {
      Tokens tokens;
      Toolbox::TokenizeString(tokens, *it, ';');

      if (tokens.size() > 0)
      {
        Dictionary parameters;
        for (size_t i = 1; i < tokens.size(); i++)
        {
          std::string key, value;
          
          if (SplitPair(key, value, tokens[i], '='))
          {
            // Remove the enclosing quotes, if present
            if (!value.empty() &&
                value[0] == '"' &&
                value[value.size() - 1] == '"')
            {
              value = value.substr(1, value.size() - 2);
            }
          }
          else
          {
            key = Toolbox::StripSpaces(tokens[i]);
            value = "";
          }

          parameters[key] = value;
        }
        
        std::string type, subtype;
        if (SplitPair(type, subtype, tokens[0], '/'))
        {
          for (Handlers::const_iterator it2 = handlers_.begin();
               it2 != handlers_.end(); ++it2)
          {
            if (it2->IsMatch(type, subtype))
            {
              SelectBestMatch(bestMatch, *it2, type, subtype, parameters);
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
      bestMatch->Call();
      return true;
    }
  }
}
