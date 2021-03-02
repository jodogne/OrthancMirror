/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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


#include <orthanc/OrthancCPlugin.h>

#include <string.h>
#include <stdio.h>
#include <string>


static OrthancPluginContext* context = NULL;


static std::string GetPath(const char* uuid)
{
  return "plugin_" + std::string(uuid);
}


static bool ReadFile(std::string& content,
                     const std::string& path)
{
  FILE* fp = fopen(path.c_str(), "rb");
  if (!fp)
  {
    return false;
  }

  if (fseek(fp, 0, SEEK_END) < 0)
  {
    fclose(fp);
    return false;
  }

  long size = ftell(fp);

  if (fseek(fp, 0, SEEK_SET) < 0)
  {
    fclose(fp);
    return false;
  }
  else
  {
    content.resize(size);

    if (size != 0)
    {
      bool success = (fread(&content[0], size, 1, fp) == 1);
      fclose(fp);
      return success;
    }
    else
    {
      fclose(fp);
      return true;
    }
  }
}


static OrthancPluginErrorCode StorageCreate(const char* uuid,
                                            const void* content,
                                            int64_t size,
                                            OrthancPluginContentType type)
{
  std::string path = GetPath(uuid);

  FILE* fp = fopen(path.c_str(), "wb");
  if (!fp)
  {
    return OrthancPluginErrorCode_StorageAreaPlugin;
  }

  bool ok = fwrite(content, size, 1, fp) == 1;
  fclose(fp);

  return ok ? OrthancPluginErrorCode_Success : OrthancPluginErrorCode_StorageAreaPlugin;
}


#if USE_LEGACY_API == 1
static OrthancPluginErrorCode StorageRead(void** content,
                                          int64_t* size,
                                          const char* uuid,
                                          OrthancPluginContentType type)
{
  const std::string path = GetPath(uuid);

  std::string s;
  if (ReadFile(s, path))
  {
    *size = s.size();

    if (s.size() == 0)
    {
      *content = NULL;
    }
    else
    {
      *content = malloc(s.size());
      if (*content == NULL)
      {
        return OrthancPluginErrorCode_StorageAreaPlugin;
      }

      if (!s.empty())
      {
        memcpy(*content, s.c_str(), s.size());
      }
    }

    return OrthancPluginErrorCode_Success;
  }
  else
  {
    return OrthancPluginErrorCode_StorageAreaPlugin;
  }
}

#else

static OrthancPluginErrorCode StorageReadWhole(OrthancPluginMemoryBuffer64* target,
                                               const char* uuid,
                                               OrthancPluginContentType type)
{
  const std::string path = GetPath(uuid);

  std::string s;
  if (ReadFile(s, path))
  {
    if (OrthancPluginCreateMemoryBuffer64(context, target, s.size()) != OrthancPluginErrorCode_Success)
    {
      return OrthancPluginErrorCode_NotEnoughMemory;
    }

    if (!s.empty())
    {
      memcpy(target->data, s.c_str(), s.size());
    }

    return OrthancPluginErrorCode_Success;
  }
  else
  {
    return OrthancPluginErrorCode_StorageAreaPlugin;
  }  
}

static OrthancPluginErrorCode StorageReadRange(OrthancPluginMemoryBuffer64* target,
                                               const char* uuid,
                                               OrthancPluginContentType type,
                                               uint64_t rangeStart)
{
  const size_t rangeSize = target->size;  // The buffer is allocated by Orthanc
  const std::string path = GetPath(uuid);

  std::string s;

  if (rangeSize == 0)
  {
    return OrthancPluginErrorCode_Success;
  }
  else if (ReadFile(s, path))
  {
    if (rangeStart + rangeSize > s.size())
    {
      return OrthancPluginErrorCode_BadRange;
    }
    else
    {
      memcpy(target->data, &s[rangeStart], rangeSize);
    }

    return OrthancPluginErrorCode_Success;
  }
  else
  {
    return OrthancPluginErrorCode_StorageAreaPlugin;
  }  
}

#endif


static OrthancPluginErrorCode StorageRemove(const char* uuid,
                                            OrthancPluginContentType type)
{
  std::string path = GetPath(uuid);

  if (remove(path.c_str()) == 0)
  {
    return OrthancPluginErrorCode_Success;
  }
  else
  {
    return OrthancPluginErrorCode_StorageAreaPlugin;
  }
}


extern "C"
{
  ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* c)
  {
    context = c;
    OrthancPluginLogWarning(context, "Storage plugin is initializing");

    /* Check the version of the Orthanc core */
    if (OrthancPluginCheckVersion(c) == 0)
    {
      char info[1024];
      sprintf(info, "Your version of Orthanc (%s) must be above %d.%d.%d to run this plugin",
              c->orthancVersion,
              ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_REVISION_NUMBER);
      OrthancPluginLogError(context, info);
      return -1;
    }

#if USE_LEGACY_API == 1
    OrthancPluginRegisterStorageArea(context, StorageCreate, StorageRead, StorageRemove);
#else
    OrthancPluginRegisterStorageArea2(context, StorageCreate, StorageReadWhole, StorageReadRange, StorageRemove);
#endif

    return 0;
  }


  ORTHANC_PLUGINS_API void OrthancPluginFinalize()
  {
    OrthancPluginLogWarning(context, "Storage plugin is finalizing");
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
  {
    return "storage";
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion()
  {
    return "1.0";
  }
}
