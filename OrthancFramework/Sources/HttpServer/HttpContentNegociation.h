/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#pragma once

#include "../OrthancFramework.h"
#include "../Compatibility.h"

#include <memory>
#include <boost/noncopyable.hpp>
#include <map>
#include <list>
#include <string>
#include <vector>
#include <stdint.h>


namespace Orthanc
{
  class ORTHANC_PUBLIC HttpContentNegociation : public boost::noncopyable
  {
  public:
    typedef std::map<std::string, std::string>  Dictionary;

    class IHandler : public boost::noncopyable
    {
    public:
      virtual ~IHandler()
      {
      }

      virtual void Handle(const std::string& type,
                          const std::string& subtype,
                          const Dictionary& parameters) = 0;
    };

  private:
    struct Handler
    {
      std::string  type_;
      std::string  subtype_;
      IHandler&    handler_;

      Handler(const std::string& type,
              const std::string& subtype,
              IHandler& handler);

      bool IsMatch(const std::string& type,
                   const std::string& subtype) const;

      void Call(const Dictionary& parameters) const
      {
        handler_.Handle(type_, subtype_, parameters);
      }
   };


    struct Reference;

    typedef std::vector<std::string>  Tokens;
    typedef std::list<Handler>   Handlers;

    Handlers  handlers_;


    static bool SplitPair(std::string& first /* out */,
                          std::string& second /* out */,
                          const std::string& source,
                          char separator);

    static void SelectBestMatch(std::unique_ptr<Reference>& target,
                                const Handler& handler,
                                const std::string& type,
                                const std::string& subtype,
                                const Dictionary& parameters);

  public:
    void Register(const std::string& mime,
                  IHandler& handler);
    
    bool Apply(const Dictionary& headers);

    bool Apply(const std::string& accept);
  };
}
