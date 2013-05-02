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
    if (status_ == Orthanc_HttpStatus_None)
    {
      return custom_.c_str();
    }
    else
    {
      return GetDescription(status_);
    }
  }

  const char* HttpException::GetDescription(Orthanc_HttpStatus status)
  {
    switch (status)
    {
    case Orthanc_HttpStatus_100_Continue:
      return "Continue";

    case Orthanc_HttpStatus_101_SwitchingProtocols:
      return "Switching Protocols";

    case Orthanc_HttpStatus_102_Processing:
      return "Processing";

    case Orthanc_HttpStatus_200_Ok:
      return "OK";

    case Orthanc_HttpStatus_201_Created:
      return "Created";

    case Orthanc_HttpStatus_202_Accepted:
      return "Accepted";

    case Orthanc_HttpStatus_203_NonAuthoritativeInformation:
      return "Non-Authoritative Information";

    case Orthanc_HttpStatus_204_NoContent:
      return "No Content";

    case Orthanc_HttpStatus_205_ResetContent:
      return "Reset Content";

    case Orthanc_HttpStatus_206_PartialContent:
      return "Partial Content";

    case Orthanc_HttpStatus_207_MultiStatus:
      return "Multi-Status";

    case Orthanc_HttpStatus_208_AlreadyReported:
      return "Already Reported";

    case Orthanc_HttpStatus_226_IMUsed:
      return "IM Used";

    case Orthanc_HttpStatus_300_MultipleChoices:
      return "Multiple Choices";

    case Orthanc_HttpStatus_301_MovedPermanently:
      return "Moved Permanently";

    case Orthanc_HttpStatus_302_Found:
      return "Found";

    case Orthanc_HttpStatus_303_SeeOther:
      return "See Other";

    case Orthanc_HttpStatus_304_NotModified:
      return "Not Modified";

    case Orthanc_HttpStatus_305_UseProxy:
      return "Use Proxy";

    case Orthanc_HttpStatus_307_TemporaryRedirect:
      return "Temporary Redirect";

    case Orthanc_HttpStatus_400_BadRequest:
      return "Bad Request";

    case Orthanc_HttpStatus_401_Unauthorized:
      return "Unauthorized";

    case Orthanc_HttpStatus_402_PaymentRequired:
      return "Payment Required";

    case Orthanc_HttpStatus_403_Forbidden:
      return "Forbidden";

    case Orthanc_HttpStatus_404_NotFound:
      return "Not Found";

    case Orthanc_HttpStatus_405_MethodNotAllowed:
      return "Method Not Allowed";

    case Orthanc_HttpStatus_406_NotAcceptable:
      return "Not Acceptable";

    case Orthanc_HttpStatus_407_ProxyAuthenticationRequired:
      return "Proxy Authentication Required";

    case Orthanc_HttpStatus_408_RequestTimeout:
      return "Request Timeout";

    case Orthanc_HttpStatus_409_Conflict:
      return "Conflict";

    case Orthanc_HttpStatus_410_Gone:
      return "Gone";

    case Orthanc_HttpStatus_411_LengthRequired:
      return "Length Required";

    case Orthanc_HttpStatus_412_PreconditionFailed:
      return "Precondition Failed";

    case Orthanc_HttpStatus_413_RequestEntityTooLarge:
      return "Request Entity Too Large";

    case Orthanc_HttpStatus_414_RequestUriTooLong:
      return "Request-URI Too Long";

    case Orthanc_HttpStatus_415_UnsupportedMediaType:
      return "Unsupported Media Type";

    case Orthanc_HttpStatus_416_RequestedRangeNotSatisfiable:
      return "Requested Range Not Satisfiable";

    case Orthanc_HttpStatus_417_ExpectationFailed:
      return "Expectation Failed";

    case Orthanc_HttpStatus_422_UnprocessableEntity:
      return "Unprocessable Entity";

    case Orthanc_HttpStatus_423_Locked:
      return "Locked";

    case Orthanc_HttpStatus_424_FailedDependency:
      return "Failed Dependency";

    case Orthanc_HttpStatus_426_UpgradeRequired:
      return "Upgrade Required";

    case Orthanc_HttpStatus_500_InternalServerError:
      return "Internal Server Error";

    case Orthanc_HttpStatus_501_NotImplemented:
      return "Not Implemented";

    case Orthanc_HttpStatus_502_BadGateway:
      return "Bad Gateway";

    case Orthanc_HttpStatus_503_ServiceUnavailable:
      return "Service Unavailable";

    case Orthanc_HttpStatus_504_GatewayTimeout:
      return "Gateway Timeout";

    case Orthanc_HttpStatus_505_HttpVersionNotSupported:
      return "HTTP Version Not Supported";

    case Orthanc_HttpStatus_506_VariantAlsoNegotiates:
      return "Variant Also Negotiates";

    case Orthanc_HttpStatus_507_InsufficientStorage:
      return "Insufficient Storage";

    case Orthanc_HttpStatus_509_BandwidthLimitExceeded:
      return "Bandwidth Limit Exceeded";

    case Orthanc_HttpStatus_510_NotExtended:
      return "Not Extended";

    default:
      throw HttpException("Unknown HTTP status");
    }
  }
}
