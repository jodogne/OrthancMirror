/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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

#include "../../../OrthancFramework/Sources/SerializationToolbox.h"
#include "../ServerContext.h"

static const char* const LOCAL_AET = "LocalAet";
static const char* const TARGET_AET = "TargetAet";
static const char* const REMOTE = "Remote";
static const char* const QUERY = "Query";
static const char* const TIMEOUT = "Timeout";

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
    explicit Unserializer(DicomMoveScuJob&  that) :
      that_(that)
    {
    }

    virtual ICommand* Unserialize(const Json::Value& source) const ORTHANC_OVERRIDE
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
      connection_.reset(new DicomControlUserConnection(parameters_));
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
      parameters_.SetLocalApplicationEntityTitle(aet);
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
      parameters_.SetRemoteModality(remote);
    }
  }


  void DicomMoveScuJob::SetTimeout(uint32_t seconds)
  {
    if (IsStarted())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      parameters_.SetTimeout(seconds);
    }
  }

  
  void DicomMoveScuJob::Stop(JobStopReason reason)
  {
    connection_.reset();
  }
  

  void DicomMoveScuJob::GetPublicContent(Json::Value& value)
  {
    SetOfCommandsJob::GetPublicContent(value);

    value["LocalAet"] = parameters_.GetLocalApplicationEntityTitle();
    value["RemoteAet"] = parameters_.GetRemoteModality().GetApplicationEntityTitle();
    value["Query"] = query_;
  }


  DicomMoveScuJob::DicomMoveScuJob(ServerContext& context,
                                   const Json::Value& serialized) :
    SetOfCommandsJob(new Unserializer(*this), serialized),
    context_(context),
    parameters_(DicomAssociationParameters::UnserializeJob(serialized)),
    targetAet_(SerializationToolbox::ReadString(serialized, TARGET_AET)),
    query_(Json::arrayValue)
  {
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
      parameters_.SerializeJob(target);
      target[TARGET_AET] = targetAet_;
      target[QUERY] = query_;
      return true;
    }
  }
}
