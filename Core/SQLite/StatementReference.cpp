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


#include "StatementReference.h"

#include "../PalantirException.h"

#include <cassert>
#include "sqlite3.h"

namespace Palantir
{
  namespace SQLite
  {
    bool StatementReference::IsRoot() const
    {
      return root_ == NULL;
    }

    StatementReference::StatementReference()
    {
      root_ = NULL;
      refCount_ = 0;
      statement_ = NULL;
      assert(IsRoot());
    }

    StatementReference::StatementReference(sqlite3* database,
                                           const char* sql)
    {
      if (database == NULL || sql == NULL)
      {
        throw PalantirException(ErrorCode_ParameterOutOfRange);
      }

      root_ = NULL;
      refCount_ = 0;

      int error = sqlite3_prepare_v2(database, sql, -1, &statement_, NULL);
      if (error != SQLITE_OK)
      {
        throw PalantirException("SQLite: " + std::string(sqlite3_errmsg(database)));
      }

      assert(IsRoot());
    }

    StatementReference::StatementReference(StatementReference& other)
    {
      refCount_ = 0;

      if (other.IsRoot())
      {
        root_ = &other;
      }
      else
      {
        root_ = other.root_;
      }

      root_->refCount_++;
      statement_ = root_->statement_;

      assert(!IsRoot());
    }

    StatementReference::~StatementReference()
    {
      if (IsRoot())
      {
        if (refCount_ != 0)
        {
          // There remain references to this object
          throw PalantirException(ErrorCode_InternalError);
        }
        else if (statement_ != NULL)
        {
          sqlite3_finalize(statement_);
        }
      }
      else
      {
        if (root_->refCount_ == 0)
        {
          throw PalantirException(ErrorCode_InternalError);
        }
        else
        {
          root_->refCount_--;
        }
      }
    }

    uint32_t StatementReference::GetReferenceCount() const
    {
      return refCount_;
    }
  }
}
