/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include "../Sources/DataSource/DataSourceReader.h"
#include "../Sources/DataSource/DataSourceSequentialReader.h"
#include "../Sources/MultiThreading/SequentialExecutorService.h"
#include "../Sources/MultiThreading/ThreadPool.h"
#include "../Sources/OrthancException.h"
#include "../Sources/SystemToolbox.h"

#include <gtest/gtest.h>
#include <boost/lexical_cast.hpp>

using namespace Orthanc;


namespace
{
  class IntegerIdentifier : public IDataIdentifier
  {
  private:
    bool fails_;
    int  value_;
    int  sleep_;

  public:
    IntegerIdentifier(bool fails,
                      int value,
                      int sleep) :
      fails_(fails),
      value_(value),
      sleep_(sleep)
    {
    }

    bool IsFails() const
    {
      return fails_;
    }

    int GetValue() const
    {
      return value_;
    }

    IDynamicObject* Create() const
    {
      if (sleep_ > 0)
      {
        SystemToolbox::USleep(1000 * sleep_);
      }

      if (fails_)
      {
        throw OrthancException(ErrorCode_Database /* some random error code */,
                               "This was value " + boost::lexical_cast<std::string>(value_), false /* don't log */);
      }
      else
      {
        return new SingleValueObject<int>(value_);
      }
    }

    virtual bool GetCacheKey(std::string& key) const ORTHANC_OVERRIDE
    {
      return false;
    }

    virtual bool EstimateValueSize(size_t& target) const ORTHANC_OVERRIDE
    {
      return false;
    }

    virtual bool HasUserData() const ORTHANC_OVERRIDE
    {
      return false;
    }

    virtual const IDynamicObject& GetUserData() const ORTHANC_OVERRIDE
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    virtual IDynamicObject* ReleaseUserData() ORTHANC_OVERRIDE
    {
      throw OrthancException(ErrorCode_InternalError);
    }
  };


  class IntegerDataSource : public IDataSource
  {
  public:
    virtual IDynamicObject* Load(const IDataIdentifier& identifier,
                                 const boost::shared_ptr<SharedObjectCache>& readerCache /* could be NULL */) ORTHANC_OVERRIDE
    {
      const IntegerIdentifier& id = dynamic_cast<const IntegerIdentifier&>(identifier);
      return id.Create();
    }

    virtual size_t GetValueSize(const IDynamicObject& value) const ORTHANC_OVERRIDE
    {
      return 10;
    }
  };


  class IntegerDisconnector : public DataSourceSequentialReader::IValueDisconnector
  {
  public:
    virtual IDynamicObject* Apply(DataSourceAnswer::Item* source) ORTHANC_OVERRIDE
    {
      std::unique_ptr<DataSourceAnswer::Item> protection(source);

      const IntegerIdentifier& id = dynamic_cast<const IntegerIdentifier&>(source->GetId());
      const SingleValueObject<int>& value = dynamic_cast<const SingleValueObject<int>&>(*source->GetValue());

      if (id.GetValue() != value.GetValue())
      {
        throw OrthancException(ErrorCode_InternalError);
      }
      else
      {
        return new SingleValueObject<int>(value.GetValue());
      }
    }
  };
}


TEST(DataSource, ParallelReader)
{
  boost::shared_ptr<ThreadPool> service(new ThreadPool);
  service->SetThreadsCount(4);
  service->SetDequeueTimeout(5);  // Stop the test fast
  service->Start();

  DataSourceReader reader(service, new IntegerDataSource);

  {
    std::unique_ptr<DataSourceAnswer::Item> item(reader.ReadSingle(new IntegerIdentifier(false, 10, 0)));
    ASSERT_TRUE(item.get() != NULL);
    ASSERT_EQ(10u, dynamic_cast<const IntegerIdentifier&>(item->GetId()).GetValue());
    ASSERT_EQ(10u, dynamic_cast<const SingleValueObject<int>&>(*item->GetValue()).GetValue());
  }

  {
    std::unique_ptr<DataSourceAnswer::Item> item(reader.ReadSingle(new IntegerIdentifier(true, 20, 0)));
    ASSERT_TRUE(item.get() != NULL);
    ASSERT_EQ(20u, dynamic_cast<const IntegerIdentifier&>(item->GetId()).GetValue());

    bool hasThrown = false;

    try
    {
      dynamic_cast<const SingleValueObject<int>&>(*item->GetValue()).GetValue();
    }
    catch (OrthancException& e)
    {
      hasThrown = true;
      ASSERT_EQ(ErrorCode_Database, e.GetErrorCode());
      ASSERT_TRUE(e.HasDetails());
      ASSERT_EQ("This was value 20", std::string(e.GetDetails()));
    }

    ASSERT_TRUE(hasThrown);
  }

  {
    boost::shared_ptr<DataSourceAnswer> answer;

    std::unique_ptr<DataSourceRequest> request(new DataSourceRequest);
    for (int i = 0; i < 10; i++)
    {
      request->Enqueue(new IntegerIdentifier(false, 10 + i, 10 - i));  // Produces out-of-order values
    }

    answer = reader.Submit(request.release());

    std::set<int> values;

    while (true)
    {
      std::unique_ptr<DataSourceAnswer::Item> item(answer->Dequeue());
      if (item)
      {
        const IntegerIdentifier& id = dynamic_cast<const IntegerIdentifier&>(item->GetId());
        const SingleValueObject<int>& value = dynamic_cast<const SingleValueObject<int>&>(*item->GetValue());
        ASSERT_EQ(id.GetValue(), value.GetValue());
        values.insert(value.GetValue());
      }
      else
      {
        break;
      }
    }

    ASSERT_EQ(10u, values.size());

    for (int i = 0; i < 10; i++)
    {
      ASSERT_TRUE(values.find(10 + i) != values.end());
    }
  }

  {
    boost::shared_ptr<DataSourceAnswer> answer;

    std::unique_ptr<DataSourceRequest> request(new DataSourceRequest);
    unsigned int countValues = 0;

    for (int i = 0; i < 100; i++)
    {
      if (i % 3 == 1)
      {
        request->Enqueue(new IntegerIdentifier(true, i, 0));  // Produces exceptions
      }
      else
      {
        countValues++;
        request->Enqueue(new IntegerIdentifier(false, i, 0));
      }
    }

    answer = reader.Submit(request.release());

    std::set<int> values;

    while (true)
    {
      std::unique_ptr<DataSourceAnswer::Item> item(answer->Dequeue());
      if (item)
      {
        const IntegerIdentifier& id = dynamic_cast<const IntegerIdentifier&>(item->GetId());

        try
        {
          const SingleValueObject<int>& value = dynamic_cast<const SingleValueObject<int>&>(*item->GetValue());
          ASSERT_TRUE(id.GetValue() % 3 != 1);
          ASSERT_EQ(id.GetValue(), value.GetValue());
          values.insert(value.GetValue());
        }
        catch (const OrthancException& e)
        {
          ASSERT_TRUE(id.GetValue() % 3 == 1);
          ASSERT_EQ(ErrorCode_Database, e.GetErrorCode());
          ASSERT_TRUE(e.HasDetails());
          ASSERT_EQ("This was value " + boost::lexical_cast<std::string>(id.GetValue()), std::string(e.GetDetails()));
        }
      }
      else
      {
        break;
      }
    }

    ASSERT_EQ(countValues, values.size());

    for (int i = 0; i < 100; i++)
    {
      if (i % 3 != 1)
      {
        ASSERT_TRUE(values.find(i) != values.end());
      }
    }
  }
}


TEST(DataSource, SequentialReader)
{
  boost::shared_ptr<ThreadPool> serviceSource(new ThreadPool);
  serviceSource->SetLoggingThreadName("SOURCE");
  serviceSource->SetThreadsCount(4);
  serviceSource->SetDequeueTimeout(5);  // Stop the test fast
  serviceSource->Start();

  boost::shared_ptr<DataSourceReader> reader(new DataSourceReader(serviceSource, new IntegerDataSource));

  boost::shared_ptr<ThreadPool> serviceSequential(new ThreadPool);
  serviceSequential->SetLoggingThreadName("SEQ");
  serviceSequential->SetThreadsCount(1);
  serviceSequential->SetDequeueTimeout(5);  // Stop the test fast
  serviceSequential->Start();

  {
    DataSourceSequentialReader seq(serviceSequential, reader, new IntegerDisconnector, 4, 0);

    for (int i = 0; i < 10; i++)
    {
      seq.Submit(new IntegerIdentifier(false, 10 + i, 10 - i));  // Produces out-of-order values
    }

    seq.Start();
    ASSERT_THROW(seq.Submit(new IntegerIdentifier(false, 42, 0)), OrthancException);

    for (int i = 0; i < 10; i++)
    {
      ASSERT_TRUE(seq.HasNext());

      std::unique_ptr<DataSourceSequentialReader::Item> item(seq.Next());
      ASSERT_TRUE(item.get() != NULL);

      const SingleValueObject<int>& value = dynamic_cast<const SingleValueObject<int>&>(item->GetValue());
      ASSERT_EQ(10 + i, value.GetValue());
    }

    ASSERT_FALSE(seq.HasNext());
    ASSERT_THROW(seq.Next(), OrthancException);
  }

  {
    DataSourceSequentialReader seq(serviceSequential, reader, new IntegerDisconnector, 4, 0);

    for (int i = 0; i < 100; i++)
    {
      if (i % 3 == 1)
      {
        seq.Submit(new IntegerIdentifier(true, i, 0));  // Produces exceptions
      }
      else
      {
        seq.Submit(new IntegerIdentifier(false, i, 0));
      }
    }

    seq.Start();

    for (int i = 0; i < 100; i++)
    {
      ASSERT_TRUE(seq.HasNext());

      std::unique_ptr<DataSourceSequentialReader::Item> item(seq.Next());
      ASSERT_TRUE(item.get() != NULL);

      if (i % 3 == 1)
      {
        bool hasThrown = false;

        try
        {
          item->GetValue();
        }
        catch (const OrthancException& e)
        {
          hasThrown = true;
          ASSERT_EQ(ErrorCode_Database, e.GetErrorCode());
          ASSERT_TRUE(e.HasDetails());
          ASSERT_EQ("This was value " + boost::lexical_cast<std::string>(i), std::string(e.GetDetails()));
        }

        ASSERT_TRUE(hasThrown);
      }
      else
      {
        const SingleValueObject<int>& value = dynamic_cast<const SingleValueObject<int>&>(item->GetValue());
        ASSERT_EQ(i, value.GetValue());
      }
    }

    ASSERT_FALSE(seq.HasNext());
    ASSERT_THROW(seq.Next(), OrthancException);
  }
}
