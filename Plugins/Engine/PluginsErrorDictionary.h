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

#if ORTHANC_ENABLE_PLUGINS == 1

#include "../Include/orthanc/OrthancCPlugin.h"
#include "../../Core/OrthancException.h"
#include "../../Core/SharedLibrary.h"

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
