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


#pragma once

#include "../Core/Cache/LeastRecentlyUsedIndex.h"

namespace Orthanc
{
  class StorageCommitmentReports
  {
  public:
    class Report : public boost::noncopyable
    {
    public:
      enum Status
      {
        Status_Success,
        Status_Failure,
        Status_Pending
      };
      
    private:
      struct Success
      {
        std::string  sopClassUid_;
        std::string  sopInstanceUid_;
      };
      
      struct Failure
      {
        std::string  sopClassUid_;
        std::string  sopInstanceUid_;
        StorageCommitmentFailureReason  reason_;
      };
      
      bool                isComplete_;
      std::list<Success>  success_;
      std::list<Failure>  failures_;
      std::string         remoteAet_;

    public:
      Report(const std::string& remoteAet) :
        isComplete_(false),
        remoteAet_(remoteAet)
      {
      }

      const std::string& GetRemoteAet() const
      {
        return remoteAet_;
      }

      void MarkAsComplete();

      void AddSuccess(const std::string& sopClassUid,
                      const std::string& sopInstanceUid);

      void AddFailure(const std::string& sopClassUid,
                      const std::string& sopInstanceUid,
                      StorageCommitmentFailureReason reason);

      Status GetStatus() const;

      void Format(Json::Value& json) const;

      void GetSuccessSopInstanceUids(std::vector<std::string>& target) const;
    };

  private:
    typedef LeastRecentlyUsedIndex<std::string, Report*>  Content;
    
    boost::mutex   mutex_;
    Content        content_;
    size_t         maxSize_;

  public:
    StorageCommitmentReports(size_t maxSize) :
      maxSize_(maxSize)
    {
    }

    ~StorageCommitmentReports();

    size_t GetMaxSize() const
    {
      return maxSize_;
    }

    void Store(const std::string& transactionUid,
               Report* report); // Takes ownership

    class Accessor : public boost::noncopyable
    {
    private:
      boost::mutex::scoped_lock  lock_;
      std::string                transactionUid_;
      Report                    *report_;

    public:
      Accessor(StorageCommitmentReports& that,
               const std::string& transactionUid);

      const std::string& GetTransactionUid() const
      {
        return transactionUid_;
      }

      bool IsValid() const
      {
        return report_ != NULL;
      }

      const Report& GetReport() const;
    };
  };
}
