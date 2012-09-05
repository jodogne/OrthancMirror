/**
 * Palanthir - A Lightweight, RESTful DICOM Store
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


#pragma once

#include "../Core/HttpServer/HttpHandler.h"
#include "ServerIndex.h"
#include "DicomProtocol/DicomUserConnection.h"

#include <set>


namespace Palanthir
{
  class PalanthirRestApi : public HttpHandler
  {
  private:
    typedef std::set<std::string> Modalities;

    ServerIndex& index_;
    FileStorage storage_;
    Modalities modalities_;

    bool Store(Json::Value& result,
               const std::string& postData);

    void ConnectToModality(DicomUserConnection& c,
                           const std::string& name);

    bool MergeQueryAndTemplate(DicomMap& result,
                               const std::string& postData);

    bool DicomFindPatient(Json::Value& result,
                          DicomUserConnection& c,
                          const std::string& postData);

    bool DicomFindStudy(Json::Value& result,
                        DicomUserConnection& c,
                        const std::string& postData);

    bool DicomFindSeries(Json::Value& result,
                         DicomUserConnection& c,
                         const std::string& postData);

    bool DicomFind(Json::Value& result,
                   DicomUserConnection& c,
                   const std::string& postData);

    bool DicomStore(Json::Value& result,
                    DicomUserConnection& c,
                    const std::string& postData);

  public:
    PalanthirRestApi(ServerIndex& index,
                     const std::string& path);

    virtual bool IsServedUri(const UriComponents& uri)
    {
      return true;
    }

    virtual void Handle(
      HttpOutput& output,
      const std::string& method,
      const UriComponents& uri,
      const Arguments& headers,
      const Arguments& arguments,
      const std::string& postData);
  };
}
