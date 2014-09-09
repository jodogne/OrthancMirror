/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2014 Medical Physics Department, CHU of Liege,
 * Belgium
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 **/


#include <OrthancCPlugin.h>

#include <string.h>
#include <stdio.h>
#include <string>

static OrthancPluginContext* context = NULL;


static std::string GetPath(const char* uuid)
{
  return "plugin_" + std::string(uuid);
}


static int32_t StorageCreate(const char* uuid,
                             const void* content,
                             int64_t size,
                             OrthancPluginContentType type)
{
  std::string path = GetPath(uuid);

  FILE* fp = fopen(path.c_str(), "wb");
  if (!fp)
  {
    return -1;
  }

  bool ok = fwrite(content, size, 1, fp) == 1;
  fclose(fp);

  return ok ? 0 : -1;
}


static int32_t StorageRead(void** content,
                           int64_t* size,
                           const char* uuid,
                           OrthancPluginContentType type)
{
  std::string path = GetPath(uuid);

  FILE* fp = fopen(path.c_str(), "rb");
  if (!fp)
  {
    return -1;
  }

  if (fseek(fp, 0, SEEK_END) < 0)
  {
    fclose(fp);
    return -1;
  }

  *size = ftell(fp);

  if (fseek(fp, 0, SEEK_SET) < 0)
  {
    fclose(fp);
    return -1;
  }

  bool ok = true;

  if (*size == 0)
  {
    *content = NULL;
  }
  else
  {
    *content = malloc(*size);
    if (content == NULL ||
        fread(*content, *size, 1, fp) != 1)
    {
      ok = false;
    }
  }

  fclose(fp);

  return ok ? 0 : -1;  
}


static int32_t StorageRemove(const char* uuid,
                             OrthancPluginContentType type)
{
  std::string path = GetPath(uuid);
  return remove(path.c_str());
}


extern "C"
{
  ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* c)
  {
    char info[1024];

    context = c;
    OrthancPluginLogWarning(context, "Storage plugin is initializing");

    /* Check the version of the Orthanc core */
    if (OrthancPluginCheckVersion(c) == 0)
    {
      sprintf(info, "Your version of Orthanc (%s) must be above %d.%d.%d to run this plugin",
              c->orthancVersion,
              ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER,
              ORTHANC_PLUGINS_MINIMAL_REVISION_NUMBER);
      OrthancPluginLogError(context, info);
      return -1;
    }

    OrthancPluginRegisterStorageArea(context, StorageCreate, StorageRead, StorageRemove);

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
