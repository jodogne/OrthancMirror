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

fs::path rootPath_;
bool multipleStoragesEnabled_ = false;
std::map<std::string, fs::path> rootPaths_;
std::string currentStorageId_;
std::string namingScheme_;
bool fsyncOnWrite_ = true;
size_t maxPathLength_ = 256;
size_t legacyPathLength = 39; // ex "/00/f7/00f7fd8b-47bd8c3a-ff917804-d180cdbc-40cf9527"

fs::path GetRootPath()
{
  if (multipleStoragesEnabled_)
  {
    return rootPaths_[currentStorageId_];
  }

  return rootPath_;
}

fs::path GetRootPath(const std::string& storageId)
{
  if (multipleStoragesEnabled_)
  {
    if (rootPaths_.find(storageId) == rootPaths_.end())
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_ParameterOutOfRange, std::string("Advanced Storage - storage '" + storageId + "' is not defined in configuration"));
    }
    return rootPaths_[storageId];
  }

  return rootPath_;
}


fs::path GetLegacyRelativePath(const std::string& uuid)
{
  if (!Orthanc::Toolbox::IsUuid(uuid))
  {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_ParameterOutOfRange);
  }

  fs::path path;

  path /= std::string(&uuid[0], &uuid[2]);
  path /= std::string(&uuid[2], &uuid[4]);
  path /= uuid;

#if BOOST_HAS_FILESYSTEM_V3 == 1
  path.make_preferred();
#endif

  return path;
}

fs::path GetPath(const std::string& uuid, const std::string& customDataString)
{
  fs::path path;

  if (!customDataString.empty())
  {
    Json::Value customData;
    Orthanc::Toolbox::ReadJson(customData, customDataString);

    if (customData["Version"].asInt() == 1)
    {
      if (customData.isMember("StorageId"))
      {
        path = GetRootPath(customData["StorageId"].asString());
      }
      else
      {
        path = GetRootPath();
      }
      
      if (customData.isMember("Path"))
      {
        path /= customData["Path"].asString();
      }
      else
      { // we are in "legacy mode" for the path part
        path /= GetLegacyRelativePath(uuid);
      }
    }
    else
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_ParameterOutOfRange, std::string("Advanced Storage - unknown version for custom data '" + boost::lexical_cast<std::string>(customData["Version"].asInt()) + "'"));
    }
  }
  else // we are in "legacy mode"
  {
    path = GetRootPath();
    path /= GetLegacyRelativePath(uuid);
  }

  path.make_preferred();
  return path;
}

void GetCustomData(std::string& output, const fs::path& path)
{
  // if we use defaults, non need to store anything in the metadata, the plugin has the same behavior as the core of Orthanc
  if (namingScheme_ == "OrthancDefault" && !multipleStoragesEnabled_)
  {
    return;
  }

  Json::Value customDataJson;
  customDataJson["Version"] = 1;

  if (namingScheme_ != "OrthancDefault")
  { // no need to store the pathc since we are in the default mode
    customDataJson["Path"] = path.string();
  }

  if (multipleStoragesEnabled_)
  {
    customDataJson["StorageId"] = currentStorageId_;
  }

  return  Orthanc::Toolbox::WriteFastJson(output, customDataJson);
}

void AddSplitDateDicomTagToPath(fs::path& path, const Json::Value& tags, const char* tagName, const char* defaultValue = NULL)
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

void AddStringDicomTagToPath(fs::path& path, const Json::Value& tags, const char* tagName, const char* defaultValue = NULL)
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

std::string GetExtension(OrthancPluginContentType type, bool isCompressed)
{
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
  
  return extension;
}

fs::path GetRelativePathFromTags(const Json::Value& tags, const char* uuid, OrthancPluginContentType type, bool isCompressed)
{
  fs::path path;

  if (!tags.isNull())
  { 
    if (namingScheme_ == "Preset1-StudyDatePatientID")
    {
      if (!tags.isMember("StudyDate"))
      {
        LOG(WARNING) << "AdvancedStorage - No 'StudyDate' in attachment " << uuid << ".  Attachment will be stored in NO_STUDY_DATE folder";
      }

      AddSplitDateDicomTagToPath(path, tags, "StudyDate", "NO_STUDY_DATE");
      AddStringDicomTagToPath(path, tags, "PatientID");  // no default value, tag is always present if the instance is accepted by Orthanc  
      
      if (tags.isMember("PatientName") && tags["PatientName"].isString() && !tags["PatientName"].asString().empty())
      {
        path += std::string(" - ") + tags["PatientName"].asString();
      }

      AddStringDicomTagToPath(path, tags, "StudyDescription");
      AddStringDicomTagToPath(path, tags, "SeriesInstanceUID");

      path /= uuid;
      path += GetExtension(type, isCompressed);
      return path;
    }
  }

  return GetLegacyRelativePath(uuid);
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
  std::string customDataString;
  GetCustomData(customDataString, relativePath);

  fs::path rootPath = GetRootPath();
  fs::path path = rootPath / relativePath;

  LOG(INFO) << "Advanced Storage - creating attachment \"" << uuid << "\" of type " << static_cast<int>(type) << " (path = " + path.string() + ")";

  // check that the final path is not 'above' the root path (this could happen if e.g., a PatientName is ../../../../toto)
  std::string canonicalPath = fs::canonical(path).string();
  if (!Orthanc::Toolbox::StartsWith(canonicalPath, rootPath.string()))
  {
    throw Orthanc::OrthancException(Orthanc::ErrorCode_InternalError, std::string("Advanced Storage - final path is above root: '") + canonicalPath + "' - '" + rootPath.string() + "'") ;
  }

  // check path length !!!!!, if too long, go back to legacy path and issue a warning
  if (path.string().size() > maxPathLength_)
  {
    fs::path legacyPath = rootPath / GetLegacyRelativePath(uuid);
    LOG(WARNING) << "Advanced Storage - WAS01 - Path is too long: '" << path.string() << "' will be stored in '" << legacyPath << "'";
    path = legacyPath;
  }

  if (fs::exists(path))
  {
    // Extremely unlikely case if uuid is included in the path: This Uuid has already been created
    // in the past.
    throw Orthanc::OrthancException(Orthanc::ErrorCode_InternalError, "Advanced Storage - path already exists");

    // TODO for the future: handle duplicates path (e.g: there's no uuid in the path and we are uploading the same file again)
  }

  if (fs::exists(path.parent_path()))
  {
    if (!fs::is_directory(path.parent_path()))
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_DirectoryOverFile);
    }
  }
  else
  {
    if (!fs::create_directories(path.parent_path()))
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_FileStorageCannotWrite);
    }
  }

  Orthanc::SystemToolbox::WriteFile(content, size, path.string(), fsyncOnWrite_);

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
  std::string path = GetPath(uuid, customData).string();

  LOG(INFO) << "Advanced Storage - Reading whole attachment \"" << uuid << "\" of type " << static_cast<int>(type) << " (path = " + path + ")";

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
  std::string path = GetPath(uuid, customData).string();

  LOG(INFO) << "Advanced Storage - Reading range of attachment \"" << uuid << "\" of type " << static_cast<int>(type) << " (path = " + path + ")";

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
  fs::path path = GetPath(uuid, customData);

  LOG(INFO) << "Advanced Storage - Deleting attachment \"" << uuid << "\" of type " << static_cast<int>(type) << " (path = " + path.string() + ")";

  try
  {
    fs::remove(path);
  }
  catch (...)
  {
    // Ignore the error
  }

  // Remove the empty parent directories, (ignoring the error code if these directories are not empty)

  try
  {
    fs::path parent = path.parent_path();

    while (parent != GetRootPath())
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

            // Enables/disables support for multiple StorageDirectories
            "MultipleStorages" : {
              "Storages" : {
                // The storgae ids below may never change since they are stored in DB
                // The storage path may change in case you move your data from one place to the other
                "1" : "/var/lib/orthanc/db",
                "2" : "/mnt/disk2/orthanc"
              },

              // the storage on which new data is stored.
              // There's currently no automatic changes of disks
              "CurrentStorage" : "2",
            },

            // Defines the storage structure and file namings.  Right now, 
            // only the "OrthancDefault" value shall be used in a production environment.  
            // All other values are currently experimental
            // "OrthancDefault" = same structure and file naming as default orthanc, 
            // "Preset1-StudyDatePatientID" = split(StudyDate)/PatientID - PatientName/StudyDescription/SeriesInstanceUID/uuid.ext
            "NamingScheme" : "OrthancDefault",

            // Defines the maximum length for path used in the storage.  If a file is longer
            // than this limit, it is stored with the default orthanc naming scheme
            // (and a warning is issued).
            // Note, on Windows, the maximum path length is 260 bytes by default but can be increased
            // through a configuration.
            "MaxPathLength" : 256
          }
        }
      */

      fsyncOnWrite_ = orthancConfiguration.GetBooleanValue("SyncStorageArea", true);

      const Json::Value& pluginJson = advancedStorage.GetJson();

      namingScheme_ = advancedStorage.GetStringValue("NamingScheme", "OrthancDefault");

      // if we have enabled multiple storage after files have been saved without this plugin, we still need the default StorageDirectory
      rootPath_ = fs::path(orthancConfiguration.GetStringValue("StorageDirectory", "OrthancStorage"));
      LOG(WARNING) << "AdvancedStorage - Path to the default storage area: " << rootPath_.string();

      maxPathLength_ = orthancConfiguration.GetIntegerValue("MaxPathLength", 256);
      LOG(WARNING) << "AdvancedStorage - Maximum path length: " << maxPathLength_;

      if (!rootPath_.is_absolute())
      {
        LOG(ERROR) << "AdvancedStorage - Path to the default storage area should be an absolute path";
        return -1;
      }

      if (rootPath_.size() > (maxPathLength_ - legacyPathLength))
      {
        LOG(ERROR) << "AdvancedStorage - Path to the default storage is too long";
        return -1;
      }

      if (pluginJson.isMember("MultipleStorages"))
      {
        multipleStoragesEnabled_ = true;
        const Json::Value& multipleStoragesJson = pluginJson["MultipleStorages"];
        
        if (multipleStoragesJson.isMember("Storages") && multipleStoragesJson.isObject() && multipleStoragesJson.isMember("CurrentStorage") && multipleStoragesJson["CurrentStorage"].isString())
        {
          const Json::Value& storagesJson = multipleStoragesJson["Storages"];
          Json::Value::Members storageIds = storagesJson.getMemberNames();
    
          for (Json::Value::Members::const_iterator it = storageIds.begin(); it != storageIds.end(); ++it)
          {
            const Json::Value& storagePath = storagesJson[*it];
            if (!storagePath.isString())
            {
              LOG(ERROR) << "AdvancedStorage - Storage path is not a string " << *it;
              return -1;
            }

            rootPaths_[*it] = storagePath.asString();

            if (!rootPaths_[*it].is_absolute())
            {
              LOG(ERROR) << "AdvancedStorage - Storage path shall be absolute path '" << storagePath.asString() << "'";
              return -1;
            }

            if (storagePath.asString().size() > (maxPathLength_ - legacyPathLength))
            {
              LOG(ERROR) << "AdvancedStorage - Storage path is too long '" << storagePath.asString() << "'";
              return -1;
            }
          }

          currentStorageId_ = multipleStoragesJson["CurrentStorage"].asString();

          if (rootPaths_.find(currentStorageId_) == rootPaths_.end())
          {
            LOG(ERROR) << "AdvancedStorage - CurrentStorage is not defined in Storages list: " << currentStorageId_;
            return -1;
          }

          LOG(WARNING) << "AdvancedStorage - multiple storages enabled.  Current storage : " << rootPaths_[currentStorageId_].string();
        }
      }

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
