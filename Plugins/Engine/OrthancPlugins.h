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


#pragma once

#include "PluginsManager.h"
#include "../../Core/HttpServer/HttpHandler.h"
#include "../../OrthancServer/ServerContext.h"
#include "../../OrthancServer/OrthancRestApi/OrthancRestApi.h"
#include "OrthancPluginDatabase.h"

#include <list>
#include <boost/shared_ptr.hpp>

namespace Orthanc
{
  class OrthancPlugins : public HttpHandler, public IPluginServiceProvider
  {
  private:
    struct PImpl;
    boost::shared_ptr<PImpl> pimpl_;

    void RegisterRestCallback(const void* parameters);

    void RegisterOnStoredInstanceCallback(const void* parameters);

    void RegisterOnChangeCallback(const void* parameters);

    void AnswerBuffer(const void* parameters);

    void Redirect(const void* parameters);

    void CompressAndAnswerPngImage(const void* parameters);

    void GetDicomForInstance(const void* parameters);

    void RestApiGet(const void* parameters,
                    bool afterPlugins);

    void RestApiPostPut(bool isPost, 
                        const void* parameters,
                        bool afterPlugins);

    void RestApiDelete(const void* parameters,
                       bool afterPlugins);

    void LookupResource(_OrthancPluginService service,
                        const void* parameters);

    void SendHttpStatusCode(const void* parameters);

    void SendUnauthorized(const void* parameters);

    void SendMethodNotAllowed(const void* parameters);

    void SetCookie(const void* parameters);

    void SetHttpHeader(const void* parameters);

  public:
    OrthancPlugins();

    virtual ~OrthancPlugins();

    void SetServerContext(ServerContext& context);

    virtual bool Handle(HttpOutput& output,
                        HttpMethod method,
                        const UriComponents& uri,
                        const Arguments& headers,
                        const Arguments& getArguments,
                        const std::string& postData);

    virtual bool InvokeService(_OrthancPluginService service,
                               const void* parameters);

    void SignalChange(const ServerIndexChange& change);

    void SignalStoredInstance(DicomInstanceToStore& instance,
                              const std::string& instanceId);

    void SetOrthancRestApi(OrthancRestApi& restApi);

    void ResetOrthancRestApi();

    bool HasStorageArea() const;

    IStorageArea* GetStorageArea();  // To be freed after use

    bool HasDatabase() const;

    IDatabaseWrapper& GetDatabase();

    void Stop();

    const char* GetProperty(const char* plugin,
                            _OrthancPluginProperty property) const;

    void SetCommandLineArguments(int argc, char* argv[]);
  };
}
