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

  sprintf(buffer, "Callback on URL [%s] with body [%s]\n", url, request->body);
  OrthancPluginLogWarning(context, buffer);

  OrthancPluginSetCookie(context, output, "hello", "world");
  OrthancPluginAnswerBuffer(context, output, buffer, strlen(buffer), "text/plain");

  OrthancPluginLogWarning(context, "");    

  for (i = 0; i < request->groupsCount; i++)
  {
    sprintf(buffer, "  REGEX GROUP %d = [%s]", i, request->groups[i]);
    OrthancPluginLogWarning(context, buffer);    
  }

  OrthancPluginLogWarning(context, "");    

  for (i = 0; i < request->getCount; i++)
  {
    sprintf(buffer, "  GET [%s] = [%s]", request->getKeys[i], request->getValues[i]);
    OrthancPluginLogWarning(context, buffer);    
  }

  OrthancPluginLogWarning(context, "");    

  for (i = 0; i < request->headersCount; i++)
  {
    sprintf(buffer, "  HEADERS [%s] = [%s]", request->headersKeys[i], request->headersValues[i]);
    OrthancPluginLogWarning(context, buffer);    
  }

  OrthancPluginLogWarning(context, "");

  return 1;
}


ORTHANC_PLUGINS_API int32_t Callback2(OrthancPluginRestOutput* output,
                                      const char* url,
                                      const OrthancPluginHttpRequest* request)
{
  /* Answer with a sample 16bpp image. */

  uint16_t buffer[256 * 256];
  uint32_t x, y, value;

  if (request->method != OrthancPluginHttpMethod_Get)
  {
    OrthancPluginSendMethodNotAllowed(context, output, "GET");
  }
  else
  {
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
  }

  return 0;
}


ORTHANC_PLUGINS_API int32_t Callback3(OrthancPluginRestOutput* output,
                                      const char* url,
                                      const OrthancPluginHttpRequest* request)
{
  if (request->method != OrthancPluginHttpMethod_Get)
  {
    OrthancPluginSendMethodNotAllowed(context, output, "GET");
  }
  else
  {
    OrthancPluginMemoryBuffer dicom;
    if (!OrthancPluginGetDicomForInstance(context, &dicom, request->groups[0]))
    {
      /* No error, forward the DICOM file */
      OrthancPluginAnswerBuffer(context, output, dicom.data, dicom.size, "application/dicom");

      /* Free memory */
      OrthancPluginFreeMemoryBuffer(context, &dicom);
    }
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

  if (request->method != OrthancPluginHttpMethod_Get)
  {
    OrthancPluginSendMethodNotAllowed(context, output, "GET");
  }
  else
  {
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
  }

  return 0;
}


ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* c)
{
  const char* pathLocator = "\"Path\" : \"";
  OrthancPluginMemoryBuffer tmp;
  char info[1024];
  char *id, *eos;

  context = c;
  OrthancPluginLogWarning(context, "Sample plugin is initializing");

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

  sprintf(info, "The version of Orthanc is '%s'", context->orthancVersion);
  OrthancPluginLogInfo(context, info);

  OrthancPluginRegisterRestCallback(context, "/(plu.*)/hello", Callback1);
  OrthancPluginRegisterRestCallback(context, "/plu.*/image", Callback2);
  OrthancPluginRegisterRestCallback(context, "/plugin/instances/([^/]+)/info", Callback3);

  OrthancPluginRegisterRestCallback(context, "/instances/([^/]+)/preview", Callback4);

  /* Make REST requests to the built-in Orthanc API */
  OrthancPluginRestApiGet(context, &tmp, "/changes");
  OrthancPluginFreeMemoryBuffer(context, &tmp);
  OrthancPluginRestApiGet(context, &tmp, "/changes?limit=1");
  OrthancPluginFreeMemoryBuffer(context, &tmp);
 
  /* Make POST request to create a new DICOM instance */
  sprintf(info, "{\"PatientName\":\"Test\"}");
  OrthancPluginRestApiPost(context, &tmp, "/tools/create-dicom", info, strlen(info));

  /**
   * Recover he ID of the created instance is constructed by a
   * quick-and-dirty parsing of a JSON string.
   **/
  id = strstr((char*) tmp.data, pathLocator) + strlen(pathLocator);
  eos = strchr(id, '\"');
  eos[0] = '\0';

  /* Delete the newly created DICOM instance. */
  OrthancPluginRestApiDelete(context, id);
  OrthancPluginFreeMemoryBuffer(context, &tmp);

  /* Play with PUT by defining a new target modality. */
  sprintf(info, "[ \"STORESCP\", \"localhost\", 2000 ]");
  OrthancPluginRestApiPut(context, &tmp, "/modalities/demo", info, strlen(info));

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

