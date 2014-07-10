/**
 * \mainpage
 *
 * This C/C++ SDK allows external developers to create plugins that
 * can be loaded into Orthanc to extend its functionality. Each
 * Orthanc plugin must expose 4 public functions with the following
 * signatures:
 * 
 * -# <tt>int32_t OrthancPluginInitialize(const OrthancPluginContext* context)</tt>:
 *    This function is invoked by Orthanc when it loads the plugin on startup.
 *    The plugin must store the context pointer so that it can use the plugin 
 *    services of Orthanc. It must also register all its callbacks using
 *    ::OrthancPluginRegisterRestCallback().
 * -# <tt>void OrthancPluginFinalize()</tt>:
 *    This function is invoked by Orthanc during its shutdown. The plugin
 *    must free all its memory.
 * -# <tt>const char* OrthancPluginGetName()</tt>:
 *    The plugin must return a short string to identify itself.
 * -# <tt>const char* OrthancPluginGetVersion()</tt>
 *    The plugin must return a string containing its version number.
 *
 * The name and the version of a plugin is only used to prevent it
 * from being loaded twice.
 * 
 * 
 **/



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
 ** Inclusion of standard libraries.
 ********************************************************************/

#ifdef _MSC_VER
#include "../../Resources/ThirdParty/VisualStudio/stdint.h"
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

  /**
   * The various HTTP methods for a REST call.
   **/
  typedef enum
  {
    OrthancPluginHttpMethod_Get = 1,    /*!< GET request */
    OrthancPluginHttpMethod_Post = 2,   /*!< POST request */
    OrthancPluginHttpMethod_Put = 3,    /*!< PUT request */
    OrthancPluginHttpMethod_Delete = 4  /*!< DELETE request */
  } OrthancPluginHttpMethod;


  /**
   * @brief The parameters of a REST request.
   **/
  typedef struct
  {
    /**
     * @brief The HTTP method.
     **/
    OrthancPluginHttpMethod method;    

    /**
     * @brief The number of groups of the regular expression.
     **/
    uint32_t                groupsCount;

    /**
     * @brief The matched values for the groups of the regular expression.
     **/
    const char* const*      groups;

    /**
     * @brief For a GET request, the number of GET parameters.
     **/
    uint32_t                getCount;

    /**
     * @brief For a GET request, the keys of the GET parameters.
     **/
    const char* const*      getKeys;

    /**
     * @brief For a GET request, the values of the GET parameters.
     **/
    const char* const*      getValues;

    /**
     * @brief For a PUT or POST request, the content of the body.
     **/
    const char*             body;

    /**
     * @brief For a PUT or POST request, the number of bytes of the body.
     **/
    uint32_t                bodySize;
  } OrthancPluginHttpRequest;

  typedef enum 
  {
    /* Generic services */
    _OrthancPluginService_LogInfo = 1,
    _OrthancPluginService_LogWarning = 2,
    _OrthancPluginService_LogError = 3,

    /* Registration of callbacks */
    _OrthancPluginService_RegisterRestCallback = 1000,

    /* Sending answers to REST calls */
    _OrthancPluginService_AnswerBuffer = 2000,
    _OrthancPluginService_CompressAndAnswerPngImage = 2001,
    _OrthancPluginService_Redirect = 2002,

    /* Access to the Orthanc database and API */
    _OrthancPluginService_GetDicomForInstance = 3000,
    _OrthancPluginService_RestApiGet = 3001,
    _OrthancPluginService_RestApiPost = 3002,
    _OrthancPluginService_RestApiDelete = 3003,
    _OrthancPluginService_RestApiPut = 3004
  } _OrthancPluginService;



  /**
   * The memory layout of the pixels of an image.
   **/
  typedef enum
  {
    /**
     * @brief Graylevel 8bpp image.
     *
     * The image is graylevel. Each pixel is unsigned and stored in
     * one byte.
     **/
    OrthancPluginPixelFormat_Grayscale8 = 1,

    /**
     * @brief Graylevel, unsigned 16bpp image.
     *
     * The image is graylevel. Each pixel is unsigned and stored in
     * two bytes.
     **/
    OrthancPluginPixelFormat_Grayscale16 = 2,

    /**
     * @brief Graylevel, signed 16bpp image.
     *
     * The image is graylevel. Each pixel is signed and stored in two
     * bytes.
     **/
    OrthancPluginPixelFormat_SignedGrayscale16 = 3,

    /**
     * @brief Color image in RGB24 format.
     *
     * This format describes a color image. The pixels are stored in 3
     * consecutive bytes. The memory layout is RGB.
     **/
    OrthancPluginPixelFormat_RGB24 = 4,

    /**
     * @brief Color image in RGBA32 format.
     *
     * This format describes a color image. The pixels are stored in 4
     * consecutive bytes. The memory layout is RGBA.
     **/
    OrthancPluginPixelFormat_RGBA32 = 5
  } OrthancPluginPixelFormat;


  /**
   * @brief A memory buffer allocated by the core system of Orthanc.
   *
   * A memory buffer allocated by the core system of Orthanc. When the
   * content of the buffer is not useful anymore, it must be free by a
   * call to ::OrthancPluginFreeMemoryBuffer().
   **/
  typedef struct
  {
    /**
     * @brief The content of the buffer.
     **/
    void*      data;

    /**
     * @brief The number of bytes in the buffer.
     **/
    uint32_t   size;
  } OrthancPluginMemoryBuffer;




  /**
   * @brief Opaque structure that represents the HTTP connection to the client application.
   **/
  typedef struct _OrthancPluginRestOutput_t OrthancPluginRestOutput;


  /**
   * @brief Signature of a callback function that answers to a REST request.
   **/
  typedef int32_t (*OrthancPluginRestCallback) (
    OrthancPluginRestOutput* output,
    const char* url,
    const OrthancPluginHttpRequest* request);


  /**
   * @brief Opaque structure that contains information about the Orthanc core.
   **/
  typedef struct _OrthancPluginContext_t
  {
    void*        pluginsManager;
    const char*  orthancVersion;
    void       (*Free) (void* buffer);
    int32_t    (*InvokeService) (struct _OrthancPluginContext_t* context,
                                 _OrthancPluginService service,
                                 const void* params);
  } OrthancPluginContext;



  /**
   * @brief Free a string.
   * 
   * Free a string that was allocated by the core system of Orthanc.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param str The string to be freed.
   **/
  ORTHANC_PLUGIN_INLINE void  OrthancPluginFreeString(
    OrthancPluginContext* context, 
    char* str)
  {
    context->Free(str);
  }


  /**
   * @brief Free a memory buffer.
   * 
   * Free a memory buffer that was allocated by the core system of Orthanc.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param buffer The memory buffer to release.
   **/
  ORTHANC_PLUGIN_INLINE void  OrthancPluginFreeMemoryBuffer(
    OrthancPluginContext* context, 
    OrthancPluginMemoryBuffer* buffer)
  {
    context->Free(buffer->data);
  }


  /**
   * @brief Log an error.
   *
   * Log an error message using the Orthanc logging system.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param message The message to be logged.
   **/
  ORTHANC_PLUGIN_INLINE void OrthancPluginLogError(
    OrthancPluginContext* context,
    const char* message)
  {
    context->InvokeService(context, _OrthancPluginService_LogError, message);
  }


  /**
   * @brief Log a warning.
   *
   * Log a warning message using the Orthanc logging system.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param message The message to be logged.
   **/
  ORTHANC_PLUGIN_INLINE void OrthancPluginLogWarning(
    OrthancPluginContext* context,
    const char* message)
  {
    context->InvokeService(context, _OrthancPluginService_LogWarning, message);
  }


  /**
   * @brief Log an information.
   *
   * Log an information message using the Orthanc logging system.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param message The message to be logged.
   **/
  ORTHANC_PLUGIN_INLINE void OrthancPluginLogInfo(
    OrthancPluginContext* context,
    const char* message)
  {
    context->InvokeService(context, _OrthancPluginService_LogInfo, message);
  }



  typedef struct
  {
    const char* pathRegularExpression;
    OrthancPluginRestCallback callback;
  } _OrthancPluginRestCallback;

  /**
   * @brief Register a REST callback.
   *
   * This function registers a REST callback against a regular
   * expression for a URI. This function must be called during the
   * initialization of the plugin, i.e. inside the
   * OrthancPluginInitialize() public function.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param pathRegularExpression Regular expression for the URI. May contain groups.
   * @param callback The callback function to handle the REST call.
   **/
  ORTHANC_PLUGIN_INLINE void OrthancPluginRegisterRestCallback(
    OrthancPluginContext*     context,
    const char*               pathRegularExpression,
    OrthancPluginRestCallback callback)
  {
    _OrthancPluginRestCallback params;
    params.pathRegularExpression = pathRegularExpression;
    params.callback = callback;
    context->InvokeService(context, _OrthancPluginService_RegisterRestCallback, &params);
  }



  typedef struct
  {
    OrthancPluginRestOutput* output;
    const char*              answer;
    uint32_t                 answerSize;
    const char*              mimeType;
  } _OrthancPluginAnswerBuffer;

  /**
   * @brief Answer to a REST request.
   *
   * This function answers to a REST request with the content of a memory buffer.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param output The HTTP connection to the client application.
   * @param answer Pointer to the memory buffer containing the answer.
   * @param answerSize Number of bytes of the answer.
   * @param mimeType The MIME type of the answer.
   **/
  ORTHANC_PLUGIN_INLINE void OrthancPluginAnswerBuffer(
    OrthancPluginContext*    context,
    OrthancPluginRestOutput* output,
    const char*              answer,
    uint32_t                 answerSize,
    const char*              mimeType)
  {
    _OrthancPluginAnswerBuffer params;
    params.output = output;
    params.answer = answer;
    params.answerSize = answerSize;
    params.mimeType = mimeType;
    context->InvokeService(context, _OrthancPluginService_AnswerBuffer, &params);
  }


  typedef struct
  {
    OrthancPluginRestOutput*  output;
    OrthancPluginPixelFormat  format;
    uint32_t                  width;
    uint32_t                  height;
    uint32_t                  pitch;
    const void*               buffer;
  } _OrthancPluginCompressAndAnswerPngImage;

  /**
   * @brief Answer to a REST request with a PNG image.
   *
   * This function answers to a REST request with a PNG image. The
   * parameters of this function describe a memory buffer that
   * contains an uncompressed image. The image will be automatically compressed
   * as a PNG image by the core system of Orthanc.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param output The HTTP connection to the client application.
   * @param format The memory layout of the uncompressed image.
   * @param width The width of the image.
   * @param height The height of the image.
   * @param pitch The pitch of the image (i.e. the number of bytes
   * between 2 successive lines of the image in the memory buffer.
   * @param buffer The memory buffer containing the uncompressed image.
   **/
  ORTHANC_PLUGIN_INLINE void OrthancPluginCompressAndAnswerPngImage(
    OrthancPluginContext*     context,
    OrthancPluginRestOutput*  output,
    OrthancPluginPixelFormat  format,
    uint32_t                  width,
    uint32_t                  height,
    uint32_t                  pitch,
    const void*               buffer)
  {
    _OrthancPluginCompressAndAnswerPngImage params;
    params.output = output;
    params.format = format;
    params.width = width;
    params.height = height;
    params.pitch = pitch;
    params.buffer = buffer;
    context->InvokeService(context, _OrthancPluginService_CompressAndAnswerPngImage, &params);
  }



  typedef struct
  {
    OrthancPluginMemoryBuffer*  target;
    const char*                 instanceId;
  } _OrthancPluginGetDicomForInstance;

  /**
   * @brief Retrieve a DICOM instance using its Orthanc identifier.
   * 
   * Retrieve a DICOM instance using its Orthanc identifier. The DICOM
   * file is stored into a newly allocated memory buffer.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param target The target memory buffer.
   * @param instanceId The Orthanc identifier of the DICOM instance of interest.
   * @return 0 if success, other value if error.
   **/
  ORTHANC_PLUGIN_INLINE int  OrthancPluginGetDicomForInstance(
    OrthancPluginContext*       context,
    OrthancPluginMemoryBuffer*  target,
    const char*                 instanceId)
  {
    _OrthancPluginGetDicomForInstance params;
    params.target = target;
    params.instanceId = instanceId;
    return context->InvokeService(context, _OrthancPluginService_GetDicomForInstance, &params);
  }



  typedef struct
  {
    OrthancPluginMemoryBuffer*  target;
    const char*                 uri;
  } _OrthancPluginRestApiGet;

  /**
   * @brief Make a GET call to the built-in Orthanc REST API.
   * 
   * Make a GET call to the built-in Orthanc REST API. The result to
   * the query is stored into a newly allocated memory buffer.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param target The target memory buffer.
   * @param uri The URI in the built-in Orthanc API.
   * @return 0 if success, other value if error.
   **/
  ORTHANC_PLUGIN_INLINE int  OrthancPluginRestApiGet(
    OrthancPluginContext*       context,
    OrthancPluginMemoryBuffer*  target,
    const char*                 uri)
  {
    _OrthancPluginRestApiGet params;
    params.target = target;
    params.uri = uri;
    return context->InvokeService(context, _OrthancPluginService_RestApiGet, &params);
  }



  typedef struct
  {
    OrthancPluginMemoryBuffer*  target;
    const char*                 uri;
    const char*                 body;
    uint32_t                    bodySize;
  } _OrthancPluginRestApiPostPut;

  /**
   * @brief Make a POST call to the built-in Orthanc REST API.
   * 
   * Make a POST call to the built-in Orthanc REST API. The result to
   * the query is stored into a newly allocated memory buffer.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param target The target memory buffer.
   * @param uri The URI in the built-in Orthanc API.
   * @param body The body of the POST request.
   * @param bodySize The size of the body.
   * @return 0 if success, other value if error.
   **/
  ORTHANC_PLUGIN_INLINE int  OrthancPluginRestApiPost(
    OrthancPluginContext*       context,
    OrthancPluginMemoryBuffer*  target,
    const char*                 uri,
    const char*                 body,
    uint32_t                    bodySize)
  {
    _OrthancPluginRestApiPostPut params;
    params.target = target;
    params.uri = uri;
    params.body = body;
    params.bodySize = bodySize;
    return context->InvokeService(context, _OrthancPluginService_RestApiPost, &params);
  }



  /**
   * @brief Make a DELETE call to the built-in Orthanc REST API.
   * 
   * Make a DELETE call to the built-in Orthanc REST API.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param uri The URI to delete in the built-in Orthanc API.
   * @return 0 if success, other value if error.
   **/
  ORTHANC_PLUGIN_INLINE int  OrthancPluginRestApiDelete(
    OrthancPluginContext*       context,
    const char*                 uri)
  {
    return context->InvokeService(context, _OrthancPluginService_RestApiDelete, uri);
  }



  /**
   * @brief Make a PUT call to the built-in Orthanc REST API.
   * 
   * Make a PUT call to the built-in Orthanc REST API. The result to
   * the query is stored into a newly allocated memory buffer.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param target The target memory buffer.
   * @param uri The URI in the built-in Orthanc API.
   * @param body The body of the PUT request.
   * @param bodySize The size of the body.
   * @return 0 if success, other value if error.
   **/
  ORTHANC_PLUGIN_INLINE int  OrthancPluginRestApiPut(
    OrthancPluginContext*       context,
    OrthancPluginMemoryBuffer*  target,
    const char*                 uri,
    const char*                 body,
    uint32_t                    bodySize)
  {
    _OrthancPluginRestApiPostPut params;
    params.target = target;
    params.uri = uri;
    params.body = body;
    params.bodySize = bodySize;
    return context->InvokeService(context, _OrthancPluginService_RestApiPut, &params);
  }


  typedef struct
  {
    OrthancPluginRestOutput* output;
    const char*              redirection;
  } _OrthancPluginRedirect;

  /**
   * @brief Redirect a GET request.
   *
   * This function answers to a REST request by redirecting the user
   * to another URI using HTTP status 301.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param output The HTTP connection to the client application.
   * @param redirection Where to redirect.
   **/
  ORTHANC_PLUGIN_INLINE void OrthancPluginRedirect(
    OrthancPluginContext*    context,
    OrthancPluginRestOutput* output,
    const char*              redirection)
  {
    _OrthancPluginRedirect params;
    params.output = output;
    params.redirection = redirection;
    context->InvokeService(context, _OrthancPluginService_Redirect, &params);
  }


#ifdef  __cplusplus
}
#endif


/** @} */

