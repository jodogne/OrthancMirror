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


#include "DicomMoveScuJob.h"

#include "../../Core/SerializationToolbox.h"
#include "../ServerContext.h"

static const char* const LOCAL_AET = "LocalAet";
static const char* const TARGET_AET = "TargetAet";
static const char* const REMOTE = "Remote";
static const char* const QUERY = "Query";

namespace Orthanc
{
  class DicomMoveScuJob::Command : public SetOfCommandsJob::ICommand
  {
  private:
    DicomMoveScuJob&           that_;
    std::unique_ptr<DicomMap>  findAnswer_;

  public:
    Command(DicomMoveScuJob& that,
            const DicomMap&  findAnswer) :
      that_(that),
      findAnswer_(findAnswer.Clone())
    {
    }

    virtual bool Execute(const std::string& jobId) ORTHANC_OVERRIDE
    {
      that_.Retrieve(*findAnswer_);
      return true;
    }

    virtual void Serialize(Json::Value& target) const ORTHANC_OVERRIDE
    {
      findAnswer_->Serialize(target);
    }
  };


  class DicomMoveScuJob::Unserializer :
    public SetOfCommandsJob::ICommandUnserializer
  {
  private:
    DicomMoveScuJob&   that_;

  public:
    Unserializer(DicomMoveScuJob&  that) :
      that_(that)
    {
    }

    virtual ICommand* Unserialize(const Json::Value& source) const
    {
      DicomMap findAnswer;
      findAnswer.Unserialize(source);
      return new Command(that_, findAnswer);
    }
  };



  void DicomMoveScuJob::Retrieve(const DicomMap& findAnswer)
  {
    if (connection_.get() == NULL)
    {
      connection_.reset(new DicomUserConnection(localAet_, remote_));
      connection_->Open();
    }
    
    connection_->Move(targetAet_, findAnswer);
  }


  static void AddTagIfString(Json::Value& target,
                             const DicomMap& answer,
                             const DicomTag& tag)
  {
    const DicomValue* value = answer.TestAndGetValue(tag);
    if (value != NULL &&
        !value->IsNull() &&
        !value->IsBinary())
    {
      target[tag.Format()] = value->GetContent();
    }
  }
  

  void DicomMoveScuJob::AddFindAnswer(const DicomMap& answer)
  {
    assert(query_.type() == Json::arrayValue);

    // Copy the identifiers tags, if they exist
    Json::Value item = Json::objectValue;
    AddTagIfString(item, answer, DICOM_TAG_QUERY_RETRIEVE_LEVEL);
    AddTagIfString(item, answer, DICOM_TAG_PATIENT_ID);
    AddTagIfString(item, answer, DICOM_TAG_STUDY_INSTANCE_UID);
    AddTagIfString(item, answer, DICOM_TAG_SERIES_INSTANCE_UID);
    AddTagIfString(item, answer, DICOM_TAG_SOP_INSTANCE_UID);
    AddTagIfString(item, answer, DICOM_TAG_ACCESSION_NUMBER);
    query_.append(item);
    
    AddCommand(new Command(*this, answer));
  }

  
  void DicomMoveScuJob::AddFindAnswer(QueryRetrieveHandler& query,
                                      size_t i)
  {
    DicomMap answer;
    query.GetAnswer(answer, i);
    AddFindAnswer(answer);
  }    


  void DicomMoveScuJob::SetLocalAet(const std::string& aet)
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

  
  void DicomMoveScuJob::SetRemoteModality(const RemoteModalityParameters& remote)
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

  
  void DicomMoveScuJob::Stop(JobStopReason reason)
  {
    connection_.reset();
  }
  

  void DicomMoveScuJob::GetPublicContent(Json::Value& value)
  {
    SetOfCommandsJob::GetPublicContent(value);
    
    value["LocalAet"] = localAet_;
    value["RemoteAet"] = remote_.GetApplicationEntityTitle();
    value["Query"] = query_;
  }


  DicomMoveScuJob::DicomMoveScuJob(ServerContext& context,
                                   const Json::Value& serialized) :
    SetOfCommandsJob(new Unserializer(*this), serialized),
    context_(context),
    query_(Json::arrayValue)
  {
    localAet_ = SerializationToolbox::ReadString(serialized, LOCAL_AET);
    targetAet_ = SerializationToolbox::ReadString(serialized, TARGET_AET);
    remote_ = RemoteModalityParameters(serialized[REMOTE]);

    if (serialized.isMember(QUERY) &&
        serialized[QUERY].type() == Json::arrayValue)
    {
      query_ = serialized[QUERY];
    }
  }

  
  bool DicomMoveScuJob::Serialize(Json::Value& target)
  {
    if (!SetOfCommandsJob::Serialize(target))
    {
      return false;
    }
    else
    {
      target[LOCAL_AET] = localAet_;
      target[TARGET_AET] = targetAet_;
      target[QUERY] = query_;
      remote_.Serialize(target[REMOTE], true /* force advanced format */);
      return true;
    }
  }
}
