/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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

#include <memory>
#include <algorithm>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>

#include "../Core/Cache/MemoryCache.h"
#include "../Core/Cache/MemoryStringCache.h"
#include "../Core/Cache/SharedArchive.h"
#include "../Core/IDynamicObject.h"
#include "../Core/Logging.h"
#include "../OrthancServer/StorageCommitmentReports.h"


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

TEST(LRU, GetAllKeys)
{
  Orthanc::LeastRecentlyUsedIndex<std::string, int> r;
  std::vector<std::string> keys;

  r.AddOrMakeMostRecent("a", 420);
  r.GetAllKeys(keys);

  ASSERT_EQ(1u, keys.size());
  ASSERT_EQ("a", keys[0]);

  r.AddOrMakeMostRecent("b", 421);
  r.GetAllKeys(keys);

  ASSERT_EQ(2u, keys.size());
  ASSERT_TRUE(std::find(keys.begin(), keys.end(),"a") != keys.end());
  ASSERT_TRUE(std::find(keys.begin(), keys.end(),"b") != keys.end());
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

    virtual ~Integer() ORTHANC_OVERRIDE
    {
      LOG(INFO) << "Removing cache entry for " << value_;
      log_ += boost::lexical_cast<std::string>(value_) + " ";
    }
  };

  class IntegerProvider : public Orthanc::Deprecated::ICachePageProvider
  {
  public:
    std::string log_;

    virtual Orthanc::IDynamicObject* Provide(const std::string& s) ORTHANC_OVERRIDE
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
    Orthanc::Deprecated::MemoryCache cache(provider, 3);
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





namespace
{
  class S : public Orthanc::IDynamicObject
  {
  private:
    std::string value_;

  public:
    S(const std::string& value) : value_(value)
    {
    }

    const std::string& GetValue() const
    {
      return value_;
    }
  };
}


TEST(LRU, SharedArchive)
{
  std::string first, second;
  Orthanc::SharedArchive a(3);
  first = a.Add(new S("First item"));
  second = a.Add(new S("Second item"));

  for (int i = 1; i < 100; i++)
  {
    a.Add(new S("Item " + boost::lexical_cast<std::string>(i)));
    
    // Continuously protect the two first items
    {
      Orthanc::SharedArchive::Accessor accessor(a, first);
      ASSERT_TRUE(accessor.IsValid());
      ASSERT_EQ("First item", dynamic_cast<S&>(accessor.GetItem()).GetValue());
    }

    {
      Orthanc::SharedArchive::Accessor accessor(a, second);
      ASSERT_TRUE(accessor.IsValid());
      ASSERT_EQ("Second item", dynamic_cast<S&>(accessor.GetItem()).GetValue());
    }

    {
      Orthanc::SharedArchive::Accessor accessor(a, "nope");
      ASSERT_FALSE(accessor.IsValid());
      ASSERT_THROW(accessor.GetItem(), Orthanc::OrthancException);
    }
  }

  std::list<std::string> i;
  a.List(i);

  size_t count = 0;
  for (std::list<std::string>::const_iterator
         it = i.begin(); it != i.end(); it++)
  {
    if (*it == first ||
        *it == second)
    {
      count++;
    }
  }

  ASSERT_EQ(2u, count);
}


TEST(MemoryStringCache, Basic)
{
  Orthanc::MemoryStringCache c;
  ASSERT_THROW(c.SetMaximumSize(0), Orthanc::OrthancException);
  
  c.SetMaximumSize(2);

  std::string v;
  ASSERT_FALSE(c.Fetch(v, "hello"));

  c.Add("hello", "a");
  ASSERT_TRUE(c.Fetch(v, "hello"));   ASSERT_EQ("a", v);
  ASSERT_FALSE(c.Fetch(v, "hello2"));
  ASSERT_FALSE(c.Fetch(v, "hello3"));

  c.Add("hello2", "b");
  ASSERT_TRUE(c.Fetch(v, "hello"));   ASSERT_EQ("a", v);
  ASSERT_TRUE(c.Fetch(v, "hello2"));  ASSERT_EQ("b", v);
  ASSERT_FALSE(c.Fetch(v, "hello3"));

  c.Add("hello3", "too large value");
  ASSERT_TRUE(c.Fetch(v, "hello"));   ASSERT_EQ("a", v);
  ASSERT_TRUE(c.Fetch(v, "hello2"));  ASSERT_EQ("b", v);
  ASSERT_FALSE(c.Fetch(v, "hello3"));
  
  c.Add("hello3", "c");
  ASSERT_FALSE(c.Fetch(v, "hello"));  // Recycled
  ASSERT_TRUE(c.Fetch(v, "hello2"));  ASSERT_EQ("b", v);
  ASSERT_TRUE(c.Fetch(v, "hello3"));  ASSERT_EQ("c", v);
}


TEST(MemoryStringCache, Invalidate)
{
  Orthanc::MemoryStringCache c;
  c.Add("hello", "a");
  c.Add("hello2", "b");

  std::string v;
  ASSERT_TRUE(c.Fetch(v, "hello"));   ASSERT_EQ("a", v);
  ASSERT_TRUE(c.Fetch(v, "hello2"));  ASSERT_EQ("b", v);

  c.Invalidate("hello");
  ASSERT_FALSE(c.Fetch(v, "hello"));
  ASSERT_TRUE(c.Fetch(v, "hello2"));  ASSERT_EQ("b", v);
}


TEST(StorageCommitmentReports, Basic)
{
  Orthanc::StorageCommitmentReports reports(2);
  ASSERT_EQ(2u, reports.GetMaxSize());

  {
    Orthanc::StorageCommitmentReports::Accessor accessor(reports, "nope");
    ASSERT_EQ("nope", accessor.GetTransactionUid());
    ASSERT_FALSE(accessor.IsValid());
    ASSERT_THROW(accessor.GetReport(), Orthanc::OrthancException);
  }

  reports.Store("a", new Orthanc::StorageCommitmentReports::Report("aet_a"));
  reports.Store("b", new Orthanc::StorageCommitmentReports::Report("aet_b"));
  reports.Store("c", new Orthanc::StorageCommitmentReports::Report("aet_c"));

  {
    Orthanc::StorageCommitmentReports::Accessor accessor(reports, "a");
    ASSERT_FALSE(accessor.IsValid());
  }

  {
    Orthanc::StorageCommitmentReports::Accessor accessor(reports, "b");
    ASSERT_TRUE(accessor.IsValid());
    ASSERT_EQ("aet_b", accessor.GetReport().GetRemoteAet());
    ASSERT_EQ(Orthanc::StorageCommitmentReports::Report::Status_Pending,
              accessor.GetReport().GetStatus());
  }

  {
    Orthanc::StorageCommitmentReports::Accessor accessor(reports, "c");
    ASSERT_EQ("aet_c", accessor.GetReport().GetRemoteAet());
    ASSERT_TRUE(accessor.IsValid());
  }

  {
    std::unique_ptr<Orthanc::StorageCommitmentReports::Report> report
      (new Orthanc::StorageCommitmentReports::Report("aet"));
    report->AddSuccess("class1", "instance1");
    report->AddFailure("class2", "instance2",
                       Orthanc::StorageCommitmentFailureReason_ReferencedSOPClassNotSupported);
    report->MarkAsComplete();
    reports.Store("a", report.release());
  }

  {
    Orthanc::StorageCommitmentReports::Accessor accessor(reports, "a");
    ASSERT_TRUE(accessor.IsValid());
    ASSERT_EQ("aet", accessor.GetReport().GetRemoteAet());
    ASSERT_EQ(Orthanc::StorageCommitmentReports::Report::Status_Failure,
              accessor.GetReport().GetStatus());
  }

  {
    Orthanc::StorageCommitmentReports::Accessor accessor(reports, "b");
    ASSERT_FALSE(accessor.IsValid());
  }

  {
    Orthanc::StorageCommitmentReports::Accessor accessor(reports, "c");
    ASSERT_TRUE(accessor.IsValid());
  }

  {
    std::unique_ptr<Orthanc::StorageCommitmentReports::Report> report
      (new Orthanc::StorageCommitmentReports::Report("aet"));
    report->AddSuccess("class1", "instance1");
    report->MarkAsComplete();
    reports.Store("a", report.release());
  }

  {
    Orthanc::StorageCommitmentReports::Accessor accessor(reports, "a");
    ASSERT_TRUE(accessor.IsValid());
    ASSERT_EQ("aet", accessor.GetReport().GetRemoteAet());
    ASSERT_EQ(Orthanc::StorageCommitmentReports::Report::Status_Success,
              accessor.GetReport().GetStatus());
  }

  {
    Orthanc::StorageCommitmentReports::Accessor accessor(reports, "b");
    ASSERT_FALSE(accessor.IsValid());
  }

  {
    Orthanc::StorageCommitmentReports::Accessor accessor(reports, "c");
    ASSERT_TRUE(accessor.IsValid());
  }
}
