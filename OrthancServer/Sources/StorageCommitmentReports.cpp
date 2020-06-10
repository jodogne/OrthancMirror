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


#include "PrecompiledHeadersServer.h"
#include "StorageCommitmentReports.h"

#include "../Core/OrthancException.h"

namespace Orthanc
{
  void StorageCommitmentReports::Report::MarkAsComplete()
  {
    if (isComplete_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      isComplete_ = true;
    }
  }

  void StorageCommitmentReports::Report::AddSuccess(const std::string& sopClassUid,
                                                    const std::string& sopInstanceUid)
  {
    if (isComplete_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      Success success;
      success.sopClassUid_ = sopClassUid;
      success.sopInstanceUid_ = sopInstanceUid;
      success_.push_back(success);
    }
  }

  void StorageCommitmentReports::Report::AddFailure(const std::string& sopClassUid,
                                                    const std::string& sopInstanceUid,
                                                    StorageCommitmentFailureReason reason)
  {
    if (isComplete_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      Failure failure;
      failure.sopClassUid_ = sopClassUid;
      failure.sopInstanceUid_ = sopInstanceUid;
      failure.reason_ = reason;
      failures_.push_back(failure);
    }
  }

  
  StorageCommitmentReports::Report::Status StorageCommitmentReports::Report::GetStatus() const
  {
    if (!isComplete_)
    {
      return Status_Pending;
    }
    else if (failures_.empty())
    {
      return Status_Success;
    }
    else
    {
      return Status_Failure;
    }
  }


  void StorageCommitmentReports::Report::Format(Json::Value& json) const
  {
    static const char* const FIELD_STATUS = "Status";
    static const char* const FIELD_SOP_CLASS_UID = "SOPClassUID";
    static const char* const FIELD_SOP_INSTANCE_UID = "SOPInstanceUID";
    static const char* const FIELD_FAILURE_REASON = "FailureReason";
    static const char* const FIELD_DESCRIPTION = "Description";
    static const char* const FIELD_REMOTE_AET = "RemoteAET";
    static const char* const FIELD_SUCCESS = "Success";
    static const char* const FIELD_FAILURES = "Failures";

    
    json = Json::objectValue;
    json[FIELD_REMOTE_AET] = remoteAet_;

    bool pending;
    
    switch (GetStatus())
    {
      case Status_Pending:
        json[FIELD_STATUS] = "Pending";
        pending = true;
        break;

      case Status_Success:
        json[FIELD_STATUS] = "Success";
        pending = false;
        break;

      case Status_Failure:
        json[FIELD_STATUS] = "Failure";
        pending = false;
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    if (!pending)
    {
      {
        Json::Value success = Json::arrayValue;
        for (std::list<Success>::const_iterator
               it = success_.begin(); it != success_.end(); ++it)
        {
          Json::Value item = Json::objectValue;
          item[FIELD_SOP_CLASS_UID] = it->sopClassUid_;
          item[FIELD_SOP_INSTANCE_UID] = it->sopInstanceUid_;
          success.append(item);
        }

        json[FIELD_SUCCESS] = success;
      }

      {
        Json::Value failures = Json::arrayValue;
        for (std::list<Failure>::const_iterator
               it = failures_.begin(); it != failures_.end(); ++it)
        {
          Json::Value item = Json::objectValue;
          item[FIELD_SOP_CLASS_UID] = it->sopClassUid_;
          item[FIELD_SOP_INSTANCE_UID] = it->sopInstanceUid_;
          item[FIELD_FAILURE_REASON] = it->reason_;
          item[FIELD_DESCRIPTION] = EnumerationToString(it->reason_);
          failures.append(item);
        }

        json[FIELD_FAILURES] = failures;
      }
    }
  }


  void StorageCommitmentReports::Report::GetSuccessSopInstanceUids(
    std::vector<std::string>& target) const
  {
    target.clear();
    target.reserve(success_.size());

    for (std::list<Success>::const_iterator
           it = success_.begin(); it != success_.end(); ++it)
    {
      target.push_back(it->sopInstanceUid_);
    }
  }


  StorageCommitmentReports::~StorageCommitmentReports()
  {
    while (!content_.IsEmpty())
    {
      Report* report = NULL;
      content_.RemoveOldest(report);

      assert(report != NULL);
      delete report;
    }
  }

  
  void StorageCommitmentReports::Store(const std::string& transactionUid,
                                       Report* report)
  {
    std::unique_ptr<Report> protection(report);
    
    boost::mutex::scoped_lock lock(mutex_);

    {
      Report* previous = NULL;
      if (content_.Contains(transactionUid, previous))
      {
        assert(previous != NULL);
        delete previous;

        content_.Invalidate(transactionUid);
      }
    }

    assert(maxSize_ == 0 ||
           content_.GetSize() <= maxSize_);

    if (maxSize_ != 0 &&
        content_.GetSize() == maxSize_)
    {
      assert(!content_.IsEmpty());
      
      Report* oldest = NULL;
      content_.RemoveOldest(oldest);

      assert(oldest != NULL);
      delete oldest;
    }

    assert(maxSize_ == 0 ||
           content_.GetSize() < maxSize_);

    content_.Add(transactionUid, protection.release());
  }


  StorageCommitmentReports::Accessor::Accessor(StorageCommitmentReports& that,
                                               const std::string& transactionUid) :
    lock_(that.mutex_),
    transactionUid_(transactionUid)
  {
    if (that.content_.Contains(transactionUid, report_))
    {
      that.content_.MakeMostRecent(transactionUid);
    }
    else
    {
      report_ = NULL;
    }
  }

  const StorageCommitmentReports::Report&
  StorageCommitmentReports::Accessor::GetReport() const
  {
    if (report_ == NULL)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      return *report_;
    }
  }
}
