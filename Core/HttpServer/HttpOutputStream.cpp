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


#include "HttpOutputStream.h"

#include "../OrthancException.h"

#include <boost/lexical_cast.hpp>

namespace Orthanc
{
  void HttpOutputStream::SendHttpStatus(HttpStatus status)
  {
    if (state_ != State_WaitingHttpStatus)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    OnHttpStatusReceived(status);
    state_ = State_WritingHeader;

    std::string s = "HTTP/1.1 " + 
      boost::lexical_cast<std::string>(status) +
      " " + std::string(EnumerationToString(status)) +
      "\r\n";

    SendHeader(&s[0], s.size());
  }

  void HttpOutputStream::SendHeaderData(const void* buffer, size_t length)
  {
    if (state_ != State_WritingHeader)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    SendHeader(buffer, length);
  }

  void HttpOutputStream::SendHeaderString(const std::string& str)
  {
    if (str.size() > 0)
    {
      SendHeaderData(&str[0], str.size());
    }
  }

  void HttpOutputStream::SendBodyData(const void* buffer, size_t length)
  {
    if (state_ == State_WaitingHttpStatus)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    if (state_ == State_WritingHeader)
    {
      // Close the HTTP header before writing the body
      SendHeader("\r\n", 2);
      state_ = State_WritingBody;
    }

    if (length > 0)
    {
      SendBody(buffer, length);
    }
  }

  void HttpOutputStream::SendBodyString(const std::string& str)
  {
    if (str.size() > 0)
    {
      SendBodyData(&str[0], str.size());
    }
  }
}
