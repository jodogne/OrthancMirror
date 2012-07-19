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


#include "Connection.h"

#include <memory>
#include <cassert>
#include <sqlite3.h>
#include <string.h>



namespace Palantir
{
  namespace SQLite
  {
    Connection::Connection() :
      db_(NULL),
      transactionNesting_(0),
      needsRollback_(false)
    {
    }


    Connection::~Connection()
    {
      Close();
    }


    void Connection::CheckIsOpen() const
    {
      if (!db_)
      {
        throw PalantirException("SQLite: The database is not opened");
      }
    }

    void Connection::Open(const std::string& path)
    {
      if (db_) 
      {
        throw PalantirException("SQLite: Connection is already open");
      }

      int err = sqlite3_open(path.c_str(), &db_);
      if (err != SQLITE_OK) 
      {
        Close();
        db_ = NULL;
        throw PalantirException("SQLite: Unable to open the database");
      }

      // Execute PRAGMAs at this point
      // http://www.sqlite.org/pragma.html
      Execute("PRAGMA FOREIGN_KEYS=ON;");

      // Performance tuning
      Execute("PRAGMA SYNCHRONOUS=NORMAL;");
      Execute("PRAGMA JOURNAL_MODE=WAL;");
      Execute("PRAGMA LOCKING_MODE=EXCLUSIVE;");
      Execute("PRAGMA WAL_AUTOCHECKPOINT=1000;");
      //Execute("PRAGMA TEMP_STORE=memory");
    }

    void Connection::OpenInMemory()
    {
      Open(":memory:");
    }

    void Connection::Close() 
    {
      ClearCache();

      if (db_)
      {
        sqlite3_close(db_);
        db_ = NULL;
      }
    }

    void Connection::ClearCache()
    {
      for (CachedStatements::iterator 
             it = cachedStatements_.begin(); 
           it != cachedStatements_.end(); it++)
      {
        delete it->second;
      }

      cachedStatements_.clear();
    }


    StatementReference& Connection::GetCachedStatement(const StatementId& id,
                                                       const char* sql)
    {
      CachedStatements::iterator i = cachedStatements_.find(id);
      if (i != cachedStatements_.end())
      {
        if (i->second->GetReferenceCount() >= 1)
        {
          throw PalantirException("SQLite: This cached statement is already being referred to");
        }

        return *i->second;
      }
      else
      {
        StatementReference* statement = new StatementReference(db_, sql);
        cachedStatements_[id] = statement;
        return *statement;
      }
    }


    bool Connection::Execute(const char* sql) 
    {
      CheckIsOpen();

      int error = sqlite3_exec(db_, sql, NULL, NULL, NULL);
      if (error == SQLITE_ERROR)
      {
        throw PalantirException("SQLite Execute error: " + std::string(sqlite3_errmsg(db_)));
      }
      else
      {
        return error == SQLITE_OK;
      }
    }

    int  Connection::ExecuteAndReturnErrorCode(const char* sql)
    {
      CheckIsOpen();
      return sqlite3_exec(db_, sql, NULL, NULL, NULL);
    }

    // Info querying -------------------------------------------------------------

    bool Connection::IsSQLValid(const char* sql) 
    {
      sqlite3_stmt* stmt = NULL;
      if (sqlite3_prepare_v2(db_, sql, -1, &stmt, NULL) != SQLITE_OK)
        return false;

      sqlite3_finalize(stmt);
      return true;
    }

    bool Connection::DoesTableOrIndexExist(const char* name, 
                                           const char* type) const
    {
      // Our SQL is non-mutating, so this cast is OK.
      Statement statement(const_cast<Connection&>(*this), 
                          "SELECT name FROM sqlite_master WHERE type=? AND name=?");
      statement.BindString(0, type);
      statement.BindString(1, name);
      return statement.Step();  // Table exists if any row was returned.
    }

    bool Connection::DoesTableExist(const char* table_name) const
    {
      return DoesTableOrIndexExist(table_name, "table");
    }

    bool Connection::DoesIndexExist(const char* index_name) const
    {
      return DoesTableOrIndexExist(index_name, "index");
    }

    bool Connection::DoesColumnExist(const char* table_name, const char* column_name) const
    {
      std::string sql("PRAGMA TABLE_INFO(");
      sql.append(table_name);
      sql.append(")");

      // Our SQL is non-mutating, so this cast is OK.
      Statement statement(const_cast<Connection&>(*this), sql.c_str());

      while (statement.Step()) {
        if (!statement.ColumnString(1).compare(column_name))
          return true;
      }
      return false;
    }

    int64_t Connection::GetLastInsertRowId() const
    {
      return sqlite3_last_insert_rowid(db_);
    }

    int Connection::GetLastChangeCount() const
    {
      return sqlite3_changes(db_);
    }

    int Connection::GetErrorCode() const 
    {
      return sqlite3_errcode(db_);
    }

    int Connection::GetLastErrno() const 
    {
      int err = 0;
      if (SQLITE_OK != sqlite3_file_control(db_, NULL, SQLITE_LAST_ERRNO, &err))
        return -2;

      return err;
    }

    const char* Connection::GetErrorMessage() const 
    {
      return sqlite3_errmsg(db_);
    }


    bool Connection::BeginTransaction()
    {
      if (needsRollback_)
      {
        assert(transactionNesting_ > 0);

        // When we're going to rollback, fail on this begin and don't actually
        // mark us as entering the nested transaction.
        return false;
      }

      bool success = true;
      if (!transactionNesting_) 
      {
        needsRollback_ = false;

        Statement begin(*this, SQLITE_FROM_HERE, "BEGIN TRANSACTION");
        if (!begin.Run())
          return false;
      }
      transactionNesting_++;
      return success;
    }

    void Connection::RollbackTransaction()
    {
      if (!transactionNesting_)
      {
        throw PalantirException("Rolling back a nonexistent transaction");
      }

      transactionNesting_--;

      if (transactionNesting_ > 0)
      {
        // Mark the outermost transaction as needing rollback.
        needsRollback_ = true;
        return;
      }

      DoRollback();
    }

    bool Connection::CommitTransaction() 
    {
      if (!transactionNesting_) 
      {
        throw PalantirException("Committing a nonexistent transaction");
      }
      transactionNesting_--;

      if (transactionNesting_ > 0) 
      {
        // Mark any nested transactions as failing after we've already got one.
        return !needsRollback_;
      }

      if (needsRollback_) 
      {
        DoRollback();
        return false;
      }

      Statement commit(*this, SQLITE_FROM_HERE, "COMMIT");
      return commit.Run();
    }

    void Connection::DoRollback() 
    {
      Statement rollback(*this, SQLITE_FROM_HERE, "ROLLBACK");
      rollback.Run();
      needsRollback_ = false;
    }






    static void ScalarFunctionCaller(sqlite3_context* rawContext,
                                     int argc,
                                     sqlite3_value** argv)
    {
      FunctionContext context(rawContext, argc, argv);

      void* payload = sqlite3_user_data(rawContext);
      assert(payload != NULL);

      IScalarFunction& func = *(IScalarFunction*) payload;
      func.Compute(context);
    }


    static void ScalarFunctionDestroyer(void* payload)
    {
      assert(payload != NULL);
      delete (IScalarFunction*) payload;
    }


    IScalarFunction* Connection::Register(IScalarFunction* func)
    {
      int err = sqlite3_create_function_v2(db_, 
                                           func->GetName(), 
                                           func->GetCardinality(),
                                           SQLITE_UTF8, 
                                           func,
                                           ScalarFunctionCaller,
                                           NULL,
                                           NULL,
                                           ScalarFunctionDestroyer);

      if (err != SQLITE_OK)
      {
        delete func;
        throw PalantirException("SQLite: Unable to register a function");
      }

      return func;
    }

  }
}
