/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * In addition, as a special exception, the copyright holders of this
 * program give permission to link the code of its release with the
 * OpenSSL project's "OpenSSL" library (or with modified versions of it
 * that use the same license as the "OpenSSL" library), and distribute
 * the linked executables. You must obey the GNU General Public License
 * in all respects for all of the code used other than "OpenSSL". If you
 * modify file(s) with this exception, you may extend this exception to
 * your version of the file(s), but you are not obligated to do so. If
 * you do not wish to do so, delete this exception statement from your
 * version. If you delete this exception statement from all source files
 * in the program, then also delete it here.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include "PrecompiledHeadersUnitTests.h"
#include "gtest/gtest.h"

#include "../Core/SystemToolbox.h"
#include "../Core/SQLite/Connection.h"
#include "../Core/SQLite/Statement.h"
#include "../Core/SQLite/Transaction.h"

#include <sqlite3.h>

using namespace Orthanc;


TEST(SQLite, Configuration)
{
  /**
   * The system-wide version of SQLite under OS X uses
   * SQLITE_THREADSAFE==2 (SQLITE_CONFIG_SERIALIZED), whereas the
   * static builds of Orthanc use SQLITE_THREADSAFE==1
   * (SQLITE_CONFIG_MULTITHREAD). In any case, we wish to ensure that
   * SQLITE_THREADSAFE!=0 (SQLITE_CONFIG_SINGLETHREAD).
   **/
  ASSERT_NE(0, sqlite3_threadsafe());
}


TEST(SQLite, Connection)
{
  SystemToolbox::RemoveFile("UnitTestsResults/coucou");
  SQLite::Connection c;
  c.Open("UnitTestsResults/coucou");
  c.Execute("CREATE TABLE c(k INTEGER PRIMARY KEY AUTOINCREMENT, v INTEGER)");
  c.Execute("INSERT INTO c VALUES(NULL, 42);");
}


TEST(SQLite, StatementReferenceBasic)
{
  sqlite3* db;
  sqlite3_open(":memory:", &db);

  {
    SQLite::StatementReference r(db, "SELECT * FROM sqlite_master");
    ASSERT_EQ(0u, r.GetReferenceCount());

    {
      SQLite::StatementReference r1(r);
      ASSERT_EQ(1u, r.GetReferenceCount());
      ASSERT_EQ(0u, r1.GetReferenceCount());

      {
        SQLite::StatementReference r2(r);
        ASSERT_EQ(2u, r.GetReferenceCount());
        ASSERT_EQ(0u, r1.GetReferenceCount());
        ASSERT_EQ(0u, r2.GetReferenceCount());

        SQLite::StatementReference r3(r2);
        ASSERT_EQ(3u, r.GetReferenceCount());
        ASSERT_EQ(0u, r1.GetReferenceCount());
        ASSERT_EQ(0u, r2.GetReferenceCount());
        ASSERT_EQ(0u, r3.GetReferenceCount());
      }

      ASSERT_EQ(1u, r.GetReferenceCount());
      ASSERT_EQ(0u, r1.GetReferenceCount());

      {
        SQLite::StatementReference r2(r);
        ASSERT_EQ(2u, r.GetReferenceCount());
        ASSERT_EQ(0u, r1.GetReferenceCount());
        ASSERT_EQ(0u, r2.GetReferenceCount());
      }

      ASSERT_EQ(1u, r.GetReferenceCount());
      ASSERT_EQ(0u, r1.GetReferenceCount());
    }

    ASSERT_EQ(0u, r.GetReferenceCount());
  }

  sqlite3_close(db);
}

TEST(SQLite, StatementBasic)
{
  SQLite::Connection c;
  c.OpenInMemory();
  
  SQLite::Statement s(c, "SELECT * from sqlite_master");
  s.Run();

  for (unsigned int i = 0; i < 5; i++)
  {
    SQLite::Statement cs(c, SQLITE_FROM_HERE, "SELECT * from sqlite_master");
    cs.Step();
  }
}


namespace
{
  static bool destroyed;

  class MyFunc : public SQLite::IScalarFunction
  {
  public:
    MyFunc()
    {
      destroyed = false;
    }

    virtual ~MyFunc()
    {
      destroyed = true;
    }

    virtual const char* GetName() const
    {
      return "MYFUNC";
    }

    virtual unsigned int GetCardinality() const
    {
      return 2;
    }

    virtual void Compute(SQLite::FunctionContext& context)
    {
      context.SetIntResult(1000 + context.GetIntValue(0) * context.GetIntValue(1));
    }
  };

  class MyDelete : public SQLite::IScalarFunction
  {
  public:
    std::set<int> deleted_;

    virtual const char* GetName() const
    {
      return "MYDELETE";
    }

    virtual unsigned int GetCardinality() const
    {
      return 1;
    }

    virtual void Compute(SQLite::FunctionContext& context)
    {
      deleted_.insert(context.GetIntValue(0));
      context.SetNullResult();
    }
  };
}

TEST(SQLite, ScalarFunction)
{
  {
    SQLite::Connection c;
    c.OpenInMemory();
    c.Register(new MyFunc());
    c.Execute("CREATE TABLE t(id INTEGER PRIMARY KEY, v1 INTEGER, v2 INTEGER);");
    c.Execute("INSERT INTO t VALUES(NULL, 2, 3);");
    c.Execute("INSERT INTO t VALUES(NULL, 4, 4);");
    c.Execute("INSERT INTO t VALUES(NULL, 6, 5);");
    SQLite::Statement t(c, "SELECT MYFUNC(v1, v2), v1, v2 FROM t");
    int i = 0;
    while (t.Step())
    {
      ASSERT_EQ(t.ColumnInt(0), 1000 + t.ColumnInt(1) * t.ColumnInt(2));
      i++;
    }
    ASSERT_EQ(3, i);
    ASSERT_FALSE(destroyed);
  }
  ASSERT_TRUE(destroyed);
}

TEST(SQLite, CascadedDeleteCallback)
{
  SQLite::Connection c;
  c.OpenInMemory();
  MyDelete *func = new MyDelete();
  c.Register(func);
  c.Execute("CREATE TABLE parent(id INTEGER PRIMARY KEY, dummy INTEGER);");
  c.Execute("CREATE TABLE child("
            "  id INTEGER PRIMARY KEY, "
            "  parent INTEGER REFERENCES parent(id) ON DELETE CASCADE, "
            "  value INTEGER);");
  c.Execute("CREATE TRIGGER childRemoved "
            "AFTER DELETE ON child "
            "FOR EACH ROW BEGIN "
            "  SELECT MYDELETE(old.value); "
            "END;");

  c.Execute("INSERT INTO parent VALUES(42, 100);");
  c.Execute("INSERT INTO parent VALUES(43, 101);");

  c.Execute("INSERT INTO child VALUES(NULL, 42, 4200);");
  c.Execute("INSERT INTO child VALUES(NULL, 42, 4201);");

  c.Execute("INSERT INTO child VALUES(NULL, 43, 4300);");
  c.Execute("INSERT INTO child VALUES(NULL, 43, 4301);");

  // The following command deletes "parent(43, 101)", then in turns
  // "child(NULL, 43, 4300/4301)", then calls the MyDelete on 4300 and
  // 4301
  c.Execute("DELETE FROM parent WHERE dummy=101");

  ASSERT_EQ(2u, func->deleted_.size());
  ASSERT_TRUE(func->deleted_.find(4300) != func->deleted_.end());
  ASSERT_TRUE(func->deleted_.find(4301) != func->deleted_.end());
}


TEST(SQLite, EmptyTransactions)
{
  try
  {
    SQLite::Connection c;
    c.OpenInMemory();

    c.Execute("CREATE TABLE a(id INTEGER PRIMARY KEY);");
    c.Execute("INSERT INTO a VALUES(NULL)");
      
    {
      SQLite::Transaction t(c);
      t.Begin();
      {
        SQLite::Statement s(c, SQLITE_FROM_HERE, "SELECT * FROM a");
        s.Step();
      }
      //t.Commit();
    }

    {
      SQLite::Statement s(c, SQLITE_FROM_HERE, "SELECT * FROM a");
      s.Step();
    }
  }
  catch (OrthancException& e)
  {
    fprintf(stderr, "Exception: [%s]\n", e.What());
    throw e;
  }
}


TEST(SQLite, Types)
{
  SQLite::Connection c;
  c.OpenInMemory();
  c.Execute("CREATE TABLE a(id INTEGER PRIMARY KEY, value)");

  {
    SQLite::Statement s(c, std::string("SELECT * FROM a"));
    ASSERT_EQ(2, s.ColumnCount());
    ASSERT_FALSE(s.Step());
  }

  {
    SQLite::Statement s(c, SQLITE_FROM_HERE, std::string("SELECT * FROM a"));
    ASSERT_FALSE(s.Step());
    ASSERT_EQ("SELECT * FROM a", s.GetOriginalSQLStatement());
  }

  {
    SQLite::Statement s(c, SQLITE_FROM_HERE, "INSERT INTO a VALUES(NULL, ?);");
    s.BindNull(0);             ASSERT_TRUE(s.Run()); s.Reset();
    s.BindBool(0, true);       ASSERT_TRUE(s.Run()); s.Reset();
    s.BindInt(0, 42);          ASSERT_TRUE(s.Run()); s.Reset();
    s.BindInt64(0, 42ll);      ASSERT_TRUE(s.Run()); s.Reset();
    s.BindDouble(0, 42.5);     ASSERT_TRUE(s.Run()); s.Reset();
    s.BindCString(0, "Hello"); ASSERT_TRUE(s.Run()); s.Reset();
    s.BindBlob(0, "Hello", 5); ASSERT_TRUE(s.Run()); s.Reset();
  }

  {
    SQLite::Statement s(c, SQLITE_FROM_HERE, std::string("SELECT * FROM a"));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ(SQLite::COLUMN_TYPE_NULL, s.GetColumnType(1));
    ASSERT_TRUE(s.ColumnIsNull(1));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ(SQLite::COLUMN_TYPE_INTEGER, s.GetColumnType(1));
    ASSERT_TRUE(s.ColumnBool(1));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ(SQLite::COLUMN_TYPE_INTEGER, s.GetColumnType(1));
    ASSERT_EQ(42, s.ColumnInt(1));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ(SQLite::COLUMN_TYPE_INTEGER, s.GetColumnType(1));
    ASSERT_EQ(42ll, s.ColumnInt64(1));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ(SQLite::COLUMN_TYPE_FLOAT, s.GetColumnType(1));
    ASSERT_DOUBLE_EQ(42.5, s.ColumnDouble(1));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ(SQLite::COLUMN_TYPE_TEXT, s.GetColumnType(1));
    ASSERT_EQ("Hello", s.ColumnString(1));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ(SQLite::COLUMN_TYPE_BLOB, s.GetColumnType(1));
    ASSERT_EQ(5, s.ColumnByteLength(1));
    ASSERT_TRUE(!memcmp("Hello", s.ColumnBlob(1), 5));

    std::string t;
    ASSERT_TRUE(s.ColumnBlobAsString(1, &t));
    ASSERT_EQ("Hello", t);

    ASSERT_FALSE(s.Step());
  }
}
