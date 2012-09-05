/**
 * Palanthir - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012 Medical Physics Department, CHU of Liege,
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


#pragma once


/**
 * This file contains the enumerations for the access to the Palanthir
 * REST API in C and C++. Namespaces are not used, in order to enable
 * the access in C.
 **/

// Most common, non-joke and non-experimental HTTP status codes
// http://en.wikipedia.org/wiki/List_of_HTTP_status_codes
enum Palanthir_HttpStatus
{
  Palanthir_HttpStatus_None = -1,

  // 1xx Informational
  Palanthir_HttpStatus_100_Continue = 100,
  Palanthir_HttpStatus_101_SwitchingProtocols = 101,
  Palanthir_HttpStatus_102_Processing = 102,

  // 2xx Success
  Palanthir_HttpStatus_200_Ok = 200,
  Palanthir_HttpStatus_201_Created = 201,
  Palanthir_HttpStatus_202_Accepted = 202,
  Palanthir_HttpStatus_203_NonAuthoritativeInformation = 203,
  Palanthir_HttpStatus_204_NoContent = 204,
  Palanthir_HttpStatus_205_ResetContent = 205,
  Palanthir_HttpStatus_206_PartialContent = 206,
  Palanthir_HttpStatus_207_MultiStatus = 207,
  Palanthir_HttpStatus_208_AlreadyReported = 208,
  Palanthir_HttpStatus_226_IMUsed = 226,

  // 3xx Redirection
  Palanthir_HttpStatus_300_MultipleChoices = 300,
  Palanthir_HttpStatus_301_MovedPermanently = 301,
  Palanthir_HttpStatus_302_Found = 302,
  Palanthir_HttpStatus_303_SeeOther = 303,
  Palanthir_HttpStatus_304_NotModified = 304,
  Palanthir_HttpStatus_305_UseProxy = 305,
  Palanthir_HttpStatus_307_TemporaryRedirect = 307,

  // 4xx Client Error
  Palanthir_HttpStatus_400_BadRequest = 400,
  Palanthir_HttpStatus_401_Unauthorized = 401,
  Palanthir_HttpStatus_402_PaymentRequired = 402,
  Palanthir_HttpStatus_403_Forbidden = 403,
  Palanthir_HttpStatus_404_NotFound = 404,
  Palanthir_HttpStatus_405_MethodNotAllowed = 405,
  Palanthir_HttpStatus_406_NotAcceptable = 406,
  Palanthir_HttpStatus_407_ProxyAuthenticationRequired = 407,
  Palanthir_HttpStatus_408_RequestTimeout = 408,
  Palanthir_HttpStatus_409_Conflict = 409,
  Palanthir_HttpStatus_410_Gone = 410,
  Palanthir_HttpStatus_411_LengthRequired = 411,
  Palanthir_HttpStatus_412_PreconditionFailed = 412,
  Palanthir_HttpStatus_413_RequestEntityTooLarge = 413,
  Palanthir_HttpStatus_414_RequestUriTooLong = 414,
  Palanthir_HttpStatus_415_UnsupportedMediaType = 415,
  Palanthir_HttpStatus_416_RequestedRangeNotSatisfiable = 416,
  Palanthir_HttpStatus_417_ExpectationFailed = 417,
  Palanthir_HttpStatus_422_UnprocessableEntity = 422,
  Palanthir_HttpStatus_423_Locked = 423,
  Palanthir_HttpStatus_424_FailedDependency = 424,
  Palanthir_HttpStatus_426_UpgradeRequired = 426,

  // 5xx Server Error
  Palanthir_HttpStatus_500_InternalServerError = 500,
  Palanthir_HttpStatus_501_NotImplemented = 501,
  Palanthir_HttpStatus_502_BadGateway = 502,
  Palanthir_HttpStatus_503_ServiceUnavailable = 503,
  Palanthir_HttpStatus_504_GatewayTimeout = 504,
  Palanthir_HttpStatus_505_HttpVersionNotSupported = 505,
  Palanthir_HttpStatus_506_VariantAlsoNegotiates = 506,
  Palanthir_HttpStatus_507_InsufficientStorage = 507,
  Palanthir_HttpStatus_509_BandwidthLimitExceeded = 509,
  Palanthir_HttpStatus_510_NotExtended = 510
};


enum Palanthir_HttpMethod
{
  Palanthir_HttpMethod_Get = 0,
  Palanthir_HttpMethod_Post = 1,
  Palanthir_HttpMethod_Delete = 2,
  Palanthir_HttpMethod_Put = 3
};
