/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2024 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2024 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#if ORTHANC_ENABLE_PLUGINS == 1

#include "../Include/orthanc/OrthancCPlugin.h"
#include "../../../OrthancFramework/Sources/OrthancException.h"
#include "../../../OrthancFramework/Sources/SharedLibrary.h"

#include <map>
#include <string>
#include <boost/noncopyable.hpp>
#include <boost/thread/mutex.hpp>
#include <json/value.h>


namespace Orthanc
{
  class PluginsErrorDictionary : public boost::noncopyable
  {
  private:
    struct Error
    {
      std::string  pluginName_;
      int32_t      pluginCode_;
      HttpStatus   httpStatus_;
      std::string  message_;
    };
    
    typedef std::map<int32_t, Error*>  Errors;

    boost::mutex  mutex_;
    int32_t  pos_;
    Errors   errors_;

  public:
    PluginsErrorDictionary();

    ~PluginsErrorDictionary();

    OrthancPluginErrorCode  Register(SharedLibrary& library,
                                     int32_t  pluginCode,
                                     uint16_t httpStatus,
                                     const char* message);

    void  LogError(ErrorCode code,
                   bool ignoreBuiltinErrors);

    void  LogError(OrthancPluginErrorCode code,
                   bool ignoreBuiltinErrors)
    {
      LogError(static_cast<ErrorCode>(code), ignoreBuiltinErrors);
    }

    bool  Format(Json::Value& message,    /* out */
                 HttpStatus& httpStatus,  /* out */
                 const OrthancException& exception);
  };
}

#endif
