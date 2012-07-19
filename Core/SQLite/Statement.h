/**
 * Palantir - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012 Medical Physics Department, CHU of Liege,
 * Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#pragma once

#include "../PalantirException.h"
#include "StatementId.h"
#include "StatementReference.h"

#include <vector>
#include <stdint.h>
#include <boost/noncopyable.hpp>

struct sqlite3_stmt;


namespace Palantir
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

    class Statement : public boost::noncopyable
    {
      friend class Connection;

    private:
      StatementReference  reference_;

      int CheckError(int err) const;

      void CheckOk(int err) const;

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

      bool Run();

      bool Step();

      // Resets the statement to its initial condition. This includes any current
      // result row, and also the bound variables if the |clear_bound_vars| is true.
      void Reset(bool clear_bound_vars = true);

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
      bool ColumnBlobAsVector(int col, std::vector<char>* val) const;
      bool ColumnBlobAsVector(int col, std::vector<unsigned char>* val) const;

    };
  }
}
