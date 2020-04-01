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
static const char* INDEX = "Index";
static const char* LOOKUP = "Lookup";
static const char* REMOTE_MODALITY = "RemoteModality";
static const char* SETUP = "Setup";
static const char* SOP_CLASS_UIDS = "SopClassUids";
static const char* SOP_INSTANCE_UIDS = "SopInstanceUids";
static const char* TRANSACTION_UID = "TransactionUid";
static const char* TYPE = "Type";



namespace Orthanc
{
  class StorageCommitmentScpJob::StorageCommitmentCommand : public SetOfCommandsJob::ICommand
  {
  public:
    virtual CommandType GetType() const = 0;
  };

  
  class StorageCommitmentScpJob::SetupCommand : public StorageCommitmentCommand
  {
  private:
    StorageCommitmentScpJob&  that_;

  public:
    SetupCommand(StorageCommitmentScpJob& that) :
      that_(that)
    {
    }

    virtual CommandType GetType() const ORTHANC_OVERRIDE
    {
      return CommandType_Setup;
    }
    
    virtual bool Execute(const std::string& jobId) ORTHANC_OVERRIDE
    {
      that_.Setup(jobId);
      return true;
    }

    virtual void Serialize(Json::Value& target) const ORTHANC_OVERRIDE
    {
      target = Json::objectValue;
      target[TYPE] = SETUP;
    }
  };


  class StorageCommitmentScpJob::LookupCommand : public StorageCommitmentCommand
  {
  private:
    StorageCommitmentScpJob&        that_;
    size_t                          index_;
    bool                            hasFailureReason_;
    StorageCommitmentFailureReason  failureReason_;

  public:
    LookupCommand(StorageCommitmentScpJob&  that,
                  size_t index) :
      that_(that),
      index_(index),
      hasFailureReason_(false)
    {
    }

    virtual CommandType GetType() const ORTHANC_OVERRIDE
    {
      return CommandType_Lookup;
    }
    
    virtual bool Execute(const std::string& jobId) ORTHANC_OVERRIDE
    {
      failureReason_ = that_.Lookup(index_);
      hasFailureReason_ = true;
      return true;
    }

    size_t GetIndex() const
    {
      return index_;
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

    virtual void Serialize(Json::Value& target) const ORTHANC_OVERRIDE
    {
      target = Json::objectValue;
      target[TYPE] = LOOKUP;
      target[INDEX] = static_cast<unsigned int>(index_);
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

    virtual CommandType GetType() const ORTHANC_OVERRIDE
    {
      return CommandType_Answer;
    }
    
    virtual bool Execute(const std::string& jobId) ORTHANC_OVERRIDE
    {
      that_.Answer();
      return true;
    }

    virtual void Serialize(Json::Value& target) const ORTHANC_OVERRIDE
    {
      target = Json::objectValue;
      target[TYPE] = ANSWER;
    }
  };
    

  class StorageCommitmentScpJob::Unserializer : public SetOfCommandsJob::ICommandUnserializer
  {
  private:
    StorageCommitmentScpJob&  that_;

  public:
    Unserializer(StorageCommitmentScpJob& that) :
      that_(that)
    {
      that_.ready_ = false;
    }

    virtual ICommand* Unserialize(const Json::Value& source) const
    {
      const std::string type = SerializationToolbox::ReadString(source, TYPE);

      if (type == SETUP)
      {
        return new SetupCommand(that_);
      }
      else if (type == LOOKUP)
      {
        return new LookupCommand(that_, SerializationToolbox::ReadUnsignedInteger(source, INDEX));
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


  void StorageCommitmentScpJob::CheckInvariants()
  {
    const size_t n = GetCommandsCount();

    if (n <= 1)
    {
      throw OrthancException(ErrorCode_InternalError);
    }
    
    for (size_t i = 0; i < n; i++)
    {
      const CommandType type = dynamic_cast<const StorageCommitmentCommand&>(GetCommand(i)).GetType();
      
      if ((i == 0 && type != CommandType_Setup) ||
          (i >= 1 && i < n - 1 && type != CommandType_Lookup) ||
          (i == n - 1 && type != CommandType_Answer))
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      if (type == CommandType_Lookup)
      {
        const LookupCommand& lookup = dynamic_cast<const LookupCommand&>(GetCommand(i));
        if (lookup.GetIndex() != i - 1)
        {
          throw OrthancException(ErrorCode_InternalError);
        }
      }
    }
  }
    

  void StorageCommitmentScpJob::Setup(const std::string& jobId)
  {
    CheckInvariants();

    const std::string& remoteAet = remoteModality_.GetApplicationEntityTitle();
    lookupHandler_.reset(context_.CreateStorageCommitment(jobId, transactionUid_, sopClassUids_,
                                                          sopInstanceUids_, remoteAet, calledAet_));
  }


  StorageCommitmentFailureReason StorageCommitmentScpJob::Lookup(size_t index)
  {
#ifndef NDEBUG
    CheckInvariants();
#endif

    if (index >= sopClassUids_.size())
    {
      throw OrthancException(ErrorCode_InternalError);
    }
    else if (lookupHandler_.get() != NULL)
    {
      return lookupHandler_->Lookup(sopClassUids_[index], sopInstanceUids_[index]);
    }
    else
    {
      // This is the default implementation of Orthanc (if no storage
      // commitment plugin is installed)
      bool success = false;
      StorageCommitmentFailureReason reason =
        StorageCommitmentFailureReason_NoSuchObjectInstance /* 0x0112 == 274 */;
      
      try
      {
        std::vector<std::string> orthancId;
        context_.GetIndex().LookupIdentifierExact(orthancId, ResourceType_Instance, DICOM_TAG_SOP_INSTANCE_UID, sopInstanceUids_[index]);

        if (orthancId.size() == 1)
        {
          std::string a, b;

          // Make sure that the DICOM file can be re-read by DCMTK
          // from the file storage, and that the actual SOP
          // class/instance UIDs do match
          ServerContext::DicomCacheLocker locker(context_, orthancId[0]);
          if (locker.GetDicom().GetTagValue(a, DICOM_TAG_SOP_CLASS_UID) &&
              locker.GetDicom().GetTagValue(b, DICOM_TAG_SOP_INSTANCE_UID) &&
              b == sopInstanceUids_[index])
          {
            if (a == sopClassUids_[index])
            {
              success = true;
              reason = StorageCommitmentFailureReason_Success;
            }
            else
            {
              // Mismatch in the SOP class UID
              reason = StorageCommitmentFailureReason_ClassInstanceConflict /* 0x0119 */;
            }
          }
        }
      }
      catch (OrthancException&)
      {
      }

      LOG(INFO) << "  Storage commitment SCP job: " << (success ? "Success" : "Failure")
                << " while looking for " << sopClassUids_[index] << " / " << sopInstanceUids_[index];

      return reason;
    }
  }
  
  
  void StorageCommitmentScpJob::Answer()
  {   
    CheckInvariants();
    LOG(INFO) << "  Storage commitment SCP job: Sending answer";

    std::vector<StorageCommitmentFailureReason> failureReasons;
    failureReasons.reserve(sopClassUids_.size());

    for (size_t i = 1; i < GetCommandsCount() - 1; i++)
    {
      const LookupCommand& lookup = dynamic_cast<const LookupCommand&>(GetCommand(i));
      failureReasons.push_back(lookup.GetFailureReason());
    }

    if (failureReasons.size() != sopClassUids_.size())
    {
      throw OrthancException(ErrorCode_InternalError);
    }
      
    DicomUserConnection scu(calledAet_, remoteModality_);
    scu.ReportStorageCommitment(transactionUid_, sopClassUids_, sopInstanceUids_, failureReasons);
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

    AddCommand(new SetupCommand(*this));
  }
    

  void StorageCommitmentScpJob::Reserve(size_t size)
  {
    if (ready_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      sopClassUids_.reserve(size);
      sopInstanceUids_.reserve(size);
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
      assert(sopClassUids_.size() == sopInstanceUids_.size());
      AddCommand(new LookupCommand(*this, sopClassUids_.size()));
      sopClassUids_.push_back(sopClassUid);
      sopInstanceUids_.push_back(sopInstanceUid);
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
    SetOfCommandsJob(new Unserializer(*this), serialized),
    context_(context)
  {
    transactionUid_ = SerializationToolbox::ReadString(serialized, TRANSACTION_UID);
    remoteModality_ = RemoteModalityParameters(serialized[REMOTE_MODALITY]);
    calledAet_ = SerializationToolbox::ReadString(serialized, CALLED_AET);
    SerializationToolbox::ReadArrayOfStrings(sopClassUids_, serialized, SOP_CLASS_UIDS);
    SerializationToolbox::ReadArrayOfStrings(sopInstanceUids_, serialized, SOP_INSTANCE_UIDS);
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
      SerializationToolbox::WriteArrayOfStrings(target, sopClassUids_, SOP_CLASS_UIDS);
      SerializationToolbox::WriteArrayOfStrings(target, sopInstanceUids_, SOP_INSTANCE_UIDS);
      return true;
    }
  }
}
