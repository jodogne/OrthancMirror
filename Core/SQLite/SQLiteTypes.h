/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 *
 * Copyright (C) 2012-2016 Sebastien Jodogne <s.jodogne@gmail.com>,
 * Medical Physics Department, CHU of Liege, Belgium
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *    * Neither the name of the CHU of Liege, nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **/


#pragma once

struct sqlite3;
struct sqlite3_context;
struct sqlite3_stmt;

#if !defined(ORTHANC_SQLITE_VERSION)
#error  Please define macro ORTHANC_SQLITE_VERSION
#endif


/**
 * "sqlite3_value" is defined as:
 * - "typedef struct Mem sqlite3_value;" up to SQLite <= 3.18.2
 * - "typedef struct sqlite3_value sqlite3_value;" since SQLite >= 3.19.0.
 * We create our own copy of this typedef to get around this API incompatibility.
 * https://github.com/mackyle/sqlite/commit/db1d90df06a78264775a14d22c3361eb5b42be17
 **/
      
#if ORTHANC_SQLITE_VERSION < 3019000
struct Mem;
#else
struct sqlite3_value;
#endif

namespace Orthanc
{
  namespace SQLite
  {
    namespace Internals
    {
#if ORTHANC_SQLITE_VERSION < 3019000
      typedef struct ::Mem  SQLiteValue;
#else
      typedef struct ::sqlite3_value  SQLiteValue;
#endif
    }
  }
}
