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


  static void AddToQuery(DicomFindAnswers& query,
                         const DicomMap& item)
  {
    query.Add(item);

    /**
     * Compatibility with Orthanc <= 1.9.4: Remove the
     * "SpecificCharacterSet" (0008,0005) tag that is automatically
     * added if creating a ParsedDicomFile object from a DicomMap.
     **/
    query.GetAnswer(query.GetSize() - 1).Remove(DICOM_TAG_SPECIFIC_CHARACTER_SET);
  }

  // this method is used to implement the retrieve part of a Q&R 
  // it keeps only the main dicom tags from the C-Find answer
  void DicomMoveScuJob::AddFindAnswer(const DicomMap& answer)
  {
    DicomMap item;
    item.CopyTagIfExists(answer, DICOM_TAG_QUERY_RETRIEVE_LEVEL);
    item.CopyTagIfExists(answer, DICOM_TAG_PATIENT_ID);
    item.CopyTagIfExists(answer, DICOM_TAG_STUDY_INSTANCE_UID);
    item.CopyTagIfExists(answer, DICOM_TAG_SERIES_INSTANCE_UID);
    item.CopyTagIfExists(answer, DICOM_TAG_SOP_INSTANCE_UID);
    item.CopyTagIfExists(answer, DICOM_TAG_ACCESSION_NUMBER);
    AddToQuery(query_, item);
    
    AddCommand(new Command(*this, answer));
  }

  // this method is used to implement a C-Move
  // it keeps all tags from the C-Move query
  void DicomMoveScuJob::AddQuery(const DicomMap& query)
  {
    AddToQuery(query_, query);
    AddCommand(new Command(*this, query));
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
  

  void DicomMoveScuJob::SetQueryFormat(DicomToJsonFormat format)
  {
    if (IsStarted())
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      queryFormat_ = format;
    }
  }


  void DicomMoveScuJob::GetPublicContent(Json::Value& value)
  {
    SetOfCommandsJob::GetPublicContent(value);

    value[LOCAL_AET] = parameters_.GetLocalApplicationEntityTitle();
    value["RemoteAet"] = parameters_.GetRemoteModality().GetApplicationEntityTitle();

    value[QUERY] = Json::objectValue;
    query_.ToJson(value[QUERY], queryFormat_);
  }


  DicomMoveScuJob::DicomMoveScuJob(ServerContext& context,
                                   const Json::Value& serialized) :
    SetOfCommandsJob(new Unserializer(*this), serialized),
    context_(context),
    parameters_(DicomAssociationParameters::UnserializeJob(serialized)),
    targetAet_(SerializationToolbox::ReadString(serialized, TARGET_AET)),
    query_(true),
    queryFormat_(DicomToJsonFormat_Short)
  {
    if (serialized.isMember(QUERY))
    {
      const Json::Value& query = serialized[QUERY];
      if (query.type() == Json::arrayValue)
      {
        for (Json::Value::ArrayIndex i = 0; i < query.size(); i++)
        {
          DicomMap item;
          FromDcmtkBridge::FromJson(item, query[i]);
          AddToQuery(query_, item);
        }
      }
    }

    if (serialized.isMember(QUERY_FORMAT))
    {
      queryFormat_ = StringToDicomToJsonFormat(SerializationToolbox::ReadString(serialized, QUERY_FORMAT));
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

      // "Short" is for compatibility with Orthanc <= 1.9.4
      target[QUERY] = Json::objectValue;
      query_.ToJson(target[QUERY], DicomToJsonFormat_Short);

      target[QUERY_FORMAT] = EnumerationToString(queryFormat_);
      
      return true;
    }
  }
}
