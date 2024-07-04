/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2024 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2024 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include <boost/math/special_functions/round.hpp>

namespace Orthanc
{
  static const boost::posix_time::ptime GetNow()
  {
    return boost::posix_time::microsec_clock::universal_time();
  }

  namespace
  {
    template <typename T>
    class TimestampedValue : public boost::noncopyable
    {
    private:
      boost::posix_time::ptime  time_;
      bool                      hasValue_;
      T                         value_;

      void SetValue(const T& value,
                    const boost::posix_time::ptime& now)
      {
        hasValue_ = true;
        value_ = value;
        time_ = now;
      }

      bool IsLargerOverPeriod(const T& value,
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

      bool IsSmallerOverPeriod(const T& value,
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
      explicit TimestampedValue() :
        hasValue_(false),
        value_(0)
      {
      }

      void Update(const T& value,
                  const MetricsUpdatePolicy& policy)
      {
        const boost::posix_time::ptime now = GetNow();

        switch (policy)
        {
          case MetricsUpdatePolicy_Directly:
            SetValue(value, now);
            break;
          
          case MetricsUpdatePolicy_MaxOver10Seconds:
            if (IsLargerOverPeriod(value, 10, now))
            {
              SetValue(value, now);
            }
            break;

          case MetricsUpdatePolicy_MaxOver1Minute:
            if (IsLargerOverPeriod(value, 60, now))
            {
              SetValue(value, now);
            }
            break;

          case MetricsUpdatePolicy_MinOver10Seconds:
            if (IsSmallerOverPeriod(value, 10, now))
            {
              SetValue(value, now);
            }
            break;

          case MetricsUpdatePolicy_MinOver1Minute:
            if (IsSmallerOverPeriod(value, 60, now))
            {
              SetValue(value, now);
            }
            break;

          default:
            throw OrthancException(ErrorCode_NotImplemented);
        }
      }

      void Increment(const T& delta)
      {
        if (hasValue_)
        {
          value_ += delta;
        }
        else
        {
          value_ = delta;
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

      const T& GetValue() const
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
  }


  class MetricsRegistry::Item : public boost::noncopyable
  {
  private:
    MetricsUpdatePolicy   policy_;
    
  public:
    explicit Item(MetricsUpdatePolicy policy) :
      policy_(policy)
    {
    }
    
    virtual ~Item()
    {
    }

    MetricsUpdatePolicy GetPolicy() const
    {
      return policy_;
    }

    virtual void UpdateFloat(float value) = 0;

    virtual void UpdateInteger(int64_t value) = 0;

    virtual void IncrementInteger(int64_t delta) = 0;

    virtual MetricsDataType GetDataType() const = 0;

    virtual bool HasValue() const = 0;

    virtual const boost::posix_time::ptime& GetTime() const = 0;
    
    virtual std::string FormatValue() const = 0;
  };

  
  class MetricsRegistry::FloatItem : public Item
  {
  private:
    TimestampedValue<float>  value_;

  public:
    explicit FloatItem(MetricsUpdatePolicy policy) :
      Item(policy)
    {
    }
    
    virtual void UpdateFloat(float value) ORTHANC_OVERRIDE
    {
      value_.Update(value, GetPolicy());
    }

    virtual void UpdateInteger(int64_t value) ORTHANC_OVERRIDE
    {
      value_.Update(static_cast<float>(value), GetPolicy());
    }

    virtual void IncrementInteger(int64_t delta) ORTHANC_OVERRIDE
    {
      value_.Increment(static_cast<float>(delta));
    }

    virtual MetricsDataType GetDataType() const ORTHANC_OVERRIDE
    {
      return MetricsDataType_Float;
    }

    virtual bool HasValue() const ORTHANC_OVERRIDE
    {
      return value_.HasValue();
    }

    virtual const boost::posix_time::ptime& GetTime() const ORTHANC_OVERRIDE
    {
      return value_.GetTime();
    }
    
    virtual std::string FormatValue() const ORTHANC_OVERRIDE
    {
      return boost::lexical_cast<std::string>(value_.GetValue());
    }
  };

  
  class MetricsRegistry::IntegerItem : public Item
  {
  private:
    TimestampedValue<int64_t>  value_;

  public:
    explicit IntegerItem(MetricsUpdatePolicy policy) :
      Item(policy)
    {
    }
    
    virtual void UpdateFloat(float value) ORTHANC_OVERRIDE
    {
      value_.Update(boost::math::llround(value), GetPolicy());
    }

    virtual void UpdateInteger(int64_t value) ORTHANC_OVERRIDE
    {
      value_.Update(value, GetPolicy());
    }

    virtual void IncrementInteger(int64_t delta) ORTHANC_OVERRIDE
    {
      value_.Increment(delta);
    }

    virtual MetricsDataType GetDataType() const ORTHANC_OVERRIDE
    {
      return MetricsDataType_Integer;
    }

    virtual bool HasValue() const ORTHANC_OVERRIDE
    {
      return value_.HasValue();
    }

    virtual const boost::posix_time::ptime& GetTime() const ORTHANC_OVERRIDE
    {
      return value_.GetTime();
    }
    
    virtual std::string FormatValue() const ORTHANC_OVERRIDE
    {
      return boost::lexical_cast<std::string>(value_.GetValue());
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
                                 MetricsUpdatePolicy policy,
                                 MetricsDataType type)
  {
    boost::mutex::scoped_lock lock(mutex_);

    if (content_.find(name) != content_.end())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls, "Cannot register twice the same metrics: " + name);
    }
    else
    {
      GetItemInternal(name, policy, type);
    }
  }


  MetricsRegistry::Item& MetricsRegistry::GetItemInternal(const std::string& name,
                                                          MetricsUpdatePolicy policy,
                                                          MetricsDataType type)
  {
    Content::iterator found = content_.find(name);

    if (found == content_.end())
    {
      Item* item = NULL;
      
      switch (type)
      {
        case MetricsDataType_Float:
          item = new FloatItem(policy);
          break;

        case MetricsDataType_Integer:
          item = new IntegerItem(policy);
          break;

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }

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


  void MetricsRegistry::SetFloatValue(const std::string& name,
                                      float value,
                                      MetricsUpdatePolicy policy)
  {
    // Inlining to avoid loosing time if metrics are disabled
    if (enabled_)
    {
      boost::mutex::scoped_lock lock(mutex_);
      GetItemInternal(name, policy, MetricsDataType_Float).UpdateFloat(value);
    }
  }
  

  void MetricsRegistry::SetIntegerValue(const std::string &name,
                                        int64_t value,
                                        MetricsUpdatePolicy policy)
  {
    // Inlining to avoid loosing time if metrics are disabled
    if (enabled_)
    {
      boost::mutex::scoped_lock lock(mutex_);
      GetItemInternal(name, policy, MetricsDataType_Integer).UpdateInteger(value);
    }
  }


  void MetricsRegistry::IncrementIntegerValue(const std::string &name,
                                              int64_t delta)
  {
    // Inlining to avoid loosing time if metrics are disabled
    if (enabled_)
    {
      boost::mutex::scoped_lock lock(mutex_);
      GetItemInternal(name, MetricsUpdatePolicy_Directly, MetricsDataType_Integer).IncrementInteger(delta);
    }
  }


  MetricsUpdatePolicy MetricsRegistry::GetUpdatePolicy(const std::string& metrics)
  {
    boost::mutex::scoped_lock lock(mutex_);

    Content::const_iterator found = content_.find(metrics);

    if (found == content_.end())
    {
      throw OrthancException(ErrorCode_InexistentItem);
    }
    else
    {
      assert(found->second != NULL);
      return found->second->GetPolicy();
    }
  }


  MetricsDataType MetricsRegistry::GetDataType(const std::string& metrics)
  {
    boost::mutex::scoped_lock lock(mutex_);

    Content::const_iterator found = content_.find(metrics);

    if (found == content_.end())
    {
      throw OrthancException(ErrorCode_InexistentItem);
    }
    else
    {
      assert(found->second != NULL);
      return found->second->GetDataType();
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
                            it->second->FormatValue() + " " + 
                            boost::lexical_cast<std::string>(diff.total_milliseconds()) + "\n");

        buffer.AddChunk(line);
      }
    }

    buffer.Flatten(s);
  }


  MetricsRegistry::SharedMetrics::SharedMetrics(MetricsRegistry &registry,
                                                const std::string &name,
                                                MetricsUpdatePolicy policy) :
    registry_(registry),
    name_(name),
    value_(0)
  {
  }

  void MetricsRegistry::SharedMetrics::Add(int64_t delta)
  {
    boost::mutex::scoped_lock lock(mutex_);
    value_ += delta;
    registry_.SetIntegerValue(name_, value_);
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
    policy_(MetricsUpdatePolicy_MaxOver10Seconds)
  {
    Start();
  }


  MetricsRegistry::Timer::Timer(MetricsRegistry &registry,
                                const std::string &name,
                                MetricsUpdatePolicy policy) :
    registry_(registry),
    name_(name),
    policy_(policy)
  {
    Start();
  }


  MetricsRegistry::Timer::~Timer()
  {
    if (active_)
    {
      boost::posix_time::time_duration diff = GetNow() - start_;
      registry_.SetIntegerValue(name_, static_cast<int64_t>(diff.total_milliseconds()), policy_);
    }
  }
}
