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

#if !defined(ORTHANC_SANDBOXED)
#  error The macro ORTHANC_SANDBOXED must be defined
#endif

#if ORTHANC_SANDBOXED == 1
#  error The class StorageAccessor cannot be used in sandboxed environments
#endif

#if !defined(ORTHANC_ENABLE_CIVETWEB)
#  error Macro ORTHANC_ENABLE_CIVETWEB must be defined to use this file
#endif

#if !defined(ORTHANC_ENABLE_MONGOOSE)
#  error Macro ORTHANC_ENABLE_MONGOOSE must be defined to use this file
#endif

#include "IStorageArea.h"
#include "FileInfo.h"

#if ORTHANC_ENABLE_CIVETWEB == 1 || ORTHANC_ENABLE_MONGOOSE == 1
#  include "../HttpServer/BufferHttpSender.h"
#  include "../RestApi/RestApiOutput.h"
#endif

#include <vector>
#include <string>
#include <boost/noncopyable.hpp>
#include <stdint.h>
#include <json/value.h>

namespace Orthanc
{
  class StorageAccessor : boost::noncopyable
  {
  private:
    IStorageArea&  area_;

#if ORTHANC_ENABLE_CIVETWEB == 1 || ORTHANC_ENABLE_MONGOOSE == 1
    void SetupSender(BufferHttpSender& sender,
                     const FileInfo& info,
                     const std::string& mime);
#endif

  public:
    StorageAccessor(IStorageArea& area) : area_(area)
    {
    }

    FileInfo Write(const void* data,
                   size_t size,
                   FileContentType type,
                   CompressionType compression,
                   bool storeMd5);

    FileInfo Write(const std::string& data, 
                   FileContentType type,
                   CompressionType compression,
                   bool storeMd5)
    {
      return Write((data.size() == 0 ? NULL : data.c_str()),
                   data.size(), type, compression, storeMd5);
    }

    void Read(std::string& content,
              const FileInfo& info);

    void Read(Json::Value& content,
              const FileInfo& info);

    void Remove(const FileInfo& info)
    {
      area_.Remove(info.GetUuid(), info.GetContentType());
    }

#if ORTHANC_ENABLE_CIVETWEB == 1 || ORTHANC_ENABLE_MONGOOSE == 1
    void AnswerFile(HttpOutput& output,
                    const FileInfo& info,
                    const std::string& mime);

    void AnswerFile(RestApiOutput& output,
                    const FileInfo& info,
                    const std::string& mime);
#endif
  };
}
