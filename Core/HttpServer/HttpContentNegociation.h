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

#pragma once

#include <memory>
#include <boost/noncopyable.hpp>
#include <map>
#include <list>
#include <string>
#include <vector>
#include <stdint.h>


namespace Orthanc
{
  class HttpContentNegociation : public boost::noncopyable
  {
  public:
    typedef std::map<std::string, std::string>  HttpHeaders;

    class IHandler : public boost::noncopyable
    {
    public:
      virtual ~IHandler()
      {
      }

      virtual void Handle(const std::string& type,
                          const std::string& subtype) = 0;
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

      void Call() const
      {
        handler_.Handle(type_, subtype_);
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

    static float GetQuality(const Tokens& parameters);

    static void SelectBestMatch(std::auto_ptr<Reference>& best,
                                const Handler& handler,
                                const std::string& type,
                                const std::string& subtype,
                                float quality);

  public:
    void Register(const std::string& mime,
                  IHandler& handler);
    
    bool Apply(const HttpHeaders& headers);

    bool Apply(const std::string& accept);
  };
}
