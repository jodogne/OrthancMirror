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
    float                     value_;
    
    void Touch(float value,
               const boost::posix_time::ptime& now)
    {
      hasValue_ = true;
      value_ = value;
      time_ = now;
    }

    void Touch(float value)
    {
      Touch(value, GetNow());
    }

    void UpdateMax(float value,
                   int duration)
    {
      if (hasValue_)
      {
        const boost::posix_time::ptime now = GetNow();

        if (value > value_ ||
            (now - time_).total_seconds() > duration)
        {
          Touch(value, now);
        }
      }
      else
      {
        Touch(value);
      }
    }
    
    void UpdateMin(float value,
                   int duration)
    {
      if (hasValue_)
      {
        const boost::posix_time::ptime now = GetNow();
        
        if (value < value_ ||
            (now - time_).total_seconds() > duration)
        {
          Touch(value, now);
        }
      }
      else
      {
        Touch(value);
      }
    }

  public:
    Item(MetricsType type) :
    type_(type),
    hasValue_(false)
    {
    }

    MetricsType GetType() const
    {
      return type_;
    }

    void Update(float value)
    {
      switch (type_)
      {
        case MetricsType_Default:
          Touch(value);
          break;
          
        case MetricsType_MaxOver10Seconds:
          UpdateMax(value, 10);
          break;

        case MetricsType_MaxOver1Minute:
          UpdateMax(value, 60);
          break;

        case MetricsType_MinOver10Seconds:
          UpdateMin(value, 10);
          break;

        case MetricsType_MinOver1Minute:
          UpdateMin(value, 60);
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

    float GetValue() const
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


  void MetricsRegistry::SetValueInternal(const std::string& name,
                                         float value,
                                         MetricsType type)
  {
    boost::mutex::scoped_lock lock(mutex_);

    Content::iterator found = content_.find(name);

    if (found == content_.end())
    {
      std::unique_ptr<Item> item(new Item(type));
      item->Update(value);
      content_[name] = item.release();
    }
    else
    {
      assert(found->second != NULL);
      found->second->Update(value);
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


  void MetricsRegistry::SharedMetrics::Add(float delta)
  {
    boost::mutex::scoped_lock lock(mutex_);
    value_ += delta;
    registry_.SetValue(name_, value_);
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


  MetricsRegistry::Timer::~Timer()
  {
    if (active_)
    {   
      boost::posix_time::time_duration diff = GetNow() - start_;
      registry_.SetValue(
        name_, static_cast<float>(diff.total_milliseconds()), type_);
    }
  }
}
