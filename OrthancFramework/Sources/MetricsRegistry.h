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


#pragma once

#include "OrthancFramework.h"

#if !defined(ORTHANC_SANDBOXED)
#  error The macro ORTHANC_SANDBOXED must be defined
#endif

#if ORTHANC_SANDBOXED == 1
#  error The class MetricsRegistry cannot be used in sandboxed environments
#endif

#include <boost/thread/mutex.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <stdint.h>

namespace Orthanc
{
  enum MetricsUpdatePolicy
  {
    MetricsUpdatePolicy_Directly,
    MetricsUpdatePolicy_MaxOver10Seconds,
    MetricsUpdatePolicy_MaxOver1Minute,
    MetricsUpdatePolicy_MinOver10Seconds,
    MetricsUpdatePolicy_MinOver1Minute
  };
  
  enum MetricsDataType
  {
    MetricsDataType_Float,
    MetricsDataType_Integer
  };
  
  class ORTHANC_PUBLIC MetricsRegistry : public boost::noncopyable
  {
  private:
    class Item;
    class FloatItem;
    class IntegerItem;

    typedef std::map<std::string, Item*>   Content;

    bool          enabled_;
    boost::mutex  mutex_;
    Content       content_;

    // The mutex must be locked
    Item& GetItemInternal(const std::string& name,
                          MetricsUpdatePolicy policy,
                          MetricsDataType type);

  public:
    MetricsRegistry();

    ~MetricsRegistry();

    bool IsEnabled() const;

    void SetEnabled(bool enabled);

    void Register(const std::string& name,
                  MetricsUpdatePolicy policy,
                  MetricsDataType type);

    void SetFloatValue(const std::string& name,
                       float value,
                       MetricsUpdatePolicy policy /* only used if this is a new metrics */);
    
    void SetFloatValue(const std::string& name,
                       float value)
    {
      SetFloatValue(name, value, MetricsUpdatePolicy_Directly);
    }
    
    void SetIntegerValue(const std::string& name,
                         int64_t value,
                         MetricsUpdatePolicy policy /* only used if this is a new metrics */);
    
    void SetIntegerValue(const std::string& name,
                         int64_t value)
    {
      SetIntegerValue(name, value, MetricsUpdatePolicy_Directly);
    }
    
    void IncrementIntegerValue(const std::string& name,
                               int64_t delta);

    MetricsUpdatePolicy GetUpdatePolicy(const std::string& metrics);

    MetricsDataType GetDataType(const std::string& metrics);

    // https://prometheus.io/docs/instrumenting/exposition_formats/#text-based-format
    void ExportPrometheusText(std::string& s);


    class ORTHANC_PUBLIC SharedMetrics : public boost::noncopyable
    {
    private:
      boost::mutex      mutex_;
      MetricsRegistry&  registry_;
      std::string       name_;
      int64_t           value_;

    public:
      SharedMetrics(MetricsRegistry& registry,
                    const std::string& name,
                    MetricsUpdatePolicy policy);

      void Add(int64_t delta);
    };


    class ORTHANC_PUBLIC ActiveCounter : public boost::noncopyable
    {
    private:
      SharedMetrics&   metrics_;

    public:
      explicit ActiveCounter(SharedMetrics& metrics);

      ~ActiveCounter();
    };


    class ORTHANC_PUBLIC Timer : public boost::noncopyable
    {
    private:
      MetricsRegistry&          registry_;
      std::string               name_;
      MetricsUpdatePolicy       policy_;
      bool                      active_;
      boost::posix_time::ptime  start_;

      void Start();

    public:
      Timer(MetricsRegistry& registry,
            const std::string& name);

      Timer(MetricsRegistry& registry,
            const std::string& name,
            MetricsUpdatePolicy policy);

      ~Timer();
    };
  };
}
