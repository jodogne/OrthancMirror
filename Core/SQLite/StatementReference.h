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
#include <stdint.h>
#include <cassert>
#include <stdlib.h>

struct sqlite3;
struct sqlite3_stmt;

namespace Palantir
{
  namespace SQLite
  {
    class StatementReference : boost::noncopyable
    {
    private:
      StatementReference* root_;   // Only used for non-root nodes
      uint32_t refCount_;         // Only used for root node
      struct sqlite3_stmt* statement_;

      bool IsRoot() const;

    public:
      StatementReference();

      StatementReference(sqlite3* database,
                         const char* sql);

      StatementReference(StatementReference& other);

      ~StatementReference();

      uint32_t GetReferenceCount() const;

      struct sqlite3_stmt* GetWrappedObject() const
      {
        assert(statement_ != NULL);
        return statement_;
      }
    };
  }
}
