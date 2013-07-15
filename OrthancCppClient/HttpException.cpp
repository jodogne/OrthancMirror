/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2013 Medical Physics Department, CHU of Liege,
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


#include "HttpException.h"

namespace Orthanc
{
  const char* HttpException::What() const
  {
    if (status_ == HttpStatus_None)
    {
      return custom_.c_str();
    }
    else
    {
      return GetDescription(status_);
    }
  }

  const char* HttpException::GetDescription(HttpStatus status)
  {
    switch (status)
    {
    case HttpStatus_100_Continue:
      return "Continue";

    case HttpStatus_101_SwitchingProtocols:
      return "Switching Protocols";

    case HttpStatus_102_Processing:
      return "Processing";

    case HttpStatus_200_Ok:
      return "OK";

    case HttpStatus_201_Created:
      return "Created";

    case HttpStatus_202_Accepted:
      return "Accepted";

    case HttpStatus_203_NonAuthoritativeInformation:
      return "Non-Authoritative Information";

    case HttpStatus_204_NoContent:
      return "No Content";

    case HttpStatus_205_ResetContent:
      return "Reset Content";

    case HttpStatus_206_PartialContent:
      return "Partial Content";

    case HttpStatus_207_MultiStatus:
      return "Multi-Status";

    case HttpStatus_208_AlreadyReported:
      return "Already Reported";

    case HttpStatus_226_IMUsed:
      return "IM Used";

    case HttpStatus_300_MultipleChoices:
      return "Multiple Choices";

    case HttpStatus_301_MovedPermanently:
      return "Moved Permanently";

    case HttpStatus_302_Found:
      return "Found";

    case HttpStatus_303_SeeOther:
      return "See Other";

    case HttpStatus_304_NotModified:
      return "Not Modified";

    case HttpStatus_305_UseProxy:
      return "Use Proxy";

    case HttpStatus_307_TemporaryRedirect:
      return "Temporary Redirect";

    case HttpStatus_400_BadRequest:
      return "Bad Request";

    case HttpStatus_401_Unauthorized:
      return "Unauthorized";

    case HttpStatus_402_PaymentRequired:
      return "Payment Required";

    case HttpStatus_403_Forbidden:
      return "Forbidden";

    case HttpStatus_404_NotFound:
      return "Not Found";

    case HttpStatus_405_MethodNotAllowed:
      return "Method Not Allowed";

    case HttpStatus_406_NotAcceptable:
      return "Not Acceptable";

    case HttpStatus_407_ProxyAuthenticationRequired:
      return "Proxy Authentication Required";

    case HttpStatus_408_RequestTimeout:
      return "Request Timeout";

    case HttpStatus_409_Conflict:
      return "Conflict";

    case HttpStatus_410_Gone:
      return "Gone";

    case HttpStatus_411_LengthRequired:
      return "Length Required";

    case HttpStatus_412_PreconditionFailed:
      return "Precondition Failed";

    case HttpStatus_413_RequestEntityTooLarge:
      return "Request Entity Too Large";

    case HttpStatus_414_RequestUriTooLong:
      return "Request-URI Too Long";

    case HttpStatus_415_UnsupportedMediaType:
      return "Unsupported Media Type";

    case HttpStatus_416_RequestedRangeNotSatisfiable:
      return "Requested Range Not Satisfiable";

    case HttpStatus_417_ExpectationFailed:
      return "Expectation Failed";

    case HttpStatus_422_UnprocessableEntity:
      return "Unprocessable Entity";

    case HttpStatus_423_Locked:
      return "Locked";

    case HttpStatus_424_FailedDependency:
      return "Failed Dependency";

    case HttpStatus_426_UpgradeRequired:
      return "Upgrade Required";

    case HttpStatus_500_InternalServerError:
      return "Internal Server Error";

    case HttpStatus_501_NotImplemented:
      return "Not Implemented";

    case HttpStatus_502_BadGateway:
      return "Bad Gateway";

    case HttpStatus_503_ServiceUnavailable:
      return "Service Unavailable";

    case HttpStatus_504_GatewayTimeout:
      return "Gateway Timeout";

    case HttpStatus_505_HttpVersionNotSupported:
      return "HTTP Version Not Supported";

    case HttpStatus_506_VariantAlsoNegotiates:
      return "Variant Also Negotiates";

    case HttpStatus_507_InsufficientStorage:
      return "Insufficient Storage";

    case HttpStatus_509_BandwidthLimitExceeded:
      return "Bandwidth Limit Exceeded";

    case HttpStatus_510_NotExtended:
      return "Not Extended";

    default:
      throw HttpException("Unknown HTTP status");
    }
  }
}
