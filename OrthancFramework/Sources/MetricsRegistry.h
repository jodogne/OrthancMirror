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
  enum MetricsUpdate
  {
    MetricsUpdate_Directly,
    MetricsUpdate_MaxOver10Seconds,
    MetricsUpdate_MaxOver1Minute,
    MetricsUpdate_MinOver10Seconds,
    MetricsUpdate_MinOver1Minute
  };
  
  class ORTHANC_PUBLIC MetricsRegistry : public boost::noncopyable
  {
  private:
    class Item;

    typedef std::map<std::string, Item*>   Content;

    bool          enabled_;
    boost::mutex  mutex_;
    Content       content_;

    // The mutex must be locked
    Item& GetItemInternal(const std::string& name,
                          MetricsUpdate update);

  public:
    MetricsRegistry();

    ~MetricsRegistry();

    bool IsEnabled() const;

    void SetEnabled(bool enabled);

    void Register(const std::string& name,
                  MetricsUpdate update);

    void SetValue(const std::string& name,
                  int64_t value,
                  MetricsUpdate update);
    
    void SetValue(const std::string& name,
                  int64_t value)
    {
      SetValue(name, value, MetricsUpdate_Directly);
    }

    void IncrementValue(const std::string& name,
                        int64_t delta);

    MetricsUpdate GetMetricsUpdate(const std::string& name);

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
                    MetricsUpdate update);

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
      MetricsUpdate             update_;
      bool                      active_;
      boost::posix_time::ptime  start_;

      void Start();

    public:
      Timer(MetricsRegistry& registry,
            const std::string& name);

      Timer(MetricsRegistry& registry,
            const std::string& name,
            MetricsUpdate update);

      ~Timer();
    };
  };
}
