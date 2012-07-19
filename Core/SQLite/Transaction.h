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

#include "Connection.h"

namespace Palantir
{
  namespace SQLite
  {
    class Transaction : public boost::noncopyable
    {
    private:
      Connection& connection_;

      // True when the transaction is open, false when it's already been committed
      // or rolled back.
      bool isOpen_;

    public:
      explicit Transaction(Connection& connection);
      ~Transaction();

      // Returns true when there is a transaction that has been successfully begun.
      bool IsOpen() const { return isOpen_; }

      // Begins the transaction. This uses the default sqlite "deferred" transaction
      // type, which means that the DB lock is lazily acquired the next time the
      // database is accessed, not in the begin transaction command.
      void Begin();

      // Rolls back the transaction. This will happen automatically if you do
      // nothing when the transaction goes out of scope.
      void Rollback();

      // Commits the transaction, returning true on success.
      void Commit();
    };
  }
}
