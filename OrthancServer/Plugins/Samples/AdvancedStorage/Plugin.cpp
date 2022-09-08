/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "../../../../OrthancFramework/Sources/Compatibility.h"
#include "../../../../OrthancFramework/Sources/OrthancException.h"
#include "../../../../OrthancFramework/Sources/SystemToolbox.h"
#include "../../../../OrthancFramework/Sources/Toolbox.h"
#include "../../../../OrthancFramework/Sources/Logging.h"
#include "../Common/OrthancPluginCppWrapper.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/stream.hpp>


#include <json/value.h>
#include <json/writer.h>
#include <string.h>
#include <iostream>
#include <algorithm>
#include <map>
#include <list>
#include <time.h>

namespace fs = boost::filesystem;

fs::path absoluteRootPath_;
bool fsyncOnWrite_ = true;


fs::path GetLegacyRelativePath(const std::string& uuid)
{

  if (!Orthanc::Toolbox::IsUuid(uuid))
  {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_ParameterOutOfRange);
  }

  fs::path path = absoluteRootPath_;

  path /= std::string(&uuid[0], &uuid[2]);
  path /= std::string(&uuid[2], &uuid[4]);
  path /= uuid;

#if BOOST_HAS_FILESYSTEM_V3 == 1
  path.make_preferred();
#endif

  return path;
}

fs::path GetAbsolutePath(const std::string& uuid, const std::string& customData)
{
  fs::path path = absoluteRootPath_;

  if (!customData.empty())
  {
    if (customData.substr(0, 2) == "1.") // version 1
    {
      path /= customData.substr(2);
    }
    else
    {
      throw "TODO: unknown version";
    }

  }
  else
  {
    path /= GetLegacyRelativePath(uuid);
  }

  path.make_preferred();
  return path;
}

std::string GetCustomData(const fs::path& path)
{
  return std::string("1.") + path.string(); // prefix the relative path with a version
}

void AddDateDicomTagToPath(fs::path& path, const Json::Value& tags, const char* tagName, const char* defaultValue = NULL)
{
  if (tags.isMember(tagName) && tags[tagName].asString().size() == 8)
  {
    std::string date = tags[tagName].asString();
    path /= date.substr(0, 4);
    path /= date.substr(4, 2);
    path /= date.substr(6, 2);
  }
  else if (defaultValue != NULL)
  {
    path /= defaultValue;
  }
}

void AddSringDicomTagToPath(fs::path& path, const Json::Value& tags, const char* tagName, const char* defaultValue = NULL)
{
  if (tags.isMember(tagName) && tags[tagName].isString() && tags[tagName].asString().size() > 0)
  {
    path /= tags[tagName].asString();
  }
  else if (defaultValue != NULL)
  {
    path /= defaultValue;
  }
}

void AddIntDicomTagToPath(fs::path& path, const Json::Value& tags, const char* tagName, size_t zeroPaddingWidth = 0, const char* defaultValue = NULL)
{
  if (tags.isMember(tagName) && tags[tagName].isString() && tags[tagName].asString().size() > 0)
  {
    std::string tagValue = tags[tagName].asString();
    if (zeroPaddingWidth > 0 && tagValue.size() < zeroPaddingWidth)
    {
      std::string padding(zeroPaddingWidth - tagValue.size(), '0');
      path /= padding + tagValue; 
    }
    else
    {
      path /= tagValue;
    }
  }
  else if (defaultValue != NULL)
  {
    path /= defaultValue;
  }
}

fs::path GetRelativePathFromTags(const Json::Value& tags, const char* uuid, OrthancPluginContentType type, bool isCompressed)
{
  fs::path path;
  
  if (type == OrthancPluginContentType_Dicom || type == OrthancPluginContentType_DicomUntilPixelData)
  {
    // TODO: allow customization ... note: right now, we always need the uuid in the path !!

    AddDateDicomTagToPath(path, tags, "StudyDate", "NO_STUDY_DATE");
    AddSringDicomTagToPath(path, tags, "PatientID");  // no default value, tag is always present if the instance is accepted by Orthanc  
    AddSringDicomTagToPath(path, tags, "StudyInstanceUID");
    AddSringDicomTagToPath(path, tags, "SeriesInstanceUID");
    //AddIntDicomTagToPath(path, tags, "InstanceNumber", 8, uuid);
    path /= uuid;
  }
  else
  {
    path = GetLegacyRelativePath(uuid);
  }

  std::string extension;

  switch (type)
  {
    case OrthancPluginContentType_Dicom:
      extension = ".dcm";
      break;
    case OrthancPluginContentType_DicomUntilPixelData:
      extension = ".dcm.head";
      break;
    default:
      extension = ".unk";
  }
  if (isCompressed)
  {
    extension = extension + ".cmp"; // compression is zlib + size -> we can not use the .zip extension
  }
  
  path += extension;

  return path;
}


OrthancPluginErrorCode StorageCreate(OrthancPluginMemoryBuffer* customData,
                                             const char* uuid,
                                             const Json::Value& tags,
                                             const void* content,
                                             int64_t size,
                                             OrthancPluginContentType type,
                                             bool isCompressed)
{
  fs::path relativePath = GetRelativePathFromTags(tags, uuid, type, isCompressed);
  std::string customDataString = GetCustomData(relativePath);

  fs::path absolutePath = absoluteRootPath_ / relativePath;

  if (fs::exists(absolutePath))
  {
    // Extremely unlikely case if uuid is included in the path: This Uuid has already been created
    // in the past.
    throw Orthanc::OrthancException(Orthanc::ErrorCode_InternalError);

    // TODO for the future: handle duplicates path (e.g: there's no uuid in the path and we are uploading the same file again)
    // OrthancPlugins::LogWarning(std::string("Overwriting file \"") + path.string() + "\" (" + uuid + ")");
  }

  if (fs::exists(absolutePath.parent_path()))
  {
    if (!fs::is_directory(absolutePath.parent_path()))
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_DirectoryOverFile);
    }
  }
  else
  {
    if (!fs::create_directories(absolutePath.parent_path()))
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_FileStorageCannotWrite);
    }
  }

  Orthanc::SystemToolbox::WriteFile(content, size, absolutePath.string(), fsyncOnWrite_);

  OrthancPluginCreateMemoryBuffer(OrthancPlugins::GetGlobalContext(), customData, customDataString.size());
  memcpy(customData->data, customDataString.data(), customDataString.size());

  return OrthancPluginErrorCode_Success;

}

OrthancPluginErrorCode StorageCreateInstance(OrthancPluginMemoryBuffer* customData,
                                             const char* uuid,
                                             const OrthancPluginDicomInstance*  instance,
                                             const void* content,
                                             int64_t size,
                                             OrthancPluginContentType type,
                                             bool isCompressed)
{
  try
  {
    OrthancPlugins::LogInfo(std::string("Creating instance attachment \"") + uuid + "\"");

    OrthancPlugins::DicomInstance dicomInstance(instance);
    Json::Value tags;
    dicomInstance.GetSimplifiedJson(tags);

    return StorageCreate(customData, uuid, tags, content, size, type, isCompressed);
  }
  catch (Orthanc::OrthancException& e)
  {
    return static_cast<OrthancPluginErrorCode>(e.GetErrorCode());
  }
  catch (...)
  {
    return OrthancPluginErrorCode_StorageAreaPlugin;
  }

  return OrthancPluginErrorCode_Success;
}


OrthancPluginErrorCode StorageCreateAttachment(OrthancPluginMemoryBuffer* customData,
                                               const char* uuid,
                                               const char* resourceId,
                                               OrthancPluginResourceType resourceType,
                                               const void* content,
                                               int64_t size,
                                               OrthancPluginContentType type,
                                               bool isCompressed)
{
  try
  {
    OrthancPlugins::LogInfo(std::string("Creating attachment \"") + uuid + "\"");

    //TODO_CUSTOM_DATA: get tags from the Rest API...
    Json::Value tags;

    return StorageCreate(customData, uuid, tags, content, size, type, isCompressed);
  }
  catch (Orthanc::OrthancException& e)
  {
    return static_cast<OrthancPluginErrorCode>(e.GetErrorCode());
  }
  catch (...)
  {
    return OrthancPluginErrorCode_StorageAreaPlugin;
  }

  return OrthancPluginErrorCode_Success;
}

OrthancPluginErrorCode StorageReadWhole(OrthancPluginMemoryBuffer64* target,
                                        const char* uuid,
                                        const char* customData,
                                        OrthancPluginContentType type)
{
  OrthancPlugins::LogInfo(std::string("Reading attachment \"") + uuid + "\"");

  std::string path = GetAbsolutePath(uuid, customData).string();

  if (!Orthanc::SystemToolbox::IsRegularFile(path))
  {
    OrthancPlugins::LogError(std::string("The path does not point to a regular file: ") + path);
    return OrthancPluginErrorCode_InexistentFile;
  }

  try
  {
    fs::ifstream f;
    f.open(path, std::ifstream::in | std::ifstream::binary);
    if (!f.good())
    {
      OrthancPlugins::LogError(std::string("The path does not point to a regular file: ") + path);
      return OrthancPluginErrorCode_InexistentFile;
    }

    // get file size
    f.seekg(0, std::ios::end);
    std::streamsize fileSize = f.tellg();
    f.seekg(0, std::ios::beg);

    // The ReadWhole must allocate the buffer itself
    if (OrthancPluginCreateMemoryBuffer64(OrthancPlugins::GetGlobalContext(), target, fileSize) != OrthancPluginErrorCode_Success)
    {
      OrthancPlugins::LogError(std::string("Unable to allocate memory to read file: ") + path);
      return OrthancPluginErrorCode_NotEnoughMemory;
    }

    if (fileSize != 0)
    {
      f.read(reinterpret_cast<char*>(target->data), fileSize);
    }

    f.close();
  }
  catch (...)
  {
    OrthancPlugins::LogError(std::string("Unexpected error while reading: ") + path);
    return OrthancPluginErrorCode_StorageAreaPlugin;
  }

  return OrthancPluginErrorCode_Success;
}


OrthancPluginErrorCode StorageReadRange (OrthancPluginMemoryBuffer64* target,
                                         const char* uuid,
                                         const char* customData,
                                         OrthancPluginContentType type,
                                         uint64_t rangeStart)
{
  OrthancPlugins::LogInfo(std::string("Reading attachment \"") + uuid + "\"");

  std::string path = GetAbsolutePath(uuid, customData).string();

  if (!Orthanc::SystemToolbox::IsRegularFile(path))
  {
    OrthancPlugins::LogError(std::string("The path does not point to a regular file: ") + path);
    return OrthancPluginErrorCode_InexistentFile;
  }

  try
  {
    fs::ifstream f;
    f.open(path, std::ifstream::in | std::ifstream::binary);
    if (!f.good())
    {
      OrthancPlugins::LogError(std::string("The path does not point to a regular file: ") + path);
      return OrthancPluginErrorCode_InexistentFile;
    }

    f.seekg(rangeStart, std::ios::beg);

    // The ReadRange uses a target that has already been allocated by orthanc
    f.read(reinterpret_cast<char*>(target->data), target->size);

    f.close();
  }
  catch (...)
  {
    OrthancPlugins::LogError(std::string("Unexpected error while reading: ") + path);
    return OrthancPluginErrorCode_StorageAreaPlugin;
  }

  return OrthancPluginErrorCode_Success;
}


OrthancPluginErrorCode StorageRemove (const char* uuid,
                                      const char* customData,
                                      OrthancPluginContentType type)
{
  // LOG(INFO) << "Deleting attachment \"" << uuid << "\" of type " << static_cast<int>(type);

  fs::path p = GetAbsolutePath(uuid, customData);

  try
  {
    fs::remove(p);
  }
  catch (...)
  {
    // Ignore the error
  }

  // Remove the empty parent directories, (ignoring the error code if these directories are not empty)

  try
  {
    fs::path parent = p.parent_path();

    while (parent != absoluteRootPath_)
    {
      fs::remove(parent);
      parent = parent.parent_path();
    }
  }
  catch (...)
  {
    // Ignore the error
  }

  return OrthancPluginErrorCode_Success;
}

extern "C"
{

  ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* context)
  {
    OrthancPlugins::SetGlobalContext(context);
    Orthanc::Logging::InitializePluginContext(context);

    /* Check the version of the Orthanc core */
    if (OrthancPluginCheckVersion(context) == 0)
    {
      OrthancPlugins::ReportMinimalOrthancVersion(ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER,
                                                  ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER,
                                                  ORTHANC_PLUGINS_MINIMAL_REVISION_NUMBER);
      return -1;
    }

    OrthancPlugins::LogWarning("AdvancedStorage plugin is initializing");
    OrthancPluginSetDescription(context, "Provides alternative layout for your storage.");

    OrthancPlugins::OrthancConfiguration orthancConfiguration;

    OrthancPlugins::OrthancConfiguration advancedStorage;
    orthancConfiguration.GetSection(advancedStorage, "AdvancedStorage");

    bool enabled = advancedStorage.GetBooleanValue("Enable", false);
    if (enabled)
    {
      /*
        {
          "AdvancedStorage": {
            
            // Enables/disables the plugin
            "Enable": false,
          }
        }
      */

      absoluteRootPath_ = fs::absolute(fs::path(orthancConfiguration.GetStringValue("StorageDirectory", "OrthancStorage")));
      LOG(WARNING) << "AdvancedStorage - Path to the storage area: " << absoluteRootPath_.string();

      OrthancPluginRegisterStorageArea3(context, StorageCreateInstance, StorageCreateAttachment, StorageReadWhole, StorageReadRange, StorageRemove);
    }
    else
    {
      OrthancPlugins::LogWarning("AdvancedStorage plugin is disabled by the configuration file");
    }

    return 0;
  }


  ORTHANC_PLUGINS_API void OrthancPluginFinalize()
  {
    OrthancPlugins::LogWarning("AdvancedStorage plugin is finalizing");
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
  {
    return "advanced-storage";
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion()
  {
    return ORTHANC_PLUGIN_VERSION;
  }
}
