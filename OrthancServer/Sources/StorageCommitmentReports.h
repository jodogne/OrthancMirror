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


#pragma once

#include "../../OrthancFramework/Sources/Cache/LeastRecentlyUsedIndex.h"

#include <boost/thread/mutex.hpp>

namespace Orthanc
{
  class StorageCommitmentReports : public boost::noncopyable
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
      explicit Report(const std::string& remoteAet) :
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
    explicit StorageCommitmentReports(size_t maxSize) :
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
