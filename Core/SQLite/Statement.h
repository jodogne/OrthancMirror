/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 *
 * Copyright (C) 2012-2016 Sebastien Jodogne <s.jodogne@gmail.com>,
 * Medical Physics Department, CHU of Liege, Belgium
 *
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
 *    * Neither the name of Google Inc., the name of the CHU of Liege,
 * nor the names of its contributors may be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
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

#include "NonCopyable.h"
#include "OrthancSQLiteException.h"
#include "StatementId.h"
#include "StatementReference.h"

#include <vector>
#include <stdint.h>

#if ORTHANC_BUILD_UNIT_TESTS == 1
#  include <gtest/gtest_prod.h>
#endif


namespace Orthanc
{
  namespace SQLite
  {
    class Connection;

    // Possible return values from ColumnType in a statement. These
    // should match the values in sqlite3.h.
    enum ColumnType 
    {
      COLUMN_TYPE_INTEGER = 1,
      COLUMN_TYPE_FLOAT = 2,
      COLUMN_TYPE_TEXT = 3,
      COLUMN_TYPE_BLOB = 4,
      COLUMN_TYPE_NULL = 5
    };

    class Statement : public NonCopyable
    {
      friend class Connection;

#if ORTHANC_BUILD_UNIT_TESTS == 1
      FRIEND_TEST(SQLStatementTest, Run);
      FRIEND_TEST(SQLStatementTest, Reset);
#endif

    private:
      StatementReference  reference_;

      int CheckError(int err, 
                     ErrorCode code) const;

      void CheckOk(int err, 
                   ErrorCode code) const;

      struct sqlite3_stmt* GetStatement() const
      {
        return reference_.GetWrappedObject();
      }

    public:
      Statement(Connection& database,
                const std::string& sql);

      Statement(Connection& database,
                const StatementId& id,
                const std::string& sql);

      Statement(Connection& database,
                const char* sql);

      Statement(Connection& database,
                const StatementId& id,
                const char* sql);

      ~Statement()
      {
        Reset();
      }

      bool Run();

      bool Step();

      // Diagnostics --------------------------------------------------------------

      std::string GetOriginalSQLStatement();


      // Binding -------------------------------------------------------------------

      // These all take a 0-based argument index
      void BindNull(int col);
      void BindBool(int col, bool val);
      void BindInt(int col, int val);
      void BindInt64(int col, int64_t val);
      void BindDouble(int col, double val);
      void BindCString(int col, const char* val);
      void BindString(int col, const std::string& val);
      //void BindString16(int col, const string16& value);
      void BindBlob(int col, const void* value, int value_len);


      // Retrieving ----------------------------------------------------------------

      // Returns the number of output columns in the result.
      int ColumnCount() const;

      // Returns the type associated with the given column.
      //
      // Watch out: the type may be undefined if you've done something to cause a
      // "type conversion." This means requesting the value of a column of a type
      // where that type is not the native type. For safety, call ColumnType only
      // on a column before getting the value out in any way.
      ColumnType GetColumnType(int col) const;
      ColumnType GetDeclaredColumnType(int col) const;

      // These all take a 0-based argument index.
      bool ColumnIsNull(int col) const ;
      bool ColumnBool(int col) const;
      int ColumnInt(int col) const;
      int64_t ColumnInt64(int col) const;
      double ColumnDouble(int col) const;
      std::string ColumnString(int col) const;
      //string16 ColumnString16(int col) const;

      // When reading a blob, you can get a raw pointer to the underlying data,
      // along with the length, or you can just ask us to copy the blob into a
      // vector. Danger! ColumnBlob may return NULL if there is no data!
      int ColumnByteLength(int col) const;
      const void* ColumnBlob(int col) const;
      bool ColumnBlobAsString(int col, std::string* blob);
      //bool ColumnBlobAsString16(int col, string16* val) const;
      //bool ColumnBlobAsVector(int col, std::vector<char>* val) const;
      //bool ColumnBlobAsVector(int col, std::vector<unsigned char>* val) const;

      // Resets the statement to its initial condition. This includes any current
      // result row, and also the bound variables if the |clear_bound_vars| is true.
      void Reset(bool clear_bound_vars = true);
    };
  }
}
