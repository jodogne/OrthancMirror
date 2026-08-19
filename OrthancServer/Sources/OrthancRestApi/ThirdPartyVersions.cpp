/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "../PrecompiledHeadersServer.h"
#include "ThirdPartyVersions.h"

#include "../../../OrthancFramework/Sources/Toolbox.h"

#include <boost/lexical_cast.hpp>
#include <boost/version.hpp>
#include <curl/curl.h>
#include <google/protobuf/stubs/common.h>
#include <jpeglib.h>
#include <json/version.h>
#include <lua.h>
#include <png.h>
#include <sqlite3.h>
#include <zlib.h>

#if ORTHANC_ENABLE_CIVETWEB == 1
#  include <civetweb.h>
#endif

#if ORTHANC_ENABLE_SSL == 1
#  include <openssl/opensslv.h>
#endif

#if ORTHANC_ENABLE_PUGIXML == 1
#  include <pugixml.hpp>
#endif

namespace Orthanc
{
  void GetThirdPartyVersions(Json::Value& target)
  {
    target = Json::objectValue;

    target["boost"] = (boost::lexical_cast<std::string>(BOOST_VERSION / 100000) + "." +
                       boost::lexical_cast<std::string>(BOOST_VERSION / 100 % 1000) + "." +
                       boost::lexical_cast<std::string>(BOOST_VERSION % 100));

#if ORTHANC_ENABLE_CIVETWEB == 1
    target["civetweb"] = (boost::lexical_cast<std::string>(CIVETWEB_VERSION_MAJOR) + "." +
                          boost::lexical_cast<std::string>(CIVETWEB_VERSION_MINOR) + "." +
                          boost::lexical_cast<std::string>(CIVETWEB_VERSION_PATCH));
#endif

    target["dcmtk"] = (boost::lexical_cast<std::string>(DCMTK_VERSION_NUMBER / 100) + "." +
                       boost::lexical_cast<std::string>(DCMTK_VERSION_NUMBER / 10 % 10) + "." +
                       boost::lexical_cast<std::string>(DCMTK_VERSION_NUMBER % 10));

    target["jsoncpp"] = JSONCPP_VERSION_STRING;

    target["libcurl"] = LIBCURL_VERSION;

    target["libjpeg"] = (boost::lexical_cast<std::string>(JPEG_LIB_VERSION / 10) + "." +
                         boost::lexical_cast<std::string>(JPEG_LIB_VERSION % 10));

    target["libpng"] = (boost::lexical_cast<std::string>(PNG_LIBPNG_VER_MAJOR) + "." +
                        boost::lexical_cast<std::string>(PNG_LIBPNG_VER_MINOR) + "." +
                        boost::lexical_cast<std::string>(PNG_LIBPNG_VER_RELEASE));

#if defined(LUA_VERSION_MAJOR)
    target["lua"] = (boost::lexical_cast<std::string>(LUA_VERSION_MAJOR) + "." +
                     boost::lexical_cast<std::string>(LUA_VERSION_MINOR) + "." +
                     boost::lexical_cast<std::string>(LUA_VERSION_RELEASE));
#else
    target["lua"] = (boost::lexical_cast<std::string>(LUA_VERSION_NUM / 100) + "." +
                     boost::lexical_cast<std::string>(LUA_VERSION_NUM % 100));
#endif

#if ORTHANC_ENABLE_SSL == 1
#  if defined(OPENSSL_VERSION_MAJOR)
    target["openssl"] = (boost::lexical_cast<std::string>(OPENSSL_VERSION_MAJOR) + "." +
                         boost::lexical_cast<std::string>(OPENSSL_VERSION_MINOR) + "." +
                         boost::lexical_cast<std::string>(OPENSSL_VERSION_PATCH));
#  else
    {
      // Backward compatibility for OpenSSL 1.x
      std::vector<std::string> v;
      Toolbox::TokenizeString(v, OPENSSL_VERSION_TEXT, ' ' );
      if (v.size() >= 2)
      {
        target["openssl"] = v[1];
      }
      else
      {
        target["openssl"] = "unknown";
      }
    }
#  endif
#endif

#if ORTHANC_ENABLE_PUGIXML == 1
    target["pugixml"] = (boost::lexical_cast<std::string>(PUGIXML_VERSION / 1000) + "." +
                         boost::lexical_cast<std::string>(PUGIXML_VERSION / 10 % 100) + "." +
                         boost::lexical_cast<std::string>(PUGIXML_VERSION % 10));
#endif

    target["protobuf"] = (boost::lexical_cast<std::string>(GOOGLE_PROTOBUF_VERSION / 1000000) + "." +
                          boost::lexical_cast<std::string>(GOOGLE_PROTOBUF_VERSION / 1000 % 1000) + "." +
                          boost::lexical_cast<std::string>(GOOGLE_PROTOBUF_VERSION % 1000));

    target["sqlite"] = SQLITE_VERSION;

    target["zlib"] = ZLIB_VERSION;
  }
}
