#include "gtest/gtest.h"

#include <memory>
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
  ASSERT_EQ("a", r.RemoveOldest(p)); ASSERT_EQ(420, p);
  ASSERT_EQ("d", r.RemoveOldest(p)); ASSERT_EQ(423, p);
  ASSERT_EQ("c", r.RemoveOldest(p)); ASSERT_EQ(422, p);

  ASSERT_TRUE(r.IsEmpty());
}


namespace Orthanc
{
  class MemoryCache
  {
  private:
    struct CacheElement
    {
      std::string id_;
      std::auto_ptr<IDynamicObject> object_;
    };

    //typedef std::map<CacheElement

    size_t places_;
    CacheIndex<const char*>  index_;

  public:
    
  };
}
