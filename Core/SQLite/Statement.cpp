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


#include "Statement.h"

#include "Connection.h"
#include "../Toolbox.h"

#include <boost/lexical_cast.hpp>
#include <sqlite3.h>
#include <string.h>

namespace Palantir
{
  namespace SQLite
  {
    int Statement::CheckError(int err) const
    {
      bool succeeded = (err == SQLITE_OK || err == SQLITE_ROW || err == SQLITE_DONE);
      if (!succeeded)
      {
        throw PalantirException("SQLite error code " + boost::lexical_cast<std::string>(err));
      }

      return err;
    }

    void Statement::CheckOk(int err) const 
    {
      if (err == SQLITE_RANGE)
      {
        // Binding to a non-existent variable is evidence of a serious error.
        throw PalantirException("Bind value out of range");
      }
      else if (err != SQLITE_OK)
      {
        throw PalantirException("SQLite error code " + boost::lexical_cast<std::string>(err));
      }
    }


    Statement::Statement(Connection& database,
                         const StatementId& id,
                         const std::string& sql) : 
      reference_(database.GetCachedStatement(id, sql.c_str()))
    {
      Reset(true);
    }


    Statement::Statement(Connection& database,
                         const StatementId& id,
                         const char* sql) : 
      reference_(database.GetCachedStatement(id, sql))
    {
      Reset(true);
    }


    Statement::Statement(Connection& database,
                         const std::string& sql) :
      reference_(database.GetWrappedObject(), sql.c_str())
    {
    }


    Statement::Statement(Connection& database,
                         const char* sql) :
      reference_(database.GetWrappedObject(), sql)
    {
    }


    bool Statement::Run()
    {
      return CheckError(sqlite3_step(GetStatement())) == SQLITE_DONE;
    }

    bool Statement::Step()
    {
      return CheckError(sqlite3_step(GetStatement())) == SQLITE_ROW;
    }

    void Statement::Reset(bool clear_bound_vars) 
    {
      // We don't call CheckError() here because sqlite3_reset() returns
      // the last error that Step() caused thereby generating a second
      // spurious error callback.
      if (clear_bound_vars)
        sqlite3_clear_bindings(GetStatement());
      sqlite3_reset(GetStatement());
    }

    std::string Statement::GetOriginalSQLStatement()
    {
      return std::string(sqlite3_sql(GetStatement()));
    }


    void Statement::BindNull(int col)
    {
      CheckOk(sqlite3_bind_null(GetStatement(), col + 1));
    }

    void Statement::BindBool(int col, bool val) 
    {
      BindInt(col, val ? 1 : 0);
    }

    void Statement::BindInt(int col, int val) 
    {
      CheckOk(sqlite3_bind_int(GetStatement(), col + 1, val));
    }

    void Statement::BindInt64(int col, int64_t val) 
    {
      CheckOk(sqlite3_bind_int64(GetStatement(), col + 1, val));
    }

    void Statement::BindDouble(int col, double val) 
    {
      CheckOk(sqlite3_bind_double(GetStatement(), col + 1, val));
    }

    void Statement::BindCString(int col, const char* val) 
    {
      CheckOk(sqlite3_bind_text(GetStatement(), col + 1, val, -1, SQLITE_TRANSIENT));
    }

    void Statement::BindString(int col, const std::string& val) 
    {
      CheckOk(sqlite3_bind_text(GetStatement(),
                                col + 1,
                                val.data(),
                                val.size(),
                                SQLITE_TRANSIENT));
    }

    /*void Statement::BindString16(int col, const string16& value) 
      {
      BindString(col, UTF16ToUTF8(value));
      }*/

    void Statement::BindBlob(int col, const void* val, int val_len) 
    {
      CheckOk(sqlite3_bind_blob(GetStatement(), col + 1, val, val_len, SQLITE_TRANSIENT));
    }


    int Statement::ColumnCount() const 
    {
      return sqlite3_column_count(GetStatement());
    }


    ColumnType Statement::GetColumnType(int col) const 
    {
      // Verify that our enum matches sqlite's values.
      assert(COLUMN_TYPE_INTEGER == SQLITE_INTEGER);
      assert(COLUMN_TYPE_FLOAT == SQLITE_FLOAT);
      assert(COLUMN_TYPE_TEXT == SQLITE_TEXT);
      assert(COLUMN_TYPE_BLOB == SQLITE_BLOB);
      assert(COLUMN_TYPE_NULL == SQLITE_NULL);

      return static_cast<ColumnType>(sqlite3_column_type(GetStatement(), col));
    }

    ColumnType Statement::GetDeclaredColumnType(int col) const 
    {
      std::string column_type(sqlite3_column_decltype(GetStatement(), col));
      Toolbox::ToLowerCase(column_type);

      if (column_type == "integer")
        return COLUMN_TYPE_INTEGER;
      else if (column_type == "float")
        return COLUMN_TYPE_FLOAT;
      else if (column_type == "text")
        return COLUMN_TYPE_TEXT;
      else if (column_type == "blob")
        return COLUMN_TYPE_BLOB;

      return COLUMN_TYPE_NULL;
    }

    bool Statement::ColumnBool(int col) const 
    {
      return !!ColumnInt(col);
    }

    int Statement::ColumnInt(int col) const 
    {
      return sqlite3_column_int(GetStatement(), col);
    }

    int64_t Statement::ColumnInt64(int col) const 
    {
      return sqlite3_column_int64(GetStatement(), col);
    }

    double Statement::ColumnDouble(int col) const 
    {
      return sqlite3_column_double(GetStatement(), col);
    }

    std::string Statement::ColumnString(int col) const 
    {
      const char* str = reinterpret_cast<const char*>(
        sqlite3_column_text(GetStatement(), col));
      int len = sqlite3_column_bytes(GetStatement(), col);

      std::string result;
      if (str && len > 0)
        result.assign(str, len);
      return result;
    }

    /*string16 Statement::ColumnString16(int col) const 
      {
      std::string s = ColumnString(col);
      return !s.empty() ? UTF8ToUTF16(s) : string16();
      }*/

    int Statement::ColumnByteLength(int col) const 
    {
      return sqlite3_column_bytes(GetStatement(), col);
    }

    const void* Statement::ColumnBlob(int col) const 
    {
      return sqlite3_column_blob(GetStatement(), col);
    }

    bool Statement::ColumnBlobAsString(int col, std::string* blob) 
    {
      const void* p = ColumnBlob(col);
      size_t len = ColumnByteLength(col);
      blob->resize(len);
      if (blob->size() != len) {
        return false;
      }
      blob->assign(reinterpret_cast<const char*>(p), len);
      return true;
    }

    /*bool Statement::ColumnBlobAsString16(int col, string16* val) const 
      {
      const void* data = ColumnBlob(col);
      size_t len = ColumnByteLength(col) / sizeof(char16);
      val->resize(len);
      if (val->size() != len)
      return false;
      val->assign(reinterpret_cast<const char16*>(data), len);
      return true;
      }*/

    bool Statement::ColumnBlobAsVector(int col, std::vector<char>* val) const 
    {
      val->clear();

      const void* data = sqlite3_column_blob(GetStatement(), col);
      int len = sqlite3_column_bytes(GetStatement(), col);
      if (data && len > 0) {
        val->resize(len);
        memcpy(&(*val)[0], data, len);
      }
      return true;
    }

    bool Statement::ColumnBlobAsVector(
      int col,
      std::vector<unsigned char>* val) const 
    {
      return ColumnBlobAsVector(col, reinterpret_cast< std::vector<char>* >(val));
    }

  }
}
