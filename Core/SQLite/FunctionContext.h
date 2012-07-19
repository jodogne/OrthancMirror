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

#include <boost/noncopyable.hpp>

#include "Statement.h"

struct sqlite3_context;
struct Mem;  // This corresponds to the opaque type "sqlite3_value"
 
namespace Palantir
{
  namespace SQLite
  {
    class FunctionContext : public boost::noncopyable
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

      double GetDoubleValue(unsigned int index) const;

      std::string GetStringValue(unsigned int index) const;
  
      void SetNullResult();

      void SetIntResult(int value);

      void SetDoubleResult(double value);

      void SetStringResult(const std::string& str);
    };
  }
}
