/**
 * @defgroup CInterface C Interface 
 * @brief The C interface to create Orthanc plugins.
 * 
 * These functions must be used to create C plugins for Orthanc.
 **/


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


#include <stdio.h>
#include <string.h>

#ifdef WIN32
#define ORTHANC_PLUGINS_API __declspec(dllexport)
#else
#define ORTHANC_PLUGINS_API
#endif



/********************************************************************
 ** Check that function inlining is properly supported. The use of
 ** inlining is required, to avoid the duplication of object code
 ** between two compilation modules that would use the Orthanc Plugin
 ** API.
 ********************************************************************/

/* If the auto-detection of the "inline" keyword below does not work
   automatically and that your compiler is known to properly support
   inlining, uncomment the following #define and adapt the definition
   of "static inline". */

/* #define ORTHANC_PLUGIN_INLINE static inline */

#ifndef ORTHANC_PLUGIN_INLINE
#  if __STDC_VERSION__ >= 199901L
/*   This is C99 or above: http://predef.sourceforge.net/prestd.html */
#    define ORTHANC_PLUGIN_INLINE static inline
#  elif defined(__cplusplus)
/*   This is C++ */
#    define ORTHANC_PLUGIN_INLINE static inline
#  elif defined(__GNUC__)
/*   This is GCC running in C89 mode */
#    define ORTHANC_PLUGIN_INLINE static __inline
#  elif defined(_MSC_VER)
/*   This is Visual Studio running in C89 mode */
#    define ORTHANC_PLUGIN_INLINE static __inline
#  else
#    error Your compiler is not known to support the "inline" keyword
#  endif
#endif



/********************************************************************
 ** Inclusion of standard libaries.
 ********************************************************************/

#ifdef _MSC_VER
#include "../../Resources/VisualStudio/stdint.h"
#else
#include <stdint.h>
#endif

#include <stdlib.h>



/********************************************************************
 ** Definition of the Orthanc Plugin API.
 ********************************************************************/

/** @{ */

#ifdef __cplusplus
extern "C"
{
#endif

  typedef enum
  {
    OrthancPluginHttpMethod_Get = 1,
    OrthancPluginHttpMethod_Post = 2,
    OrthancPluginHttpMethod_Put = 3,
    OrthancPluginHttpMethod_Delete = 4
  } OrthancPluginHttpMethod;

  typedef struct
  {
    OrthancPluginHttpMethod method;

    /* For GET requests */
    const char* const*      getKeys;
    const char* const*      getValues;
    uint32_t                getCount;

    /* For POST and PUT requests */
    const char*             body;
    uint32_t                bodySize;
  } OrthancPluginHttpRequest;

  typedef enum 
  {
    /* Generic services */
    OrthancPluginService_LogInfo = 1,
    OrthancPluginService_LogWarning = 2,
    OrthancPluginService_LogError = 3,

    /* Registration of callbacks */
    OrthancPluginService_RegisterRestCallback = 1000,

    /* Sending answers to REST calls */
    OrthancPluginService_AnswerBuffer = 2000
  } OrthancPluginService;




  typedef struct _OrthancPluginRestOutput_t OrthancPluginRestOutput;

  typedef int32_t (*OrthancPluginRestCallback) (
    OrthancPluginRestOutput* output,
    const char* url,
    const OrthancPluginHttpRequest* request);

  typedef struct _OrthancPluginContext_t
  {
    void*        pluginsManager;
    const char*  orthancVersion;
    void       (*FreeBuffer) (void* buffer);
    int32_t    (*InvokeService) (struct _OrthancPluginContext_t* context,
                                 OrthancPluginService service,
                                 const void* params);
  } OrthancPluginContext;


  typedef struct
  {
    const char* pathRegularExpression;
    OrthancPluginRestCallback callback;
  } _OrthancPluginRestCallbackParams;


  typedef struct
  {
    OrthancPluginRestOutput* output;
    const char*              answer;
    uint32_t                 answerSize;
    const char*              mimeType;
  } _OrthancPluginAnswerBufferParams;


  ORTHANC_PLUGIN_INLINE void OrthancPluginLogError(
    OrthancPluginContext* context,
    const char* str)
  {
    context->InvokeService(context, OrthancPluginService_LogError, str);
  }


  ORTHANC_PLUGIN_INLINE void OrthancPluginLogWarning(
    OrthancPluginContext* context,
    const char* str)
  {
    context->InvokeService(context, OrthancPluginService_LogWarning, str);
  }


  ORTHANC_PLUGIN_INLINE void OrthancPluginLogInfo(
    OrthancPluginContext* context,
    const char* str)
  {
    context->InvokeService(context, OrthancPluginService_LogInfo, str);
  }


  ORTHANC_PLUGIN_INLINE void OrthancPluginRegisterRestCallback(
    OrthancPluginContext*     context,
    const char*               pathRegularExpression,
    OrthancPluginRestCallback callback)
  {
    _OrthancPluginRestCallbackParams params;
    params.pathRegularExpression = pathRegularExpression;
    params.callback = callback;
    context->InvokeService(context, OrthancPluginService_RegisterRestCallback, &params);
  }


  ORTHANC_PLUGIN_INLINE void OrthancPluginAnswerBuffer(
    OrthancPluginContext*    context,
    OrthancPluginRestOutput* output,
    const char*              answer,
    uint32_t                 answerSize,
    const char*              mimeType)
  {
    _OrthancPluginAnswerBufferParams params;
    params.output = output;
    params.answer = answer;
    params.answerSize = answerSize;
    params.mimeType = mimeType;
    context->InvokeService(context, OrthancPluginService_AnswerBuffer, &params);
  }



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


/** @} */

