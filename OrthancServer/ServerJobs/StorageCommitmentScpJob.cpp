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


#include "../PrecompiledHeadersServer.h"
#include "StorageCommitmentScpJob.h"

#include "../../Core/DicomNetworking/DicomUserConnection.h"
#include "../../Core/Logging.h"
#include "../../Core/OrthancException.h"
#include "../../Core/SerializationToolbox.h"
#include "../OrthancConfiguration.h"
#include "../ServerContext.h"


static const char* ANSWER = "Answer";
static const char* CALLED_AET = "CalledAet";
static const char* LOOKUP = "Lookup";
static const char* REMOTE_MODALITY = "RemoteModality";
static const char* SOP_CLASS_UID = "SopClassUid";
static const char* SOP_INSTANCE_UID = "SopInstanceUid";
static const char* TRANSACTION_UID = "TransactionUid";
static const char* TYPE = "Type";



namespace Orthanc
{
  class StorageCommitmentScpJob::StorageCommitmentCommand : public SetOfCommandsJob::ICommand
  {
  public:
    virtual bool IsAnswer() const = 0;
  };

  
  class StorageCommitmentScpJob::LookupCommand : public StorageCommitmentCommand
  {
  private:
    ServerContext&  context_;
    bool            hasFailureReason_;
    std::string     sopClassUid_;
    std::string     sopInstanceUid_;
    StorageCommitmentFailureReason  failureReason_;

  public:
    LookupCommand(ServerContext& context,
                  const std::string& sopClassUid,
                  const std::string& sopInstanceUid) :
      context_(context),
      hasFailureReason_(false),
      sopClassUid_(sopClassUid),
      sopInstanceUid_(sopInstanceUid)
    {
    }

    virtual bool IsAnswer() const
    {
      return false;
    }
    
    virtual bool Execute(const std::string& jobId) ORTHANC_OVERRIDE
    {
      if (hasFailureReason_)
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
      
      bool success = false;
      
      try
      {
        std::vector<std::string> orthancId;
        context_.GetIndex().LookupIdentifierExact(orthancId, ResourceType_Instance, DICOM_TAG_SOP_INSTANCE_UID, sopInstanceUid_);

        if (orthancId.size() == 1)
        {
          std::string a, b;

          // Make sure that the DICOM file can be re-read by DCMTK
          // from the file storage, and that the actual SOP
          // class/instance UIDs do match
          ServerContext::DicomCacheLocker locker(context_, orthancId[0]);
          if (locker.GetDicom().GetTagValue(a, DICOM_TAG_SOP_CLASS_UID) &&
              locker.GetDicom().GetTagValue(b, DICOM_TAG_SOP_INSTANCE_UID) &&
              a == sopClassUid_ &&
              b == sopInstanceUid_)
          {
            success = true;
          }
        }
      }
      catch (OrthancException&)
      {
      }

      LOG(INFO) << "  Storage commitment SCP job: " << (success ? "Success" : "Failure")
                << " while looking for " << sopClassUid_ << " / " << sopInstanceUid_;

      failureReason_ = (success ?
                        StorageCommitmentFailureReason_Success : 
                        StorageCommitmentFailureReason_NoSuchObjectInstance /* 0x0112 == 274 */);
      hasFailureReason_ = true;
      
      return true;
    }

    const std::string& GetSopClassUid() const
    {
      return sopClassUid_;
    }
    
    const std::string& GetSopInstanceUid() const
    {
      return sopInstanceUid_;
    }
    
    StorageCommitmentFailureReason GetFailureReason() const
    {
      if (hasFailureReason_)
      {
        return failureReason_;
      }
      else
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
    }

    virtual void Serialize(Json::Value& target) const
    {
      target = Json::objectValue;
      target[TYPE] = LOOKUP;
      target[SOP_CLASS_UID] = sopClassUid_;
      target[SOP_INSTANCE_UID] = sopInstanceUid_;
    }
  };

  
  class StorageCommitmentScpJob::AnswerCommand : public StorageCommitmentCommand
  {
  private:
    StorageCommitmentScpJob&  that_;

  public:
    AnswerCommand(StorageCommitmentScpJob& that) :
      that_(that)
    {
      if (that_.ready_)
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
      else
      {
        that_.ready_ = true;
      }
    }

    virtual bool IsAnswer() const
    {
      return true;
    }
    
    virtual bool Execute(const std::string& jobId) ORTHANC_OVERRIDE
    {
      that_.Answer();
      return true;
    }

    virtual void Serialize(Json::Value& target) const
    {
      target = Json::objectValue;
      target[TYPE] = ANSWER;
    }
  };
    

  class StorageCommitmentScpJob::Unserializer : public SetOfCommandsJob::ICommandUnserializer
  {
  private:
    StorageCommitmentScpJob&  that_;
    ServerContext&            context_;

  public:
    Unserializer(StorageCommitmentScpJob&  that,
                 ServerContext& context) :
      that_(that),
      context_(context)
    {
      that_.ready_ = false;
    }

    virtual ICommand* Unserialize(const Json::Value& source) const
    {
      const std::string type = SerializationToolbox::ReadString(source, TYPE);

      if (type == LOOKUP)
      {
        return new LookupCommand(context_,
                                 SerializationToolbox::ReadString(source, SOP_CLASS_UID),
                                 SerializationToolbox::ReadString(source, SOP_INSTANCE_UID));
      }
      else if (type == ANSWER)
      {
        return new AnswerCommand(that_);
      }
      else
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }
    }
  };

  
  void StorageCommitmentScpJob::Answer()
  {   
    LOG(INFO) << "  Storage commitment SCP job: Sending answer";

    const size_t n = GetCommandsCount();

    if (n == 0)
    {
      throw OrthancException(ErrorCode_InternalError);
    }
    
    std::vector<std::string> sopClassUids, sopInstanceUids;
    std::vector<StorageCommitmentFailureReason> failureReasons;

    sopClassUids.reserve(n);
    sopInstanceUids.reserve(n);
    failureReasons.reserve(n);

    for (size_t i = 0; i < n; i++)
    {
      const StorageCommitmentCommand& command = dynamic_cast<const StorageCommitmentCommand&>(GetCommand(i));

      if (i == n - 1)
      {
        if (!command.IsAnswer())
        {
          throw OrthancException(ErrorCode_InternalError);
        }
      }
      else
      {      
        if (command.IsAnswer())
        {
          throw OrthancException(ErrorCode_InternalError);
        }

        const LookupCommand& lookup = dynamic_cast<const LookupCommand&>(command);

        sopClassUids.push_back(lookup.GetSopClassUid());
        sopInstanceUids.push_back(lookup.GetSopInstanceUid());
        failureReasons.push_back(lookup.GetFailureReason());
      }
    }
      
    DicomUserConnection scu(calledAet_, remoteModality_);
    scu.ReportStorageCommitment(transactionUid_, sopClassUids, sopInstanceUids, failureReasons);
  }
    

  StorageCommitmentScpJob::StorageCommitmentScpJob(ServerContext& context,
                                                   const std::string& transactionUid,
                                                   const std::string& remoteAet,
                                                   const std::string& calledAet) :
    context_(context),
    ready_(false),
    transactionUid_(transactionUid),
    calledAet_(calledAet)
  {
    {
      OrthancConfiguration::ReaderLock lock;
      if (!lock.GetConfiguration().LookupDicomModalityUsingAETitle(remoteModality_, remoteAet))
      {
        throw OrthancException(ErrorCode_InexistentItem,
                               "Unknown remote modality for storage commitment SCP: " + remoteAet);
      }
    }
  }
    

  void StorageCommitmentScpJob::AddInstance(const std::string& sopClassUid,
                                            const std::string& sopInstanceUid)
  {
    if (ready_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      AddCommand(new LookupCommand(context_, sopClassUid, sopInstanceUid));        
    }
  }
    

  void StorageCommitmentScpJob::MarkAsReady()
  {
    AddCommand(new AnswerCommand(*this));
  }


  void StorageCommitmentScpJob::GetPublicContent(Json::Value& value)
  {
    SetOfCommandsJob::GetPublicContent(value);
      
    value["CalledAet"] = calledAet_;
    value["RemoteAet"] = remoteModality_.GetApplicationEntityTitle();
    value["TransactionUid"] = transactionUid_;
  }


  StorageCommitmentScpJob::StorageCommitmentScpJob(ServerContext& context,
                                                   const Json::Value& serialized) :
    SetOfCommandsJob(new Unserializer(*this, context), serialized),
    context_(context)
  {
    transactionUid_ = SerializationToolbox::ReadString(serialized, TRANSACTION_UID);
    remoteModality_ = RemoteModalityParameters(serialized[REMOTE_MODALITY]);
    calledAet_ = SerializationToolbox::ReadString(serialized, CALLED_AET);
  }
  

  bool StorageCommitmentScpJob::Serialize(Json::Value& target)
  {
    if (!SetOfCommandsJob::Serialize(target))
    {
      return false;
    }
    else
    {
      target[TRANSACTION_UID] = transactionUid_;
      remoteModality_.Serialize(target[REMOTE_MODALITY], true /* force advanced format */);
      target[CALLED_AET] = calledAet_;
      return true;
    }
  }
}
