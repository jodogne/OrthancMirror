/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 **/


#if ORTHANC_UNIT_TESTS_LINK_FRAMEWORK == 1
// Must be the first to be sure to use the Orthanc framework shared library
#  include <OrthancFramework.h>
#endif

#include <gtest/gtest.h>

#include "../Sources/Toolbox.h"
#include "../Sources/SQLite/Connection.h"
#include "../Sources/SQLite/Statement.h"
#include "../Sources/SQLite/Transaction.h"

#include <sqlite3.h>


using namespace Orthanc;
using namespace Orthanc::SQLite;


/********************************************************************
 ** Tests from
 ** http://src.chromium.org/viewvc/chrome/trunk/src/sql/connection_unittest.cc
 ********************************************************************/

namespace
{
  class SQLConnectionTest : public testing::Test 
  {
  public:
    SQLConnectionTest()
    {
    }

    virtual ~SQLConnectionTest() ORTHANC_OVERRIDE
    {
    }

    virtual void SetUp() ORTHANC_OVERRIDE
    {
      db_.OpenInMemory();
    }

    virtual void TearDown() ORTHANC_OVERRIDE
    {
      db_.Close();
    }

    Connection& db() 
    { 
      return db_; 
    }

  private:
    Connection db_;
  };
}



TEST_F(SQLConnectionTest, Execute) 
{
  // Valid statement should return true.
  ASSERT_TRUE(db().Execute("CREATE TABLE foo (a, b)"));
  EXPECT_EQ(SQLITE_OK, db().GetErrorCode());

  // Invalid statement should fail.
  ASSERT_EQ(SQLITE_ERROR,
            db().ExecuteAndReturnErrorCode("CREATE TAB foo (a, b"));
  EXPECT_EQ(SQLITE_ERROR, db().GetErrorCode());
}

TEST_F(SQLConnectionTest, ExecuteWithErrorCode) {
  ASSERT_EQ(SQLITE_OK,
            db().ExecuteAndReturnErrorCode("CREATE TABLE foo (a, b)"));
  ASSERT_EQ(SQLITE_ERROR,
            db().ExecuteAndReturnErrorCode("CREATE TABLE TABLE"));
  ASSERT_EQ(SQLITE_ERROR,
            db().ExecuteAndReturnErrorCode(
              "INSERT INTO foo(a, b) VALUES (1, 2, 3, 4)"));
}

TEST_F(SQLConnectionTest, CachedStatement) {
  StatementId id1("foo", 12);
  ASSERT_TRUE(db().Execute("CREATE TABLE foo (a, b)"));
  ASSERT_TRUE(db().Execute("INSERT INTO foo(a, b) VALUES (12, 13)"));

  // Create a new cached statement.
  {
    Statement s(db(), id1, "SELECT a FROM foo");
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(12, s.ColumnInt(0));
  }

  // The statement should be cached still.
  EXPECT_TRUE(db().HasCachedStatement(id1));

  {
    // Get the same statement using different SQL. This should ignore our
    // SQL and use the cached one (so it will be valid).
    Statement s(db(), id1, "something invalid(");
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(12, s.ColumnInt(0));
  }

  // Make sure other statements aren't marked as cached.
  EXPECT_FALSE(db().HasCachedStatement(SQLITE_FROM_HERE));
}

TEST_F(SQLConnectionTest, IsSQLValidTest) {
  ASSERT_TRUE(db().Execute("CREATE TABLE foo (a, b)"));
  ASSERT_TRUE(db().IsSQLValid("SELECT a FROM foo"));
  ASSERT_FALSE(db().IsSQLValid("SELECT no_exist FROM foo"));
}



TEST_F(SQLConnectionTest, DoesStuffExist) {
  // Test DoesTableExist.
  EXPECT_FALSE(db().DoesTableExist("foo"));
  ASSERT_TRUE(db().Execute("CREATE TABLE foo (a, b)"));
  EXPECT_TRUE(db().DoesTableExist("foo"));

  // Should be case sensitive.
  EXPECT_FALSE(db().DoesTableExist("FOO"));

  // Test DoesColumnExist.
  EXPECT_FALSE(db().DoesColumnExist("foo", "bar"));
  EXPECT_TRUE(db().DoesColumnExist("foo", "a"));

  // Testing for a column on a nonexistent table.
  EXPECT_FALSE(db().DoesColumnExist("bar", "b"));
}

TEST_F(SQLConnectionTest, GetLastInsertRowId) {
  ASSERT_TRUE(db().Execute("CREATE TABLE foo (id INTEGER PRIMARY KEY, value)"));

  ASSERT_TRUE(db().Execute("INSERT INTO foo (value) VALUES (12)"));

  // Last insert row ID should be valid.
  int64_t row = db().GetLastInsertRowId();
  EXPECT_LT(0, row);

  // It should be the primary key of the row we just inserted.
  Statement s(db(), "SELECT value FROM foo WHERE id=?");
  s.BindInt64(0, row);
  ASSERT_TRUE(s.Step());
  EXPECT_EQ(12, s.ColumnInt(0));
}

TEST_F(SQLConnectionTest, Rollback) {
  ASSERT_TRUE(db().BeginTransaction());
  ASSERT_TRUE(db().BeginTransaction());
  EXPECT_EQ(2, db().GetTransactionNesting());
  db().RollbackTransaction();
  EXPECT_FALSE(db().CommitTransaction());
  EXPECT_TRUE(db().BeginTransaction());
}




/********************************************************************
 ** Tests from
 ** http://src.chromium.org/viewvc/chrome/trunk/src/sql/statement_unittest.cc
 ********************************************************************/

namespace Orthanc
{
  namespace SQLite
  {
    class SQLStatementTest : public SQLConnectionTest
    {
    };

    TEST_F(SQLStatementTest, Run) {
      ASSERT_TRUE(db().Execute("CREATE TABLE foo (a, b)"));
      ASSERT_TRUE(db().Execute("INSERT INTO foo (a, b) VALUES (3, 12)"));

      Statement s(db(), "SELECT b FROM foo WHERE a=?");
      // Stepping it won't work since we haven't bound the value.
      EXPECT_FALSE(s.Step());

      // Run should fail since this produces output, and we should use Step(). This
      // gets a bit wonky since sqlite says this is OK so succeeded is set.
      s.Reset(true);
      s.BindInt(0, 3);
      EXPECT_FALSE(s.Run());
      EXPECT_EQ(SQLITE_ROW, db().GetErrorCode());

      // Resetting it should put it back to the previous state (not runnable).
      s.Reset(true);

      // Binding and stepping should produce one row.
      s.BindInt(0, 3);
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(12, s.ColumnInt(0));
      EXPECT_FALSE(s.Step());
    }

    TEST_F(SQLStatementTest, BasicErrorCallback) {
      ASSERT_TRUE(db().Execute("CREATE TABLE foo (a INTEGER PRIMARY KEY, b)"));
      // Insert in the foo table the primary key. It is an error to insert
      // something other than an number. This error causes the error callback
      // handler to be called with SQLITE_MISMATCH as error code.
      Statement s(db(), "INSERT INTO foo (a) VALUES (?)");
      s.BindCString(0, "bad bad");
      EXPECT_THROW(s.Run(), OrthancException);
    }

    TEST_F(SQLStatementTest, Reset) {
      ASSERT_TRUE(db().Execute("CREATE TABLE foo (a, b)"));
      ASSERT_TRUE(db().Execute("INSERT INTO foo (a, b) VALUES (3, 12)"));
      ASSERT_TRUE(db().Execute("INSERT INTO foo (a, b) VALUES (4, 13)"));

      Statement s(db(), "SELECT b FROM foo WHERE a = ? ");
      s.BindInt(0, 3);
      ASSERT_TRUE(s.Step());
      EXPECT_EQ(12, s.ColumnInt(0));
      ASSERT_FALSE(s.Step());

      s.Reset(false);
      // Verify that we can get all rows again.
      ASSERT_TRUE(s.Step());
      EXPECT_EQ(12, s.ColumnInt(0));
      EXPECT_FALSE(s.Step());

      s.Reset(true);
      ASSERT_FALSE(s.Step());
    }
  }
}






/********************************************************************
 ** Tests from
 ** http://src.chromium.org/viewvc/chrome/trunk/src/sql/transaction_unittest.cc
 ********************************************************************/

namespace
{
  class SQLTransactionTest : public SQLConnectionTest
  {
  public:
    virtual void SetUp() ORTHANC_OVERRIDE
    {
      SQLConnectionTest::SetUp();
      ASSERT_TRUE(db().Execute("CREATE TABLE foo (a, b)"));
    }

    // Returns the number of rows in table "foo".
    int CountFoo() 
    {
      Statement count(db(), "SELECT count(*) FROM foo");
      count.Step();
      return count.ColumnInt(0);
    }
  };
}


TEST_F(SQLTransactionTest, Commit) {
  {
    Transaction t(db());
    EXPECT_FALSE(t.IsOpen());
    t.Begin();
    EXPECT_TRUE(t.IsOpen());

    EXPECT_TRUE(db().Execute("INSERT INTO foo (a, b) VALUES (1, 2)"));

    t.Commit();
    EXPECT_FALSE(t.IsOpen());
  }

  EXPECT_EQ(1, CountFoo());
}

TEST_F(SQLTransactionTest, Rollback) {
  // Test some basic initialization, and that rollback runs when you exit the
  // scope.
  {
    Transaction t(db());
    EXPECT_FALSE(t.IsOpen());
    t.Begin();
    EXPECT_TRUE(t.IsOpen());

    EXPECT_TRUE(db().Execute("INSERT INTO foo (a, b) VALUES (1, 2)"));
  }

  // Nothing should have been committed since it was implicitly rolled back.
  EXPECT_EQ(0, CountFoo());

  // Test explicit rollback.
  Transaction t2(db());
  EXPECT_FALSE(t2.IsOpen());
  t2.Begin();

  EXPECT_TRUE(db().Execute("INSERT INTO foo (a, b) VALUES (1, 2)"));
  t2.Rollback();
  EXPECT_FALSE(t2.IsOpen());

  // Nothing should have been committed since it was explicitly rolled back.
  EXPECT_EQ(0, CountFoo());
}

// Rolling back any part of a transaction should roll back all of them.
TEST_F(SQLTransactionTest, NestedRollback) {
  EXPECT_EQ(0, db().GetTransactionNesting());

  // Outermost transaction.
  {
    Transaction outer(db());
    outer.Begin();
    EXPECT_EQ(1, db().GetTransactionNesting());

    // The first inner one gets committed.
    {
      Transaction inner1(db());
      inner1.Begin();
      EXPECT_TRUE(db().Execute("INSERT INTO foo (a, b) VALUES (1, 2)"));
      EXPECT_EQ(2, db().GetTransactionNesting());

      inner1.Commit();
      EXPECT_EQ(1, db().GetTransactionNesting());
    }

    // One row should have gotten inserted.
    EXPECT_EQ(1, CountFoo());

    // The second inner one gets rolled back.
    {
      Transaction inner2(db());
      inner2.Begin();
      EXPECT_TRUE(db().Execute("INSERT INTO foo (a, b) VALUES (1, 2)"));
      EXPECT_EQ(2, db().GetTransactionNesting());

      inner2.Rollback();
      EXPECT_EQ(1, db().GetTransactionNesting());
    }

    // A third inner one will fail in Begin since one has already been rolled
    // back.
    EXPECT_EQ(1, db().GetTransactionNesting());
    {
      Transaction inner3(db());
      EXPECT_THROW(inner3.Begin(), OrthancException);
      EXPECT_EQ(1, db().GetTransactionNesting());
    }
  }
  EXPECT_EQ(0, db().GetTransactionNesting());
  EXPECT_EQ(0, CountFoo());
}
