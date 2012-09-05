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


#include "HttpException.h"

namespace Palanthir
{
  const char* HttpException::What() const
  {
    if (status_ == Palanthir_HttpStatus_None)
    {
      return custom_.c_str();
    }
    else
    {
      return GetDescription(status_);
    }
  }

  const char* HttpException::GetDescription(Palanthir_HttpStatus status)
  {
    switch (status)
    {
    case Palanthir_HttpStatus_100_Continue:
      return "Continue";

    case Palanthir_HttpStatus_101_SwitchingProtocols:
      return "Switching Protocols";

    case Palanthir_HttpStatus_102_Processing:
      return "Processing";

    case Palanthir_HttpStatus_200_Ok:
      return "OK";

    case Palanthir_HttpStatus_201_Created:
      return "Created";

    case Palanthir_HttpStatus_202_Accepted:
      return "Accepted";

    case Palanthir_HttpStatus_203_NonAuthoritativeInformation:
      return "Non-Authoritative Information";

    case Palanthir_HttpStatus_204_NoContent:
      return "No Content";

    case Palanthir_HttpStatus_205_ResetContent:
      return "Reset Content";

    case Palanthir_HttpStatus_206_PartialContent:
      return "Partial Content";

    case Palanthir_HttpStatus_207_MultiStatus:
      return "Multi-Status";

    case Palanthir_HttpStatus_208_AlreadyReported:
      return "Already Reported";

    case Palanthir_HttpStatus_226_IMUsed:
      return "IM Used";

    case Palanthir_HttpStatus_300_MultipleChoices:
      return "Multiple Choices";

    case Palanthir_HttpStatus_301_MovedPermanently:
      return "Moved Permanently";

    case Palanthir_HttpStatus_302_Found:
      return "Found";

    case Palanthir_HttpStatus_303_SeeOther:
      return "See Other";

    case Palanthir_HttpStatus_304_NotModified:
      return "Not Modified";

    case Palanthir_HttpStatus_305_UseProxy:
      return "Use Proxy";

    case Palanthir_HttpStatus_307_TemporaryRedirect:
      return "Temporary Redirect";

    case Palanthir_HttpStatus_400_BadRequest:
      return "Bad Request";

    case Palanthir_HttpStatus_401_Unauthorized:
      return "Unauthorized";

    case Palanthir_HttpStatus_402_PaymentRequired:
      return "Payment Required";

    case Palanthir_HttpStatus_403_Forbidden:
      return "Forbidden";

    case Palanthir_HttpStatus_404_NotFound:
      return "Not Found";

    case Palanthir_HttpStatus_405_MethodNotAllowed:
      return "Method Not Allowed";

    case Palanthir_HttpStatus_406_NotAcceptable:
      return "Not Acceptable";

    case Palanthir_HttpStatus_407_ProxyAuthenticationRequired:
      return "Proxy Authentication Required";

    case Palanthir_HttpStatus_408_RequestTimeout:
      return "Request Timeout";

    case Palanthir_HttpStatus_409_Conflict:
      return "Conflict";

    case Palanthir_HttpStatus_410_Gone:
      return "Gone";

    case Palanthir_HttpStatus_411_LengthRequired:
      return "Length Required";

    case Palanthir_HttpStatus_412_PreconditionFailed:
      return "Precondition Failed";

    case Palanthir_HttpStatus_413_RequestEntityTooLarge:
      return "Request Entity Too Large";

    case Palanthir_HttpStatus_414_RequestUriTooLong:
      return "Request-URI Too Long";

    case Palanthir_HttpStatus_415_UnsupportedMediaType:
      return "Unsupported Media Type";

    case Palanthir_HttpStatus_416_RequestedRangeNotSatisfiable:
      return "Requested Range Not Satisfiable";

    case Palanthir_HttpStatus_417_ExpectationFailed:
      return "Expectation Failed";

    case Palanthir_HttpStatus_422_UnprocessableEntity:
      return "Unprocessable Entity";

    case Palanthir_HttpStatus_423_Locked:
      return "Locked";

    case Palanthir_HttpStatus_424_FailedDependency:
      return "Failed Dependency";

    case Palanthir_HttpStatus_426_UpgradeRequired:
      return "Upgrade Required";

    case Palanthir_HttpStatus_500_InternalServerError:
      return "Internal Server Error";

    case Palanthir_HttpStatus_501_NotImplemented:
      return "Not Implemented";

    case Palanthir_HttpStatus_502_BadGateway:
      return "Bad Gateway";

    case Palanthir_HttpStatus_503_ServiceUnavailable:
      return "Service Unavailable";

    case Palanthir_HttpStatus_504_GatewayTimeout:
      return "Gateway Timeout";

    case Palanthir_HttpStatus_505_HttpVersionNotSupported:
      return "HTTP Version Not Supported";

    case Palanthir_HttpStatus_506_VariantAlsoNegotiates:
      return "Variant Also Negotiates";

    case Palanthir_HttpStatus_507_InsufficientStorage:
      return "Insufficient Storage";

    case Palanthir_HttpStatus_509_BandwidthLimitExceeded:
      return "Bandwidth Limit Exceeded";

    case Palanthir_HttpStatus_510_NotExtended:
      return "Not Extended";

    default:
      throw HttpException("Unknown HTTP status");
    }
  }
}
