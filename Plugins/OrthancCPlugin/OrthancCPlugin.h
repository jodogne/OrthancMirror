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


#pragma once


#ifdef _MSC_VER
#include "../../Resources/VisualStudio/stdint.h"
#else
#include <stdint.h>
#endif


#ifdef WIN32
#define ORTHANC_PLUGINS_API __declspec(dllexport)
#else
#define ORTHANC_PLUGINS_API
#endif


#include <stdlib.h>


#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct OrthancRestOutput_t OrthancRestOutput;

  typedef enum
  {
    OrthancHttpMethod_Get = 1,
    OrthancHttpMethod_Post = 2,
    OrthancHttpMethod_Put = 3,
    OrthancHttpMethod_Delete = 4
  } OrthancHttpMethod;

  typedef struct OrthancRestUrl_t
  {
    const char* path;
    const char* const* components;
    uint32_t componentsSize;
    const char* const* parameters;
    uint32_t parametersSize;                                          
  } OrthancRestUrl;


  typedef int32_t (*OrthancPluginService) (const char* serviceName,
                                           const void* serviceParameters);

  typedef int32_t (*OrthancRestCallback) (OrthancRestOutput* output,
                                          OrthancHttpMethod method,
                                          const OrthancRestUrl* url,
                                          const char* body,
                                          uint32_t bodySize);

  typedef struct OrthancPluginContext_t
  {
    void* pimpl;

    const char* orthancVersion;
    void (*FreeBuffer) (void* buffer);

    /* Logging functions */
    void (*LogError) (const char* str);
    void (*LogWarning) (const char* str);
    void (*LogInfo) (const char* str);

    /* REST API */
    void (*RegisterRestCallback) (const struct OrthancPluginContext_t* context,
                                  const char* path, 
                                  OrthancRestCallback callback);

    void (*AnswerBuffer) (OrthancRestOutput* output,
                          const char* answer,
                          uint32_t answerSize,
                          const char* mimeType);
  } OrthancPluginContext;


  /**
     Each plugin must define 4 functions, whose signature are:
     - int32_t OrthancPluginInitialize(const OrthancPluginContext*);
     - void OrthancPluginFinalize();
     - const char* OrthancPluginGetName();
     - const char* OrthancPluginGetVersion();

     nm -C -D --defined-only libPluginTest.so
   **/

#ifdef  __cplusplus
}
#endif
