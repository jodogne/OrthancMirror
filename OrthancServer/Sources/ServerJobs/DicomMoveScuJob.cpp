/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "DicomMoveScuJob.h"

#include "../../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"
#include "../../../OrthancFramework/Sources/SerializationToolbox.h"
#include "../ServerContext.h"

static const char* const LOCAL_AET = "LocalAet";
static const char* const QUERY = "Query";
static const char* const QUERY_FORMAT = "QueryFormat";  // New in 1.9.5
static const char* const REMOTE = "Remote";
static const char* const TARGET_AET = "TargetAet";
static const char* const TIMEOUT = "Timeout";

namespace Orthanc
{



  void DicomMoveScuJob::Retrieve(const DicomMap& findAnswer)
  {
    if (connection_.get() == NULL)
    {
      connection_.reset(new DicomControlUserConnection(parameters_, ScuOperationFlags_Move));
    }
    
    connection_->SetProgressListener(this);
    connection_->Move(targetAet_, findAnswer);
  }


  
  void DicomMoveScuJob::SetTargetAet(const std::string& aet)
  {
    if (IsStarted())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      targetAet_ = aet;
    }
  }

  


  void DicomMoveScuJob::GetPublicContent(Json::Value& value) const
  {
    DicomRetrieveScuBaseJob::GetPublicContent(value);

    value[TARGET_AET] = targetAet_;
  }


  DicomMoveScuJob::DicomMoveScuJob(ServerContext& context,
                                   const Json::Value& serialized) :
    DicomRetrieveScuBaseJob(context, serialized),
    targetAet_(SerializationToolbox::ReadString(serialized, TARGET_AET))
  {
  }

  
  bool DicomMoveScuJob::Serialize(Json::Value& target) const
  {
    if (!DicomRetrieveScuBaseJob::Serialize(target))
    {
      return false;
    }
    else
    {
      target[TARGET_AET] = targetAet_;
      
      return true;
    }
  }
}
