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


#include "PrecompiledHeadersServer.h"
#include "ServerContext.h"

#include "../Core/HttpServer/FilesystemHttpSender.h"
#include "../Core/Lua/LuaFunctionCall.h"
#include "FromDcmtkBridge.h"
#include "ServerToolbox.h"
#include "OrthancInitialization.h"

#include <glog/logging.h>
#include <EmbeddedResources.h>
#include <dcmtk/dcmdata/dcfilefo.h>

#define ENABLE_DICOM_CACHE  1

static const char* RECEIVED_INSTANCE_FILTER = "ReceivedInstanceFilter";

static const size_t DICOM_CACHE_SIZE = 2;

/**
 * IMPORTANT: We make the assumption that the same instance of
 * FileStorage can be accessed from multiple threads. This seems OK
 * since the filesystem implements the required locking mechanisms,
 * but maybe a read-writer lock on the "FileStorage" could be
 * useful. Conversely, "ServerIndex" already implements mutex-based
 * locking.
 **/

namespace Orthanc
{
  ServerContext::ServerContext(const boost::filesystem::path& storagePath,
                               const boost::filesystem::path& indexPath) :
    storage_(storagePath.string()),
    index_(*this, indexPath.string()),
    accessor_(storage_),
    compressionEnabled_(false),
    provider_(*this),
    dicomCache_(provider_, DICOM_CACHE_SIZE)
  {
    scu_.SetLocalApplicationEntityTitle(Configuration::GetGlobalStringParameter("DicomAet", "ORTHANC"));
    //scu_.SetMillisecondsBeforeClose(1);  // The connection is always released

    lua_.Execute(Orthanc::EmbeddedResources::LUA_TOOLBOX);
  }

  void ServerContext::SetCompressionEnabled(bool enabled)
  {
    if (enabled)
      LOG(WARNING) << "Disk compression is enabled";
    else
      LOG(WARNING) << "Disk compression is disabled";

    compressionEnabled_ = enabled;
  }

  void ServerContext::RemoveFile(const std::string& fileUuid)
  {
    storage_.Remove(fileUuid);
  }

  StoreStatus ServerContext::Store(const char* dicomInstance,
                                   size_t dicomSize,
                                   const DicomMap& dicomSummary,
                                   const Json::Value& dicomJson,
                                   const std::string& remoteAet)
  {
    // Test if the instance must be filtered out
    if (lua_.IsExistingFunction(RECEIVED_INSTANCE_FILTER))
    {
      Json::Value simplified;
      SimplifyTags(simplified, dicomJson);

      LuaFunctionCall call(lua_, RECEIVED_INSTANCE_FILTER);
      call.PushJSON(simplified);
      call.PushString(remoteAet);

      if (!call.ExecutePredicate())
      {
        LOG(INFO) << "An incoming instance has been discarded by the filter";
        return StoreStatus_FilteredOut;
      }
    }

    if (compressionEnabled_)
    {
      accessor_.SetCompressionForNextOperations(CompressionType_Zlib);
    }
    else
    {
      accessor_.SetCompressionForNextOperations(CompressionType_None);
    }      

    FileInfo dicomInfo = accessor_.Write(dicomInstance, dicomSize, FileContentType_Dicom);
    FileInfo jsonInfo = accessor_.Write(dicomJson.toStyledString(), FileContentType_DicomAsJson);

    ServerIndex::Attachments attachments;
    attachments.push_back(dicomInfo);
    attachments.push_back(jsonInfo);

    StoreStatus status = index_.Store(dicomSummary, attachments, remoteAet);

    if (status != StoreStatus_Success)
    {
      storage_.Remove(dicomInfo.GetUuid());
      storage_.Remove(jsonInfo.GetUuid());
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

      default:
        // This should never happen
        break;
    }

    return status;
  }

  
  void ServerContext::AnswerDicomFile(RestApiOutput& output,
                                      const std::string& instancePublicId,
                                      FileContentType content)
  {
    FileInfo attachment;
    if (!index_.LookupAttachment(attachment, instancePublicId, content))
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    accessor_.SetCompressionForNextOperations(attachment.GetCompressionType());

    std::auto_ptr<HttpFileSender> sender(accessor_.ConstructHttpFileSender(attachment.GetUuid()));
    sender->SetContentType("application/dicom");
    sender->SetDownloadFilename(instancePublicId + ".dcm");
    output.AnswerFile(*sender);
  }


  void ServerContext::ReadJson(Json::Value& result,
                               const std::string& instancePublicId)
  {
    std::string s;
    ReadFile(s, instancePublicId, FileContentType_DicomAsJson);

    Json::Reader reader;
    if (!reader.parse(s, result))
    {
      throw OrthancException("Corrupted JSON file");
    }
  }


  void ServerContext::ReadFile(std::string& result,
                               const std::string& instancePublicId,
                               FileContentType content,
                               bool uncompressIfNeeded)
  {
    FileInfo attachment;
    if (!index_.LookupAttachment(attachment, instancePublicId, content))
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    if (uncompressIfNeeded)
    {
      accessor_.SetCompressionForNextOperations(attachment.GetCompressionType());
    }
    else
    {
      accessor_.SetCompressionForNextOperations(CompressionType_None);
    }

    accessor_.Read(result, attachment.GetUuid());
  }


  IDynamicObject* ServerContext::DicomCacheProvider::Provide(const std::string& instancePublicId)
  {
    std::string content;
    context_.ReadFile(content, instancePublicId, FileContentType_Dicom);
    return new ParsedDicomFile(content);
  }


  ServerContext::DicomCacheLocker::DicomCacheLocker(ServerContext& that,
                                                    const std::string& instancePublicId) : 
    that_(that)
  {
#if ENABLE_DICOM_CACHE == 0
    static std::auto_ptr<IDynamicObject> p;
    p.reset(provider_.Provide(instancePublicId));
    dicom_ = dynamic_cast<ParsedDicomFile*>(p.get());
#else
    that_.dicomCacheMutex_.lock();
    dicom_ = &dynamic_cast<ParsedDicomFile&>(that_.dicomCache_.Access(instancePublicId));
#endif
  }


  ServerContext::DicomCacheLocker::~DicomCacheLocker()
  {
#if ENABLE_DICOM_CACHE == 0
#else
    that_.dicomCacheMutex_.unlock();
#endif
  }


  static DcmFileFormat& GetDicom(ParsedDicomFile& file)
  {
    return *reinterpret_cast<DcmFileFormat*>(file.GetDcmtkObject());
  }


  StoreStatus ServerContext::Store(std::string& resultPublicId,
                                   ParsedDicomFile& dicomInstance,
                                   const char* dicomBuffer,
                                   size_t dicomSize)
  {
    DicomMap dicomSummary;
    FromDcmtkBridge::Convert(dicomSummary, *GetDicom(dicomInstance).getDataset());

    try
    {
      DicomInstanceHasher hasher(dicomSummary);
      resultPublicId = hasher.HashInstance();

      Json::Value dicomJson;
      FromDcmtkBridge::ToJson(dicomJson, *GetDicom(dicomInstance).getDataset());
      
      StoreStatus status = StoreStatus_Failure;
      if (dicomSize > 0)
      {
        status = Store(dicomBuffer, dicomSize, dicomSummary, dicomJson, "");
      }   

      return status;
    }
    catch (OrthancException& e)
    {
      if (e.GetErrorCode() == ErrorCode_InexistentTag)
      {
        LogMissingRequiredTag(dicomSummary);
      }

      throw;
    }
  }


  StoreStatus ServerContext::Store(std::string& resultPublicId,
                                   ParsedDicomFile& dicomInstance)
  {
    std::string buffer;
    if (!FromDcmtkBridge::SaveToMemoryBuffer(buffer, GetDicom(dicomInstance).getDataset()))
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    if (buffer.size() == 0)
      return Store(resultPublicId, dicomInstance, NULL, 0);
    else
      return Store(resultPublicId, dicomInstance, &buffer[0], buffer.size());
  }


  StoreStatus ServerContext::Store(std::string& resultPublicId,
                                   const char* dicomBuffer,
                                   size_t dicomSize)
  {
    ParsedDicomFile dicom(dicomBuffer, dicomSize);
    return Store(resultPublicId, dicom, dicomBuffer, dicomSize);
  }


  StoreStatus ServerContext::Store(std::string& resultPublicId,
                                   const std::string& dicomContent)
  {
    if (dicomContent.size() == 0)
    {
      return Store(resultPublicId, NULL, 0);
    }
    else
    {
      return Store(resultPublicId, &dicomContent[0], dicomContent.size());
    }
  }


  void ServerContext::SetStoreMD5ForAttachments(bool storeMD5)
  {
    LOG(INFO) << "Storing MD5 for attachments: " << (storeMD5 ? "yes" : "no");
    accessor_.SetStoreMD5(storeMD5);
  }


  bool ServerContext::AddAttachment(const std::string& resourceId,
                                    FileContentType attachmentType,
                                    const void* data,
                                    size_t size)
  {
    LOG(INFO) << "Adding attachment " << EnumerationToString(attachmentType) << " to resource " << resourceId;
    
    if (compressionEnabled_)
    {
      accessor_.SetCompressionForNextOperations(CompressionType_Zlib);
    }
    else
    {
      accessor_.SetCompressionForNextOperations(CompressionType_None);
    }      

    FileInfo info = accessor_.Write(data, size, attachmentType);
    StoreStatus status = index_.AddAttachment(info, resourceId);

    if (status != StoreStatus_Success)
    {
      storage_.Remove(info.GetUuid());
      return false;
    }
    else
    {
      return true;
    }
  }
}
