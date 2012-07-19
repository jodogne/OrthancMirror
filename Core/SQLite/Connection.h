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

#include "Statement.h"
#include "IScalarFunction.h"

#include <string>
#include <boost/noncopyable.hpp>
#include <map>

struct sqlite3;
struct sqlite3_stmt;

#define SQLITE_FROM_HERE SQLite::StatementId(__FILE__, __LINE__)

namespace Palantir
{
  namespace SQLite
  {
    class Connection : boost::noncopyable
    {
      friend class Statement;
      friend class Transaction;

    private:
      // All cached statements. Keeping a reference to these statements means that
      // they'll remain active.
      typedef std::map<StatementId, StatementReference*>  CachedStatements;
      CachedStatements cachedStatements_;

      // The actual sqlite database. Will be NULL before Init has been called or if
      // Init resulted in an error.
      sqlite3* db_;

      // Number of currently-nested transactions.
      int transactionNesting_;

      // True if any of the currently nested transactions have been rolled back.
      // When we get to the outermost transaction, this will determine if we do
      // a rollback instead of a commit.
      bool needsRollback_;

      void ClearCache();

      void CheckIsOpen() const;

      sqlite3* GetWrappedObject()
      {
        return db_;
      }

      StatementReference& GetCachedStatement(const StatementId& id,
                                             const char* sql);

      bool DoesTableOrIndexExist(const char* name, 
                                 const char* type) const;

      void DoRollback();

    public:
      // The database is opened by calling Open[InMemory](). Any uncommitted
      // transactions will be rolled back when this object is deleted.
      Connection();
      ~Connection();

      void Open(const std::string& path);

      void OpenInMemory();

      void Close();

      bool Execute(const char* sql);

      bool Execute(const std::string& sql)
      {
        return Execute(sql.c_str());
      }

      IScalarFunction* Register(IScalarFunction* func);  // Takes the ownership of the function

      // Info querying -------------------------------------------------------------

      // Used to check a |sql| statement for syntactic validity. If the
      // statement is valid SQL, returns true.
      bool IsSQLValid(const char* sql);

      // Returns true if the given table exists.
      bool DoesTableExist(const char* table_name) const;

      // Returns true if the given index exists.
      bool DoesIndexExist(const char* index_name) const;
    
      // Returns true if a column with the given name exists in the given table.
      bool DoesColumnExist(const char* table_name, const char* column_name) const;

      // Returns sqlite's internal ID for the last inserted row. Valid only
      // immediately after an insert.
      int64_t GetLastInsertRowId() const;

      // Returns sqlite's count of the number of rows modified by the last
      // statement executed. Will be 0 if no statement has executed or the database
      // is closed.
      int GetLastChangeCount() const;

      // Errors --------------------------------------------------------------------

      // Returns the error code associated with the last sqlite operation.
      int GetErrorCode() const;

      // Returns the errno associated with GetErrorCode().  See
      // SQLITE_LAST_ERRNO in SQLite documentation.
      int GetLastErrno() const;

      // Returns a pointer to a statically allocated string associated with the
      // last sqlite operation.
      const char* GetErrorMessage() const;


      // Diagnostics (for unit tests) ----------------------------------------------

      int ExecuteAndReturnErrorCode(const char* sql);
    
      bool HasCachedStatement(const StatementId& id) const
      {
        return cachedStatements_.find(id) != cachedStatements_.end();
      }

      int GetTransactionNesting() const
      {
        return transactionNesting_;
      }

      // Transactions --------------------------------------------------------------

      bool BeginTransaction();
      void RollbackTransaction();
      bool CommitTransaction();
    };
  }
}
