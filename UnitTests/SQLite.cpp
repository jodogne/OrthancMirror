#include "gtest/gtest.h"

#include "../Core/Toolbox.h"
#include "../Core/SQLite/Connection.h"
#include "../Core/SQLite/Statement.h"

#include <sqlite3.h>

using namespace Palanthir;


TEST(SQLite, Configuration)
{
  ASSERT_EQ(1, sqlite3_threadsafe());
}


TEST(SQLite, Connection)
{
  Toolbox::RemoveFile("coucou");
  SQLite::Connection c;
  c.Open("coucou");
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
