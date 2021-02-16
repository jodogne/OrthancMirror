/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 **/


#pragma once

#include "OrthancFramework.h"

#include <set>
#include <string>


namespace Orthanc
{
  static const char* const URI_SCHEME_PREFIX_BINARY = "data:application/octet-stream;base64,";

  static const char* const MIME_BINARY = "application/octet-stream";
  static const char* const MIME_JPEG = "image/jpeg";
  static const char* const MIME_JSON = "application/json";
  static const char* const MIME_JSON_UTF8 = "application/json; charset=utf-8";
  static const char* const MIME_PDF = "application/pdf";
  static const char* const MIME_PNG = "image/png";
  static const char* const MIME_XML = "application/xml";
  static const char* const MIME_XML_UTF8 = "application/xml; charset=utf-8";

  /**
   * "No Internet Media Type (aka MIME type, content type) for PBM has
   * been registered with IANA, but the unofficial value
   * image/x-portable-arbitrarymap is assigned by this specification,
   * to be consistent with conventional values for the older Netpbm
   * formats."  http://netpbm.sourceforge.net/doc/pam.html
   **/
  static const char* const MIME_PAM = "image/x-portable-arbitrarymap";


  enum MimeType
  {
    MimeType_Binary,
    MimeType_Css,
    MimeType_Dicom,
    MimeType_Gif,
    MimeType_Gzip,
    MimeType_Html,
    MimeType_JavaScript,
    MimeType_Jpeg,
    MimeType_Jpeg2000,
    MimeType_Json,
    MimeType_NaCl,
    MimeType_PNaCl,
    MimeType_Pam,
    MimeType_Pdf,
    MimeType_PlainText,
    MimeType_Png,
    MimeType_Svg,
    MimeType_WebAssembly,
    MimeType_Xml,
    MimeType_Woff,            // Web Open Font Format
    MimeType_Woff2,
    MimeType_Zip,
    MimeType_PrometheusText,  // Prometheus text-based exposition format (for metrics)
    MimeType_DicomWebJson,
    MimeType_DicomWebXml
  };

  
  enum Endianness
  {
    Endianness_Unknown,
    Endianness_Big,
    Endianness_Little
  };

  // This enumeration is autogenerated by the script
  // "Resources/GenerateErrorCodes.py"
  enum ErrorCode
  {
    ErrorCode_InternalError = -1    /*!< Internal error */,
    ErrorCode_Success = 0    /*!< Success */,
    ErrorCode_Plugin = 1    /*!< Error encountered within the plugin engine */,
    ErrorCode_NotImplemented = 2    /*!< Not implemented yet */,
    ErrorCode_ParameterOutOfRange = 3    /*!< Parameter out of range */,
    ErrorCode_NotEnoughMemory = 4    /*!< The server hosting Orthanc is running out of memory */,
    ErrorCode_BadParameterType = 5    /*!< Bad type for a parameter */,
    ErrorCode_BadSequenceOfCalls = 6    /*!< Bad sequence of calls */,
    ErrorCode_InexistentItem = 7    /*!< Accessing an inexistent item */,
    ErrorCode_BadRequest = 8    /*!< Bad request */,
    ErrorCode_NetworkProtocol = 9    /*!< Error in the network protocol */,
    ErrorCode_SystemCommand = 10    /*!< Error while calling a system command */,
    ErrorCode_Database = 11    /*!< Error with the database engine */,
    ErrorCode_UriSyntax = 12    /*!< Badly formatted URI */,
    ErrorCode_InexistentFile = 13    /*!< Inexistent file */,
    ErrorCode_CannotWriteFile = 14    /*!< Cannot write to file */,
    ErrorCode_BadFileFormat = 15    /*!< Bad file format */,
    ErrorCode_Timeout = 16    /*!< Timeout */,
    ErrorCode_UnknownResource = 17    /*!< Unknown resource */,
    ErrorCode_IncompatibleDatabaseVersion = 18    /*!< Incompatible version of the database */,
    ErrorCode_FullStorage = 19    /*!< The file storage is full */,
    ErrorCode_CorruptedFile = 20    /*!< Corrupted file (e.g. inconsistent MD5 hash) */,
    ErrorCode_InexistentTag = 21    /*!< Inexistent tag */,
    ErrorCode_ReadOnly = 22    /*!< Cannot modify a read-only data structure */,
    ErrorCode_IncompatibleImageFormat = 23    /*!< Incompatible format of the images */,
    ErrorCode_IncompatibleImageSize = 24    /*!< Incompatible size of the images */,
    ErrorCode_SharedLibrary = 25    /*!< Error while using a shared library (plugin) */,
    ErrorCode_UnknownPluginService = 26    /*!< Plugin invoking an unknown service */,
    ErrorCode_UnknownDicomTag = 27    /*!< Unknown DICOM tag */,
    ErrorCode_BadJson = 28    /*!< Cannot parse a JSON document */,
    ErrorCode_Unauthorized = 29    /*!< Bad credentials were provided to an HTTP request */,
    ErrorCode_BadFont = 30    /*!< Badly formatted font file */,
    ErrorCode_DatabasePlugin = 31    /*!< The plugin implementing a custom database back-end does not fulfill the proper interface */,
    ErrorCode_StorageAreaPlugin = 32    /*!< Error in the plugin implementing a custom storage area */,
    ErrorCode_EmptyRequest = 33    /*!< The request is empty */,
    ErrorCode_NotAcceptable = 34    /*!< Cannot send a response which is acceptable according to the Accept HTTP header */,
    ErrorCode_NullPointer = 35    /*!< Cannot handle a NULL pointer */,
    ErrorCode_DatabaseUnavailable = 36    /*!< The database is currently not available (probably a transient situation) */,
    ErrorCode_CanceledJob = 37    /*!< This job was canceled */,
    ErrorCode_BadGeometry = 38    /*!< Geometry error encountered in Stone */,
    ErrorCode_SslInitialization = 39    /*!< Cannot initialize SSL encryption, check out your certificates */,
    ErrorCode_DiscontinuedAbi = 40    /*!< Calling a function that has been removed from the Orthanc Framework */,
    ErrorCode_BadRange = 41    /*!< Incorrect range request */,
    ErrorCode_SQLiteNotOpened = 1000    /*!< SQLite: The database is not opened */,
    ErrorCode_SQLiteAlreadyOpened = 1001    /*!< SQLite: Connection is already open */,
    ErrorCode_SQLiteCannotOpen = 1002    /*!< SQLite: Unable to open the database */,
    ErrorCode_SQLiteStatementAlreadyUsed = 1003    /*!< SQLite: This cached statement is already being referred to */,
    ErrorCode_SQLiteExecute = 1004    /*!< SQLite: Cannot execute a command */,
    ErrorCode_SQLiteRollbackWithoutTransaction = 1005    /*!< SQLite: Rolling back a nonexistent transaction (have you called Begin()?) */,
    ErrorCode_SQLiteCommitWithoutTransaction = 1006    /*!< SQLite: Committing a nonexistent transaction */,
    ErrorCode_SQLiteRegisterFunction = 1007    /*!< SQLite: Unable to register a function */,
    ErrorCode_SQLiteFlush = 1008    /*!< SQLite: Unable to flush the database */,
    ErrorCode_SQLiteCannotRun = 1009    /*!< SQLite: Cannot run a cached statement */,
    ErrorCode_SQLiteCannotStep = 1010    /*!< SQLite: Cannot step over a cached statement */,
    ErrorCode_SQLiteBindOutOfRange = 1011    /*!< SQLite: Bing a value while out of range (serious error) */,
    ErrorCode_SQLitePrepareStatement = 1012    /*!< SQLite: Cannot prepare a cached statement */,
    ErrorCode_SQLiteTransactionAlreadyStarted = 1013    /*!< SQLite: Beginning the same transaction twice */,
    ErrorCode_SQLiteTransactionCommit = 1014    /*!< SQLite: Failure when committing the transaction */,
    ErrorCode_SQLiteTransactionBegin = 1015    /*!< SQLite: Cannot start a transaction */,
    ErrorCode_DirectoryOverFile = 2000    /*!< The directory to be created is already occupied by a regular file */,
    ErrorCode_FileStorageCannotWrite = 2001    /*!< Unable to create a subdirectory or a file in the file storage */,
    ErrorCode_DirectoryExpected = 2002    /*!< The specified path does not point to a directory */,
    ErrorCode_HttpPortInUse = 2003    /*!< The TCP port of the HTTP server is privileged or already in use */,
    ErrorCode_DicomPortInUse = 2004    /*!< The TCP port of the DICOM server is privileged or already in use */,
    ErrorCode_BadHttpStatusInRest = 2005    /*!< This HTTP status is not allowed in a REST API */,
    ErrorCode_RegularFileExpected = 2006    /*!< The specified path does not point to a regular file */,
    ErrorCode_PathToExecutable = 2007    /*!< Unable to get the path to the executable */,
    ErrorCode_MakeDirectory = 2008    /*!< Cannot create a directory */,
    ErrorCode_BadApplicationEntityTitle = 2009    /*!< An application entity title (AET) cannot be empty or be longer than 16 characters */,
    ErrorCode_NoCFindHandler = 2010    /*!< No request handler factory for DICOM C-FIND SCP */,
    ErrorCode_NoCMoveHandler = 2011    /*!< No request handler factory for DICOM C-MOVE SCP */,
    ErrorCode_NoCStoreHandler = 2012    /*!< No request handler factory for DICOM C-STORE SCP */,
    ErrorCode_NoApplicationEntityFilter = 2013    /*!< No application entity filter */,
    ErrorCode_NoSopClassOrInstance = 2014    /*!< DicomUserConnection: Unable to find the SOP class and instance */,
    ErrorCode_NoPresentationContext = 2015    /*!< DicomUserConnection: No acceptable presentation context for modality */,
    ErrorCode_DicomFindUnavailable = 2016    /*!< DicomUserConnection: The C-FIND command is not supported by the remote SCP */,
    ErrorCode_DicomMoveUnavailable = 2017    /*!< DicomUserConnection: The C-MOVE command is not supported by the remote SCP */,
    ErrorCode_CannotStoreInstance = 2018    /*!< Cannot store an instance */,
    ErrorCode_CreateDicomNotString = 2019    /*!< Only string values are supported when creating DICOM instances */,
    ErrorCode_CreateDicomOverrideTag = 2020    /*!< Trying to override a value inherited from a parent module */,
    ErrorCode_CreateDicomUseContent = 2021    /*!< Use \"Content\" to inject an image into a new DICOM instance */,
    ErrorCode_CreateDicomNoPayload = 2022    /*!< No payload is present for one instance in the series */,
    ErrorCode_CreateDicomUseDataUriScheme = 2023    /*!< The payload of the DICOM instance must be specified according to Data URI scheme */,
    ErrorCode_CreateDicomBadParent = 2024    /*!< Trying to attach a new DICOM instance to an inexistent resource */,
    ErrorCode_CreateDicomParentIsInstance = 2025    /*!< Trying to attach a new DICOM instance to an instance (must be a series, study or patient) */,
    ErrorCode_CreateDicomParentEncoding = 2026    /*!< Unable to get the encoding of the parent resource */,
    ErrorCode_UnknownModality = 2027    /*!< Unknown modality */,
    ErrorCode_BadJobOrdering = 2028    /*!< Bad ordering of filters in a job */,
    ErrorCode_JsonToLuaTable = 2029    /*!< Cannot convert the given JSON object to a Lua table */,
    ErrorCode_CannotCreateLua = 2030    /*!< Cannot create the Lua context */,
    ErrorCode_CannotExecuteLua = 2031    /*!< Cannot execute a Lua command */,
    ErrorCode_LuaAlreadyExecuted = 2032    /*!< Arguments cannot be pushed after the Lua function is executed */,
    ErrorCode_LuaBadOutput = 2033    /*!< The Lua function does not give the expected number of outputs */,
    ErrorCode_NotLuaPredicate = 2034    /*!< The Lua function is not a predicate (only true/false outputs allowed) */,
    ErrorCode_LuaReturnsNoString = 2035    /*!< The Lua function does not return a string */,
    ErrorCode_StorageAreaAlreadyRegistered = 2036    /*!< Another plugin has already registered a custom storage area */,
    ErrorCode_DatabaseBackendAlreadyRegistered = 2037    /*!< Another plugin has already registered a custom database back-end */,
    ErrorCode_DatabaseNotInitialized = 2038    /*!< Plugin trying to call the database during its initialization */,
    ErrorCode_SslDisabled = 2039    /*!< Orthanc has been built without SSL support */,
    ErrorCode_CannotOrderSlices = 2040    /*!< Unable to order the slices of the series */,
    ErrorCode_NoWorklistHandler = 2041    /*!< No request handler factory for DICOM C-Find Modality SCP */,
    ErrorCode_AlreadyExistingTag = 2042    /*!< Cannot override the value of a tag that already exists */,
    ErrorCode_NoStorageCommitmentHandler = 2043    /*!< No request handler factory for DICOM N-ACTION SCP (storage commitment) */,
    ErrorCode_NoCGetHandler = 2044    /*!< No request handler factory for DICOM C-GET SCP */,
    ErrorCode_UnsupportedMediaType = 3000    /*!< Unsupported media type */,
    ErrorCode_START_PLUGINS = 1000000
  };

  // This enumeration is autogenerated by the script
  // "Resources/GenerateTransferSyntaxes.py"
  enum DicomTransferSyntax
  {
    DicomTransferSyntax_LittleEndianImplicit    /*!< Implicit VR Little Endian */,
    DicomTransferSyntax_LittleEndianExplicit    /*!< Explicit VR Little Endian */,
    DicomTransferSyntax_DeflatedLittleEndianExplicit    /*!< Deflated Explicit VR Little Endian */,
    DicomTransferSyntax_BigEndianExplicit    /*!< Explicit VR Big Endian */,
    DicomTransferSyntax_JPEGProcess1    /*!< JPEG Baseline (process 1, lossy) */,
    DicomTransferSyntax_JPEGProcess2_4    /*!< JPEG Extended Sequential (processes 2 & 4) */,
    DicomTransferSyntax_JPEGProcess3_5    /*!< JPEG Extended Sequential (lossy, 8/12 bit), arithmetic coding */,
    DicomTransferSyntax_JPEGProcess6_8    /*!< JPEG Spectral Selection, Nonhierarchical (lossy, 8/12 bit) */,
    DicomTransferSyntax_JPEGProcess7_9    /*!< JPEG Spectral Selection, Nonhierarchical (lossy, 8/12 bit), arithmetic coding */,
    DicomTransferSyntax_JPEGProcess10_12    /*!< JPEG Full Progression, Nonhierarchical (lossy, 8/12 bit) */,
    DicomTransferSyntax_JPEGProcess11_13    /*!< JPEG Full Progression, Nonhierarchical (lossy, 8/12 bit), arithmetic coding */,
    DicomTransferSyntax_JPEGProcess14    /*!< JPEG Lossless, Nonhierarchical with any selection value (process 14) */,
    DicomTransferSyntax_JPEGProcess15    /*!< JPEG Lossless with any selection value, arithmetic coding */,
    DicomTransferSyntax_JPEGProcess16_18    /*!< JPEG Extended Sequential, Hierarchical (lossy, 8/12 bit) */,
    DicomTransferSyntax_JPEGProcess17_19    /*!< JPEG Extended Sequential, Hierarchical (lossy, 8/12 bit), arithmetic coding */,
    DicomTransferSyntax_JPEGProcess20_22    /*!< JPEG Spectral Selection, Hierarchical (lossy, 8/12 bit) */,
    DicomTransferSyntax_JPEGProcess21_23    /*!< JPEG Spectral Selection, Hierarchical (lossy, 8/12 bit), arithmetic coding */,
    DicomTransferSyntax_JPEGProcess24_26    /*!< JPEG Full Progression, Hierarchical (lossy, 8/12 bit) */,
    DicomTransferSyntax_JPEGProcess25_27    /*!< JPEG Full Progression, Hierarchical (lossy, 8/12 bit), arithmetic coding */,
    DicomTransferSyntax_JPEGProcess28    /*!< JPEG Lossless, Hierarchical */,
    DicomTransferSyntax_JPEGProcess29    /*!< JPEG Lossless, Hierarchical, arithmetic coding */,
    DicomTransferSyntax_JPEGProcess14SV1    /*!< JPEG Lossless, Nonhierarchical, First-Order Prediction (Processes 14 [Selection Value 1]) */,
    DicomTransferSyntax_JPEGLSLossless    /*!< JPEG-LS (lossless) */,
    DicomTransferSyntax_JPEGLSLossy    /*!< JPEG-LS (lossy or near-lossless) */,
    DicomTransferSyntax_JPEG2000LosslessOnly    /*!< JPEG 2000 (lossless) */,
    DicomTransferSyntax_JPEG2000    /*!< JPEG 2000 (lossless or lossy) */,
    DicomTransferSyntax_JPEG2000MulticomponentLosslessOnly    /*!< JPEG 2000 part 2 multicomponent extensions (lossless) */,
    DicomTransferSyntax_JPEG2000Multicomponent    /*!< JPEG 2000 part 2 multicomponent extensions (lossless or lossy) */,
    DicomTransferSyntax_JPIPReferenced    /*!< JPIP Referenced */,
    DicomTransferSyntax_JPIPReferencedDeflate    /*!< JPIP Referenced Deflate */,
    DicomTransferSyntax_MPEG2MainProfileAtMainLevel    /*!< MPEG2 Main Profile / Main Level */,
    DicomTransferSyntax_MPEG2MainProfileAtHighLevel    /*!< MPEG2 Main Profile / High Level */,
    DicomTransferSyntax_MPEG4HighProfileLevel4_1    /*!< MPEG4 AVC/H.264 High Profile / Level 4.1 */,
    DicomTransferSyntax_MPEG4BDcompatibleHighProfileLevel4_1    /*!< MPEG4 AVC/H.264 BD-compatible High Profile / Level 4.1 */,
    DicomTransferSyntax_MPEG4HighProfileLevel4_2_For2DVideo    /*!< MPEG4 AVC/H.264 High Profile / Level 4.2 For 2D Video */,
    DicomTransferSyntax_MPEG4HighProfileLevel4_2_For3DVideo    /*!< MPEG4 AVC/H.264 High Profile / Level 4.2 For 3D Video */,
    DicomTransferSyntax_MPEG4StereoHighProfileLevel4_2    /*!< MPEG4 AVC/H.264 Stereo High Profile / Level 4.2 */,
    DicomTransferSyntax_HEVCMainProfileLevel5_1    /*!< HEVC/H.265 Main Profile / Level 5.1 */,
    DicomTransferSyntax_HEVCMain10ProfileLevel5_1    /*!< HEVC/H.265 Main 10 Profile / Level 5.1 */,
    DicomTransferSyntax_RLELossless    /*!< RLE - Run Length Encoding (lossless) */,
    DicomTransferSyntax_RFC2557MimeEncapsulation    /*!< RFC 2557 MIME Encapsulation */,
    DicomTransferSyntax_XML    /*!< XML Encoding */
  };


  /**
   * {summary}{The memory layout of the pixels (resp. voxels) of a 2D (resp. 3D) image.}
   **/
  enum PixelFormat
  {
    /**
     * {summary}{Color image in RGB24 format.}
     * {description}{This format describes a color image. The pixels are stored in 3
     * consecutive bytes. The memory layout is RGB.}
     **/
    PixelFormat_RGB24 = 1,

    /**
     * {summary}{Color image in RGBA32 format.}
     * {description}{This format describes a color image. The pixels are stored in 4
     * consecutive bytes. The memory layout is RGBA.}
     **/
    PixelFormat_RGBA32 = 2,

    /**
     * {summary}{Graylevel 8bpp image.}
     * {description}{The image is graylevel. Each pixel is unsigned and stored in one byte.}
     **/
    PixelFormat_Grayscale8 = 3,
      
    /**
     * {summary}{Graylevel, unsigned 16bpp image.}
     * {description}{The image is graylevel. Each pixel is unsigned and stored in two bytes.}
     **/
    PixelFormat_Grayscale16 = 4,
      
    /**
     * {summary}{Graylevel, signed 16bpp image.}
     * {description}{The image is graylevel. Each pixel is signed and stored in two bytes.}
     **/
    PixelFormat_SignedGrayscale16 = 5,
      
    /**
     * {summary}{Graylevel, floating-point image.}
     * {description}{The image is graylevel. Each pixel is floating-point and stored in 4 bytes.}
     **/
    PixelFormat_Float32 = 6,

    // This is the memory layout for Cairo (for internal use in Stone of Orthanc)
    PixelFormat_BGRA32 = 7,

    /**
     * {summary}{Graylevel, unsigned 32bpp image.}
     * {description}{The image is graylevel. Each pixel is unsigned and stored in 4 bytes.}
     **/
    PixelFormat_Grayscale32 = 8,
    
    /**
     * {summary}{Color image in RGB48 format.}
     * {description}{This format describes a color image. The pixels are stored in 6
     * consecutive bytes. The memory layout is RGB.}
     **/
    PixelFormat_RGB48 = 9,

    /**
     * {summary}{Graylevel, unsigned 64bpp image.}
     * {description}{The image is graylevel. Each pixel is unsigned and stored in 8 bytes.}
     **/
    PixelFormat_Grayscale64 = 10
  };


  /**
   * {summary}{The extraction mode specifies the way the values of the pixels are scaled when downloading a 2D image.}
   **/
  enum ImageExtractionMode
  {
    /**
     * {summary}{Rescaled to 8bpp.}
     * {description}{The minimum value of the image is set to 0, and its maximum value is set to 255.}
     **/
    ImageExtractionMode_Preview = 1,

    /**
     * {summary}{Truncation to the [0, 255] range.}
     **/
    ImageExtractionMode_UInt8 = 2,

    /**
     * {summary}{Truncation to the [0, 65535] range.}
     **/
    ImageExtractionMode_UInt16 = 3,

    /**
     * {summary}{Truncation to the [-32768, 32767] range.}
     **/
    ImageExtractionMode_Int16 = 4
  };


  /**
   * Most common, non-joke and non-experimental HTTP status codes
   * http://en.wikipedia.org/wiki/List_of_HTTP_status_codes
   **/
  enum HttpStatus
  {
    HttpStatus_None = -1,

    // 1xx Informational
    HttpStatus_100_Continue = 100,
    HttpStatus_101_SwitchingProtocols = 101,
    HttpStatus_102_Processing = 102,

    // 2xx Success
    HttpStatus_200_Ok = 200,
    HttpStatus_201_Created = 201,
    HttpStatus_202_Accepted = 202,
    HttpStatus_203_NonAuthoritativeInformation = 203,
    HttpStatus_204_NoContent = 204,
    HttpStatus_205_ResetContent = 205,
    HttpStatus_206_PartialContent = 206,
    HttpStatus_207_MultiStatus = 207,
    HttpStatus_208_AlreadyReported = 208,
    HttpStatus_226_IMUsed = 226,

    // 3xx Redirection
    HttpStatus_300_MultipleChoices = 300,
    HttpStatus_301_MovedPermanently = 301,
    HttpStatus_302_Found = 302,
    HttpStatus_303_SeeOther = 303,
    HttpStatus_304_NotModified = 304,
    HttpStatus_305_UseProxy = 305,
    HttpStatus_307_TemporaryRedirect = 307,

    // 4xx Client Error
    HttpStatus_400_BadRequest = 400,
    HttpStatus_401_Unauthorized = 401,
    HttpStatus_402_PaymentRequired = 402,
    HttpStatus_403_Forbidden = 403,
    HttpStatus_404_NotFound = 404,
    HttpStatus_405_MethodNotAllowed = 405,
    HttpStatus_406_NotAcceptable = 406,
    HttpStatus_407_ProxyAuthenticationRequired = 407,
    HttpStatus_408_RequestTimeout = 408,
    HttpStatus_409_Conflict = 409,
    HttpStatus_410_Gone = 410,
    HttpStatus_411_LengthRequired = 411,
    HttpStatus_412_PreconditionFailed = 412,
    HttpStatus_413_RequestEntityTooLarge = 413,
    HttpStatus_414_RequestUriTooLong = 414,
    HttpStatus_415_UnsupportedMediaType = 415,
    HttpStatus_416_RequestedRangeNotSatisfiable = 416,
    HttpStatus_417_ExpectationFailed = 417,
    HttpStatus_422_UnprocessableEntity = 422,
    HttpStatus_423_Locked = 423,
    HttpStatus_424_FailedDependency = 424,
    HttpStatus_426_UpgradeRequired = 426,

    // 5xx Server Error
    HttpStatus_500_InternalServerError = 500,
    HttpStatus_501_NotImplemented = 501,
    HttpStatus_502_BadGateway = 502,
    HttpStatus_503_ServiceUnavailable = 503,
    HttpStatus_504_GatewayTimeout = 504,
    HttpStatus_505_HttpVersionNotSupported = 505,
    HttpStatus_506_VariantAlsoNegotiates = 506,
    HttpStatus_507_InsufficientStorage = 507,
    HttpStatus_509_BandwidthLimitExceeded = 509,
    HttpStatus_510_NotExtended = 510
  };


  enum HttpMethod
  {
    HttpMethod_Get = 0,
    HttpMethod_Post = 1,
    HttpMethod_Delete = 2,
    HttpMethod_Put = 3
  };


  enum ImageFormat
  {
    ImageFormat_Png = 1
  };


  // https://en.wikipedia.org/wiki/HTTP_compression
  enum HttpCompression
  {
    HttpCompression_None,
    HttpCompression_Deflate,
    HttpCompression_Gzip
  };


  // Specific Character Sets
  // http://dicom.nema.org/medical/dicom/current/output/html/part03.html#sect_C.12.1.1.2
  enum Encoding
  {
    Encoding_Ascii,
    Encoding_Utf8,
    Encoding_Latin1,
    Encoding_Latin2,
    Encoding_Latin3,
    Encoding_Latin4,
    Encoding_Latin5,                        // Turkish
    Encoding_Cyrillic,
    Encoding_Windows1251,                   // Windows-1251 (commonly used for Cyrillic)
    Encoding_Arabic,
    Encoding_Greek,
    Encoding_Hebrew,
    Encoding_Thai,                          // TIS 620-2533
    Encoding_Japanese,                      // JIS X 0201 (Shift JIS): Katakana
    Encoding_Chinese,                       // GB18030 - Chinese simplified
    Encoding_JapaneseKanji,                 // Multibyte - JIS X 0208: Kanji
    //Encoding_JapaneseSupplementaryKanji,  // Multibyte - JIS X 0212: Supplementary Kanji set
    Encoding_Korean,                        // Multibyte - KS X 1001: Hangul and Hanja
    Encoding_SimplifiedChinese              // ISO 2022 IR 58
  };


  // http://dicom.nema.org/medical/dicom/current/output/html/part03.html#sect_C.7.6.3.1.2
  enum PhotometricInterpretation
  {
    PhotometricInterpretation_ARGB,  // Retired
    PhotometricInterpretation_CMYK,  // Retired
    PhotometricInterpretation_HSV,   // Retired
    PhotometricInterpretation_Monochrome1,
    PhotometricInterpretation_Monochrome2,
    PhotometricInterpretation_Palette,
    PhotometricInterpretation_RGB,
    PhotometricInterpretation_YBRFull,
    PhotometricInterpretation_YBRFull422,
    PhotometricInterpretation_YBRPartial420,
    PhotometricInterpretation_YBRPartial422,
    PhotometricInterpretation_YBR_ICT,
    PhotometricInterpretation_YBR_RCT,
    PhotometricInterpretation_Unknown
  };

  enum DicomModule
  {
    DicomModule_Patient,
    DicomModule_Study,
    DicomModule_Series,
    DicomModule_Instance,
    DicomModule_Image
  };

  enum RequestOrigin
  {
    RequestOrigin_Unknown,
    RequestOrigin_DicomProtocol,
    RequestOrigin_RestApi,
    RequestOrigin_Plugins,
    RequestOrigin_Lua,
    RequestOrigin_WebDav,   // New in Orthanc 1.8.0
    RequestOrigin_Documentation  // New in Orthanc in Orthanc 1.8.3 for API documentation (OpenAPI)
  };

  enum ServerBarrierEvent
  {
    ServerBarrierEvent_Stop,
    ServerBarrierEvent_Reload  // SIGHUP signal: reload configuration file
  };

  enum FileMode
  {
    FileMode_ReadBinary,
    FileMode_WriteBinary
  };

  /**
   * The value representations Orthanc knows about. They correspond to
   * the DICOM 2016b version of the standard.
   * http://dicom.nema.org/medical/dicom/current/output/chtml/part05/sect_6.2.html
   **/
  enum ValueRepresentation
  {
    ValueRepresentation_ApplicationEntity = 1,     // AE
    ValueRepresentation_AgeString = 2,             // AS
    ValueRepresentation_AttributeTag = 3,          // AT (2 x uint16_t)
    ValueRepresentation_CodeString = 4,            // CS
    ValueRepresentation_Date = 5,                  // DA
    ValueRepresentation_DecimalString = 6,         // DS
    ValueRepresentation_DateTime = 7,              // DT
    ValueRepresentation_FloatingPointSingle = 8,   // FL (float)
    ValueRepresentation_FloatingPointDouble = 9,   // FD (double)
    ValueRepresentation_IntegerString = 10,        // IS
    ValueRepresentation_LongString = 11,           // LO
    ValueRepresentation_LongText = 12,             // LT
    ValueRepresentation_OtherByte = 13,            // OB
    ValueRepresentation_OtherDouble = 14,          // OD
    ValueRepresentation_OtherFloat = 15,           // OF
    ValueRepresentation_OtherLong = 16,            // OL
    ValueRepresentation_OtherWord = 17,            // OW
    ValueRepresentation_PersonName = 18,           // PN
    ValueRepresentation_ShortString = 19,          // SH
    ValueRepresentation_SignedLong = 20,           // SL (int32_t)
    ValueRepresentation_Sequence = 21,             // SQ
    ValueRepresentation_SignedShort = 22,          // SS (int16_t)
    ValueRepresentation_ShortText = 23,            // ST
    ValueRepresentation_Time = 24,                 // TM
    ValueRepresentation_UnlimitedCharacters = 25,  // UC
    ValueRepresentation_UniqueIdentifier = 26,     // UI (UID)
    ValueRepresentation_UnsignedLong = 27,         // UL (uint32_t)
    ValueRepresentation_Unknown = 28,              // UN
    ValueRepresentation_UniversalResource = 29,    // UR (URI or URL)
    ValueRepresentation_UnsignedShort = 30,        // US (uint16_t)
    ValueRepresentation_UnlimitedText = 31,        // UT
    ValueRepresentation_NotSupported               // Not supported by Orthanc, or tag not in dictionary
  };

  enum DicomReplaceMode
  {
    DicomReplaceMode_InsertIfAbsent,
    DicomReplaceMode_ThrowIfAbsent,
    DicomReplaceMode_IgnoreIfAbsent
  };

  enum DicomToJsonFormat
  {
    DicomToJsonFormat_Full,
    DicomToJsonFormat_Short,
    DicomToJsonFormat_Human
  };

  enum DicomToJsonFlags
  {
    DicomToJsonFlags_IncludeBinary         = (1 << 0),
    DicomToJsonFlags_IncludePrivateTags    = (1 << 1),
    DicomToJsonFlags_IncludeUnknownTags    = (1 << 2),
    DicomToJsonFlags_IncludePixelData      = (1 << 3),
    DicomToJsonFlags_ConvertBinaryToAscii  = (1 << 4),
    DicomToJsonFlags_ConvertBinaryToNull   = (1 << 5),
    DicomToJsonFlags_StopAfterPixelData    = (1 << 6),  // New in Orthanc 1.9.1
    DicomToJsonFlags_SkipGroupLengths      = (1 << 7),  // New in Orthanc 1.9.1

    // Some predefined combinations
    DicomToJsonFlags_None     = 0,
    DicomToJsonFlags_Default  = (DicomToJsonFlags_IncludeBinary |
                                 DicomToJsonFlags_IncludePixelData | 
                                 DicomToJsonFlags_IncludePrivateTags | 
                                 DicomToJsonFlags_IncludeUnknownTags | 
                                 DicomToJsonFlags_ConvertBinaryToNull |
                                 DicomToJsonFlags_StopAfterPixelData /* added in 1.9.1 */)
  };
  
  enum DicomFromJsonFlags
  {
    DicomFromJsonFlags_DecodeDataUriScheme = (1 << 0),
    DicomFromJsonFlags_GenerateIdentifiers = (1 << 1),

    // Some predefined combinations
    DicomFromJsonFlags_None = 0
  };

  // If adding a new DICOM version below, update the
  // "DeidentifyLogsDicomVersion" configuration option
  enum DicomVersion
  {
    DicomVersion_2008,
    DicomVersion_2017c
  };

  enum ModalityManufacturer
  {
    ModalityManufacturer_Generic,
    ModalityManufacturer_GenericNoWildcardInDates,
    ModalityManufacturer_GenericNoUniversalWildcard,
    ModalityManufacturer_Vitrea,
    ModalityManufacturer_GE
  };

  enum DicomRequestType
  {
    DicomRequestType_Echo,
    DicomRequestType_Find,
    DicomRequestType_Get,
    DicomRequestType_Move,
    DicomRequestType_Store,
    DicomRequestType_NAction,
    DicomRequestType_NEventReport
  };

  enum JobState
  {
    JobState_Pending,
    JobState_Running,
    JobState_Success,
    JobState_Failure,
    JobState_Paused,
    JobState_Retry
  };

  enum JobStepCode
  {
    JobStepCode_Success,
    JobStepCode_Failure,
    JobStepCode_Continue,
    JobStepCode_Retry
  };

  enum JobStopReason
  {
    JobStopReason_Paused,
    JobStopReason_Canceled,
    JobStopReason_Success,
    JobStopReason_Failure,
    JobStopReason_Retry
  };

  
  // http://dicom.nema.org/medical/dicom/current/output/chtml/part03/sect_C.14.html#sect_C.14.1.1
  enum StorageCommitmentFailureReason
  {
    StorageCommitmentFailureReason_Success = 0,

    // A general failure in processing the operation was encountered
    StorageCommitmentFailureReason_ProcessingFailure = 0x0110,

    // One or more of the elements in the Referenced SOP Instance
    // Sequence was not available
    StorageCommitmentFailureReason_NoSuchObjectInstance = 0x0112,

    // The SCP does not currently have enough resources to store the
    // requested SOP Instance(s)
    StorageCommitmentFailureReason_ResourceLimitation = 0x0213,

    // Storage Commitment has been requested for a SOP Instance with a
    // SOP Class that is not supported by the SCP
    StorageCommitmentFailureReason_ReferencedSOPClassNotSupported = 0x0122,

    // The SOP Class of an element in the Referenced SOP Instance
    // Sequence did not correspond to the SOP class registered for
    // this SOP Instance at the SCP
    StorageCommitmentFailureReason_ClassInstanceConflict = 0x0119,

    // The Transaction UID of the Storage Commitment Request is already in use
    StorageCommitmentFailureReason_DuplicateTransactionUID = 0x0131
  };


  enum DicomAssociationRole
  {
    DicomAssociationRole_Default,
    DicomAssociationRole_Scu,
    DicomAssociationRole_Scp
  };


  /**
   * WARNING: Do not change the explicit values in the enumerations
   * below this point. This would result in incompatible databases
   * between versions of Orthanc!
   **/

  enum CompressionType
  {
    /**
     * Buffer/file that is stored as-is, in a raw fashion, without
     * compression.
     **/
    CompressionType_None = 1,

    /**
     * Buffer that is compressed using the "deflate" algorithm (RFC
     * 1951), wrapped inside the zlib data format (RFC 1950), prefixed
     * with a "uint64_t" (8 bytes) that encodes the size of the
     * uncompressed buffer. If the compressed buffer is empty, its
     * represents an empty uncompressed buffer. This format is
     * internal to Orthanc. If the 8 first bytes are skipped AND the
     * buffer is non-empty, the buffer is compatible with the
     * "deflate" HTTP compression.
     **/
    CompressionType_ZlibWithSize = 2
  };

  enum FileContentType
  {
    // If you add a value below, insert it in "PluginStorageArea" in
    // the file "Plugins/Engine/OrthancPlugins.cpp"
    FileContentType_Unknown = 0,
    FileContentType_Dicom = 1,
    FileContentType_DicomAsJson = 2,          // For Orthanc <= 1.9.0
    FileContentType_DicomUntilPixelData = 3,  // New in Orthanc 1.9.1

    // Make sure that the value "65535" can be stored into this enumeration
    FileContentType_StartUser = 1024,
    FileContentType_EndUser = 65535
  };

  enum ResourceType
  {
    ResourceType_Patient = 1,
    ResourceType_Study = 2,
    ResourceType_Series = 3,
    ResourceType_Instance = 4
  };


  ORTHANC_PUBLIC
  const char* EnumerationToString(ErrorCode code);

  ORTHANC_PUBLIC
  const char* EnumerationToString(HttpMethod method);

  ORTHANC_PUBLIC
  const char* EnumerationToString(HttpStatus status);

  ORTHANC_PUBLIC
  const char* EnumerationToString(ResourceType type);

  ORTHANC_PUBLIC
  const char* EnumerationToString(ImageFormat format);

  ORTHANC_PUBLIC
  const char* EnumerationToString(Encoding encoding);

  ORTHANC_PUBLIC
  const char* EnumerationToString(PhotometricInterpretation photometric);

  ORTHANC_PUBLIC
  const char* EnumerationToString(RequestOrigin origin);

  ORTHANC_PUBLIC
  const char* EnumerationToString(PixelFormat format);

  ORTHANC_PUBLIC
  const char* EnumerationToString(ModalityManufacturer manufacturer);

  ORTHANC_PUBLIC
  const char* EnumerationToString(DicomRequestType type);

  ORTHANC_PUBLIC
  const char* EnumerationToString(DicomVersion version);

  ORTHANC_PUBLIC
  const char* EnumerationToString(ValueRepresentation vr);

  ORTHANC_PUBLIC
  const char* EnumerationToString(JobState state);

  ORTHANC_PUBLIC
  const char* EnumerationToString(MimeType mime);

  ORTHANC_PUBLIC
  const char* EnumerationToString(Endianness endianness);

  ORTHANC_PUBLIC
  const char* EnumerationToString(StorageCommitmentFailureReason reason);

  ORTHANC_PUBLIC
  Encoding StringToEncoding(const char* encoding);

  ORTHANC_PUBLIC
  ResourceType StringToResourceType(const char* type);

  ORTHANC_PUBLIC
  ImageFormat StringToImageFormat(const char* format);

  ORTHANC_PUBLIC
  ValueRepresentation StringToValueRepresentation(const std::string& vr,
                                                  bool throwIfUnsupported);

  ORTHANC_PUBLIC
  PhotometricInterpretation StringToPhotometricInterpretation(const char* value);

  ORTHANC_PUBLIC
  ModalityManufacturer StringToModalityManufacturer(const std::string& manufacturer);

  ORTHANC_PUBLIC
  DicomVersion StringToDicomVersion(const std::string& version);

  ORTHANC_PUBLIC
  JobState StringToJobState(const std::string& state);
  
  ORTHANC_PUBLIC
  RequestOrigin StringToRequestOrigin(const std::string& origin);

  ORTHANC_PUBLIC
  MimeType StringToMimeType(const std::string& mime);
  
  ORTHANC_PUBLIC
  bool LookupMimeType(MimeType& target,
                      const std::string& source);
  
  ORTHANC_PUBLIC
  unsigned int GetBytesPerPixel(PixelFormat format);

  ORTHANC_PUBLIC
  bool GetDicomEncoding(Encoding& encoding,
                        const char* specificCharacterSet);

  ORTHANC_PUBLIC
  ResourceType GetChildResourceType(ResourceType type);

  ORTHANC_PUBLIC
  ResourceType GetParentResourceType(ResourceType type);

  ORTHANC_PUBLIC
  bool IsResourceLevelAboveOrEqual(ResourceType level,
                                   ResourceType reference);

  ORTHANC_PUBLIC
  DicomModule GetModule(ResourceType type);

  ORTHANC_PUBLIC
  const char* GetDicomSpecificCharacterSet(Encoding encoding);

  ORTHANC_PUBLIC
  HttpStatus ConvertErrorCodeToHttpStatus(ErrorCode error);

  ORTHANC_PUBLIC
  bool IsUserContentType(FileContentType type);

  ORTHANC_PUBLIC
  bool IsBinaryValueRepresentation(ValueRepresentation vr);
  
  ORTHANC_PUBLIC
  Encoding GetDefaultDicomEncoding();

  ORTHANC_PUBLIC
  void SetDefaultDicomEncoding(Encoding encoding);

  ORTHANC_PUBLIC
  const char* GetTransferSyntaxUid(DicomTransferSyntax syntax);

  ORTHANC_PUBLIC
  bool IsRetiredTransferSyntax(DicomTransferSyntax syntax);

  ORTHANC_PUBLIC
  bool LookupTransferSyntax(DicomTransferSyntax& target,
                            const std::string& uid);

  ORTHANC_PUBLIC
  const char* GetResourceTypeText(ResourceType type,
                                  bool isPlural,
                                  bool isLowerCase);

  ORTHANC_PUBLIC
  void GetAllDicomTransferSyntaxes(std::set<DicomTransferSyntax>& target);
}
