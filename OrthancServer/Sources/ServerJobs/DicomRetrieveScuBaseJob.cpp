/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
#include "../../../OrthancFramework/Sources/DicomNetworking/DimseErrorPayload.h"
#include "../../../OrthancFramework/Sources/SerializationToolbox.h"
#include "../ServerContext.h"
#include <dcmtk/dcmnet/dimse.h>
#include <algorithm>
#include "../../../OrthancFramework/Sources/Logging.h"
#include <boost/thread/mutex.hpp>

static const char* const LOCAL_AET = "LocalAet";
static const char* const QUERY = "Query";
static const char* const QUERY_FORMAT = "QueryFormat";  // New in 1.9.5
static const char* const REMOTE = "Remote";
static const char* const TIMEOUT = "Timeout";

static std::map<std::string, Orthanc::SetOfCommandsJob::ICommand*> messagesRegistry; 
static uint16_t messageRegistryCurrentId = 1000;
static boost::mutex messageRegistryMutex;

namespace Orthanc
{
  std::string GetKey(const std::string& aet, uint16_t messageId)
  {
    return aet + "-" + boost::lexical_cast<std::string>(messageId);
  }

  uint16_t DicomRetrieveScuBaseJob::GetMessageId(const std::string& localAet)
  {
    assert(currentCommand_ != NULL);
    boost::mutex::scoped_lock lock(messageRegistryMutex);
    
    // Each resource retrieval (command) has its own messageId.  
    // We start at 1000 to clearly differentiate them from other messages.  We can actually use ANY value between 0 & 65535.
    messageRegistryCurrentId = std::max(1000, (messageRegistryCurrentId + 1) % 0xFFFF);
    messagesRegistry[GetKey(localAet, messageRegistryCurrentId)] = currentCommand_;
    
    return messageRegistryCurrentId;
  }

  void DicomRetrieveScuBaseJob::AddReceivedInstanceFromCStore(uint16_t originatorMessageId, 
                                                              const std::string& originatorAet, 
                                                              const std::string& instanceId)
  {
    boost::mutex::scoped_lock lock(messageRegistryMutex);

    std::string key = GetKey(originatorAet, originatorMessageId);

    if (messagesRegistry.find(key) != messagesRegistry.end())
    {
      dynamic_cast<DicomRetrieveScuBaseJob::Command*>(messagesRegistry[key])->AddReceivedInstance(instanceId);
    }
  }

  DicomRetrieveScuBaseJob::Command::~Command()
  {
    // remove the command from the messageRegistry
    boost::mutex::scoped_lock lock(messageRegistryMutex);

    for (std::map<std::string, Orthanc::SetOfCommandsJob::ICommand*>::const_iterator it = messagesRegistry.begin();
         it != messagesRegistry.end(); ++it)
    {
      if (it->second == this)
      {
        messagesRegistry.erase(it->first);
        return;
      }
    }
  }

  bool DicomRetrieveScuBaseJob::Command::Execute(const std::string &jobId)
  {
    try
    {
      that_.currentCommand_ = this;
      that_.Retrieve(*findAnswer_); // here
    }
    catch (OrthancException& e)
    {
      if (e.GetPayload().HasContent() &&
          e.GetPayload().GetType() == ErrorPayloadType_Dimse)
      {
        dimseErrorStatus_ = GetDimseErrorStatusFromPayload(e.GetPayload());
      }

      throw;
    }
    return true;
  }

  void DicomRetrieveScuBaseJob::Command::Serialize(Json::Value &target) const
  {
    findAnswer_->Serialize(target["Query"]);
    target["DimseErrorStatus"] = dimseErrorStatus_;
    SerializationToolbox::WriteListOfStrings(target, receivedInstancesIds_, "ReceivedInstancesIds");
  }

  SetOfCommandsJob::ICommand* DicomRetrieveScuBaseJob::Unserializer::Unserialize(const Json::Value &source) const
  {
    DicomMap findAnswer;

    if (!source.isMember("Query")) // old format before 1.12.10, just in case we need to read old jobs after an upgrade
    {
      findAnswer.Unserialize(source);
      return new DicomRetrieveScuBaseJob::Command(that_, findAnswer);
    }

    findAnswer.Unserialize(source["Query"]);

    std::unique_ptr<DicomRetrieveScuBaseJob::Command> command(new DicomRetrieveScuBaseJob::Command(that_, findAnswer));
    command->SetDimseErrorStatus(static_cast<uint16_t>(source["DimseErrorStatus"].asUInt()));

    std::list<std::string> receivedInstancesIds;
    SerializationToolbox::ReadListOfStrings(receivedInstancesIds, source, "ReceivedInstancesIds");
    for (std::list<std::string>::const_iterator it = receivedInstancesIds.begin(); it != receivedInstancesIds.end(); ++it)
    {
      command->AddReceivedInstance(*it);
    }

    return command.release();
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
  void DicomRetrieveScuBaseJob::AddFindAnswer(const DicomMap& answer)
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

  void DicomRetrieveScuBaseJob::AddFindAnswer(QueryRetrieveHandler& query,
                                              size_t i)
  {
    DicomMap answer;
    query.GetAnswer(answer, i);
    AddFindAnswer(answer);
  }    

  // this method is used to implement a C-Move
  // it keeps all tags from the C-Move query
  void DicomRetrieveScuBaseJob::AddQuery(const DicomMap& query)
  {
    AddToQuery(query_, query);
    AddCommand(new Command(*this, query));
  }
 

  void DicomRetrieveScuBaseJob::SetLocalAet(const std::string& aet)
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

  
  void DicomRetrieveScuBaseJob::SetRemoteModality(const RemoteModalityParameters& remote)
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


  void DicomRetrieveScuBaseJob::SetTimeout(uint32_t seconds)
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

  
  void DicomRetrieveScuBaseJob::Stop(JobStopReason reason)
  {
    connection_.reset();
  }
  

  void DicomRetrieveScuBaseJob::SetQueryFormat(DicomToJsonFormat format)
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


  void DicomRetrieveScuBaseJob::GetPublicContent(Json::Value& value) const
  {
    SetOfCommandsJob::GetPublicContent(value);

    value[LOCAL_AET] = parameters_.GetLocalApplicationEntityTitle();
    value["RemoteAet"] = parameters_.GetRemoteModality().GetApplicationEntityTitle();

    value[QUERY] = Json::objectValue;
    query_.ToJson(value[QUERY], queryFormat_);

    value["Details"] = Json::arrayValue;
    
    for (size_t i = 0; i < GetCommandsCount(); ++i)
    {
      const DicomRetrieveScuBaseJob::Command& command = dynamic_cast<const DicomRetrieveScuBaseJob::Command&>(GetCommand(i));

      Json::Value v;
      v["DimseErrorStatus"] = command.GetDimseErrorStatus();
      query_.ToJson(v["Query"], i, DicomToJsonFormat_Short);
      SerializationToolbox::WriteListOfStrings(v, command.GetReceivedInstancesIds(), "ReceivedInstancesIds");

      value["Details"].append(v);
    }
  }


  DicomRetrieveScuBaseJob::DicomRetrieveScuBaseJob(ServerContext &context) :
    context_(context),
    query_(false /* this is not for worklists */),
    queryFormat_(DicomToJsonFormat_Short),
    nbRemainingSubOperations_(0),
    nbCompletedSubOperations_(0),
    nbFailedSubOperations_(0),
    nbWarningSubOperations_(0),
    currentCommand_(NULL)
  {
  }


  DicomRetrieveScuBaseJob::DicomRetrieveScuBaseJob(ServerContext& context,
                                                   const Json::Value& serialized) :
    SetOfCommandsJob(new Unserializer(*this), serialized),
    context_(context),
    parameters_(DicomAssociationParameters::UnserializeJob(serialized)),
    query_(false /* this is not for worklists */),
    queryFormat_(DicomToJsonFormat_Short),
    nbRemainingSubOperations_(0),
    nbCompletedSubOperations_(0),
    nbFailedSubOperations_(0),
    nbWarningSubOperations_(0),
    currentCommand_(NULL)
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

  
  bool DicomRetrieveScuBaseJob::Serialize(Json::Value& target) const
  {
    if (!SetOfCommandsJob::Serialize(target))
    {
      return false;
    }
    else
    {
      parameters_.SerializeJob(target);

      // "Short" is for compatibility with Orthanc <= 1.9.4
      target[QUERY] = Json::objectValue;
      query_.ToJson(target[QUERY], DicomToJsonFormat_Short);

      target[QUERY_FORMAT] = EnumerationToString(queryFormat_);
      
      return true;
    }
  }

  void DicomRetrieveScuBaseJob::OnProgressUpdated(uint16_t nbRemainingSubOperations,
                                                  uint16_t nbCompletedSubOperations,
                                                  uint16_t nbFailedSubOperations,
                                                  uint16_t nbWarningSubOperations)
  {
    boost::mutex::scoped_lock lock(progressMutex_);

    nbRemainingSubOperations_ = nbRemainingSubOperations;
    nbCompletedSubOperations_ = nbCompletedSubOperations;
    nbFailedSubOperations_ = nbFailedSubOperations;
    nbWarningSubOperations_ = nbWarningSubOperations;
  }

  float DicomRetrieveScuBaseJob::GetProgress() const
  {
    boost::mutex::scoped_lock lock(progressMutex_);
    
    uint32_t totalOperations = nbRemainingSubOperations_ + nbCompletedSubOperations_ + nbFailedSubOperations_ + nbWarningSubOperations_;
    if (totalOperations == 0)
    {
      return 0.0f;
    }

    return float(nbCompletedSubOperations_ + nbFailedSubOperations_ + nbWarningSubOperations_) / float(totalOperations);
  }

  void DicomRetrieveScuBaseJob::AddReceivedInstance(const std::string& instanceId)
  {
    if (currentCommand_ != NULL)
    {
      currentCommand_->AddReceivedInstance(instanceId);
    }
  }

  void DicomRetrieveScuBaseJob::LookupErrorPayload(ErrorPayload& payload) const
  {
    Json::Value publicContent;
    GetPublicContent(publicContent);

    payload.SetContent(ErrorPayloadType_RetrieveJob, publicContent["Details"]);
  }
}
