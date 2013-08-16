#include "gtest/gtest.h"

#include <glog/logging.h>
#include <memory>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>
#include "../Core/IDynamicObject.h"
#include "../Core/Cache/MemoryCache.h"


TEST(LRU, Basic)
{
  Orthanc::LeastRecentlyUsedIndex<std::string> r;
  
  r.Add("d");
  r.Add("a");
  r.Add("c");
  r.Add("b");

  r.MakeMostRecent("a");
  r.MakeMostRecent("d");
  r.MakeMostRecent("b");
  r.MakeMostRecent("c");
  r.MakeMostRecent("d");
  r.MakeMostRecent("c");

  ASSERT_EQ("a", r.GetOldest());
  ASSERT_EQ("a", r.RemoveOldest());
  ASSERT_EQ("b", r.GetOldest());
  ASSERT_EQ("b", r.RemoveOldest());
  ASSERT_EQ("d", r.GetOldest());
  ASSERT_EQ("d", r.RemoveOldest());
  ASSERT_EQ("c", r.GetOldest());
  ASSERT_EQ("c", r.RemoveOldest());

  ASSERT_TRUE(r.IsEmpty());

  ASSERT_THROW(r.GetOldest(), Orthanc::OrthancException);
  ASSERT_THROW(r.RemoveOldest(), Orthanc::OrthancException);
}


TEST(LRU, Payload)
{
  Orthanc::LeastRecentlyUsedIndex<std::string, int> r;
  
  r.Add("a", 420);
  r.Add("b", 421);
  r.Add("c", 422);
  r.Add("d", 423);

  r.MakeMostRecent("a");
  r.MakeMostRecent("d");
  r.MakeMostRecent("b");
  r.MakeMostRecent("c");
  r.MakeMostRecent("d");
  r.MakeMostRecent("c");

  ASSERT_TRUE(r.Contains("b"));
  ASSERT_EQ(421, r.Invalidate("b"));
  ASSERT_FALSE(r.Contains("b"));

  int p;
  ASSERT_TRUE(r.Contains("a", p)); ASSERT_EQ(420, p);
  ASSERT_TRUE(r.Contains("c", p)); ASSERT_EQ(422, p);
  ASSERT_TRUE(r.Contains("d", p)); ASSERT_EQ(423, p);

  ASSERT_EQ("a", r.GetOldest());
  ASSERT_EQ(420, r.GetOldestPayload());
  ASSERT_EQ("a", r.RemoveOldest(p)); ASSERT_EQ(420, p);

  ASSERT_EQ("d", r.GetOldest());
  ASSERT_EQ(423, r.GetOldestPayload());
  ASSERT_EQ("d", r.RemoveOldest(p)); ASSERT_EQ(423, p);

  ASSERT_EQ("c", r.GetOldest());
  ASSERT_EQ(422, r.GetOldestPayload());
  ASSERT_EQ("c", r.RemoveOldest(p)); ASSERT_EQ(422, p);

  ASSERT_TRUE(r.IsEmpty());
}


TEST(LRU, PayloadUpdate)
{
  Orthanc::LeastRecentlyUsedIndex<std::string, int> r;
  
  r.Add("a", 420);
  r.Add("b", 421);
  r.Add("d", 423);

  r.MakeMostRecent("a", 424);
  r.MakeMostRecent("d", 421);

  ASSERT_EQ("b", r.GetOldest());
  ASSERT_EQ(421, r.GetOldestPayload());
  r.RemoveOldest();

  ASSERT_EQ("a", r.GetOldest());
  ASSERT_EQ(424, r.GetOldestPayload());
  r.RemoveOldest();

  ASSERT_EQ("d", r.GetOldest());
  ASSERT_EQ(421, r.GetOldestPayload());
  r.RemoveOldest();

  ASSERT_TRUE(r.IsEmpty());
}



TEST(LRU, PayloadUpdateBis)
{
  Orthanc::LeastRecentlyUsedIndex<std::string, int> r;
  
  r.AddOrMakeMostRecent("a", 420);
  r.AddOrMakeMostRecent("b", 421);
  r.AddOrMakeMostRecent("d", 423);
  r.AddOrMakeMostRecent("a", 424);
  r.AddOrMakeMostRecent("d", 421);

  ASSERT_EQ("b", r.GetOldest());
  ASSERT_EQ(421, r.GetOldestPayload());
  r.RemoveOldest();

  ASSERT_EQ("a", r.GetOldest());
  ASSERT_EQ(424, r.GetOldestPayload());
  r.RemoveOldest();

  ASSERT_EQ("d", r.GetOldest());
  ASSERT_EQ(421, r.GetOldestPayload());
  r.RemoveOldest();

  ASSERT_TRUE(r.IsEmpty());
}




namespace
{
  class Integer : public Orthanc::IDynamicObject
  {
  private:
    std::string& log_;
    int value_;

  public:
    Integer(std::string& log, int v) : log_(log), value_(v)
    {
    }

    virtual ~Integer()
    {
      LOG(INFO) << "Removing cache entry for " << value_;
      log_ += boost::lexical_cast<std::string>(value_) + " ";
    }

    int GetValue() const 
    {
      return value_;
    }
  };

  class IntegerProvider : public Orthanc::ICachePageProvider
  {
  public:
    std::string log_;

    Orthanc::IDynamicObject* Provide(const std::string& s)
    {
      LOG(INFO) << "Providing " << s;
      return new Integer(log_, boost::lexical_cast<int>(s));
    }
  };
}


TEST(MemoryCache, Basic)
{
  IntegerProvider provider;

  {
    Orthanc::MemoryCache cache(provider, 3);
    cache.Access("42");  // 42 -> exit
    cache.Access("43");  // 43, 42 -> exit
    cache.Access("45");  // 45, 43, 42 -> exit
    cache.Access("42");  // 42, 45, 43 -> exit
    cache.Access("43");  // 43, 42, 45 -> exit
    cache.Access("47");  // 45 is removed; 47, 43, 42 -> exit 
    cache.Access("44");  // 42 is removed; 44, 47, 43 -> exit
    cache.Access("42");  // 43 is removed; 42, 44, 47 -> exit
    // Closing the cache: 47, 44, 42 are successively removed
  }

  ASSERT_EQ("45 42 43 47 44 42 ", provider.log_);
}


#include "../OrthancServer/ServerEnumerations.h"

namespace
{
  struct Payload
  {
    Orthanc::ResourceType type_;
    boost::posix_time::ptime time_;

    Payload() : type_(Orthanc::ResourceType_Instance)
    {
    }

    Payload(Orthanc::ResourceType type) : type_(type)
    {
      time_ = boost::posix_time::second_clock::local_time();
    }

    unsigned int GetAge() const
    {
      return (boost::posix_time::second_clock::local_time() - time_).total_seconds();
    }
  };


  class StableResourcesMonitor
  {
  private:
    bool done_;
    boost::mutex mutex_;
    unsigned int stableTimeout_;
    Orthanc::LeastRecentlyUsedIndex<std::string, Payload>  unstableResources_;
    boost::thread thread_;

    static void Thread(StableResourcesMonitor* that)
    {
      static const unsigned int SLEEP = 1;  // Check for stable resources each second

      while (!that->done_)
      {
        boost::this_thread::sleep(boost::posix_time::seconds(SLEEP));

        boost::mutex::scoped_lock lock(that->mutex_);
        while (!that->unstableResources_.IsEmpty() &&
               that->unstableResources_.GetOldestPayload().GetAge() > that->stableTimeout_)
        {
          // This DICOM resource has not received any new instance for
          // some time. It can be considered as stable.
          
          Payload payload;
          std::string id = that->unstableResources_.RemoveOldest(payload);

          LOG(INFO) << "Stable resource: " << id << " (type " << payload.type_ << ")";
        }
      }

      LOG(INFO) << "Closing the monitor for stable resources";
    }

  public:
    StableResourcesMonitor(unsigned int stableTimeout)
    {
      done_ = false;
      stableTimeout_ = stableTimeout;
      thread_ = boost::thread(Thread, this);
    }

    ~StableResourcesMonitor()
    {
      done_ = true;

      if (thread_.joinable())
      {
        thread_.join();
      }
    }

    void ResourceUpdated(const std::string& id,
                         Orthanc::ResourceType type)
    {
      assert(type == Orthanc::ResourceType_Patient ||
             type == Orthanc::ResourceType_Study ||
             type == Orthanc::ResourceType_Series);

      boost::mutex::scoped_lock lock(mutex_);
      unstableResources_.AddOrMakeMostRecent(id, type);
    }
  };
}

TEST(LRU, Hello)
{
  StableResourcesMonitor m(5);
  boost::this_thread::sleep(boost::posix_time::seconds(1));
  m.ResourceUpdated("Hello", Orthanc::ResourceType_Study);
  m.ResourceUpdated("World", Orthanc::ResourceType_Series);
  boost::this_thread::sleep(boost::posix_time::seconds(2));
  m.ResourceUpdated("Hello", Orthanc::ResourceType_Study);
  boost::this_thread::sleep(boost::posix_time::seconds(10));
}
