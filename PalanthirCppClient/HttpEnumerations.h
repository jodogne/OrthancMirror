/**
 * Palantir - A Lightweight, RESTful DICOM Store
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
 * This file contains the enumerations for the access to the Palantir
 * REST API in C and C++. Namespaces are not used, in order to enable
 * the access in C.
 **/

// Most common, non-joke and non-experimental HTTP status codes
// http://en.wikipedia.org/wiki/List_of_HTTP_status_codes
enum Palantir_HttpStatus
{
  Palantir_HttpStatus_None = -1,

  // 1xx Informational
  Palantir_HttpStatus_100_Continue = 100,
  Palantir_HttpStatus_101_SwitchingProtocols = 101,
  Palantir_HttpStatus_102_Processing = 102,

  // 2xx Success
  Palantir_HttpStatus_200_Ok = 200,
  Palantir_HttpStatus_201_Created = 201,
  Palantir_HttpStatus_202_Accepted = 202,
  Palantir_HttpStatus_203_NonAuthoritativeInformation = 203,
  Palantir_HttpStatus_204_NoContent = 204,
  Palantir_HttpStatus_205_ResetContent = 205,
  Palantir_HttpStatus_206_PartialContent = 206,
  Palantir_HttpStatus_207_MultiStatus = 207,
  Palantir_HttpStatus_208_AlreadyReported = 208,
  Palantir_HttpStatus_226_IMUsed = 226,

  // 3xx Redirection
  Palantir_HttpStatus_300_MultipleChoices = 300,
  Palantir_HttpStatus_301_MovedPermanently = 301,
  Palantir_HttpStatus_302_Found = 302,
  Palantir_HttpStatus_303_SeeOther = 303,
  Palantir_HttpStatus_304_NotModified = 304,
  Palantir_HttpStatus_305_UseProxy = 305,
  Palantir_HttpStatus_307_TemporaryRedirect = 307,

  // 4xx Client Error
  Palantir_HttpStatus_400_BadRequest = 400,
  Palantir_HttpStatus_401_Unauthorized = 401,
  Palantir_HttpStatus_402_PaymentRequired = 402,
  Palantir_HttpStatus_403_Forbidden = 403,
  Palantir_HttpStatus_404_NotFound = 404,
  Palantir_HttpStatus_405_MethodNotAllowed = 405,
  Palantir_HttpStatus_406_NotAcceptable = 406,
  Palantir_HttpStatus_407_ProxyAuthenticationRequired = 407,
  Palantir_HttpStatus_408_RequestTimeout = 408,
  Palantir_HttpStatus_409_Conflict = 409,
  Palantir_HttpStatus_410_Gone = 410,
  Palantir_HttpStatus_411_LengthRequired = 411,
  Palantir_HttpStatus_412_PreconditionFailed = 412,
  Palantir_HttpStatus_413_RequestEntityTooLarge = 413,
  Palantir_HttpStatus_414_RequestUriTooLong = 414,
  Palantir_HttpStatus_415_UnsupportedMediaType = 415,
  Palantir_HttpStatus_416_RequestedRangeNotSatisfiable = 416,
  Palantir_HttpStatus_417_ExpectationFailed = 417,
  Palantir_HttpStatus_422_UnprocessableEntity = 422,
  Palantir_HttpStatus_423_Locked = 423,
  Palantir_HttpStatus_424_FailedDependency = 424,
  Palantir_HttpStatus_426_UpgradeRequired = 426,

  // 5xx Server Error
  Palantir_HttpStatus_500_InternalServerError = 500,
  Palantir_HttpStatus_501_NotImplemented = 501,
  Palantir_HttpStatus_502_BadGateway = 502,
  Palantir_HttpStatus_503_ServiceUnavailable = 503,
  Palantir_HttpStatus_504_GatewayTimeout = 504,
  Palantir_HttpStatus_505_HttpVersionNotSupported = 505,
  Palantir_HttpStatus_506_VariantAlsoNegotiates = 506,
  Palantir_HttpStatus_507_InsufficientStorage = 507,
  Palantir_HttpStatus_509_BandwidthLimitExceeded = 509,
  Palantir_HttpStatus_510_NotExtended = 510
};


enum Palantir_HttpMethod
{
  Palantir_HttpMethod_Get = 0,
  Palantir_HttpMethod_Post = 1,
  Palantir_HttpMethod_Delete = 2,
  Palantir_HttpMethod_Put = 3
};
