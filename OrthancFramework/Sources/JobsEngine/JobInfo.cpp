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


#include "../PrecompiledHeaders.h"

#ifdef __EMSCRIPTEN__
/* 
Avoid this error:

.../boost/math/special_functions/round.hpp:118:12: warning: implicit conversion from 'std::__2::numeric_limits<long long>::type' (aka 'long long') to 'float' changes value from 9223372036854775807 to 9223372036854775808 [-Wimplicit-int-float-conversion]
.../boost/math/special_functions/round.hpp:125:11: note: in instantiation of function template specialization 'boost::math::llround<float, boost::math::policies::policy<boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy> >' requested here
.../orthanc/Core/JobsEngine/JobInfo.cpp:69:44: note: in instantiation of function template specialization 'boost::math::llround<float>' requested here

.../boost/math/special_functions/round.hpp:86:12: warning: implicit conversion from 'std::__2::numeric_limits<int>::type' (aka 'int') to 'float' changes value from 2147483647 to 2147483648 [-Wimplicit-int-float-conversion]
.../boost/math/special_functions/round.hpp:93:11: note: in instantiation of function template specialization 'boost::math::iround<float, boost::math::policies::policy<boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy, boost::math::policies::default_policy> >' requested here
.../orthanc/Core/JobsEngine/JobInfo.cpp:133:39: note: in instantiation of function template specialization 'boost::math::iround<float>' requested here
*/
#pragma GCC diagnostic ignored "-Wimplicit-int-float-conversion"
#endif 

#include "JobInfo.h"

#include "../OrthancException.h"

// This "include" is mandatory for Release builds using Linux Standard Base
#include <boost/math/special_functions/round.hpp>

namespace Orthanc
{
  JobInfo::JobInfo(const std::string& id,
                   int priority,
                   JobState state,
                   const JobStatus& status,
                   const boost::posix_time::ptime& creationTime,
                   const boost::posix_time::ptime& lastStateChangeTime,
                   const boost::posix_time::time_duration& runtime) :
    id_(id),
    priority_(priority),
    state_(state),
    timestamp_(boost::posix_time::microsec_clock::universal_time()),
    creationTime_(creationTime),
    lastStateChangeTime_(lastStateChangeTime),
    runtime_(runtime),
    hasEta_(false),
    status_(status)
  {
    if (state_ == JobState_Running)
    {
      float ms = static_cast<float>(runtime_.total_milliseconds());

      if (status_.GetProgress() > 0.01f &&
          ms > 0.01f)
      {
        float ratio = static_cast<float>(1.0 - status_.GetProgress());
        long long remaining = boost::math::llround(ratio * ms);
        eta_ = timestamp_ + boost::posix_time::milliseconds(remaining);
        hasEta_ = true;
      }
    }
  }


  JobInfo::JobInfo() :
    priority_(0),
    state_(JobState_Failure),
    timestamp_(boost::posix_time::microsec_clock::universal_time()),
    creationTime_(timestamp_),
    lastStateChangeTime_(timestamp_),
    runtime_(boost::posix_time::milliseconds(0)),
    hasEta_(false)
  {
  }


  bool JobInfo::HasCompletionTime() const
  {
    return (state_ == JobState_Success ||
            state_ == JobState_Failure);
  }


  const boost::posix_time::ptime& JobInfo::GetEstimatedTimeOfArrival() const
  {
    if (hasEta_)
    {
      return eta_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  const boost::posix_time::ptime& JobInfo::GetCompletionTime() const
  {
    if (HasCompletionTime())
    {
      return lastStateChangeTime_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  void JobInfo::Format(Json::Value& target) const
  {
    target = Json::objectValue;
    target["ID"] = id_;
    target["Priority"] = priority_;
    target["ErrorCode"] = static_cast<int>(status_.GetErrorCode());
    target["ErrorDescription"] = EnumerationToString(status_.GetErrorCode());
    target["State"] = EnumerationToString(state_);
    target["Timestamp"] = boost::posix_time::to_iso_string(timestamp_);
    target["CreationTime"] = boost::posix_time::to_iso_string(creationTime_);
    target["EffectiveRuntime"] = static_cast<double>(runtime_.total_milliseconds()) / 1000.0;
    target["Progress"] = boost::math::iround(status_.GetProgress() * 100.0f);

    target["Type"] = status_.GetJobType();
    target["Content"] = status_.GetPublicContent();

    if (HasEstimatedTimeOfArrival())
    {
      target["EstimatedTimeOfArrival"] = boost::posix_time::to_iso_string(GetEstimatedTimeOfArrival());
    }

    if (HasCompletionTime())
    {
      target["CompletionTime"] = boost::posix_time::to_iso_string(GetCompletionTime());
    }
  }
}
