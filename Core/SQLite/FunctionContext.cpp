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


#include "FunctionContext.h"

#include <sqlite3.h>

namespace Palantir
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
        throw PalantirException(ErrorCode_ParameterOutOfRange);
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
