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
 *    The plugin must:
 *    - Check its compatibility with the Orthanc version using
 *      ::OrthancPluginCheckVersion().
 *    - Store the context pointer so that it can use the plugin 
 *      services of Orthanc.
 *    - Register all its REST callbacks using ::OrthancPluginRegisterRestCallback().
 *    - Possibly register its callback for received DICOM instances using ::OrthancPluginRegisterOnStoredInstanceCallback().
 *    - Possibly register its callback for changes to the DICOM store using ::OrthancPluginRegisterOnChangeCallback().
 *    - Possibly register a custom storage area using ::OrthancPluginRegisterStorageArea().
 *    - Possibly register a custom database back-end area using OrthancPluginRegisterDatabaseBackend().
 * -# <tt>void OrthancPluginFinalize()</tt>:
 *    This function is invoked by Orthanc during its shutdown. The plugin
 *    must free all its memory.
 * -# <tt>const char* OrthancPluginGetName()</tt>:
 *    The plugin must return a short string to identify itself.
 * -# <tt>const char* OrthancPluginGetVersion()</tt>:
 *    The plugin must return a string containing its version number.
 *
 * The name and the version of a plugin is only used to prevent it
 * from being loaded twice.
 * 
 * To ensure multi-threading safety, the various REST callbacks are
 * guaranteed to be executed in mutual exclusion since Orthanc
 * 0.8.5. If this feature is undesired (notably when developing
 * high-performance plugins handling simultaneous requests), use
 * ::OrthancPluginRegisterRestCallbackNoLock().
 **/



/**
 * @defgroup CInterface C Interface 
 * @brief The C interface to create Orthanc plugins.
 * 
 * These functions must be used to create C plugins for Orthanc.
 **/



/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
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

#define ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER     0
#define ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER     9
#define ORTHANC_PLUGINS_MINIMAL_REVISION_NUMBER  4



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

/**
 * For Microsoft Visual Studio, a compatibility "stdint.h" can be
 * downloaded at the following URL:
 * https://orthanc.googlecode.com/hg/Resources/ThirdParty/VisualStudio/stdint.h
 **/
#include <stdint.h>

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
   * The various error codes that can be returned by the Orthanc core.
   **/
  typedef enum
  {
    OrthancPluginErrorCode_InternalError = -1    /*!< Internal error */,
    OrthancPluginErrorCode_Success = 0    /*!< Success */,
    OrthancPluginErrorCode_Plugin = 1    /*!< Error encountered within the plugin engine */,
    OrthancPluginErrorCode_NotImplemented = 2    /*!< Not implemented yet */,
    OrthancPluginErrorCode_ParameterOutOfRange = 3    /*!< Parameter out of range */,
    OrthancPluginErrorCode_NotEnoughMemory = 4    /*!< Not enough memory */,
    OrthancPluginErrorCode_BadParameterType = 5    /*!< Bad type for a parameter */,
    OrthancPluginErrorCode_BadSequenceOfCalls = 6    /*!< Bad sequence of calls */,
    OrthancPluginErrorCode_InexistentItem = 7    /*!< Accessing an inexistent item */,
    OrthancPluginErrorCode_BadRequest = 8    /*!< Bad request */,
    OrthancPluginErrorCode_NetworkProtocol = 9    /*!< Error in the network protocol */,
    OrthancPluginErrorCode_SystemCommand = 10    /*!< Error while calling a system command */,
    OrthancPluginErrorCode_Database = 11    /*!< Error with the database engine */,
    OrthancPluginErrorCode_UriSyntax = 12    /*!< Badly formatted URI */,
    OrthancPluginErrorCode_InexistentFile = 13    /*!< Inexistent file */,
    OrthancPluginErrorCode_CannotWriteFile = 14    /*!< Cannot write to file */,
    OrthancPluginErrorCode_BadFileFormat = 15    /*!< Bad file format */,
    OrthancPluginErrorCode_Timeout = 16    /*!< Timeout */,
    OrthancPluginErrorCode_UnknownResource = 17    /*!< Unknown resource */,
    OrthancPluginErrorCode_IncompatibleDatabaseVersion = 18    /*!< Incompatible version of the database */,
    OrthancPluginErrorCode_FullStorage = 19    /*!< The file storage is full */,
    OrthancPluginErrorCode_CorruptedFile = 20    /*!< Corrupted file (e.g. inconsistent MD5 hash) */,
    OrthancPluginErrorCode_InexistentTag = 21    /*!< Inexistent tag */,
    OrthancPluginErrorCode_ReadOnly = 22    /*!< Cannot modify a read-only data structure */,
    OrthancPluginErrorCode_IncompatibleImageFormat = 23    /*!< Incompatible format of the images */,
    OrthancPluginErrorCode_IncompatibleImageSize = 24    /*!< Incompatible size of the images */,
    OrthancPluginErrorCode_SharedLibrary = 25    /*!< Error while using a shared library (plugin) */,
    OrthancPluginErrorCode_UnknownPluginService = 26    /*!< Plugin invoking an unknown service */,
    OrthancPluginErrorCode_UnknownDicomTag = 27    /*!< Unknown DICOM tag */,
    OrthancPluginErrorCode_BadJson = 28    /*!< Cannot parse a JSON document */,
    OrthancPluginErrorCode_SQLiteNotOpened = 1000    /*!< SQLite: The database is not opened */,
    OrthancPluginErrorCode_SQLiteAlreadyOpened = 1001    /*!< SQLite: Connection is already open */,
    OrthancPluginErrorCode_SQLiteCannotOpen = 1002    /*!< SQLite: Unable to open the database */,
    OrthancPluginErrorCode_SQLiteStatementAlreadyUsed = 1003    /*!< SQLite: This cached statement is already being referred to */,
    OrthancPluginErrorCode_SQLiteExecute = 1004    /*!< SQLite: Cannot execute a command */,
    OrthancPluginErrorCode_SQLiteRollbackWithoutTransaction = 1005    /*!< SQLite: Rolling back a nonexistent transaction (have you called Begin()?) */,
    OrthancPluginErrorCode_SQLiteCommitWithoutTransaction = 1006    /*!< SQLite: Committing a nonexistent transaction */,
    OrthancPluginErrorCode_SQLiteRegisterFunction = 1007    /*!< SQLite: Unable to register a function */,
    OrthancPluginErrorCode_SQLiteFlush = 1008    /*!< SQLite: Unable to flush the database */,
    OrthancPluginErrorCode_SQLiteCannotRun = 1009    /*!< SQLite: Cannot run a cached statement */,
    OrthancPluginErrorCode_SQLiteCannotStep = 1010    /*!< SQLite: Cannot step over a cached statement */,
    OrthancPluginErrorCode_SQLiteBindOutOfRange = 1011    /*!< SQLite: Bing a value while out of range (serious error) */,
    OrthancPluginErrorCode_SQLitePrepareStatement = 1012    /*!< SQLite: Cannot prepare a cached statement */,
    OrthancPluginErrorCode_SQLiteTransactionAlreadyStarted = 1013    /*!< SQLite: Beginning the same transaction twice */,
    OrthancPluginErrorCode_SQLiteTransactionCommit = 1014    /*!< SQLite: Failure when committing the transaction */,
    OrthancPluginErrorCode_SQLiteTransactionBegin = 1015    /*!< SQLite: Cannot start a transaction */,
    OrthancPluginErrorCode_DirectoryOverFile = 2000    /*!< The directory to be created is already occupied by a regular file */,
    OrthancPluginErrorCode_FileStorageCannotWrite = 2001    /*!< Unable to create a subdirectory or a file in the file storage */,
    OrthancPluginErrorCode_DirectoryExpected = 2002    /*!< The specified path does not point to a directory */,
    OrthancPluginErrorCode_HttpPortInUse = 2003    /*!< The TCP port of the HTTP server is already in use */,
    OrthancPluginErrorCode_DicomPortInUse = 2004    /*!< The TCP port of the DICOM server is already in use */,
    OrthancPluginErrorCode_BadHttpStatusInRest = 2005    /*!< This HTTP status is not allowed in a REST API */,
    OrthancPluginErrorCode_RegularFileExpected = 2006    /*!< The specified path does not point to a regular file */,
    OrthancPluginErrorCode_PathToExecutable = 2007    /*!< Unable to get the path to the executable */,
    OrthancPluginErrorCode_MakeDirectory = 2008    /*!< Cannot create a directory */,
    OrthancPluginErrorCode_BadApplicationEntityTitle = 2009    /*!< An application entity title (AET) cannot be empty or be longer than 16 characters */,
    OrthancPluginErrorCode_NoCFindHandler = 2010    /*!< No request handler factory for DICOM C-FIND SCP */,
    OrthancPluginErrorCode_NoCMoveHandler = 2011    /*!< No request handler factory for DICOM C-MOVE SCP */,
    OrthancPluginErrorCode_NoCStoreHandler = 2012    /*!< No request handler factory for DICOM C-STORE SCP */,
    OrthancPluginErrorCode_NoApplicationEntityFilter = 2013    /*!< No application entity filter */,
    OrthancPluginErrorCode_NoSopClassOrInstance = 2014    /*!< DicomUserConnection: Unable to find the SOP class and instance */,
    OrthancPluginErrorCode_NoPresentationContext = 2015    /*!< DicomUserConnection: No acceptable presentation context for modality */,
    OrthancPluginErrorCode_DicomFindUnavailable = 2016    /*!< DicomUserConnection: The C-FIND command is not supported by the remote SCP */,
    OrthancPluginErrorCode_DicomMoveUnavailable = 2017    /*!< DicomUserConnection: The C-MOVE command is not supported by the remote SCP */,
    OrthancPluginErrorCode_CannotStoreInstance = 2018    /*!< Cannot store an instance */,
    OrthancPluginErrorCode_CreateDicomNotString = 2019    /*!< Only string values are supported when creating DICOM instances */,
    OrthancPluginErrorCode_CreateDicomOverrideTag = 2020    /*!< Trying to override a value inherited from a parent module */,
    OrthancPluginErrorCode_CreateDicomUseContent = 2021    /*!< Use \"Content\" to inject an image into a new DICOM instance */,
    OrthancPluginErrorCode_CreateDicomNoPayload = 2022    /*!< No payload is present for one instance in the series */,
    OrthancPluginErrorCode_CreateDicomUseDataUriScheme = 2023    /*!< The payload of the DICOM instance must be specified according to Data URI scheme */,
    OrthancPluginErrorCode_CreateDicomBadParent = 2024    /*!< Trying to attach a new DICOM instance to an inexistent resource */,
    OrthancPluginErrorCode_CreateDicomParentIsInstance = 2025    /*!< Trying to attach a new DICOM instance to an instance (must be a series, study or patient) */,
    OrthancPluginErrorCode_CreateDicomParentEncoding = 2026    /*!< Unable to get the encoding of the parent resource */,
    OrthancPluginErrorCode_UnknownModality = 2027    /*!< Unknown modality */,
    OrthancPluginErrorCode_BadJobOrdering = 2028    /*!< Bad ordering of filters in a job */,
    OrthancPluginErrorCode_JsonToLuaTable = 2029    /*!< Cannot convert the given JSON object to a Lua table */,
    OrthancPluginErrorCode_CannotCreateLua = 2030    /*!< Cannot create the Lua context */,
    OrthancPluginErrorCode_CannotExecuteLua = 2031    /*!< Cannot execute a Lua command */,
    OrthancPluginErrorCode_LuaAlreadyExecuted = 2032    /*!< Arguments cannot be pushed after the Lua function is executed */,
    OrthancPluginErrorCode_LuaBadOutput = 2033    /*!< The Lua function does not give the expected number of outputs */,
    OrthancPluginErrorCode_NotLuaPredicate = 2034    /*!< The Lua function is not a predicate (only true/false outputs allowed) */,
    OrthancPluginErrorCode_LuaReturnsNoString = 2035    /*!< The Lua function does not return a string */,

    _OrthancPluginErrorCode_INTERNAL = 0x7fffffff
  } OrthancPluginErrorCode;


  /**
   * Forward declaration of one of the mandatory functions for Orthanc
   * plugins.
   **/
  ORTHANC_PLUGINS_API const char* OrthancPluginGetName();


  /**
   * The various HTTP methods for a REST call.
   **/
  typedef enum
  {
    OrthancPluginHttpMethod_Get = 1,    /*!< GET request */
    OrthancPluginHttpMethod_Post = 2,   /*!< POST request */
    OrthancPluginHttpMethod_Put = 3,    /*!< PUT request */
    OrthancPluginHttpMethod_Delete = 4, /*!< DELETE request */

    _OrthancPluginHttpMethod_INTERNAL = 0x7fffffff
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


    /* --------------------------------------------------
       New in version 0.8.1
       -------------------------------------------------- */

    /**
     * @brief The number of HTTP headers.
     **/
    uint32_t                headersCount;

    /**
     * @brief The keys of the HTTP headers (always converted to low-case).
     **/
    const char* const*      headersKeys;

    /**
     * @brief The values of the HTTP headers.
     **/
    const char* const*      headersValues;

  } OrthancPluginHttpRequest;


  typedef enum 
  {
    /* Generic services */
    _OrthancPluginService_LogInfo = 1,
    _OrthancPluginService_LogWarning = 2,
    _OrthancPluginService_LogError = 3,
    _OrthancPluginService_GetOrthancPath = 4,
    _OrthancPluginService_GetOrthancDirectory = 5,
    _OrthancPluginService_GetConfigurationPath = 6,
    _OrthancPluginService_SetPluginProperty = 7,
    _OrthancPluginService_GetGlobalProperty = 8,
    _OrthancPluginService_SetGlobalProperty = 9,
    _OrthancPluginService_GetCommandLineArgumentsCount = 10,
    _OrthancPluginService_GetCommandLineArgument = 11,
    _OrthancPluginService_GetExpectedDatabaseVersion = 12,
    _OrthancPluginService_GetConfiguration = 13,
    _OrthancPluginService_BufferCompression = 14,
    _OrthancPluginService_ReadFile = 15,
    _OrthancPluginService_WriteFile = 16,
    _OrthancPluginService_GetErrorDescription = 17,

    /* Registration of callbacks */
    _OrthancPluginService_RegisterRestCallback = 1000,
    _OrthancPluginService_RegisterOnStoredInstanceCallback = 1001,
    _OrthancPluginService_RegisterStorageArea = 1002,
    _OrthancPluginService_RegisterOnChangeCallback = 1003,
    _OrthancPluginService_RegisterRestCallbackNoLock = 1004,

    /* Sending answers to REST calls */
    _OrthancPluginService_AnswerBuffer = 2000,
    _OrthancPluginService_CompressAndAnswerPngImage = 2001,
    _OrthancPluginService_Redirect = 2002,
    _OrthancPluginService_SendHttpStatusCode = 2003,
    _OrthancPluginService_SendUnauthorized = 2004,
    _OrthancPluginService_SendMethodNotAllowed = 2005,
    _OrthancPluginService_SetCookie = 2006,
    _OrthancPluginService_SetHttpHeader = 2007,
    _OrthancPluginService_StartMultipartAnswer = 2008,
    _OrthancPluginService_SendMultipartItem = 2009,
    _OrthancPluginService_SendHttpStatus = 2010,

    /* Access to the Orthanc database and API */
    _OrthancPluginService_GetDicomForInstance = 3000,
    _OrthancPluginService_RestApiGet = 3001,
    _OrthancPluginService_RestApiPost = 3002,
    _OrthancPluginService_RestApiDelete = 3003,
    _OrthancPluginService_RestApiPut = 3004,
    _OrthancPluginService_LookupPatient = 3005,
    _OrthancPluginService_LookupStudy = 3006,
    _OrthancPluginService_LookupSeries = 3007,
    _OrthancPluginService_LookupInstance = 3008,
    _OrthancPluginService_LookupStudyWithAccessionNumber = 3009,
    _OrthancPluginService_RestApiGetAfterPlugins = 3010,
    _OrthancPluginService_RestApiPostAfterPlugins = 3011,
    _OrthancPluginService_RestApiDeleteAfterPlugins = 3012,
    _OrthancPluginService_RestApiPutAfterPlugins = 3013,

    /* Access to DICOM instances */
    _OrthancPluginService_GetInstanceRemoteAet = 4000,
    _OrthancPluginService_GetInstanceSize = 4001,
    _OrthancPluginService_GetInstanceData = 4002,
    _OrthancPluginService_GetInstanceJson = 4003,
    _OrthancPluginService_GetInstanceSimplifiedJson = 4004,
    _OrthancPluginService_HasInstanceMetadata = 4005,
    _OrthancPluginService_GetInstanceMetadata = 4006,

    /* Services for plugins implementing a database back-end */
    _OrthancPluginService_RegisterDatabaseBackend = 5000,
    _OrthancPluginService_DatabaseAnswer = 5001,

    /* Primitives for handling images */
    _OrthancPluginService_GetImagePixelFormat = 6000,
    _OrthancPluginService_GetImageWidth = 6001,
    _OrthancPluginService_GetImageHeight = 6002,
    _OrthancPluginService_GetImagePitch = 6003,
    _OrthancPluginService_GetImageBuffer = 6004,
    _OrthancPluginService_UncompressImage = 6005,
    _OrthancPluginService_FreeImage = 6006,
    _OrthancPluginService_CompressImage = 6007,

    _OrthancPluginService_INTERNAL = 0x7fffffff
  } _OrthancPluginService;


  typedef enum
  {
    _OrthancPluginProperty_Description = 1,
    _OrthancPluginProperty_RootUri = 2,
    _OrthancPluginProperty_OrthancExplorer = 3,

    _OrthancPluginProperty_INTERNAL = 0x7fffffff
  } _OrthancPluginProperty;



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
    OrthancPluginPixelFormat_RGBA32 = 5,

    OrthancPluginPixelFormat_Unknown = 6,   /*!< Unknown pixel format */

    _OrthancPluginPixelFormat_INTERNAL = 0x7fffffff
  } OrthancPluginPixelFormat;



  /**
   * The content types that are supported by Orthanc plugins.
   **/
  typedef enum
  {
    OrthancPluginContentType_Unknown = 0,      /*!< Unknown content type */
    OrthancPluginContentType_Dicom = 1,        /*!< DICOM */
    OrthancPluginContentType_DicomAsJson = 2,  /*!< JSON summary of a DICOM file */

    _OrthancPluginContentType_INTERNAL = 0x7fffffff
  } OrthancPluginContentType;



  /**
   * The supported types of DICOM resources.
   **/
  typedef enum
  {
    OrthancPluginResourceType_Patient = 0,     /*!< Patient */
    OrthancPluginResourceType_Study = 1,       /*!< Study */
    OrthancPluginResourceType_Series = 2,      /*!< Series */
    OrthancPluginResourceType_Instance = 3,    /*!< Instance */

    _OrthancPluginResourceType_INTERNAL = 0x7fffffff
  } OrthancPluginResourceType;



  /**
   * The supported types of changes that can happen to DICOM resources.
   **/
  typedef enum
  {
    OrthancPluginChangeType_CompletedSeries = 0,    /*!< Series is now complete */
    OrthancPluginChangeType_Deleted = 1,            /*!< Deleted resource */
    OrthancPluginChangeType_NewChildInstance = 2,   /*!< A new instance was added to this resource */
    OrthancPluginChangeType_NewInstance = 3,        /*!< New instance received */
    OrthancPluginChangeType_NewPatient = 4,         /*!< New patient created */
    OrthancPluginChangeType_NewSeries = 5,          /*!< New series created */
    OrthancPluginChangeType_NewStudy = 6,           /*!< New study created */
    OrthancPluginChangeType_StablePatient = 7,      /*!< Timeout: No new instance in this patient */
    OrthancPluginChangeType_StableSeries = 8,       /*!< Timeout: No new instance in this series */
    OrthancPluginChangeType_StableStudy = 9,        /*!< Timeout: No new instance in this study */

    _OrthancPluginChangeType_INTERNAL = 0x7fffffff
  } OrthancPluginChangeType;


  /**
   * The compression algorithms that are known by the Orthanc core.
   **/
  typedef enum
  {
    OrthancPluginCompressionType_Zlib = 0,          /*!< Standard zlib compression */
    OrthancPluginCompressionType_ZlibWithSize = 1,  /*!< zlib, prefixed with uncompressed size (uint64_t) */
    OrthancPluginCompressionType_Gzip = 2,          /*!< Standard gzip compression */
    OrthancPluginCompressionType_GzipWithSize = 3,  /*!< gzip, prefixed with uncompressed size (uint64_t) */

    _OrthancPluginCompressionType_INTERNAL = 0x7fffffff
  } OrthancPluginCompressionType;


  typedef enum
  {
    OrthancPluginImageFormat_Png = 0,   /*!< Image compressed using PNG */
    OrthancPluginImageFormat_Jpeg = 1,  /*!< Image compressed using JPEG */

    _OrthancPluginImageFormat_INTERNAL = 0x7fffffff
  } OrthancPluginImageFormat;



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
   * @brief Opaque structure that represents a DICOM instance received by Orthanc.
   **/
  typedef struct _OrthancPluginDicomInstance_t OrthancPluginDicomInstance;



  /**
   * @brief Opaque structure that represents a uncompressed image in memory.
   **/
  typedef struct _OrthancPluginImage_t OrthancPluginImage;



  /**
   * @brief Signature of a callback function that answers to a REST request.
   **/
  typedef int32_t (*OrthancPluginRestCallback) (
    OrthancPluginRestOutput* output,
    const char* url,
    const OrthancPluginHttpRequest* request);



  /**
   * @brief Signature of a callback function that is triggered when Orthanc receives a DICOM instance.
   **/
  typedef int32_t (*OrthancPluginOnStoredInstanceCallback) (
    OrthancPluginDicomInstance* instance,
    const char* instanceId);



  /**
   * @brief Signature of a callback function that is triggered when a change happens to some DICOM resource.
   **/
  typedef int32_t (*OrthancPluginOnChangeCallback) (
    OrthancPluginChangeType changeType,
    OrthancPluginResourceType resourceType,
    const char* resourceId);



  /**
   * @brief Signature of a function to free dynamic memory.
   **/
  typedef void (*OrthancPluginFree) (void* buffer);



  /**
   * @brief Callback for writing to the storage area.
   *
   * Signature of a callback function that is triggered when Orthanc writes a file to the storage area.
   *
   * @param uuid The UUID of the file.
   * @param content The content of the file.
   * @param size The size of the file.
   * @param type The content type corresponding to this file. 
   * @return 0 if success, other value if error.
   **/
  typedef int32_t (*OrthancPluginStorageCreate) (
    const char* uuid,
    const void* content,
    int64_t size,
    OrthancPluginContentType type);



  /**
   * @brief Callback for reading from the storage area.
   *
   * Signature of a callback function that is triggered when Orthanc reads a file from the storage area.
   *
   * @param content The content of the file (output).
   * @param size The size of the file (output).
   * @param uuid The UUID of the file of interest.
   * @param type The content type corresponding to this file. 
   * @return 0 if success, other value if error.
   **/
  typedef int32_t (*OrthancPluginStorageRead) (
    void** content,
    int64_t* size,
    const char* uuid,
    OrthancPluginContentType type);



  /**
   * @brief Callback for removing a file from the storage area.
   *
   * Signature of a callback function that is triggered when Orthanc deletes a file from the storage area.
   *
   * @param uuid The UUID of the file to be removed.
   * @param type The content type corresponding to this file. 
   * @return 0 if success, other value if error.
   **/
  typedef int32_t (*OrthancPluginStorageRemove) (
    const char* uuid,
    OrthancPluginContentType type);



  /**
   * @brief Data structure that contains information about the Orthanc core.
   **/
  typedef struct _OrthancPluginContext_t
  {
    void*                     pluginsManager;
    const char*               orthancVersion;
    OrthancPluginFree         Free;
    OrthancPluginErrorCode  (*InvokeService) (struct _OrthancPluginContext_t* context,
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
    if (str != NULL)
    {
      context->Free(str);
    }
  }


  /**
   * @brief Check the compatibility of the plugin wrt. the version of its hosting Orthanc.
   * 
   * This function checks whether the version of this C header is
   * compatible with the current version of Orthanc. The result of
   * this function should always be checked in the
   * OrthancPluginInitialize() entry point of the plugin.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @return 1 if and only if the versions are compatible. If the
   * result is 0, the initialization of the plugin should fail.
   **/
  ORTHANC_PLUGIN_INLINE int  OrthancPluginCheckVersion(
    OrthancPluginContext* context)
  {
    int major, minor, revision;

    if (sizeof(int32_t) != sizeof(OrthancPluginErrorCode) ||
        sizeof(int32_t) != sizeof(OrthancPluginHttpMethod) ||
        sizeof(int32_t) != sizeof(_OrthancPluginService) ||
        sizeof(int32_t) != sizeof(_OrthancPluginProperty) ||
        sizeof(int32_t) != sizeof(OrthancPluginPixelFormat) ||
        sizeof(int32_t) != sizeof(OrthancPluginContentType) ||
        sizeof(int32_t) != sizeof(OrthancPluginResourceType) ||
        sizeof(int32_t) != sizeof(OrthancPluginChangeType) ||
        sizeof(int32_t) != sizeof(OrthancPluginCompressionType) ||
        sizeof(int32_t) != sizeof(OrthancPluginImageFormat))
    {
      /* Mismatch in the size of the enumerations */
      return 0;
    }

    /* Assume compatibility with the mainline */
    if (!strcmp(context->orthancVersion, "mainline"))
    {
      return 1;
    }

    /* Parse the version of the Orthanc core */
    if ( 
#ifdef _MSC_VER
      sscanf_s
#else
      sscanf
#endif
      (context->orthancVersion, "%4d.%4d.%4d", &major, &minor, &revision) != 3)
    {
      return 0;
    }

    /* Check the major number of the version */

    if (major > ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER)
    {
      return 1;
    }

    if (major < ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER)
    {
      return 0;
    }

    /* Check the minor number of the version */

    if (minor > ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER)
    {
      return 1;
    }

    if (minor < ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER)
    {
      return 0;
    }

    /* Check the revision number of the version */

    if (revision >= ORTHANC_PLUGINS_MINIMAL_REVISION_NUMBER)
    {
      return 1;
    }
    else
    {
      return 0;
    }
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
   * Each REST callback is guaranteed to run in mutual exclusion.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param pathRegularExpression Regular expression for the URI. May contain groups.
   * @param callback The callback function to handle the REST call.
   * @see OrthancPluginRegisterRestCallbackNoLock()
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



  /**
   * @brief Register a REST callback, without locking.
   *
   * This function registers a REST callback against a regular
   * expression for a URI. This function must be called during the
   * initialization of the plugin, i.e. inside the
   * OrthancPluginInitialize() public function.
   *
   * Contrarily to OrthancPluginRegisterRestCallback(), the callback
   * will NOT be invoked in mutual exclusion. This can be useful for
   * high-performance plugins that must handle concurrent requests
   * (Orthanc uses a pool of threads, one thread being assigned to
   * each incoming HTTP request). Of course, it is up to the plugin to
   * implement the required locking mechanisms.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param pathRegularExpression Regular expression for the URI. May contain groups.
   * @param callback The callback function to handle the REST call.
   * @see OrthancPluginRegisterRestCallback()
   **/
  ORTHANC_PLUGIN_INLINE void OrthancPluginRegisterRestCallbackNoLock(
    OrthancPluginContext*     context,
    const char*               pathRegularExpression,
    OrthancPluginRestCallback callback)
  {
    _OrthancPluginRestCallback params;
    params.pathRegularExpression = pathRegularExpression;
    params.callback = callback;
    context->InvokeService(context, _OrthancPluginService_RegisterRestCallbackNoLock, &params);
  }



  typedef struct
  {
    OrthancPluginOnStoredInstanceCallback callback;
  } _OrthancPluginOnStoredInstanceCallback;

  /**
   * @brief Register a callback for received instances.
   *
   * This function registers a callback function that is called
   * whenever a new DICOM instance is stored into the Orthanc core.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param callback The callback function.
   **/
  ORTHANC_PLUGIN_INLINE void OrthancPluginRegisterOnStoredInstanceCallback(
    OrthancPluginContext*                  context,
    OrthancPluginOnStoredInstanceCallback  callback)
  {
    _OrthancPluginOnStoredInstanceCallback params;
    params.callback = callback;

    context->InvokeService(context, _OrthancPluginService_RegisterOnStoredInstanceCallback, &params);
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
   * @return 0 if success, or the error code if failure.
   **/
  ORTHANC_PLUGIN_INLINE OrthancPluginErrorCode  OrthancPluginGetDicomForInstance(
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
   * @return 0 if success, or the error code if failure.
   **/
  ORTHANC_PLUGIN_INLINE OrthancPluginErrorCode  OrthancPluginRestApiGet(
    OrthancPluginContext*       context,
    OrthancPluginMemoryBuffer*  target,
    const char*                 uri)
  {
    _OrthancPluginRestApiGet params;
    params.target = target;
    params.uri = uri;
    return context->InvokeService(context, _OrthancPluginService_RestApiGet, &params);
  }



  /**
   * @brief Make a GET call to the REST API, as tainted by the plugins.
   * 
   * Make a GET call to the Orthanc REST API, after all the plugins
   * are applied. In other words, if some plugin overrides or adds the
   * called URI to the built-in Orthanc REST API, this call will
   * return the result provided by this plugin. The result to the
   * query is stored into a newly allocated memory buffer.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param target The target memory buffer.
   * @param uri The URI in the built-in Orthanc API.
   * @return 0 if success, or the error code if failure.
   **/
  ORTHANC_PLUGIN_INLINE OrthancPluginErrorCode  OrthancPluginRestApiGetAfterPlugins(
    OrthancPluginContext*       context,
    OrthancPluginMemoryBuffer*  target,
    const char*                 uri)
  {
    _OrthancPluginRestApiGet params;
    params.target = target;
    params.uri = uri;
    return context->InvokeService(context, _OrthancPluginService_RestApiGetAfterPlugins, &params);
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
   * @return 0 if success, or the error code if failure.
   **/
  ORTHANC_PLUGIN_INLINE OrthancPluginErrorCode  OrthancPluginRestApiPost(
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
   * @brief Make a POST call to the REST API, as tainted by the plugins.
   * 
   * Make a POST call to the Orthanc REST API, after all the plugins
   * are applied. In other words, if some plugin overrides or adds the
   * called URI to the built-in Orthanc REST API, this call will
   * return the result provided by this plugin. The result to the
   * query is stored into a newly allocated memory buffer.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param target The target memory buffer.
   * @param uri The URI in the built-in Orthanc API.
   * @param body The body of the POST request.
   * @param bodySize The size of the body.
   * @return 0 if success, or the error code if failure.
   **/
  ORTHANC_PLUGIN_INLINE OrthancPluginErrorCode  OrthancPluginRestApiPostAfterPlugins(
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
    return context->InvokeService(context, _OrthancPluginService_RestApiPostAfterPlugins, &params);
  }



  /**
   * @brief Make a DELETE call to the built-in Orthanc REST API.
   * 
   * Make a DELETE call to the built-in Orthanc REST API.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param uri The URI to delete in the built-in Orthanc API.
   * @return 0 if success, or the error code if failure.
   **/
  ORTHANC_PLUGIN_INLINE OrthancPluginErrorCode  OrthancPluginRestApiDelete(
    OrthancPluginContext*       context,
    const char*                 uri)
  {
    return context->InvokeService(context, _OrthancPluginService_RestApiDelete, uri);
  }


  /**
   * @brief Make a DELETE call to the REST API, as tainted by the plugins.
   * 
   * Make a DELETE call to the Orthanc REST API, after all the plugins
   * are applied. In other words, if some plugin overrides or adds the
   * called URI to the built-in Orthanc REST API, this call will
   * return the result provided by this plugin. 
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param uri The URI to delete in the built-in Orthanc API.
   * @return 0 if success, or the error code if failure.
   **/
  ORTHANC_PLUGIN_INLINE OrthancPluginErrorCode  OrthancPluginRestApiDeleteAfterPlugins(
    OrthancPluginContext*       context,
    const char*                 uri)
  {
    return context->InvokeService(context, _OrthancPluginService_RestApiDeleteAfterPlugins, uri);
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
   * @return 0 if success, or the error code if failure.
   **/
  ORTHANC_PLUGIN_INLINE OrthancPluginErrorCode  OrthancPluginRestApiPut(
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



  /**
   * @brief Make a PUT call to the REST API, as tainted by the plugins.
   * 
   * Make a PUT call to the Orthanc REST API, after all the plugins
   * are applied. In other words, if some plugin overrides or adds the
   * called URI to the built-in Orthanc REST API, this call will
   * return the result provided by this plugin. The result to the
   * query is stored into a newly allocated memory buffer.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param target The target memory buffer.
   * @param uri The URI in the built-in Orthanc API.
   * @param body The body of the PUT request.
   * @param bodySize The size of the body.
   * @return 0 if success, or the error code if failure.
   **/
  ORTHANC_PLUGIN_INLINE OrthancPluginErrorCode  OrthancPluginRestApiPutAfterPlugins(
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
    return context->InvokeService(context, _OrthancPluginService_RestApiPutAfterPlugins, &params);
  }



  typedef struct
  {
    OrthancPluginRestOutput* output;
    const char*              argument;
  } _OrthancPluginOutputPlusArgument;

  /**
   * @brief Redirect a REST request.
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
    _OrthancPluginOutputPlusArgument params;
    params.output = output;
    params.argument = redirection;
    context->InvokeService(context, _OrthancPluginService_Redirect, &params);
  }



  typedef struct
  {
    char**       result;
    const char*  argument;
  } _OrthancPluginRetrieveDynamicString;

  /**
   * @brief Look for a patient.
   *
   * Look for a patient stored in Orthanc, using its Patient ID tag (0x0010, 0x0020).
   * This function uses the database index to run as fast as possible (it does not loop
   * over all the stored patients).
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param patientID The Patient ID of interest.
   * @return The NULL value if the patient is non-existent, or a string containing the 
   * Orthanc ID of the patient. This string must be freed by OrthancPluginFreeString().
   **/
  ORTHANC_PLUGIN_INLINE char* OrthancPluginLookupPatient(
    OrthancPluginContext*  context,
    const char*            patientID)
  {
    char* result;

    _OrthancPluginRetrieveDynamicString params;
    params.result = &result;
    params.argument = patientID;

    if (context->InvokeService(context, _OrthancPluginService_LookupPatient, &params))
    {
      /* Error */
      return NULL;
    }
    else
    {
      return result;
    }
  }


  /**
   * @brief Look for a study.
   *
   * Look for a study stored in Orthanc, using its Study Instance UID tag (0x0020, 0x000d).
   * This function uses the database index to run as fast as possible (it does not loop
   * over all the stored studies).
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param studyUID The Study Instance UID of interest.
   * @return The NULL value if the study is non-existent, or a string containing the 
   * Orthanc ID of the study. This string must be freed by OrthancPluginFreeString().
   **/
  ORTHANC_PLUGIN_INLINE char* OrthancPluginLookupStudy(
    OrthancPluginContext*  context,
    const char*            studyUID)
  {
    char* result;

    _OrthancPluginRetrieveDynamicString params;
    params.result = &result;
    params.argument = studyUID;

    if (context->InvokeService(context, _OrthancPluginService_LookupStudy, &params))
    {
      /* Error */
      return NULL;
    }
    else
    {
      return result;
    }
  }


  /**
   * @brief Look for a study, using the accession number.
   *
   * Look for a study stored in Orthanc, using its Accession Number tag (0x0008, 0x0050).
   * This function uses the database index to run as fast as possible (it does not loop
   * over all the stored studies).
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param accessionNumber The Accession Number of interest.
   * @return The NULL value if the study is non-existent, or a string containing the 
   * Orthanc ID of the study. This string must be freed by OrthancPluginFreeString().
   **/
  ORTHANC_PLUGIN_INLINE char* OrthancPluginLookupStudyWithAccessionNumber(
    OrthancPluginContext*  context,
    const char*            accessionNumber)
  {
    char* result;

    _OrthancPluginRetrieveDynamicString params;
    params.result = &result;
    params.argument = accessionNumber;

    if (context->InvokeService(context, _OrthancPluginService_LookupStudyWithAccessionNumber, &params))
    {
      /* Error */
      return NULL;
    }
    else
    {
      return result;
    }
  }


  /**
   * @brief Look for a series.
   *
   * Look for a series stored in Orthanc, using its Series Instance UID tag (0x0020, 0x000e).
   * This function uses the database index to run as fast as possible (it does not loop
   * over all the stored series).
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param seriesUID The Series Instance UID of interest.
   * @return The NULL value if the series is non-existent, or a string containing the 
   * Orthanc ID of the series. This string must be freed by OrthancPluginFreeString().
   **/
  ORTHANC_PLUGIN_INLINE char* OrthancPluginLookupSeries(
    OrthancPluginContext*  context,
    const char*            seriesUID)
  {
    char* result;

    _OrthancPluginRetrieveDynamicString params;
    params.result = &result;
    params.argument = seriesUID;

    if (context->InvokeService(context, _OrthancPluginService_LookupSeries, &params))
    {
      /* Error */
      return NULL;
    }
    else
    {
      return result;
    }
  }


  /**
   * @brief Look for an instance.
   *
   * Look for an instance stored in Orthanc, using its SOP Instance UID tag (0x0008, 0x0018).
   * This function uses the database index to run as fast as possible (it does not loop
   * over all the stored instances).
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param sopInstanceUID The SOP Instance UID of interest.
   * @return The NULL value if the instance is non-existent, or a string containing the 
   * Orthanc ID of the instance. This string must be freed by OrthancPluginFreeString().
   **/
  ORTHANC_PLUGIN_INLINE char* OrthancPluginLookupInstance(
    OrthancPluginContext*  context,
    const char*            sopInstanceUID)
  {
    char* result;

    _OrthancPluginRetrieveDynamicString params;
    params.result = &result;
    params.argument = sopInstanceUID;

    if (context->InvokeService(context, _OrthancPluginService_LookupInstance, &params))
    {
      /* Error */
      return NULL;
    }
    else
    {
      return result;
    }
  }



  typedef struct
  {
    OrthancPluginRestOutput* output;
    uint16_t                 status;
  } _OrthancPluginSendHttpStatusCode;

  /**
   * @brief Send a HTTP status code.
   *
   * This function answers to a REST request by sending a HTTP status
   * code (such as "400 - Bad Request"). Note that:
   * - Successful requests (status 200) must use ::OrthancPluginAnswerBuffer().
   * - Redirections (status 301) must use ::OrthancPluginRedirect().
   * - Unauthorized access (status 401) must use ::OrthancPluginSendUnauthorized().
   * - Methods not allowed (status 405) must use ::OrthancPluginSendMethodNotAllowed().
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param output The HTTP connection to the client application.
   * @param status The HTTP status code to be sent.
   * @see OrthancPluginSendHttpStatus()
   **/
  ORTHANC_PLUGIN_INLINE void OrthancPluginSendHttpStatusCode(
    OrthancPluginContext*    context,
    OrthancPluginRestOutput* output,
    uint16_t                 status)
  {
    _OrthancPluginSendHttpStatusCode params;
    params.output = output;
    params.status = status;
    context->InvokeService(context, _OrthancPluginService_SendHttpStatusCode, &params);
  }


  /**
   * @brief Signal that a REST request is not authorized.
   *
   * This function answers to a REST request by signaling that it is
   * not authorized.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param output The HTTP connection to the client application.
   * @param realm The realm for the authorization process.
   **/
  ORTHANC_PLUGIN_INLINE void OrthancPluginSendUnauthorized(
    OrthancPluginContext*    context,
    OrthancPluginRestOutput* output,
    const char*              realm)
  {
    _OrthancPluginOutputPlusArgument params;
    params.output = output;
    params.argument = realm;
    context->InvokeService(context, _OrthancPluginService_SendUnauthorized, &params);
  }


  /**
   * @brief Signal that this URI does not support this HTTP method.
   *
   * This function answers to a REST request by signaling that the
   * queried URI does not support this method.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param output The HTTP connection to the client application.
   * @param allowedMethods The allowed methods for this URI (e.g. "GET,POST" after a PUT or a POST request).
   **/
  ORTHANC_PLUGIN_INLINE void OrthancPluginSendMethodNotAllowed(
    OrthancPluginContext*    context,
    OrthancPluginRestOutput* output,
    const char*              allowedMethods)
  {
    _OrthancPluginOutputPlusArgument params;
    params.output = output;
    params.argument = allowedMethods;
    context->InvokeService(context, _OrthancPluginService_SendMethodNotAllowed, &params);
  }


  typedef struct
  {
    OrthancPluginRestOutput* output;
    const char*              key;
    const char*              value;
  } _OrthancPluginSetHttpHeader;

  /**
   * @brief Set a cookie.
   *
   * This function sets a cookie in the HTTP client.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param output The HTTP connection to the client application.
   * @param cookie The cookie to be set.
   * @param value The value of the cookie.
   **/
  ORTHANC_PLUGIN_INLINE void OrthancPluginSetCookie(
    OrthancPluginContext*    context,
    OrthancPluginRestOutput* output,
    const char*              cookie,
    const char*              value)
  {
    _OrthancPluginSetHttpHeader params;
    params.output = output;
    params.key = cookie;
    params.value = value;
    context->InvokeService(context, _OrthancPluginService_SetCookie, &params);
  }


  /**
   * @brief Set some HTTP header.
   *
   * This function sets a HTTP header in the HTTP answer.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param output The HTTP connection to the client application.
   * @param key The HTTP header to be set.
   * @param value The value of the HTTP header.
   **/
  ORTHANC_PLUGIN_INLINE void OrthancPluginSetHttpHeader(
    OrthancPluginContext*    context,
    OrthancPluginRestOutput* output,
    const char*              key,
    const char*              value)
  {
    _OrthancPluginSetHttpHeader params;
    params.output = output;
    params.key = key;
    params.value = value;
    context->InvokeService(context, _OrthancPluginService_SetHttpHeader, &params);
  }


  typedef struct
  {
    char**                      resultStringToFree;
    const char**                resultString;
    int64_t*                    resultInt64;
    const char*                 key;
    OrthancPluginDicomInstance* instance;
  } _OrthancPluginAccessDicomInstance;


  /**
   * @brief Get the AET of a DICOM instance.
   *
   * This function returns the Application Entity Title (AET) of the
   * DICOM modality from which a DICOM instance originates.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param instance The instance of interest.
   * @return The AET if success, NULL if error.
   **/
  ORTHANC_PLUGIN_INLINE const char* OrthancPluginGetInstanceRemoteAet(
    OrthancPluginContext*        context,
    OrthancPluginDicomInstance*  instance)
  {
    const char* result;

    _OrthancPluginAccessDicomInstance params;
    memset(&params, 0, sizeof(params));
    params.resultString = &result;
    params.instance = instance;

    if (context->InvokeService(context, _OrthancPluginService_GetInstanceRemoteAet, &params))
    {
      /* Error */
      return NULL;
    }
    else
    {
      return result;
    }
  }


  /**
   * @brief Get the size of a DICOM file.
   *
   * This function returns the number of bytes of the given DICOM instance.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param instance The instance of interest.
   * @return The size of the file, -1 in case of error.
   **/
  ORTHANC_PLUGIN_INLINE int64_t OrthancPluginGetInstanceSize(
    OrthancPluginContext*       context,
    OrthancPluginDicomInstance* instance)
  {
    int64_t size;

    _OrthancPluginAccessDicomInstance params;
    memset(&params, 0, sizeof(params));
    params.resultInt64 = &size;
    params.instance = instance;

    if (context->InvokeService(context, _OrthancPluginService_GetInstanceSize, &params))
    {
      /* Error */
      return -1;
    }
    else
    {
      return size;
    }
  }


  /**
   * @brief Get the data of a DICOM file.
   *
   * This function returns a pointer to the content of the given DICOM instance.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param instance The instance of interest.
   * @return The pointer to the DICOM data, NULL in case of error.
   **/
  ORTHANC_PLUGIN_INLINE const char* OrthancPluginGetInstanceData(
    OrthancPluginContext*        context,
    OrthancPluginDicomInstance*  instance)
  {
    const char* result;

    _OrthancPluginAccessDicomInstance params;
    memset(&params, 0, sizeof(params));
    params.resultString = &result;
    params.instance = instance;

    if (context->InvokeService(context, _OrthancPluginService_GetInstanceData, &params))
    {
      /* Error */
      return NULL;
    }
    else
    {
      return result;
    }
  }


  /**
   * @brief Get the DICOM tag hierarchy as a JSON file.
   *
   * This function returns a pointer to a newly created string
   * containing a JSON file. This JSON file encodes the tag hierarchy
   * of the given DICOM instance.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param instance The instance of interest.
   * @return The NULL value in case of error, or a string containing the JSON file.
   * This string must be freed by OrthancPluginFreeString().
   **/
  ORTHANC_PLUGIN_INLINE char* OrthancPluginGetInstanceJson(
    OrthancPluginContext*        context,
    OrthancPluginDicomInstance*  instance)
  {
    char* result;

    _OrthancPluginAccessDicomInstance params;
    memset(&params, 0, sizeof(params));
    params.resultStringToFree = &result;
    params.instance = instance;

    if (context->InvokeService(context, _OrthancPluginService_GetInstanceJson, &params))
    {
      /* Error */
      return NULL;
    }
    else
    {
      return result;
    }
  }


  /**
   * @brief Get the DICOM tag hierarchy as a JSON file (with simplification).
   *
   * This function returns a pointer to a newly created string
   * containing a JSON file. This JSON file encodes the tag hierarchy
   * of the given DICOM instance. In contrast with
   * ::OrthancPluginGetInstanceJson(), the returned JSON file is in
   * its simplified version.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param instance The instance of interest.
   * @return The NULL value in case of error, or a string containing the JSON file.
   * This string must be freed by OrthancPluginFreeString().
   **/
  ORTHANC_PLUGIN_INLINE char* OrthancPluginGetInstanceSimplifiedJson(
    OrthancPluginContext*        context,
    OrthancPluginDicomInstance*  instance)
  {
    char* result;

    _OrthancPluginAccessDicomInstance params;
    memset(&params, 0, sizeof(params));
    params.resultStringToFree = &result;
    params.instance = instance;

    if (context->InvokeService(context, _OrthancPluginService_GetInstanceSimplifiedJson, &params))
    {
      /* Error */
      return NULL;
    }
    else
    {
      return result;
    }
  }


  /**
   * @brief Check whether a DICOM instance is associated with some metadata.
   *
   * This function checks whether the DICOM instance of interest is
   * associated with some metadata. As of Orthanc 0.8.1, in the
   * callbacks registered by
   * ::OrthancPluginRegisterOnStoredInstanceCallback(), the only
   * possibly available metadata are "ReceptionDate", "RemoteAET" and
   * "IndexInSeries".
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param instance The instance of interest.
   * @param metadata The metadata of interest.
   * @return 1 if the metadata is present, 0 if it is absent, -1 in case of error.
   **/
  ORTHANC_PLUGIN_INLINE int  OrthancPluginHasInstanceMetadata(
    OrthancPluginContext*        context,
    OrthancPluginDicomInstance*  instance,
    const char*                  metadata)
  {
    int64_t result;

    _OrthancPluginAccessDicomInstance params;
    memset(&params, 0, sizeof(params));
    params.resultInt64 = &result;
    params.instance = instance;
    params.key = metadata;

    if (context->InvokeService(context, _OrthancPluginService_HasInstanceMetadata, &params))
    {
      /* Error */
      return -1;
    }
    else
    {
      return (result != 0);
    }
  }


  /**
   * @brief Get the value of some metadata associated with a given DICOM instance.
   *
   * This functions returns the value of some metadata that is associated with the DICOM instance of interest.
   * Before calling this function, the existence of the metadata must have been checked with
   * ::OrthancPluginHasInstanceMetadata().
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param instance The instance of interest.
   * @param metadata The metadata of interest.
   * @return The metadata value if success, NULL if error.
   **/
  ORTHANC_PLUGIN_INLINE const char* OrthancPluginGetInstanceMetadata(
    OrthancPluginContext*        context,
    OrthancPluginDicomInstance*  instance,
    const char*                  metadata)
  {
    const char* result;

    _OrthancPluginAccessDicomInstance params;
    memset(&params, 0, sizeof(params));
    params.resultString = &result;
    params.instance = instance;
    params.key = metadata;

    if (context->InvokeService(context, _OrthancPluginService_GetInstanceMetadata, &params))
    {
      /* Error */
      return NULL;
    }
    else
    {
      return result;
    }
  }



  typedef struct
  {
    OrthancPluginStorageCreate  create;
    OrthancPluginStorageRead    read;
    OrthancPluginStorageRemove  remove;
    OrthancPluginFree           free;
  } _OrthancPluginRegisterStorageArea;

  /**
   * @brief Register a custom storage area.
   *
   * This function registers a custom storage area, to replace the
   * built-in way Orthanc stores its files on the filesystem. This
   * function must be called during the initialization of the plugin,
   * i.e. inside the OrthancPluginInitialize() public function.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param create The callback function to store a file on the custom storage area.
   * @param read The callback function to read a file from the custom storage area.
   * @param remove The callback function to remove a file from the custom storage area.
   **/
  ORTHANC_PLUGIN_INLINE void OrthancPluginRegisterStorageArea(
    OrthancPluginContext*       context,
    OrthancPluginStorageCreate  create,
    OrthancPluginStorageRead    read,
    OrthancPluginStorageRemove  remove)
  {
    _OrthancPluginRegisterStorageArea params;
    params.create = create;
    params.read = read;
    params.remove = remove;

#ifdef  __cplusplus
    params.free = ::free;
#else
    params.free = free;
#endif

    context->InvokeService(context, _OrthancPluginService_RegisterStorageArea, &params);
  }



  /**
   * @brief Return the path to the Orthanc executable.
   *
   * This function returns the path to the Orthanc executable.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @return NULL in the case of an error, or a newly allocated string
   * containing the path. This string must be freed by
   * OrthancPluginFreeString().
   **/
  ORTHANC_PLUGIN_INLINE char *OrthancPluginGetOrthancPath(OrthancPluginContext* context)
  {
    char* result;

    _OrthancPluginRetrieveDynamicString params;
    params.result = &result;
    params.argument = NULL;

    if (context->InvokeService(context, _OrthancPluginService_GetOrthancPath, &params))
    {
      /* Error */
      return NULL;
    }
    else
    {
      return result;
    }
  }


  /**
   * @brief Return the directory containing the Orthanc.
   *
   * This function returns the path to the directory containing the Orthanc executable.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @return NULL in the case of an error, or a newly allocated string
   * containing the path. This string must be freed by
   * OrthancPluginFreeString().
   **/
  ORTHANC_PLUGIN_INLINE char *OrthancPluginGetOrthancDirectory(OrthancPluginContext* context)
  {
    char* result;

    _OrthancPluginRetrieveDynamicString params;
    params.result = &result;
    params.argument = NULL;

    if (context->InvokeService(context, _OrthancPluginService_GetOrthancDirectory, &params))
    {
      /* Error */
      return NULL;
    }
    else
    {
      return result;
    }
  }


  /**
   * @brief Return the path to the configuration file(s).
   *
   * This function returns the path to the configuration file(s) that
   * was specified when starting Orthanc. Since version 0.9.1, this
   * path can refer to a folder that stores a set of configuration
   * files. This function is deprecated in favor of
   * OrthancPluginGetConfiguration().
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @return NULL in the case of an error, or a newly allocated string
   * containing the path. This string must be freed by
   * OrthancPluginFreeString().
   * @see OrthancPluginGetConfiguration()
   **/
  ORTHANC_PLUGIN_INLINE char *OrthancPluginGetConfigurationPath(OrthancPluginContext* context)
  {
    char* result;

    _OrthancPluginRetrieveDynamicString params;
    params.result = &result;
    params.argument = NULL;

    if (context->InvokeService(context, _OrthancPluginService_GetConfigurationPath, &params))
    {
      /* Error */
      return NULL;
    }
    else
    {
      return result;
    }
  }



  typedef struct
  {
    OrthancPluginOnChangeCallback callback;
  } _OrthancPluginOnChangeCallback;

  /**
   * @brief Register a callback to monitor changes.
   *
   * This function registers a callback function that is called
   * whenever a change happens to some DICOM resource.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param callback The callback function.
   **/
  ORTHANC_PLUGIN_INLINE void OrthancPluginRegisterOnChangeCallback(
    OrthancPluginContext*          context,
    OrthancPluginOnChangeCallback  callback)
  {
    _OrthancPluginOnChangeCallback params;
    params.callback = callback;

    context->InvokeService(context, _OrthancPluginService_RegisterOnChangeCallback, &params);
  }



  typedef struct
  {
    const char* plugin;
    _OrthancPluginProperty property;
    const char* value;
  } _OrthancPluginSetPluginProperty;


  /**
   * @brief Set the URI where the plugin provides its Web interface.
   *
   * For plugins that come with a Web interface, this function
   * declares the entry path where to find this interface. This
   * information is notably used in the "Plugins" page of Orthanc
   * Explorer.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param uri The root URI for this plugin.
   **/ 
  ORTHANC_PLUGIN_INLINE void OrthancPluginSetRootUri(
    OrthancPluginContext*  context,
    const char*            uri)
  {
    _OrthancPluginSetPluginProperty params;
    params.plugin = OrthancPluginGetName();
    params.property = _OrthancPluginProperty_RootUri;
    params.value = uri;

    context->InvokeService(context, _OrthancPluginService_SetPluginProperty, &params);
  }


  /**
   * @brief Set a description for this plugin.
   *
   * Set a description for this plugin. It is displayed in the
   * "Plugins" page of Orthanc Explorer.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param description The description.
   **/ 
  ORTHANC_PLUGIN_INLINE void OrthancPluginSetDescription(
    OrthancPluginContext*  context,
    const char*            description)
  {
    _OrthancPluginSetPluginProperty params;
    params.plugin = OrthancPluginGetName();
    params.property = _OrthancPluginProperty_Description;
    params.value = description;

    context->InvokeService(context, _OrthancPluginService_SetPluginProperty, &params);
  }


  /**
   * @brief Extend the JavaScript code of Orthanc Explorer.
   *
   * Add JavaScript code to customize the default behavior of Orthanc
   * Explorer. This can for instance be used to add new buttons.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param javascript The custom JavaScript code.
   **/ 
  ORTHANC_PLUGIN_INLINE void OrthancPluginExtendOrthancExplorer(
    OrthancPluginContext*  context,
    const char*            javascript)
  {
    _OrthancPluginSetPluginProperty params;
    params.plugin = OrthancPluginGetName();
    params.property = _OrthancPluginProperty_OrthancExplorer;
    params.value = javascript;

    context->InvokeService(context, _OrthancPluginService_SetPluginProperty, &params);
  }


  typedef struct
  {
    char**       result;
    int32_t      property;
    const char*  value;
  } _OrthancPluginGlobalProperty;


  /**
   * @brief Get the value of a global property.
   *
   * Get the value of a global property that is stored in the Orthanc database. Global
   * properties whose index is below 1024 are reserved by Orthanc.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param property The global property of interest.
   * @param defaultValue The value to return, if the global property is unset.
   * @return The value of the global property, or NULL in the case of an error. This
   * string must be freed by OrthancPluginFreeString().
   **/
  ORTHANC_PLUGIN_INLINE char* OrthancPluginGetGlobalProperty(
    OrthancPluginContext*  context,
    int32_t                property,
    const char*            defaultValue)
  {
    char* result;

    _OrthancPluginGlobalProperty params;
    params.result = &result;
    params.property = property;
    params.value = defaultValue;

    if (context->InvokeService(context, _OrthancPluginService_GetGlobalProperty, &params))
    {
      /* Error */
      return NULL;
    }
    else
    {
      return result;
    }
  }


  /**
   * @brief Set the value of a global property.
   *
   * Set the value of a global property into the Orthanc
   * database. Setting a global property can be used by plugins to
   * save their internal parameters. Plugins are only allowed to set
   * properties whose index are above or equal to 1024 (properties
   * below 1024 are read-only and reserved by Orthanc).
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param property The global property of interest.
   * @param value The value to be set in the global property.
   * @return 0 if success, or the error code if failure.
   **/
  ORTHANC_PLUGIN_INLINE OrthancPluginErrorCode OrthancPluginSetGlobalProperty(
    OrthancPluginContext*  context,
    int32_t                property,
    const char*            value)
  {
    _OrthancPluginGlobalProperty params;
    params.result = NULL;
    params.property = property;
    params.value = value;

    return context->InvokeService(context, _OrthancPluginService_SetGlobalProperty, &params);
  }



  typedef struct
  {
    int32_t   *resultInt32;
    uint32_t  *resultUint32;
    int64_t   *resultInt64;
    uint64_t  *resultUint64;
  } _OrthancPluginReturnSingleValue;

  /**
   * @brief Get the number of command-line arguments.
   *
   * Retrieve the number of command-line arguments that were used to launch Orthanc.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @return The number of arguments.
   **/
  ORTHANC_PLUGIN_INLINE uint32_t OrthancPluginGetCommandLineArgumentsCount(
    OrthancPluginContext*  context)
  {
    uint32_t count = 0;

    _OrthancPluginReturnSingleValue params;
    memset(&params, 0, sizeof(params));
    params.resultUint32 = &count;

    if (context->InvokeService(context, _OrthancPluginService_GetCommandLineArgumentsCount, &params))
    {
      /* Error */
      return 0;
    }
    else
    {
      return count;
    }
  }



  /**
   * @brief Get the value of a command-line argument.
   *
   * Get the value of one of the command-line arguments that were used
   * to launch Orthanc. The number of available arguments can be
   * retrieved by OrthancPluginGetCommandLineArgumentsCount().
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param argument The index of the argument.
   * @return The value of the argument, or NULL in the case of an error. This
   * string must be freed by OrthancPluginFreeString().
   **/
  ORTHANC_PLUGIN_INLINE char* OrthancPluginGetCommandLineArgument(
    OrthancPluginContext*  context,
    uint32_t               argument)
  {
    char* result;

    _OrthancPluginGlobalProperty params;
    params.result = &result;
    params.property = (int32_t) argument;
    params.value = NULL;

    if (context->InvokeService(context, _OrthancPluginService_GetCommandLineArgument, &params))
    {
      /* Error */
      return NULL;
    }
    else
    {
      return result;
    }
  }


  /**
   * @brief Get the expected version of the database schema.
   *
   * Retrieve the expected version of the database schema.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @return The version.
   **/
  ORTHANC_PLUGIN_INLINE uint32_t OrthancPluginGetExpectedDatabaseVersion(
    OrthancPluginContext*  context)
  {
    uint32_t count = 0;

    _OrthancPluginReturnSingleValue params;
    memset(&params, 0, sizeof(params));
    params.resultUint32 = &count;

    if (context->InvokeService(context, _OrthancPluginService_GetExpectedDatabaseVersion, &params))
    {
      /* Error */
      return 0;
    }
    else
    {
      return count;
    }
  }



  /**
   * @brief Return the content of the configuration file(s).
   *
   * This function returns the content of the configuration that is
   * used by Orthanc, formatted as a JSON string.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @return NULL in the case of an error, or a newly allocated string
   * containing the configuration. This string must be freed by
   * OrthancPluginFreeString().
   **/
  ORTHANC_PLUGIN_INLINE char *OrthancPluginGetConfiguration(OrthancPluginContext* context)
  {
    char* result;

    _OrthancPluginRetrieveDynamicString params;
    params.result = &result;
    params.argument = NULL;

    if (context->InvokeService(context, _OrthancPluginService_GetConfiguration, &params))
    {
      /* Error */
      return NULL;
    }
    else
    {
      return result;
    }
  }



  typedef struct
  {
    OrthancPluginRestOutput* output;
    const char*              subType;
    const char*              contentType;
  } _OrthancPluginStartMultipartAnswer;

  /**
   * @brief Start an HTTP multipart answer.
   *
   * Initiates a HTTP multipart answer, as the result of a REST request.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param output The HTTP connection to the client application.
   * @param subType The sub-type of the multipart answer ("mixed" or "related").
   * @param contentType The MIME type of the items in the multipart answer.
   * @return 0 if success, or the error code if failure.
   * @see OrthancPluginSendMultipartItem()
   **/
  ORTHANC_PLUGIN_INLINE OrthancPluginErrorCode OrthancPluginStartMultipartAnswer(
    OrthancPluginContext*    context,
    OrthancPluginRestOutput* output,
    const char*              subType,
    const char*              contentType)
  {
    _OrthancPluginStartMultipartAnswer params;
    params.output = output;
    params.subType = subType;
    params.contentType = contentType;
    return context->InvokeService(context, _OrthancPluginService_StartMultipartAnswer, &params);
  }


  /**
   * @brief Send an item as a part of some HTTP multipart answer.
   *
   * This function sends an item as a part of some HTTP multipart
   * answer that was initiated by OrthancPluginStartMultipartAnswer().
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param output The HTTP connection to the client application.
   * @param answer Pointer to the memory buffer containing the item.
   * @param answerSize Number of bytes of the item.
   * @return 0 if success, or the error code if failure (this notably happens
   * if the connection is closed by the client).
   **/
  ORTHANC_PLUGIN_INLINE OrthancPluginErrorCode OrthancPluginSendMultipartItem(
    OrthancPluginContext*    context,
    OrthancPluginRestOutput* output,
    const char*              answer,
    uint32_t                 answerSize)
  {
    _OrthancPluginAnswerBuffer params;
    params.output = output;
    params.answer = answer;
    params.answerSize = answerSize;
    params.mimeType = NULL;
    return context->InvokeService(context, _OrthancPluginService_SendMultipartItem, &params);
  }



  typedef struct
  {
    OrthancPluginMemoryBuffer*    target;
    const void*                   source;
    uint32_t                      size;
    OrthancPluginCompressionType  compression;
    uint8_t                       uncompress;
  } _OrthancPluginBufferCompression;


  /**
   * @brief Compress or decompress a buffer.
   *
   * This function compresses or decompresses a buffer, using the
   * version of the zlib library that is used by the Orthanc core.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param target The target memory buffer.
   * @param source The source buffer.
   * @param size The size in bytes of the source buffer.
   * @param compression The compression algorithm.
   * @param uncompress If set to "0", the buffer must be compressed. 
   * If set to "1", the buffer must be uncompressed.
   * @return 0 if success, or the error code if failure.
   **/
  ORTHANC_PLUGIN_INLINE OrthancPluginErrorCode OrthancPluginBufferCompression(
    OrthancPluginContext*         context,
    OrthancPluginMemoryBuffer*    target,
    const void*                   source,
    uint32_t                      size,
    OrthancPluginCompressionType  compression,
    uint8_t                       uncompress)
  {
    _OrthancPluginBufferCompression params;
    params.target = target;
    params.source = source;
    params.size = size;
    params.compression = compression;
    params.uncompress = uncompress;

    return context->InvokeService(context, _OrthancPluginService_BufferCompression, &params);
  }



  typedef struct
  {
    OrthancPluginMemoryBuffer*  target;
    const char*                 path;
  } _OrthancPluginReadFile;

  /**
   * @brief Read a file.
   * 
   * Read the content of a file on the filesystem, and returns it into
   * a newly allocated memory buffer.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param target The target memory buffer.
   * @param path The path of the file to be read.
   * @return 0 if success, or the error code if failure.
   **/
  ORTHANC_PLUGIN_INLINE OrthancPluginErrorCode  OrthancPluginReadFile(
    OrthancPluginContext*       context,
    OrthancPluginMemoryBuffer*  target,
    const char*                 path)
  {
    _OrthancPluginReadFile params;
    params.target = target;
    params.path = path;
    return context->InvokeService(context, _OrthancPluginService_ReadFile, &params);
  }



  typedef struct
  {
    const char*  path;
    const void*  data;
    uint32_t     size;
  } _OrthancPluginWriteFile;

  /**
   * @brief Write a file.
   * 
   * Write the content of a memory buffer to the filesystem.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param path The path of the file to be written.
   * @param data The content of the memory buffer.
   * @param size The size of the memory buffer.
   * @return 0 if success, or the error code if failure.
   **/
  ORTHANC_PLUGIN_INLINE OrthancPluginErrorCode  OrthancPluginWriteFile(
    OrthancPluginContext*  context,
    const char*            path,
    const void*            data,
    uint32_t               size)
  {
    _OrthancPluginWriteFile params;
    params.path = path;
    params.data = data;
    params.size = size;
    return context->InvokeService(context, _OrthancPluginService_WriteFile, &params);
  }



  typedef struct
  {
    const char**            target;
    OrthancPluginErrorCode  error;
  } _OrthancPluginGetErrorDescription;

  /**
   * @brief Get the description of a given error code.
   *
   * This function returns the description of a given error code.
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param error The error code of interest.
   * @return The error description. This is a statically-allocated
   * string, do not free it.
   **/
  ORTHANC_PLUGIN_INLINE const char* OrthancPluginGetErrorDescription(
    OrthancPluginContext*    context,
    OrthancPluginErrorCode   error)
  {
    const char* result = NULL;

    _OrthancPluginGetErrorDescription params;
    params.target = &result;
    params.error = error;

    if (context->InvokeService(context, _OrthancPluginService_GetErrorDescription, &params) ||
        result == NULL)
    {
      return "Unknown error code";
    }
    else
    {
      return result;
    }
  }



  typedef struct
  {
    OrthancPluginRestOutput* output;
    uint16_t                 status;
    const char*              body;
    uint32_t                 bodySize;
  } _OrthancPluginSendHttpStatus;

  /**
   * @brief Send a HTTP status, with a custom body.
   *
   * This function answers to a HTTP request by sending a HTTP status
   * code (such as "400 - Bad Request"), together with a body
   * describing the error. The body will only be returned if the
   * configuration option "HttpDescribeErrors" of Orthanc is set to "true".
   * 
   * Note that:
   * - Successful requests (status 200) must use ::OrthancPluginAnswerBuffer().
   * - Redirections (status 301) must use ::OrthancPluginRedirect().
   * - Unauthorized access (status 401) must use ::OrthancPluginSendUnauthorized().
   * - Methods not allowed (status 405) must use ::OrthancPluginSendMethodNotAllowed().
   * 
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param output The HTTP connection to the client application.
   * @param status The HTTP status code to be sent.
   * @param body The body of the answer.
   * @param bodySize The size of the body.
   * @see OrthancPluginSendHttpStatusCode()
   **/
  ORTHANC_PLUGIN_INLINE void OrthancPluginSendHttpStatus(
    OrthancPluginContext*    context,
    OrthancPluginRestOutput* output,
    uint16_t                 status,
    const char*              body,
    uint32_t                 bodySize)
  {
    _OrthancPluginSendHttpStatus params;
    params.output = output;
    params.status = status;
    params.body = body;
    params.bodySize = bodySize;
    context->InvokeService(context, _OrthancPluginService_SendHttpStatus, &params);
  }



  typedef struct
  {
    const OrthancPluginImage*  image;
    uint32_t*                  resultUint32;
    OrthancPluginPixelFormat*  resultPixelFormat;
    const void**               resultBuffer;
  } _OrthancPluginGetImageInfo;


  ORTHANC_PLUGIN_INLINE OrthancPluginPixelFormat  OrthancPluginGetImagePixelFormat(
    OrthancPluginContext*      context,
    const OrthancPluginImage*  image)
  {
    OrthancPluginPixelFormat target;
    
    _OrthancPluginGetImageInfo params;
    memset(&params, 0, sizeof(params));
    params.image = image;
    params.resultPixelFormat = &target;

    if (context->InvokeService(context, _OrthancPluginService_GetImagePixelFormat, image) < 0)
    {
      return OrthancPluginPixelFormat_Unknown;
    }
    else
    {
      return static_cast<OrthancPluginPixelFormat>(target);
    }
  }



  ORTHANC_PLUGIN_INLINE uint32_t  OrthancPluginGetImageWidth(
    OrthancPluginContext*      context,
    const OrthancPluginImage*  image)
  {
    uint32_t width;
    
    _OrthancPluginGetImageInfo params;
    memset(&params, 0, sizeof(params));
    params.image = image;
    params.resultUint32 = &width;

    if (context->InvokeService(context, _OrthancPluginService_GetImagePixelFormat, image) < 0)
    {
      return 0;
    }
    else
    {
      return width;
    }
  }



  ORTHANC_PLUGIN_INLINE uint32_t  OrthancPluginGetImageHeight(
    OrthancPluginContext*      context,
    const OrthancPluginImage*  image)
  {
    uint32_t height;
    
    _OrthancPluginGetImageInfo params;
    memset(&params, 0, sizeof(params));
    params.image = image;
    params.resultUint32 = &height;

    if (context->InvokeService(context, _OrthancPluginService_GetImagePixelFormat, image) < 0)
    {
      return 0;
    }
    else
    {
      return height;
    }
  }



  ORTHANC_PLUGIN_INLINE uint32_t  OrthancPluginGetImagePitch(
    OrthancPluginContext*      context,
    const OrthancPluginImage*  image)
  {
    uint32_t pitch;
    
    _OrthancPluginGetImageInfo params;
    memset(&params, 0, sizeof(params));
    params.image = image;
    params.resultUint32 = &pitch;

    if (context->InvokeService(context, _OrthancPluginService_GetImagePixelFormat, image) < 0)
    {
      return 0;
    }
    else
    {
      return pitch;
    }
  }



  ORTHANC_PLUGIN_INLINE const void*  OrthancPluginGetImageBuffer(
    OrthancPluginContext*      context,
    const OrthancPluginImage*  image)
  {
    const void* target = NULL;

    _OrthancPluginGetImageInfo params;
    memset(&params, 0, sizeof(params));
    params.resultBuffer = &target;
    params.image = image;

    if (context->InvokeService(context, _OrthancPluginService_GetImageBuffer, &params) < 0)
    {
      return NULL;
    }
    else
    {
      return target;
    }
  }


  typedef struct
  {
    OrthancPluginImage**       target;
    const void*                data;
    uint32_t                   size;
    OrthancPluginImageFormat   format;
  } _OrthancPluginUncompressImage;

  ORTHANC_PLUGIN_INLINE OrthancPluginImage *OrthancPluginUncompressImage(
    OrthancPluginContext*      context,
    const void*                data,
    uint32_t                   size,
    OrthancPluginImageFormat   format)
  {
    OrthancPluginImage* target = NULL;

    _OrthancPluginUncompressImage params;
    memset(&params, 0, sizeof(params));
    params.target = &target;
    params.data = data;
    params.size = size;
    params.format = format;

    if (context->InvokeService(context, _OrthancPluginService_UncompressImage, &params) < 0)
    {
      return NULL;
    }
    else
    {
      return target;
    }
  }



  ORTHANC_PLUGIN_INLINE void  OrthancPluginFreeImage(
    OrthancPluginContext* context, 
    OrthancPluginImage*   image)
  {
    _OrthancPluginGetImageInfo params;
    memset(&params, 0, sizeof(params));
    params.image = image;

    context->InvokeService(context, _OrthancPluginService_FreeImage, &params);
  }




  typedef struct
  {
    OrthancPluginMemoryBuffer* target;
    OrthancPluginImageFormat   imageFormat;
    OrthancPluginPixelFormat   pixelFormat;
    uint32_t                   width;
    uint32_t                   height;
    uint32_t                   pitch;
    const void*                buffer;
    uint32_t                   quality;
  } _OrthancPluginCompressImage;



  ORTHANC_PLUGIN_INLINE OrthancPluginErrorCode OrthancPluginCompressPngImage(
    OrthancPluginContext*         context,
    OrthancPluginMemoryBuffer*    target,
    OrthancPluginPixelFormat      pixelFormat,
    uint32_t                      width,
    uint32_t                      height,
    uint32_t                      pitch,
    const void*                   buffer,
    uint32_t                      quality)
  {
    _OrthancPluginCompressImage params;
    memset(&params, 0, sizeof(params));
    params.target = target;
    params.imageFormat = OrthancPluginImageFormat_Png;
    params.pixelFormat = pixelFormat;
    params.width = width;
    params.height = height;
    params.pitch = pitch;
    params.buffer = buffer;
    params.quality = quality;

    return context->InvokeService(context, _OrthancPluginService_CompressImage, &params);
  }


  ORTHANC_PLUGIN_INLINE OrthancPluginErrorCode OrthancPluginCompressJpegImage(
    OrthancPluginContext*         context,
    OrthancPluginMemoryBuffer*    target,
    OrthancPluginPixelFormat      pixelFormat,
    uint32_t                      width,
    uint32_t                      height,
    uint32_t                      pitch,
    const void*                   buffer,
    uint32_t                      quality)
  {
    _OrthancPluginCompressImage params;
    memset(&params, 0, sizeof(params));
    params.target = target;
    params.imageFormat = OrthancPluginImageFormat_Jpeg;
    params.pixelFormat = pixelFormat;
    params.width = width;
    params.height = height;
    params.pitch = pitch;
    params.buffer = buffer;
    params.quality = quality;

    return context->InvokeService(context, _OrthancPluginService_CompressImage, &params);
  }



#ifdef  __cplusplus
}
#endif


/** @} */

