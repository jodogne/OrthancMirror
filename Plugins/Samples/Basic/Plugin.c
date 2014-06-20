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

static OrthancPluginContext* context = NULL;


ORTHANC_PLUGINS_API int32_t Callback1(OrthancPluginRestOutput* output,
                                      const char* url,
                                      const OrthancPluginHttpRequest* request)
{
  char buffer[1024];
  uint32_t i;

  sprintf(buffer, "Callback on URL [%s] with body [%s]", url, request->body);
  OrthancPluginLogInfo(context, buffer);

  OrthancPluginAnswerBuffer(context, output, buffer, strlen(buffer), "text/plain");

  for (i = 0; i < request->getCount; i++)
  {
    sprintf(buffer, "  [%s] = [%s]", request->getKeys[i], request->getValues[i]);
    OrthancPluginLogInfo(context, buffer);    
  }

  printf("** %d\n", request->groupsCount);
  for (i = 0; i < request->groupsCount; i++)
  {
    printf("** [%s]\n",  request->groups[i]);
  }

  return 1;
}


ORTHANC_PLUGINS_API int32_t Callback2(OrthancPluginRestOutput* output,
                                      const char* url,
                                      const OrthancPluginHttpRequest* request)
{
  /* Answer with a sample 16bpp image. */

  uint16_t buffer[256 * 256];
  uint32_t x, y, value;

  value = 0;
  for (y = 0; y < 256; y++)
  {
    for (x = 0; x < 256; x++, value++)
    {
      buffer[value] = value;
    }
  }

  OrthancPluginCompressAndAnswerPngImage(context, output, OrthancPluginPixelFormat_Grayscale16,
                                         256, 256, sizeof(uint16_t) * 256, buffer);
  return 0;
}


ORTHANC_PLUGINS_API int32_t Callback3(OrthancPluginRestOutput* output,
                                      const char* url,
                                      const OrthancPluginHttpRequest* request)
{
  OrthancPluginMemoryBuffer dicom;
  if (!OrthancPluginGetDicomForInstance(context, &dicom, request->groups[0]))
  {
    /* No error, forward the DICOM file */
    OrthancPluginAnswerBuffer(context, output, dicom.data, dicom.size, "application/dicom");

    /* Free memory */
    OrthancPluginFreeMemoryBuffer(context, &dicom);
  }

  return 0;
}


ORTHANC_PLUGINS_API int32_t Callback4(OrthancPluginRestOutput* output,
                                      const char* url,
                                      const OrthancPluginHttpRequest* request)
{
  /* Answer with a sample 8bpp image. */

  uint8_t  buffer[256 * 256];
  uint32_t x, y, value;

  value = 0;
  for (y = 0; y < 256; y++)
  {
    for (x = 0; x < 256; x++, value++)
    {
      buffer[value] = x;
    }
  }

  OrthancPluginCompressAndAnswerPngImage(context, output, OrthancPluginPixelFormat_Grayscale8,
                                         256, 256, 256, buffer);
  return 0;
}


ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* c)
{
  OrthancPluginMemoryBuffer tmp;
  char info[1024];

  context = c;
  OrthancPluginLogWarning(context, "Sample plugin is initializing");

  sprintf(info, "The version of Orthanc is '%s'", context->orthancVersion);
  OrthancPluginLogInfo(context, info);

  OrthancPluginRegisterRestCallback(context, "/(plu.*)/hello", Callback1);
  OrthancPluginRegisterRestCallback(context, "/plu.*/image", Callback2);
  OrthancPluginRegisterRestCallback(context, "/plugin/instances/([^/]+)/info", Callback3);

  OrthancPluginRegisterRestCallback(context, "/instances/([^/]+)/preview", Callback4);

  
  printf(">> %d\n", OrthancPluginRestApiGet(context, &tmp, "/instances"));
  printf(">> [%s]\n", (const char*) tmp.data);
  OrthancPluginFreeMemoryBuffer(context, &tmp);

  return 0;
}


ORTHANC_PLUGINS_API void OrthancPluginFinalize()
{
  OrthancPluginLogWarning(context, "Sample plugin is finalizing");
}


ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
{
  return "sample";
}


ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion()
{
  return "1.0";
}

