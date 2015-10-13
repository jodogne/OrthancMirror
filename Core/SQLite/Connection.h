/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 *
 * Copyright (C) 2012-2015 Sebastien Jodogne <s.jodogne@gmail.com>,
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


#pragma once

#include "Statement.h"
#include "IScalarFunction.h"

#include <string>
#include <map>

struct sqlite3;
struct sqlite3_stmt;

#define SQLITE_FROM_HERE ::Orthanc::SQLite::StatementId(__FILE__, __LINE__)

namespace Orthanc
{
  namespace SQLite
  {
    class Connection : NonCopyable
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

      void FlushToDisk();

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
