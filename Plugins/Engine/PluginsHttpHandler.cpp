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


#include "PluginsHttpHandler.h"

#include "../../Core/OrthancException.h"
#include "../../Core/Toolbox.h"

#include <boost/regex.hpp> 
#include <glog/logging.h>

namespace Orthanc
{
  struct PluginsHttpHandler::PImpl
  {
    typedef std::pair<boost::regex*, OrthancPluginRestCallback> Callback;
    typedef std::list<Callback>  Callbacks;

    Callbacks callbacks_;
    OrthancPluginRestCallback currentCallback_;

    PImpl() : currentCallback_(NULL)
    {
    }
  };


  PluginsHttpHandler::PluginsHttpHandler(const PluginsManager& manager)
  {
    pimpl_.reset(new PImpl);

    for (PluginsManager::RestCallbacks::const_iterator
           it = manager.GetRestCallbacks().begin(); it != manager.GetRestCallbacks().end(); ++it)
    {
      pimpl_->callbacks_.push_back(std::make_pair(new boost::regex(it->first), it->second));
    }
  }

  
  PluginsHttpHandler::~PluginsHttpHandler()
  {
    for (PImpl::Callbacks::iterator it = pimpl_->callbacks_.begin(); 
         it != pimpl_->callbacks_.end(); it++)
    {
      delete it->first;
    }
  }


  bool PluginsHttpHandler::IsServedUri(const UriComponents& uri)
  {
    pimpl_->currentCallback_ = NULL;    
    std::string tmp = Toolbox::FlattenUri(uri);

    for (PImpl::Callbacks::const_iterator it = pimpl_->callbacks_.begin(); 
         it != pimpl_->callbacks_.end(); it++)
    {
      if (boost::regex_match(tmp, *(it->first)))
      {
        pimpl_->currentCallback_ = it->second;
        return true;
      }
    }

    return false;
  }

  bool PluginsHttpHandler::Handle(HttpOutput& output,
                                  HttpMethod method,
                                  const UriComponents& uri,
                                  const Arguments& headers,
                                  const Arguments& getArguments,
                                  const std::string& postData)
  {
    std::string flatUri = Toolbox::FlattenUri(uri);

    LOG(INFO) << "Delegating HTTP request to plugin for URI: " << flatUri;

    std::vector<const char*> getKeys(getArguments.size());
    std::vector<const char*> getValues(getArguments.size());

    OrthancPluginHttpMethod methodPlugin;
    switch (method)
    {
      case HttpMethod_Get:
      {
        methodPlugin = OrthancPluginHttpMethod_Get;

        size_t i = 0;
        for (Arguments::const_iterator it = getArguments.begin(); 
             it != getArguments.end(); it++, i++)
        {
          getKeys[i] = it->first.c_str();
          getValues[i] = it->second.c_str();
        }

        break;
      }

      case HttpMethod_Post:
        methodPlugin = OrthancPluginHttpMethod_Post;
        break;

      case HttpMethod_Delete:
        methodPlugin = OrthancPluginHttpMethod_Delete;
        break;

      case HttpMethod_Put:
        methodPlugin = OrthancPluginHttpMethod_Put;
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    assert(pimpl_->currentCallback_ != NULL);
    assert(getKeys.size() == getValues.size());
    int32_t error = (*pimpl_->currentCallback_) (reinterpret_cast<OrthancPluginRestOutput*>(&output), 
                                                 methodPlugin, flatUri.c_str(), 
                                                 getKeys.size() ? &getKeys[0] : NULL,
                                                 getKeys.size() ? &getValues[0] : NULL,
                                                 getKeys.size(), 
                                                 postData.size() ? &postData[0] : NULL, postData.size());

    if (error < 0)
    {
      LOG(ERROR) << "Plugin failed with error code " << error;
      return false;
    }
    else
    {
      if (error > 0)
      {
        LOG(WARNING) << "Plugin finished with warning code " << error;
      }

      return true;
    }
  }
}
