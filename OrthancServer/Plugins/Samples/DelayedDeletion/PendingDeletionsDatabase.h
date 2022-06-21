#pragma once


#include "../../../../OrthancFramework/Sources/SQLite/Connection.h"
#include <boost/thread/mutex.hpp>
#include <boost/noncopyable.hpp>

class PendingDeletionsDatabase : public boost::noncopyable
{
private:
  boost::mutex                 mutex_;
  Orthanc::SQLite::Connection  db_;

  void Setup();
  
public:
  PendingDeletionsDatabase(const std::string& path);

  void Enqueue(const std::string& uuid,
               Orthanc::FileContentType type);
  
  bool Dequeue(std::string& uuid,
               Orthanc::FileContentType& type);

  unsigned int GetSize();
};
