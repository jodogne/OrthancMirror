/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2014 Medical Physics Department, CHU of Liege,
 * Belgium
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

#include "PluginsManager.h"
#include "../../Core/HttpServer/HttpHandler.h"
#include "../../OrthancServer/ServerContext.h"
#include "../../OrthancServer/OrthancRestApi/OrthancRestApi.h"
#include "../OrthancCPlugin/OrthancCPlugin.h"

#include <list>
#include <boost/shared_ptr.hpp>

namespace Orthanc
{
  class PluginsHttpHandler : public HttpHandler, public IPluginServiceProvider
  {
  private:
    struct PImpl;

    boost::shared_ptr<PImpl> pimpl_;

    void RegisterRestCallback(const void* parameters);

    void AnswerBuffer(const void* parameters);

    void CompressAndAnswerPngImage(const void* parameters);

    void GetDicomForInstance(const void* parameters);

    void RestApiGet(const void* parameters);

    void RestApiPostPut(bool isPost, const void* parameters);

    void RestApiDelete(const void* parameters);

  public:
    PluginsHttpHandler(ServerContext& context);

    virtual ~PluginsHttpHandler();

    virtual bool Handle(HttpOutput& output,
                        HttpMethod method,
                        const UriComponents& uri,
                        const Arguments& headers,
                        const Arguments& getArguments,
                        const std::string& postData);

    virtual bool InvokeService(_OrthancPluginService service,
                               const void* parameters);

    void SetOrthancRestApi(OrthancRestApi& restApi);
  };
}
