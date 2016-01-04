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

#include "Statement.h"

struct sqlite3_context;
struct Mem;  // This corresponds to the opaque type "sqlite3_value"
 
namespace Orthanc
{
  namespace SQLite
  {
    class FunctionContext : public NonCopyable
    {
      friend class Connection;

    private:
      struct sqlite3_context* context_;
      unsigned int argc_;
      struct ::Mem** argv_;

      void CheckIndex(unsigned int index) const;

    public:
      FunctionContext(struct sqlite3_context* context,
                      int argc,
                      struct ::Mem** argv);

      ColumnType GetColumnType(unsigned int index) const;
 
      unsigned int GetParameterCount() const
      {
        return argc_;
      }

      int GetIntValue(unsigned int index) const;

      int64_t GetInt64Value(unsigned int index) const;

      double GetDoubleValue(unsigned int index) const;

      std::string GetStringValue(unsigned int index) const;

      bool IsNullValue(unsigned int index) const;
  
      void SetNullResult();

      void SetIntResult(int value);

      void SetDoubleResult(double value);

      void SetStringResult(const std::string& str);
    };
  }
}
