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


#pragma once

#include "RestApiGetCall.h"
#include "RestApiPostCall.h"
#include "RestApiPutCall.h"
#include "RestApiDeleteCall.h"

#include <set>

namespace Orthanc
{
  class ORTHANC_PUBLIC RestApiHierarchy : public boost::noncopyable
  {
  public:
    class ORTHANC_PUBLIC Resource : public boost::noncopyable
    {
    private:
      RestApiGetCall::Handler     getHandler_;
      RestApiPostCall::Handler    postHandler_;
      RestApiPutCall::Handler     putHandler_;
      RestApiDeleteCall::Handler  deleteHandler_;

    public:
      Resource();

      bool HasHandler(HttpMethod method) const;

      void Register(RestApiGetCall::Handler handler);

      void Register(RestApiPutCall::Handler handler);

      void Register(RestApiPostCall::Handler handler);

      void Register(RestApiDeleteCall::Handler handler);

      bool IsEmpty() const;

      bool Handle(RestApiGetCall& call) const;

      bool Handle(RestApiPutCall& call) const;

      bool Handle(RestApiPostCall& call) const;

      bool Handle(RestApiDeleteCall& call) const;
    };


    class IVisitor : public boost::noncopyable
    {
    public:
      virtual ~IVisitor()
      {
      }

      virtual bool Visit(const Resource& resource,
                         const UriComponents& uri,
                         bool hasTrailing,
                         // "uriArguments" only contains their name if using "ExploreAllResources()"
                         const HttpToolbox::Arguments& uriArguments,
                         // "trailing" is empty if using "ExploreAllResources()"
                         const UriComponents& trailing) = 0;
    };


  private:
    typedef std::map<std::string, RestApiHierarchy*>  Children;

    Resource  handlers_;
    Children  children_;
    Children  wildcardChildren_;
    Resource  handlersWithTrailing_;

    static RestApiHierarchy& AddChild(Children& children,
                                      const std::string& name);

    static void DeleteChildren(Children& children);

    template <typename Handler>
    void RegisterInternal(const RestApiPath& path,
                          Handler handler,
                          size_t level);

    bool CanGenerateDirectory() const;

    bool LookupResource(HttpToolbox::Arguments& components,
                        const UriComponents& uri,
                        IVisitor& visitor,
                        size_t level);

    bool GetDirectory(Json::Value& result,
                      const UriComponents& uri,
                      size_t level);

  public:
    ~RestApiHierarchy();

    void Register(const std::string& uri,
                  RestApiGetCall::Handler handler);

    void Register(const std::string& uri,
                  RestApiPutCall::Handler handler);

    void Register(const std::string& uri,
                  RestApiPostCall::Handler handler);

    void Register(const std::string& uri,
                  RestApiDeleteCall::Handler handler);

    void CreateSiteMap(Json::Value& target) const;

    bool GetDirectory(Json::Value& result,
                      const UriComponents& uri);

    bool LookupResource(const UriComponents& uri,
                        IVisitor& visitor);

    void GetAcceptedMethods(std::set<HttpMethod>& methods,
                            const UriComponents& uri);

    void ExploreAllResources(IVisitor& visitor,
                             const UriComponents& path,
                             const std::set<std::string>& uriArguments) const;
  };
}
