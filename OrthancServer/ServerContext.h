/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012 Medical Physics Department, CHU of Liege,
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

#include "ServerIndex.h"
#include "../Core/FileStorage.h"
#include "../Core/RestApi/RestApiOutput.h"
#include "../Core/FileStorage/CompressedFileStorageAccessor.h"

namespace Orthanc
{
  /**
   * This class is responsible for maintaining the storage area on the
   * filesystem (including compression), as well as the index of the
   * DICOM store. It implements the required locking mechanisms.
   **/
  class ServerContext
  {
  private:
    FileStorage storage_;
    ServerIndex index_;
    CompressedFileStorageAccessor accessor_;

  public:
    ServerContext(const boost::filesystem::path& path);

    ServerIndex& GetIndex()
    {
      return index_;
    }

    void RemoveFile(const std::string& fileUuid);

    StoreStatus Store(const char* dicomFile,
                      size_t dicomSize,
                      const DicomMap& dicomSummary,
                      const Json::Value& dicomJson,
                      const std::string& remoteAet);

    void AnswerFile(RestApiOutput& output,
                    const std::string& instancePublicId,
                    FileType content);

    void ReadJson(Json::Value& result,
                  const std::string& instancePublicId);

    // TODO CACHING MECHANISM AT THIS POINT
    void ReadFile(std::string& result,
                  const std::string& instancePublicId,
                  FileType content);
  };
}
