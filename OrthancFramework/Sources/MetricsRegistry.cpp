/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "PrecompiledHeaders.h"
#include "MetricsRegistry.h"

#include "ChunkedBuffer.h"
#include "Compatibility.h"
#include "OrthancException.h"

namespace Orthanc
{
  static const boost::posix_time::ptime GetNow()
  {
    return boost::posix_time::microsec_clock::universal_time();
  }

  class MetricsRegistry::Item
  {
  private:
    MetricsType               type_;
    boost::posix_time::ptime  time_;
    bool                      hasValue_;
    int64_t                   value_;
    
    void SetValue(int64_t value,
                  const boost::posix_time::ptime& now)
    {
      hasValue_ = true;
      value_ = value;
      time_ = now;
    }

    bool IsLargerOverPeriod(int64_t value,
                            int duration,
                            const boost::posix_time::ptime& now) const
    {
      if (hasValue_)
      {
        return (value > value_ ||
                (now - time_).total_seconds() > duration /* old value has expired */);
      }
      else
      {
        return true;  // No value yet
      }
    }

    bool IsSmallerOverPeriod(int64_t value,
                             int duration,
                             const boost::posix_time::ptime& now) const
    {
      if (hasValue_)
      {
        return (value < value_ ||
                (now - time_).total_seconds() > duration /* old value has expired */);
      }
      else
      {
        return true;  // No value yet
      }
    }

  public:
    explicit Item(MetricsType type) :
      type_(type),
      hasValue_(false),
      value_(0)
    {
    }

    MetricsType GetType() const
    {
      return type_;
    }

    void Update(int64_t value)
    {
      const boost::posix_time::ptime now = GetNow();

      switch (type_)
      {
        case MetricsType_Default:
          SetValue(value, now);
          break;
          
        case MetricsType_MaxOver10Seconds:
          if (IsLargerOverPeriod(value, 10, now))
          {
            SetValue(value, now);
          }
          break;

        case MetricsType_MaxOver1Minute:
          if (IsLargerOverPeriod(value, 60, now))
          {
            SetValue(value, now);
          }
          break;

        case MetricsType_MinOver10Seconds:
          if (IsSmallerOverPeriod(value, 10, now))
          {
            SetValue(value, now);
          }
          break;

        case MetricsType_MinOver1Minute:
          if (IsSmallerOverPeriod(value, 60, now))
          {
            SetValue(value, now);
          }
          break;

        default:
          throw OrthancException(ErrorCode_NotImplemented);
      }
    }

    bool HasValue() const
    {
      return hasValue_;
    }

    const boost::posix_time::ptime& GetTime() const
    {
      if (hasValue_)
      {
        return time_;
      }
      else
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
    }

    int64_t GetValue() const
    {
      if (hasValue_)
      {
        return value_;
      }
      else
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
    }
  };


  MetricsRegistry::~MetricsRegistry()
  {
    for (Content::iterator it = content_.begin(); it != content_.end(); ++it)
    {
      assert(it->second != NULL);
      delete it->second;
    }
  }

  bool MetricsRegistry::IsEnabled() const
  {
    return enabled_;
  }


  void MetricsRegistry::SetEnabled(bool enabled)
  {
    boost::mutex::scoped_lock lock(mutex_);
    enabled_ = enabled;
  }


  void MetricsRegistry::Register(const std::string& name,
                                 MetricsType type)
  {
    boost::mutex::scoped_lock lock(mutex_);

    Content::iterator found = content_.find(name);

    if (found == content_.end())
    {
      content_[name] = new Item(type);
    }
    else
    {
      assert(found->second != NULL);

      // This metrics already exists: Only recreate it if there is a
      // mismatch in the type of metrics
      if (found->second->GetType() != type)
      {
        delete found->second;
        found->second = new Item(type);
      }
    }    
  }


  MetricsRegistry::Item& MetricsRegistry::GetItemInternal(const std::string& name,
                                                          MetricsType type)
  {
    Content::iterator found = content_.find(name);

    if (found == content_.end())
    {
      Item* item = new Item(type);
      content_[name] = item;
      return *item;
    }
    else
    {
      assert(found->second != NULL);
      return *found->second;
    }
  }

  MetricsRegistry::MetricsRegistry() :
    enabled_(true)
  {
  }


  void MetricsRegistry::SetValue(const std::string &name,
                                 int64_t value,
                                 MetricsType type)
  {
    // Inlining to avoid loosing time if metrics are disabled
    if (enabled_)
    {
      boost::mutex::scoped_lock lock(mutex_);
      GetItemInternal(name, type).Update(value);
    }
  }


  void MetricsRegistry::IncrementValue(const std::string &name,
                                       int64_t delta)
  {
    // Inlining to avoid loosing time if metrics are disabled
    if (enabled_)
    {
      boost::mutex::scoped_lock lock(mutex_);
      Item& item = GetItemInternal(name, MetricsType_Default);

      if (item.HasValue())
      {
        item.Update(item.GetValue() + delta);
      }
      else
      {
        item.Update(delta);
      }
    }
  }


  MetricsType MetricsRegistry::GetMetricsType(const std::string& name)
  {
    boost::mutex::scoped_lock lock(mutex_);

    Content::const_iterator found = content_.find(name);

    if (found == content_.end())
    {
      throw OrthancException(ErrorCode_InexistentItem);
    }
    else
    {
      assert(found->second != NULL);
      return found->second->GetType();
    }
  }


  void MetricsRegistry::ExportPrometheusText(std::string& s)
  {
    // https://www.boost.org/doc/libs/1_69_0/doc/html/date_time/examples.html#date_time.examples.seconds_since_epoch
    static const boost::posix_time::ptime EPOCH(boost::gregorian::date(1970, 1, 1));

    boost::mutex::scoped_lock lock(mutex_);

    s.clear();

    if (!enabled_)
    {
      return;
    }

    ChunkedBuffer buffer;

    for (Content::const_iterator it = content_.begin();
         it != content_.end(); ++it)
    {
      assert(it->second != NULL);

      if (it->second->HasValue())
      {
        boost::posix_time::time_duration diff = it->second->GetTime() - EPOCH;

        std::string line = (it->first + " " +
                            boost::lexical_cast<std::string>(it->second->GetValue()) + " " + 
                            boost::lexical_cast<std::string>(diff.total_milliseconds()) + "\n");

        buffer.AddChunk(line);
      }
    }

    buffer.Flatten(s);
  }


  MetricsRegistry::SharedMetrics::SharedMetrics(MetricsRegistry &registry,
                                                const std::string &name,
                                                MetricsType type) :
    registry_(registry),
    name_(name),
    value_(0)
  {
  }

  void MetricsRegistry::SharedMetrics::Add(int64_t delta)
  {
    boost::mutex::scoped_lock lock(mutex_);
    value_ += delta;
    registry_.SetValue(name_, value_);
  }


  MetricsRegistry::ActiveCounter::ActiveCounter(MetricsRegistry::SharedMetrics &metrics) :
    metrics_(metrics)
  {
    metrics_.Add(1);
  }

  MetricsRegistry::ActiveCounter::~ActiveCounter()
  {
    metrics_.Add(-1);
  }


  void  MetricsRegistry::Timer::Start()
  {
    if (registry_.IsEnabled())
    {
      active_ = true;
      start_ = GetNow();
    }
    else
    {
      active_ = false;
    }
  }


  MetricsRegistry::Timer::Timer(MetricsRegistry &registry,
                                const std::string &name) :
    registry_(registry),
    name_(name),
    type_(MetricsType_MaxOver10Seconds)
  {
    Start();
  }


  MetricsRegistry::Timer::Timer(MetricsRegistry &registry,
                                const std::string &name,
                                MetricsType type) :
    registry_(registry),
    name_(name),
    type_(type)
  {
    Start();
  }


  MetricsRegistry::Timer::~Timer()
  {
    if (active_)
    {
      boost::posix_time::time_duration diff = GetNow() - start_;
      registry_.SetValue(
            name_, static_cast<int64_t>(diff.total_milliseconds()), type_);
    }
  }
}
