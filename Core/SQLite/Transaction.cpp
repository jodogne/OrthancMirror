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


#include "Transaction.h"

namespace Palantir
{
  namespace SQLite
  {
    Transaction::Transaction(Connection& connection) :
      connection_(connection),
      isOpen_(false)
    {
    }

    Transaction::~Transaction()
    {
      if (isOpen_)
      {
        connection_.RollbackTransaction();
      }
    }

    void Transaction::Begin()
    {
      if (isOpen_) 
      {
        throw PalantirException("SQLite: Beginning a transaction twice!");
      }

      isOpen_ = connection_.BeginTransaction();
      if (!isOpen_)
      {
        throw PalantirException("SQLite: Unable to create a transaction");
      }
    }

    void Transaction::Rollback() 
    {
      if (!isOpen_) 
      {
        throw PalantirException("SQLite: Attempting to roll back a nonexistent transaction. "
                                "Did you remember to call Begin()?");
      }

      isOpen_ = false;

      connection_.RollbackTransaction();
    }

    void Transaction::Commit() 
    {
      if (!isOpen_) 
      {
        throw PalantirException("SQLite: Attempting to roll back a nonexistent transaction. "
                                "Did you remember to call Begin()?");
      }

      isOpen_ = false;

      if (!connection_.CommitTransaction())
      {
        throw PalantirException("SQLite: Failure when committing the transaction");
      }
    }
  }
}
