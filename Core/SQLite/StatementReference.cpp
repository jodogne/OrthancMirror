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


#if ORTHANC_SQLITE_STANDALONE != 1
#include "../PrecompiledHeaders.h"
#endif

#include "StatementReference.h"
#include "OrthancSQLiteException.h"

#if ORTHANC_SQLITE_STANDALONE != 1
#include "../Logging.h"
#endif

#include <string>
#include <cassert>
#include "sqlite3.h"

namespace Orthanc
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
        throw OrthancSQLiteException(ErrorCode_ParameterOutOfRange);
      }

      root_ = NULL;
      refCount_ = 0;

      int error = sqlite3_prepare_v2(database, sql, -1, &statement_, NULL);
      if (error != SQLITE_OK)
      {
#if ORTHANC_SQLITE_STANDALONE != 1
        LOG(ERROR) << "SQLite: " << sqlite3_errmsg(database);
#endif

        throw OrthancSQLiteException(ErrorCode_SQLitePrepareStatement);
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
          // There remain references to this object. We cannot throw
          // an exception because:
          // http://www.parashift.com/c++-faq/dtors-shouldnt-throw.html

#if ORTHANC_SQLITE_STANDALONE != 1
          LOG(ERROR) << "Bad value of the reference counter";
#endif
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
          // There remain references to this object. We cannot throw
          // an exception because:
          // http://www.parashift.com/c++-faq/dtors-shouldnt-throw.html

#if ORTHANC_SQLITE_STANDALONE != 1
          LOG(ERROR) << "Bad value of the reference counter";
#endif
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
