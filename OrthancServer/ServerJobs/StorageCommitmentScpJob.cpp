/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2019 Osimis S.A., Belgium
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
#include "../OrthancConfiguration.h"
#include "../ServerContext.h"


namespace Orthanc
{
  class StorageCommitmentScpJob::LookupCommand : public SetOfCommandsJob::ICommand
  {
  private:
    StorageCommitmentScpJob&  that_;
    std::string               sopClassUid_;
    std::string               sopInstanceUid_;

  public:
    LookupCommand(StorageCommitmentScpJob& that,
                  const std::string& sopClassUid,
                  const std::string& sopInstanceUid) :
      that_(that),
      sopClassUid_(sopClassUid),
      sopInstanceUid_(sopInstanceUid)
    {
    }

    virtual bool Execute()
    {
      that_.LookupInstance(sopClassUid_, sopInstanceUid_);
      return true;
    }

    virtual void Serialize(Json::Value& target) const
    {
      target = Json::objectValue;
      target["Type"] = "Lookup";
      target["SopClassUid"] = sopClassUid_;
      target["SopInstanceUid"] = sopInstanceUid_;
    }
  };

  
  class StorageCommitmentScpJob::AnswerCommand : public SetOfCommandsJob::ICommand
  {
  private:
    StorageCommitmentScpJob&  that_;

  public:
    AnswerCommand(StorageCommitmentScpJob& that) :
      that_(that)
    {
    }

    virtual bool Execute()
    {
      that_.Answer();
      return true;
    }

    virtual void Serialize(Json::Value& target) const
    {
      target = Json::objectValue;
      target["Type"] = "Answer";
    }
  };
    

  class StorageCommitmentScpJob::Unserializer : public SetOfCommandsJob::ICommandUnserializer
  {
  private:
    StorageCommitmentScpJob&   that_;

  public:
    Unserializer(StorageCommitmentScpJob&  that) :
      that_(that)
    {
    }

    virtual ICommand* Unserialize(const Json::Value& source) const
    {
      std::cout << "===================================\n";
      std::cout << source.toStyledString();
        
      /*DicomMap findAnswer;
        findAnswer.Unserialize(source);
        return new Command(that_, findAnswer);*/

      throw OrthancException(ErrorCode_NotImplemented);
    }
  };

  
  void StorageCommitmentScpJob::LookupInstance(const std::string& sopClassUid,
                                               const std::string& sopInstanceUid)
  {
    bool success = false;
      
    try
    {
      std::vector<std::string> orthancId;
      context_.GetIndex().LookupIdentifierExact(orthancId, ResourceType_Instance, DICOM_TAG_SOP_INSTANCE_UID, sopInstanceUid);

      if (orthancId.size() == 1)
      {
        std::string a, b;

        // Make sure that the DICOM file can be re-read by DCMTK
        // from the file storage, and that the actual SOP
        // class/instance UIDs do match
        ServerContext::DicomCacheLocker locker(context_, orthancId[0]);
        if (locker.GetDicom().GetTagValue(a, DICOM_TAG_SOP_CLASS_UID) &&
            locker.GetDicom().GetTagValue(b, DICOM_TAG_SOP_INSTANCE_UID) &&
            a == sopClassUid &&
            b == sopInstanceUid)
        {
          success = true;
        }
      }
    }
    catch (OrthancException&)
    {
    }

    LOG(INFO) << "  Storage commitment SCP job: " << (success ? "Success" : "Failure")
              << " while looking for " << sopClassUid << " / " << sopInstanceUid;

    if (success)
    {
      successSopClassUids_.push_back(sopClassUid);
      successSopInstanceUids_.push_back(sopInstanceUid);
    }
    else
    {
      failedSopClassUids_.push_back(sopClassUid);
      failedSopInstanceUids_.push_back(sopInstanceUid);
    }
  }

    
  void StorageCommitmentScpJob::Answer()
  {
    LOG(INFO) << "  Storage commitment SCP job: Sending answer";
      
    DicomUserConnection scu(calledAet_, remoteModality_);
    scu.ReportStorageCommitment(transactionUid_, successSopClassUids_, successSopInstanceUids_,
                                failedSopClassUids_, failedSopInstanceUids_);
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
      AddCommand(new LookupCommand(*this, sopClassUid, sopInstanceUid));        
    }
  }
    

  void StorageCommitmentScpJob::MarkAsReady()
  {
    if (ready_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      AddCommand(new AnswerCommand(*this));
      ready_ = true;
    }
  }


  void StorageCommitmentScpJob::GetPublicContent(Json::Value& value)
  {
    SetOfCommandsJob::GetPublicContent(value);
      
    value["LocalAet"] = calledAet_;
    value["RemoteAet"] = remoteModality_.GetApplicationEntityTitle();
    value["TransactionUid"] = transactionUid_;
  }
}
