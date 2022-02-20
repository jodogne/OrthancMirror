/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include "CleaningInstancesJob.h"

#include "../../../OrthancFramework/Sources/SerializationToolbox.h"
#include "../ServerContext.h"


namespace Orthanc
{
  bool CleaningInstancesJob::HandleTrailingStep()
  {
    if (!keepSource_)
    {
      const size_t n = GetInstancesCount();

      for (size_t i = 0; i < n; i++)
      {
        Json::Value tmp;
        context_.DeleteResource(tmp, GetInstance(i), ResourceType_Instance);
      }
    }

    return true;
  }

  
  void CleaningInstancesJob::SetKeepSource(bool keep)
  {
    if (IsStarted())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    keepSource_ = keep;
  }


  static const char* KEEP_SOURCE = "KeepSource";


  CleaningInstancesJob::CleaningInstancesJob(ServerContext& context,
                                             const Json::Value& serialized,
                                             bool defaultKeepSource) :
    SetOfInstancesJob(serialized),  // (*)
    context_(context)
  {
    if (!HasTrailingStep())
    {
      // Should have been set by (*)
      throw OrthancException(ErrorCode_InternalError);
    }

    if (serialized.isMember(KEEP_SOURCE))
    {
      keepSource_ = SerializationToolbox::ReadBoolean(serialized, KEEP_SOURCE);
    }
    else
    {
      keepSource_ = defaultKeepSource;
    }
  }

  
  bool CleaningInstancesJob::Serialize(Json::Value& target)
  {
    if (!SetOfInstancesJob::Serialize(target))
    {
      return false;
    }
    else
    {
      target[KEEP_SOURCE] = keepSource_;
      return true;
    }
  }


  void CleaningInstancesJob::Start()
  {
    if (!HasTrailingStep())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls,
                             "AddTrailingStep() should have been called before submitting the job");
    }

    SetOfInstancesJob::Start();
  }
}
