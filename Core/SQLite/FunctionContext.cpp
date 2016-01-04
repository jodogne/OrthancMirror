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


#if ORTHANC_SQLITE_STANDALONE != 1
#include "../PrecompiledHeaders.h"
#endif

#include "FunctionContext.h"
#include "OrthancSQLiteException.h"

#include <string>

#include "sqlite3.h"

namespace Orthanc
{
  namespace SQLite
  {
    FunctionContext::FunctionContext(struct sqlite3_context* context,
                                     int argc,
                                     struct ::Mem** argv)
    {
      assert(context != NULL);
      assert(argc >= 0);
      assert(argv != NULL);

      context_ = context;
      argc_ = static_cast<unsigned int>(argc);
      argv_ = argv;
    }

    void FunctionContext::CheckIndex(unsigned int index) const
    {
      if (index >= argc_)
      {
        throw OrthancSQLiteException(ErrorCode_ParameterOutOfRange);
      }
    }

    ColumnType FunctionContext::GetColumnType(unsigned int index) const
    {
      CheckIndex(index);
      return static_cast<SQLite::ColumnType>(sqlite3_value_type(argv_[index]));
    }

    int FunctionContext::GetIntValue(unsigned int index) const
    {
      CheckIndex(index);
      return sqlite3_value_int(argv_[index]);
    }

    int64_t FunctionContext::GetInt64Value(unsigned int index) const
    {
      CheckIndex(index);
      return sqlite3_value_int64(argv_[index]);
    }

    double FunctionContext::GetDoubleValue(unsigned int index) const
    {
      CheckIndex(index);
      return sqlite3_value_double(argv_[index]);
    }

    std::string FunctionContext::GetStringValue(unsigned int index) const
    {
      CheckIndex(index);
      return std::string(reinterpret_cast<const char*>(sqlite3_value_text(argv_[index])));
    }

    bool FunctionContext::IsNullValue(unsigned int index) const
    {
      CheckIndex(index);
      return sqlite3_value_type(argv_[index]) == SQLITE_NULL;
    }
  
    void FunctionContext::SetNullResult()
    {
      sqlite3_result_null(context_);
    }

    void FunctionContext::SetIntResult(int value)
    {
      sqlite3_result_int(context_, value);
    }

    void FunctionContext::SetDoubleResult(double value)
    {
      sqlite3_result_double(context_, value);
    }

    void FunctionContext::SetStringResult(const std::string& str)
    {
      sqlite3_result_text(context_, str.data(), str.size(), SQLITE_TRANSIENT);
    }
  }
}
