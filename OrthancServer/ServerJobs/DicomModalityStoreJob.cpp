/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
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


#include "../PrecompiledHeadersServer.h"
#include "DicomModalityStoreJob.h"

#include "../../Core/Logging.h"
#include "../../Core/SerializationToolbox.h"

namespace Orthanc
{
  void DicomModalityStoreJob::OpenConnection()
  {
    if (connection_.get() == NULL)
    {
      connection_.reset(new DicomUserConnection);
      connection_->SetLocalApplicationEntityTitle(localAet_);
      connection_->SetRemoteModality(remote_);
    }
  }


  bool DicomModalityStoreJob::HandleInstance(const std::string& instance)
  {
    assert(IsStarted());
    OpenConnection();

    LOG(INFO) << "Sending instance " << instance << " to modality \"" 
              << remote_.GetApplicationEntityTitle() << "\"";

    std::string dicom;

    try
    {
      context_.ReadDicom(dicom, instance);
    }
    catch (OrthancException& e)
    {
      LOG(WARNING) << "An instance was removed after the job was issued: " << instance;
      return false;
    }

    if (HasMoveOriginator())
    {
      connection_->Store(dicom, moveOriginatorAet_, moveOriginatorId_);
    }
    else
    {
      connection_->Store(dicom);
    }

    //boost::this_thread::sleep(boost::posix_time::milliseconds(500));

    return true;
  }
    

  DicomModalityStoreJob::DicomModalityStoreJob(ServerContext& context) :
    context_(context),
    localAet_("ORTHANC"),
    moveOriginatorId_(0)  // By default, not a C-MOVE
  {
  }


  void DicomModalityStoreJob::SetLocalAet(const std::string& aet)
  {
    if (IsStarted())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      localAet_ = aet;
    }
  }


  void DicomModalityStoreJob::SetRemoteModality(const RemoteModalityParameters& remote)
  {
    if (IsStarted())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      remote_ = remote;
    }
  }

    
  const std::string& DicomModalityStoreJob::GetMoveOriginatorAet() const
  {
    if (HasMoveOriginator())
    {
      return moveOriginatorAet_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }

    
  uint16_t DicomModalityStoreJob::GetMoveOriginatorId() const
  {
    if (HasMoveOriginator())
    {
      return moveOriginatorId_;
    }
    else
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
  }


  void DicomModalityStoreJob::SetMoveOriginator(const std::string& aet,
                                                int id)
  {
    if (IsStarted())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else if (id < 0 || 
             id >= 65536)
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
    else
    {
      moveOriginatorId_ = static_cast<uint16_t>(id);
      moveOriginatorAet_ = aet;
    }
  }

  void DicomModalityStoreJob::ReleaseResources()   // For pausing jobs
  {
    connection_.reset(NULL);
  }


  void DicomModalityStoreJob::GetPublicContent(Json::Value& value)
  {
    SetOfInstancesJob::GetPublicContent(value);
    
    value["LocalAet"] = localAet_;
    value["RemoteAet"] = remote_.GetApplicationEntityTitle();

    if (HasMoveOriginator())
    {
      value["MoveOriginatorAET"] = GetMoveOriginatorAet();
      value["MoveOriginatorID"] = GetMoveOriginatorId();
    }
  }


  static const char* LOCAL_AET = "LocalAet";
  static const char* REMOTE = "Remote";
  static const char* MOVE_ORIGINATOR_AET = "MoveOriginatorAet";
  static const char* MOVE_ORIGINATOR_ID = "MoveOriginatorId";
  

  DicomModalityStoreJob::DicomModalityStoreJob(ServerContext& context,
                                               const Json::Value& serialized) :
    SetOfInstancesJob(serialized),
    context_(context)
  {
    localAet_ = SerializationToolbox::ReadString(serialized, LOCAL_AET);
    remote_ = RemoteModalityParameters(serialized[REMOTE]);
    moveOriginatorAet_ = SerializationToolbox::ReadString(serialized, MOVE_ORIGINATOR_AET);
    moveOriginatorId_ = static_cast<uint16_t>
      (SerializationToolbox::ReadUnsignedInteger(serialized, MOVE_ORIGINATOR_ID));
  }


  bool DicomModalityStoreJob::Serialize(Json::Value& target)
  {
    if (!SetOfInstancesJob::Serialize(target))
    {
      return false;
    }
    else
    {
      target[LOCAL_AET] = localAet_;
      remote_.Serialize(target[REMOTE]);
      target[MOVE_ORIGINATOR_AET] = moveOriginatorAet_;
      target[MOVE_ORIGINATOR_ID] = moveOriginatorId_;
      return true;
    }
  }  
}
