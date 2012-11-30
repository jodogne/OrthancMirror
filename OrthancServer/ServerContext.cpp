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


#include "ServerContext.h"

#include "../Core/HttpServer/FilesystemHttpSender.h"

#include <glog/logging.h>

namespace Orthanc
{
  ServerContext::ServerContext(const boost::filesystem::path& path) :
    storage_(path.string()),
    index_(*this, path.string())
  {
  }

  StoreStatus ServerContext::Store(const char* dicomFile,
                                   size_t dicomSize,
                                   const DicomMap& dicomSummary,
                                   const Json::Value& dicomJson,
                                   const std::string& remoteAet)
  {
    std::string fileUuid = storage_.Create(dicomFile, dicomSize);
    std::string jsonUuid = storage_.Create(dicomJson.toStyledString());
    StoreStatus status = index_.Store(dicomSummary, fileUuid, dicomSize, jsonUuid, remoteAet);

    if (status != StoreStatus_Success)
    {
      storage_.Remove(fileUuid);
      storage_.Remove(jsonUuid);
    }

    switch (status)
    {
    case StoreStatus_Success:
      LOG(INFO) << "New instance stored";
      break;

    case StoreStatus_AlreadyStored:
      LOG(INFO) << "Already stored";
      break;

    case StoreStatus_Failure:
      LOG(ERROR) << "Store failure";
      break;
    }

    return status;
  }

  
  void ServerContext::AnswerFile(RestApiOutput& output,
                                 const std::string& instancePublicId,
                                 AttachedFileType content)
  {
    CompressionType compressionType;
    std::string fileUuid;

    if (index_.GetFile(fileUuid, compressionType, 
                       instancePublicId, AttachedFileType_Dicom))
    {
      assert(compressionType == CompressionType_None);

      FilesystemHttpSender sender(storage_, fileUuid);
      sender.SetDownloadFilename(fileUuid + ".dcm");
      sender.SetContentType("application/dicom");
      output.AnswerFile(sender);
    }
  }


  void ServerContext::ReadJson(Json::Value& result,
                               const std::string& instancePublicId)
  {
    CompressionType compressionType;
    std::string fileUuid;
    if (!index_.GetFile(fileUuid, compressionType, instancePublicId, AttachedFileType_Json))
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    assert(compressionType == CompressionType_None);

    std::string s;
    storage_.ReadFile(s, fileUuid);

    Json::Reader reader;
    if (!reader.parse(s, result))
    {
      throw OrthancException("Corrupted JSON file");
    }
  }
}
