#include "PendingDeletionsDatabase.h"

#include "../../../../OrthancFramework/Sources/SQLite/Statement.h"
#include "../../../../OrthancFramework/Sources/SQLite/Transaction.h"

void PendingDeletionsDatabase::Setup()
{
  // Performance tuning of SQLite with PRAGMAs
  // http://www.sqlite.org/pragma.html
  db_.Execute("PRAGMA SYNCHRONOUS=NORMAL;");
  db_.Execute("PRAGMA JOURNAL_MODE=WAL;");
  db_.Execute("PRAGMA LOCKING_MODE=EXCLUSIVE;");
  db_.Execute("PRAGMA WAL_AUTOCHECKPOINT=1000;");

  {
    Orthanc::SQLite::Transaction t(db_);
    t.Begin();

    if (!db_.DoesTableExist("Pending"))
    {
      db_.Execute("CREATE TABLE Pending(uuid TEXT, type INTEGER)");
    }
    
    t.Commit();
  }
}
  

PendingDeletionsDatabase::PendingDeletionsDatabase(const std::string& path)
{
  db_.Open(path);
  Setup();
}
  

void PendingDeletionsDatabase::Enqueue(const std::string& uuid,
                                       Orthanc::FileContentType type)
{
  boost::mutex::scoped_lock lock(mutex_);

  Orthanc::SQLite::Transaction t(db_);
  t.Begin();

  {
    Orthanc::SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO Pending VALUES(?, ?)");
    s.BindString(0, uuid);
    s.BindInt(1, type);
    s.Run();
  }

  t.Commit();
}
  

bool PendingDeletionsDatabase::Dequeue(std::string& uuid,
                                       Orthanc::FileContentType& type)
{
  bool ok = false;
    
  boost::mutex::scoped_lock lock(mutex_);

  Orthanc::SQLite::Transaction t(db_);
  t.Begin();

  {
    Orthanc::SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT uuid, type FROM Pending LIMIT 1");

    if (s.Step())
    {
      uuid = s.ColumnString(0);
      type = static_cast<Orthanc::FileContentType>(s.ColumnInt(1));

      Orthanc::SQLite::Statement s(db_, SQLITE_FROM_HERE, "DELETE FROM Pending WHERE uuid=?");
      s.BindString(0, uuid);
      s.Run();
      
      ok = true;
    }
  }

  t.Commit();

  return ok;
}


unsigned int PendingDeletionsDatabase::GetSize()
{
  boost::mutex::scoped_lock lock(mutex_);

  unsigned int value = 0;
  
  Orthanc::SQLite::Transaction t(db_);
  t.Begin();

  {
    Orthanc::SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT COUNT(*) FROM Pending");

    if (s.Step())
    {
      int tmp = s.ColumnInt(0);
      if (tmp > 0)
      {
        value = static_cast<unsigned int>(tmp);
      }
    }
  }

  t.Commit();

  return value;
}
