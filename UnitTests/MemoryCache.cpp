#include "gtest/gtest.h"

#include <glog/logging.h>
#include <memory>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>
#include "../Core/IDynamicObject.h"
#include "../Core/MultiThreading/CacheIndex.h"


TEST(CacheIndex, Basic)
{
  Orthanc::CacheIndex<std::string> r;
  
  r.Add("d");
  r.Add("a");
  r.Add("c");
  r.Add("b");

  r.TagAsMostRecent("a");
  r.TagAsMostRecent("d");
  r.TagAsMostRecent("b");
  r.TagAsMostRecent("c");
  r.TagAsMostRecent("d");
  r.TagAsMostRecent("c");

  ASSERT_EQ("a", r.RemoveOldest());
  ASSERT_EQ("b", r.RemoveOldest());
  ASSERT_EQ("d", r.RemoveOldest());
  ASSERT_EQ("c", r.RemoveOldest());

  ASSERT_TRUE(r.IsEmpty());
}


TEST(CacheIndex, Payload)
{
  Orthanc::CacheIndex<std::string, int> r;
  
  r.Add("a", 420);
  r.Add("b", 421);
  r.Add("c", 422);
  r.Add("d", 423);

  r.TagAsMostRecent("a");
  r.TagAsMostRecent("d");
  r.TagAsMostRecent("b");
  r.TagAsMostRecent("c");
  r.TagAsMostRecent("d");
  r.TagAsMostRecent("c");

  ASSERT_TRUE(r.Contains("b"));
  ASSERT_EQ(421, r.Invalidate("b"));
  ASSERT_FALSE(r.Contains("b"));

  int p;
  ASSERT_TRUE(r.Contains("a", p)); ASSERT_EQ(420, p);
  ASSERT_TRUE(r.Contains("c", p)); ASSERT_EQ(422, p);
  ASSERT_TRUE(r.Contains("d", p)); ASSERT_EQ(423, p);

  ASSERT_EQ("a", r.RemoveOldest(p)); ASSERT_EQ(420, p);
  ASSERT_EQ("d", r.RemoveOldest(p)); ASSERT_EQ(423, p);
  ASSERT_EQ("c", r.RemoveOldest(p)); ASSERT_EQ(422, p);

  ASSERT_TRUE(r.IsEmpty());
}


namespace Orthanc
{

  class ICacheProvider
  {
  public:
    virtual ~ICacheProvider()
    {
    }

    virtual IDynamicObject* Provide(const std::string& id) = 0;
  };

  class MemoryCache
  {
  private:
    struct Page
    {
      std::string id_;
      std::auto_ptr<IDynamicObject> content_;
    };

    ICacheProvider& provider_;
    size_t cacheSize_;
    CacheIndex<std::string, Page*>  index_;

    Page& Load(const std::string& id)
    {
      // Reuse the cache entry if it already exists
      Page* p = NULL;
      if (index_.Contains(id, p))
      {
        assert(p != NULL);
        index_.TagAsMostRecent(id);
        return *p;
      }

      // The id is not in the cache yet. Make some room if the cache
      // is full.
      if (index_.GetSize() == cacheSize_)
      {
        index_.RemoveOldest(p);
        delete p;
      }

      // Create a new cache page
      std::auto_ptr<Page> result(new Page);
      result->id_ = id;
      result->content_.reset(provider_.Provide(id));

      // Add the newly create page to the cache
      p = result.release();
      index_.Add(id, p);
      return *p;
    }

  public:
    class Accessor
    {
      friend class MemoryCache;

    private:
      Page& element_;

      Accessor(Page& element) : 
        element_(element)
      {
      }

    public:
      const std::string GetId() const
      {
        return element_.id_;
      }

      IDynamicObject& GetContent()
      {
        return *element_.content_;
      }

      const IDynamicObject& GetContent() const
      {
        return *element_.content_;
      }
    };

    MemoryCache(ICacheProvider& provider,
                size_t cacheSize) : 
      provider_(provider),
      cacheSize_(cacheSize)
    {
    }

    ~MemoryCache()
    {
      while (!index_.IsEmpty())
      {
        Page* element = NULL;
        index_.RemoveOldest(element);
        assert(element != NULL);
        delete element;
      }
    }

    Accessor* Access(const std::string& id)
    {
      Page& element = Load(id);
      return new Accessor(element);
    }
  };
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

  class IntegerProvider : public Orthanc::ICacheProvider
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
    std::auto_ptr<Orthanc::MemoryCache::Accessor> a;
    a.reset(cache.Access("42"));  // 42 -> exit
    a.reset(cache.Access("43"));  // 43, 42 -> exit
    a.reset(cache.Access("45"));  // 45, 43, 42 -> exit
    a.reset(cache.Access("42"));  // 42, 45, 43 -> exit
    a.reset(cache.Access("43"));  // 43, 42, 45 -> exit
    a.reset(cache.Access("47"));  // 45 is removed; 47, 43, 42 -> exit 
    a.reset(cache.Access("44"));  // 42 is removed; 44, 47, 43 -> exit
    a.reset(cache.Access("42"));  // 43 is removed; 42, 44, 47 -> exit
    // Closing the cache: 47, 44, 42 are successively removed
  }

  ASSERT_EQ("45 42 43 47 44 42 ", provider.log_);
}
