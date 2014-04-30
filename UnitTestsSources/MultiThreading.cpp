#include "gtest/gtest.h"

#include <glog/logging.h>

#include "../Core/OrthancException.h"
#include "../Core/Toolbox.h"
#include "../Core/MultiThreading/ArrayFilledByThreads.h"
#include "../Core/MultiThreading/Locker.h"
#include "../Core/MultiThreading/Mutex.h"
#include "../Core/MultiThreading/ReaderWriterLock.h"
#include "../Core/MultiThreading/ThreadedCommandProcessor.h"

using namespace Orthanc;

namespace
{
  class DynamicInteger : public ICommand
  {
  private:
    int value_;
    std::set<int>& target_;

  public:
    DynamicInteger(int value, std::set<int>& target) : 
      value_(value), target_(target)
    {
    }

    int GetValue() const
    {
      return value_;
    }

    virtual bool Execute()
    {
      static boost::mutex mutex;
      boost::mutex::scoped_lock lock(mutex);
      target_.insert(value_);
      return true;
    }
  };

  class MyFiller : public ArrayFilledByThreads::IFiller
  {
  private:
    int size_;
    unsigned int created_;
    std::set<int> set_;

  public:
    MyFiller(int size) : size_(size), created_(0)
    {
    }

    virtual size_t GetFillerSize()
    {
      return size_;
    }

    virtual IDynamicObject* GetFillerItem(size_t index)
    {
      static boost::mutex mutex;
      boost::mutex::scoped_lock lock(mutex);
      created_++;
      return new DynamicInteger(index * 2, set_);
    }

    unsigned int GetCreatedCount() const
    {
      return created_;
    }

    std::set<int> GetSet()
    {
      return set_;
    }    
  };
}




TEST(MultiThreading, SharedMessageQueueBasic)
{
  std::set<int> s;

  SharedMessageQueue q;
  ASSERT_TRUE(q.WaitEmpty(0));
  q.Enqueue(new DynamicInteger(10, s));
  ASSERT_FALSE(q.WaitEmpty(1));
  q.Enqueue(new DynamicInteger(20, s));
  q.Enqueue(new DynamicInteger(30, s));
  q.Enqueue(new DynamicInteger(40, s));

  std::auto_ptr<DynamicInteger> i;
  i.reset(dynamic_cast<DynamicInteger*>(q.Dequeue(1))); ASSERT_EQ(10, i->GetValue());
  i.reset(dynamic_cast<DynamicInteger*>(q.Dequeue(1))); ASSERT_EQ(20, i->GetValue());
  i.reset(dynamic_cast<DynamicInteger*>(q.Dequeue(1))); ASSERT_EQ(30, i->GetValue());
  ASSERT_FALSE(q.WaitEmpty(1));
  i.reset(dynamic_cast<DynamicInteger*>(q.Dequeue(1))); ASSERT_EQ(40, i->GetValue());
  ASSERT_TRUE(q.WaitEmpty(0));
  ASSERT_EQ(NULL, q.Dequeue(1));
}


TEST(MultiThreading, SharedMessageQueueClean)
{
  std::set<int> s;

  try
  {
    SharedMessageQueue q;
    q.Enqueue(new DynamicInteger(10, s));
    q.Enqueue(new DynamicInteger(20, s));  
    throw OrthancException("Nope");
  }
  catch (OrthancException&)
  {
  }
}


TEST(MultiThreading, ArrayFilledByThreadEmpty)
{
  MyFiller f(0);
  ArrayFilledByThreads a(f);
  a.SetThreadCount(1);
  ASSERT_EQ(0, a.GetSize());
}


TEST(MultiThreading, ArrayFilledByThread1)
{
  MyFiller f(100);
  ArrayFilledByThreads a(f);
  a.SetThreadCount(1);
  ASSERT_EQ(100, a.GetSize());
  for (size_t i = 0; i < a.GetSize(); i++)
  {
    ASSERT_EQ(2 * i, dynamic_cast<DynamicInteger&>(a.GetItem(i)).GetValue());
  }
}


TEST(MultiThreading, ArrayFilledByThread4)
{
  MyFiller f(100);
  ArrayFilledByThreads a(f);
  a.SetThreadCount(4);
  ASSERT_EQ(100, a.GetSize());
  for (size_t i = 0; i < a.GetSize(); i++)
  {
    ASSERT_EQ(2 * i, dynamic_cast<DynamicInteger&>(a.GetItem(i)).GetValue());
  }

  ASSERT_EQ(100u, f.GetCreatedCount());

  a.Invalidate();

  ASSERT_EQ(100, a.GetSize());
  ASSERT_EQ(200u, f.GetCreatedCount());
  ASSERT_EQ(4u, a.GetThreadCount());
  ASSERT_TRUE(f.GetSet().empty());

  for (size_t i = 0; i < a.GetSize(); i++)
  {
    ASSERT_EQ(2 * i, dynamic_cast<DynamicInteger&>(a.GetItem(i)).GetValue());
  }
}


TEST(MultiThreading, CommandProcessor)
{
  ThreadedCommandProcessor p(4);

  std::set<int> s;

  for (size_t i = 0; i < 100; i++)
  {
    p.Post(new DynamicInteger(i * 2, s));
  }

  p.Join();

  for (size_t i = 0; i < 200; i++)
  {
    if (i % 2)
      ASSERT_TRUE(s.find(i) == s.end());
    else
      ASSERT_TRUE(s.find(i) != s.end());
  }
}


TEST(MultiThreading, Mutex)
{
  Mutex mutex;
  Locker locker(mutex);
}


TEST(MultiThreading, ReaderWriterLock)
{
  ReaderWriterLock lock;

  {
    Locker locker1(lock.ForReader());
    Locker locker2(lock.ForReader());
  }

  {
    Locker locker3(lock.ForWriter());
  }
}





#include "../OrthancServer/DicomProtocol/ReusableDicomUserConnection.h"

TEST(ReusableDicomUserConnection, DISABLED_Basic)
{
  ReusableDicomUserConnection c;
  c.SetMillisecondsBeforeClose(200);
  printf("START\n"); fflush(stdout);
  {
    ReusableDicomUserConnection::Connection cc(c, "STORESCP", "localhost", 2000, ModalityManufacturer_Generic);
    cc.GetConnection().StoreFile("/home/jodogne/DICOM/Cardiac/MR.X.1.2.276.0.7230010.3.1.4.2831157719.2256.1336386844.676281");
  }

  printf("**\n"); fflush(stdout);
  Toolbox::USleep(1000000);
  printf("**\n"); fflush(stdout);

  {
    ReusableDicomUserConnection::Connection cc(c, "STORESCP", "localhost", 2000, ModalityManufacturer_Generic);
    cc.GetConnection().StoreFile("/home/jodogne/DICOM/Cardiac/MR.X.1.2.276.0.7230010.3.1.4.2831157719.2256.1336386844.676277");
  }

  Toolbox::ServerBarrier();
  printf("DONE\n"); fflush(stdout);
}
