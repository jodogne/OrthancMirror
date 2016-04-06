/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 *
 * Copyright (C) 2012-2016 Sebastien Jodogne <s.jodogne@gmail.com>,
 * Medical Physics Department, CHU of Liege, Belgium
 *
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *    * Neither the name of Google Inc., the name of the CHU of Liege,
 * nor the names of its contributors may be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **/


#if ORTHANC_SQLITE_STANDALONE != 1
#include "../PrecompiledHeaders.h"
#endif

#include "Connection.h"
#include "OrthancSQLiteException.h"

#include <memory>
#include <cassert>
#include <string.h>

#if ORTHANC_SQLITE_STANDALONE != 1
#include "../Logging.h"
#endif

#include "sqlite3.h"


namespace Orthanc
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
        throw OrthancSQLiteException(ErrorCode_SQLiteNotOpened);
      }
    }

    void Connection::Open(const std::string& path)
    {
      if (db_) 
      {
        throw OrthancSQLiteException(ErrorCode_SQLiteAlreadyOpened);
      }

      int err = sqlite3_open(path.c_str(), &db_);
      if (err != SQLITE_OK) 
      {
        Close();
        db_ = NULL;
        throw OrthancSQLiteException(ErrorCode_SQLiteCannotOpen);
      }

      // Execute PRAGMAs at this point
      // http://www.sqlite.org/pragma.html
      Execute("PRAGMA FOREIGN_KEYS=ON;");
      Execute("PRAGMA RECURSIVE_TRIGGERS=ON;");
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
           it != cachedStatements_.end(); ++it)
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
          throw OrthancSQLiteException(ErrorCode_SQLiteStatementAlreadyUsed);
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
#if ORTHANC_SQLITE_STANDALONE != 1
      VLOG(1) << "SQLite::Connection::Execute " << sql;
#endif

      CheckIsOpen();

      int error = sqlite3_exec(db_, sql, NULL, NULL, NULL);
      if (error == SQLITE_ERROR)
      {
#if ORTHANC_SQLITE_STANDALONE != 1
        LOG(ERROR) << "SQLite execute error: " << sqlite3_errmsg(db_)
                   << " (" << sqlite3_extended_errcode(db_) << ")";
#endif

        throw OrthancSQLiteException(ErrorCode_SQLiteExecute);
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
        throw OrthancSQLiteException(ErrorCode_SQLiteRollbackWithoutTransaction);
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
        throw OrthancSQLiteException(ErrorCode_SQLiteCommitWithoutTransaction);
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

      IScalarFunction& func = *reinterpret_cast<IScalarFunction*>(payload);
      func.Compute(context);
    }


    static void ScalarFunctionDestroyer(void* payload)
    {
      assert(payload != NULL);
      delete reinterpret_cast<IScalarFunction*>(payload);
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
        throw OrthancSQLiteException(ErrorCode_SQLiteRegisterFunction);
      }

      return func;
    }


    void Connection::FlushToDisk()
    {
#if ORTHANC_SQLITE_STANDALONE != 1
      VLOG(1) << "SQLite::Connection::FlushToDisk";
#endif

      int err = sqlite3_wal_checkpoint(db_, NULL);

      if (err != SQLITE_OK)
      {
        throw OrthancSQLiteException(ErrorCode_SQLiteFlush);
      }
    }
  }
}
