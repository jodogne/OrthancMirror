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

namespace Palantir
{
  // Most common, non-joke and non-experimental HTTP status codes
  // http://en.wikipedia.org/wiki/List_of_HTTP_status_codes
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
    HttpMethod_Get,
    HttpMethod_Post,
    HttpMethod_Delete,
    HttpMethod_Put
  };
}
