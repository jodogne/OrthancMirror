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


ORTHANC_PLUGINS_API int32_t Callback(OrthancPluginRestOutput* output,
                                     OrthancPluginHttpMethod method,
                                     const char* url,
                                     const char* const* getKeys,
                                     const char* const* getValues,
                                     uint32_t getSize,
                                     const char* body,
                                     uint32_t bodySize)
{
  char buffer[1024];
  uint32_t i;

  sprintf(buffer, "Callback on URL [%s] with body [%s]", url, body);
  OrthancPluginLogInfo(context, buffer);

  context->AnswerBuffer(output, buffer, strlen(buffer), "text/plain");

  for (i = 0; i < getSize; i++)
  {
    sprintf(buffer, "  [%s] = [%s]", getKeys[i], getValues[i]);
    OrthancPluginLogInfo(context, buffer);    
  }

  return 1;
}


ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* c)
{
  char info[1024];

  context = c;
  OrthancPluginLogWarning(context, "Plugin is initializing");

  sprintf(info, "The version of Orthanc is '%s'", context->orthancVersion);
  OrthancPluginLogInfo(context, info);

  context->RegisterRestCallback(c, "/plu.*/hello", Callback);

  return 0;
}


ORTHANC_PLUGINS_API void OrthancPluginFinalize()
{
  OrthancPluginLogWarning(context, "Plugin is finalizing");
}


ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
{
  return "sample";
}


ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion()
{
  return "1.0";
}

