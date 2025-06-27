/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "../../../../OrthancFramework/Sources/Logging.h"
#include "../../../../OrthancFramework/Sources/SystemToolbox.h"
#include "../../../../OrthancFramework/Sources/Toolbox.h"
#include "../Common/OrthancPluginCppWrapper.h"

#include <boost/filesystem.hpp>


static boost::filesystem::path storageDirectory_;


static std::string GetStorageDirectoryPath(const char* uuid)
{
  if (uuid == NULL)
  {
    throw Orthanc::OrthancException(Orthanc::ErrorCode_NullPointer);
  }
  else
  {
    return (storageDirectory_ / std::string(uuid)).string();
  }
}


#define CATCH_EXCEPTIONS(errorValue)                    \
  catch (Orthanc::OrthancException& e)                  \
  {                                                     \
    LOG(ERROR) << "Orthanc exception: " << e.What();    \
    return errorValue;                                  \
  }                                                     \
  catch (std::runtime_error& e)                         \
  {                                                     \
    LOG(ERROR) << "Native exception: " << e.what();     \
    return errorValue;                                  \
  }                                                     \
  catch (...)                                           \
  {                                                     \
    return errorValue;                                  \
  }


OrthancPluginErrorCode StorageCreate(OrthancPluginMemoryBuffer* customData,
                                     const char* uuid,
                                     const void* content,
                                     uint64_t size,
                                     OrthancPluginContentType type,
                                     OrthancPluginCompressionType compressionType,
                                     const OrthancPluginDicomInstance* dicomInstance)
{
  try
  {
    Json::Value info;
    info["IsAdopted"] = false;

    OrthancPlugins::MemoryBuffer buffer;
    buffer.Assign(info.toStyledString());
    *customData = buffer.Release();

    const std::string path = GetStorageDirectoryPath(uuid);
    LOG(WARNING) << "Creating non-adopted file: " << path;
    Orthanc::SystemToolbox::WriteFile(content, size, path);

    return OrthancPluginErrorCode_Success;
  }
  CATCH_EXCEPTIONS(OrthancPluginErrorCode_Plugin);
}


OrthancPluginErrorCode StorageReadRange(OrthancPluginMemoryBuffer64* target,
                                        const char* uuid,
                                        OrthancPluginContentType type,
                                        uint64_t rangeStart,
                                        const void* customData,
                                        uint32_t customDataSize)
{
  try
  {
    Json::Value info;
    if (!Orthanc::Toolbox::ReadJson(info, customData, customDataSize))
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_BadFileFormat);
    }

    std::string path;

    if (info["IsAdopted"].asBool())
    {
      path = info["AdoptedPath"].asString();
      LOG(WARNING) << "Reading adopted file from: " << path;
    }
    else
    {
      path = GetStorageDirectoryPath(uuid);
      LOG(WARNING) << "Reading non-adopted file from: " << path;
    }

    std::string range;
    Orthanc::SystemToolbox::ReadFileRange(range, path, rangeStart, rangeStart + target->size, true);

    assert(range.size() == target->size);

    if (target->size != 0)
    {
      memcpy(target->data, range.c_str(), target->size);
    }

    return OrthancPluginErrorCode_Success;
  }
  CATCH_EXCEPTIONS(OrthancPluginErrorCode_Plugin);
}


OrthancPluginErrorCode StorageRemove(const char* uuid,
                                     OrthancPluginContentType type,
                                     const void* customData,
                                     uint32_t customDataSize)
{
  try
  {
    Json::Value info;
    if (!Orthanc::Toolbox::ReadJson(info, customData, customDataSize))
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_BadFileFormat);
    }

    // Only remove non-adopted files (i.e., whose for which Orthanc has the ownership)
    if (info["IsAdopted"].asBool())
    {
      LOG(WARNING) << "Don't removing adopted file: " << info["AdoptedPath"].asString();
    }
    else
    {
      const std::string path = GetStorageDirectoryPath(uuid);
      LOG(WARNING) << "Removing non-adopted file from: " << path;
      Orthanc::SystemToolbox::RemoveFile(path);
    }

    return OrthancPluginErrorCode_Success;
  }
  CATCH_EXCEPTIONS(OrthancPluginErrorCode_Plugin);
}


OrthancPluginErrorCode Adopt(OrthancPluginRestOutput* output,
                             const char* url,
                             const OrthancPluginHttpRequest* request)
{
  try
  {
    if (request->method != OrthancPluginHttpMethod_Post)
    {
      OrthancPluginSendMethodNotAllowed(OrthancPlugins::GetGlobalContext(), output, "POST");
      return OrthancPluginErrorCode_Success;
    }
    else
    {
      const std::string path(reinterpret_cast<const char*>(request->body), request->bodySize);
      LOG(WARNING) << "Adopting DICOM instance from path: " << path;

      std::string dicom;
      Orthanc::SystemToolbox::ReadFile(dicom, path);

      Json::Value info;
      info["IsAdopted"] = true;
      info["AdoptedPath"] = path;

      const std::string customData = info.toStyledString();

      OrthancPluginStoreStatus status;
      OrthancPlugins::MemoryBuffer instanceId, attachmentUuid;

      OrthancPluginErrorCode code = OrthancPluginAdoptDicomInstance(
        OrthancPlugins::GetGlobalContext(), *instanceId, *attachmentUuid, &status,
        dicom.empty() ? NULL : dicom.c_str(), dicom.size(),
        customData.empty() ? NULL : customData.c_str(), customData.size());

      if (code == OrthancPluginErrorCode_Success)
      {
        const std::string answer = "OK\n";
        OrthancPluginAnswerBuffer(OrthancPlugins::GetGlobalContext(), output, answer.c_str(), answer.size(), "text/plain");
        return OrthancPluginErrorCode_Success;
      }
      else
      {
        return code;
      }
    }
  }
  CATCH_EXCEPTIONS(OrthancPluginErrorCode_Plugin);
}


extern "C"
{
  ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* context)
  {
    OrthancPlugins::SetGlobalContext(context, PLUGIN_NAME);
    Orthanc::Logging::InitializePluginContext(context, PLUGIN_NAME);

    /* Check the version of the Orthanc core */
    if (OrthancPluginCheckVersion(context) == 0)
    {
      char info[1024];
      sprintf(info, "Your version of Orthanc (%s) must be above %d.%d.%d to run this plugin",
              context->orthancVersion,
              ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_REVISION_NUMBER);
      OrthancPluginLogError(context, info);
      return -1;
    }

    OrthancPluginSetDescription2(context, PLUGIN_NAME, "Sample plugin illustrating the adoption of DICOM instances.");
    OrthancPluginRegisterStorageArea3(context, StorageCreate, StorageReadRange, StorageRemove);

    try
    {
      OrthancPlugins::OrthancConfiguration config;
      storageDirectory_ = config.GetStringValue("StorageDirectory", "OrthancStorage");

      Orthanc::SystemToolbox::MakeDirectory(storageDirectory_.string());

      OrthancPluginRegisterRestCallback(context, "/adopt", Adopt);
    }
    CATCH_EXCEPTIONS(-1)

    return 0;
  }

  ORTHANC_PLUGINS_API void OrthancPluginFinalize()
  {
  }

  ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
  {
    return PLUGIN_NAME;
  }

  ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion()
  {
    return PLUGIN_VERSION;
  }
}
