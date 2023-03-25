/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "../../Sources/PrecompiledHeadersServer.h"
#include "PluginsErrorDictionary.h"

#if ORTHANC_ENABLE_PLUGINS != 1
#error The plugin support is disabled
#endif



#include "PluginsEnumerations.h"
#include "PluginsManager.h"
#include "../../../OrthancFramework/Sources/Logging.h"

#include <memory>


namespace Orthanc
{
  PluginsErrorDictionary::PluginsErrorDictionary() : 
    pos_(ErrorCode_START_PLUGINS)
  {
  }


  PluginsErrorDictionary::~PluginsErrorDictionary()
  {
    for (Errors::iterator it = errors_.begin(); it != errors_.end(); ++it)
    {
      delete it->second;
    }
  }


  OrthancPluginErrorCode PluginsErrorDictionary::Register(SharedLibrary& library,
                                                          int32_t  pluginCode,
                                                          uint16_t httpStatus,
                                                          const char* message)
  {
    std::unique_ptr<Error> error(new Error);

    error->pluginName_ = PluginsManager::GetPluginName(library);
    error->pluginCode_ = pluginCode;
    error->message_ = message;
    error->httpStatus_ = static_cast<HttpStatus>(httpStatus);

    OrthancPluginErrorCode code;

    {
      boost::mutex::scoped_lock lock(mutex_);
      errors_[pos_] = error.release();
      code = static_cast<OrthancPluginErrorCode>(pos_);
      pos_ += 1;
    }

    return code;
  }


  void  PluginsErrorDictionary::LogError(ErrorCode code,
                                         bool ignoreBuiltinErrors)
  {
    if (code >= ErrorCode_START_PLUGINS)
    {
      boost::mutex::scoped_lock lock(mutex_);
      Errors::const_iterator error = errors_.find(static_cast<int32_t>(code));
      
      if (error != errors_.end())
      {
        LOG(ERROR) << "Error code " << error->second->pluginCode_
                   << " inside plugin \"" << error->second->pluginName_
                   << "\": " << error->second->message_;
        return;
      }
    }

    if (!ignoreBuiltinErrors)
    {
      LOG(ERROR) << "Exception inside the plugin engine: "
                 << EnumerationToString(code);
    }
  }


  bool  PluginsErrorDictionary::Format(Json::Value& message,    /* out */
                                       HttpStatus& httpStatus,  /* out */
                                       const OrthancException& exception)
  {
    if (exception.GetErrorCode() >= ErrorCode_START_PLUGINS)
    {
      boost::mutex::scoped_lock lock(mutex_);
      Errors::const_iterator error = errors_.find(static_cast<int32_t>(exception.GetErrorCode()));
      
      if (error != errors_.end())
      {
        httpStatus = error->second->httpStatus_;
        message["PluginName"] = error->second->pluginName_;
        message["PluginCode"] = error->second->pluginCode_;
        message["Message"] = error->second->message_;

        return true;
      }
    }

    return false;
  }
}
