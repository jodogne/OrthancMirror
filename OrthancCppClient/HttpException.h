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

#include "HttpEnumerations.h"

#include <string>

namespace Palanthir
{
  class HttpException
  {
  private:
    Palanthir_HttpStatus status_;
    std::string custom_;

  public:
    static const char* GetDescription(Palanthir_HttpStatus status);

    HttpException(const std::string& custom)
    {
      status_ = Palanthir_HttpStatus_None;
      custom_ = custom;
    }

    HttpException(Palanthir_HttpStatus status)
    {
      status_ = status;
    }

    Palanthir_HttpStatus GetHttpStatus() const
    {
      return status_;
    }

    const char* What() const;
  };
}
