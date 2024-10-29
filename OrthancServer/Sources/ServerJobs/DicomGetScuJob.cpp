/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2024 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2024 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "DicomGetScuJob.h"

#include "../../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"
#include "../../../OrthancFramework/Sources/SerializationToolbox.h"
#include "../ServerContext.h"
#include <dcmtk/dcmnet/dimse.h>

static const char* const LOCAL_AET = "LocalAet";
static const char* const QUERY = "Query";
static const char* const QUERY_FORMAT = "QueryFormat";  // New in 1.9.5
static const char* const REMOTE = "Remote";
static const char* const TIMEOUT = "Timeout";

namespace Orthanc
{
  class DicomGetScuJob::Command : public SetOfCommandsJob::ICommand
  {
  private:
    DicomGetScuJob&            that_;
    std::unique_ptr<DicomMap>  findAnswer_;

  public:
    Command(DicomGetScuJob& that,
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


  class DicomGetScuJob::Unserializer :
    public SetOfCommandsJob::ICommandUnserializer
  {
  private:
    DicomGetScuJob&   that_;

  public:
    explicit Unserializer(DicomGetScuJob&  that) :
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


  static uint16_t InstanceReceivedHandler(void* callbackContext,
                                          DcmDataset& dataset,
                                          const std::string& remoteAet,
                                          const std::string& remoteIp,
                                          const std::string& calledAet)
  {
    // this code is equivalent to OrthancStoreRequestHandler
    ServerContext* context = reinterpret_cast<ServerContext*>(callbackContext);

    std::unique_ptr<DicomInstanceToStore> toStore(DicomInstanceToStore::CreateFromDcmDataset(dataset));
    
    if (toStore->GetBufferSize() > 0)
    {
      toStore->SetOrigin(DicomInstanceOrigin::FromDicomProtocol
                         (remoteIp.c_str(), remoteAet.c_str(), calledAet.c_str()));

      std::string id;
      ServerContext::StoreResult result = context->Store(id, *toStore, StoreInstanceMode_Default);
      return result.GetCStoreStatusCode();
    }

    return STATUS_STORE_Error_CannotUnderstand;
  }

  void DicomGetScuJob::Retrieve(const DicomMap& findAnswer)
  {
    if (connection_.get() == NULL)
    {
      connection_.reset(new DicomControlUserConnection(parameters_));
    }
    
    connection_->Get(findAnswer, InstanceReceivedHandler, &context_);
  }

  void DicomGetScuJob::AddResourceToRetrieve(ResourceType level, const std::string& dicomId)
  {
    // TODO-GET: when retrieving a single series, one must provide the StudyInstanceUID too
    DicomMap item;

    switch (level)
    {
    case ResourceType_Patient:
      item.SetValue(DICOM_TAG_QUERY_RETRIEVE_LEVEL, ResourceTypeToDicomQueryRetrieveLevel(level), false);
      item.SetValue(DICOM_TAG_PATIENT_ID, dicomId, false);
      break;

    case ResourceType_Study:
      item.SetValue(DICOM_TAG_QUERY_RETRIEVE_LEVEL, ResourceTypeToDicomQueryRetrieveLevel(level), false);
      item.SetValue(DICOM_TAG_STUDY_INSTANCE_UID, dicomId, false);
      break;

    case ResourceType_Series:
      item.SetValue(DICOM_TAG_QUERY_RETRIEVE_LEVEL, ResourceTypeToDicomQueryRetrieveLevel(level), false);
      item.SetValue(DICOM_TAG_SERIES_INSTANCE_UID, dicomId, false);
      break;

    case ResourceType_Instance:
      item.SetValue(DICOM_TAG_QUERY_RETRIEVE_LEVEL, ResourceTypeToDicomQueryRetrieveLevel(level), false);
      item.SetValue(DICOM_TAG_SOP_INSTANCE_UID, dicomId, false);
      break;

    default:
      throw OrthancException(ErrorCode_InternalError);
    }
    query_.Add(item);
    
    AddCommand(new Command(*this, item));
  }

  void DicomGetScuJob::SetLocalAet(const std::string& aet)
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

  
  void DicomGetScuJob::SetRemoteModality(const RemoteModalityParameters& remote)
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


  void DicomGetScuJob::SetTimeout(uint32_t seconds)
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

  
  void DicomGetScuJob::Stop(JobStopReason reason)
  {
    connection_.reset();
  }
  

  void DicomGetScuJob::SetQueryFormat(DicomToJsonFormat format)
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


  void DicomGetScuJob::GetPublicContent(Json::Value& value)
  {
    SetOfCommandsJob::GetPublicContent(value);

    value[LOCAL_AET] = parameters_.GetLocalApplicationEntityTitle();
    value["RemoteAet"] = parameters_.GetRemoteModality().GetApplicationEntityTitle();

    value[QUERY] = Json::objectValue;
    // query_.ToJson(value[QUERY], queryFormat_);
  }


  DicomGetScuJob::DicomGetScuJob(ServerContext& context,
                                 const Json::Value& serialized) :
    SetOfCommandsJob(new Unserializer(*this), serialized),
    context_(context),
    parameters_(DicomAssociationParameters::UnserializeJob(serialized)),
    // targetAet_(SerializationToolbox::ReadString(serialized, TARGET_AET)),
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
          // AddToQuery(query_, item);
        }
      }
    }

    if (serialized.isMember(QUERY_FORMAT))
    {
      queryFormat_ = StringToDicomToJsonFormat(SerializationToolbox::ReadString(serialized, QUERY_FORMAT));
    }
  }

  
  bool DicomGetScuJob::Serialize(Json::Value& target)
  {
    if (!SetOfCommandsJob::Serialize(target))
    {
      return false;
    }
    else
    {
      parameters_.SerializeJob(target);
      // target[TARGET_AET] = targetAet_;

      // "Short" is for compatibility with Orthanc <= 1.9.4
      target[QUERY] = Json::objectValue;
      // query_.ToJson(target[QUERY], DicomToJsonFormat_Short);

      target[QUERY_FORMAT] = EnumerationToString(queryFormat_);
      
      return true;
    }
  }
}
