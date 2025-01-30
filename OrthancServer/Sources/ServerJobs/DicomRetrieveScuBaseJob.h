/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include "../../../OrthancFramework/Sources/Compatibility.h"
#include "../../../OrthancFramework/Sources/DicomNetworking/DicomControlUserConnection.h"
#include "../../../OrthancFramework/Sources/JobsEngine/SetOfCommandsJob.h"
#include <boost/thread/mutex.hpp>

#include "../QueryRetrieveHandler.h"

namespace Orthanc
{
  class ServerContext;

  class DicomRetrieveScuBaseJob : public SetOfCommandsJob, public DicomControlUserConnection::IProgressListener
  {
  protected:
    class Command : public SetOfCommandsJob::ICommand
    {
    private:
      DicomRetrieveScuBaseJob &that_;
      std::unique_ptr<DicomMap> findAnswer_;

    public:
      Command(DicomRetrieveScuBaseJob &that,
              const DicomMap &findAnswer) :
      that_(that),
      findAnswer_(findAnswer.Clone())
      {
      }

      virtual bool Execute(const std::string &jobId) ORTHANC_OVERRIDE
      {
        that_.Retrieve(*findAnswer_);
        return true;
      }

      virtual void Serialize(Json::Value &target) const ORTHANC_OVERRIDE
      {
        findAnswer_->Serialize(target);
      }
    };

    class Unserializer : public SetOfCommandsJob::ICommandUnserializer
    {
    protected:
      DicomRetrieveScuBaseJob &that_;

    public:
      explicit Unserializer(DicomRetrieveScuBaseJob &that) : 
      that_(that)
      {
      }

      virtual ICommand *Unserialize(const Json::Value &source) const ORTHANC_OVERRIDE
      {
        DicomMap findAnswer;
        findAnswer.Unserialize(source);
        return new Command(that_, findAnswer);
      }
    };

    ServerContext &context_;
    DicomAssociationParameters parameters_;
    DicomFindAnswers query_;
    DicomToJsonFormat queryFormat_; // New in 1.9.5

    std::unique_ptr<DicomControlUserConnection> connection_;

    mutable boost::mutex progressMutex_;
    uint16_t nbRemainingSubOperations_;
    uint16_t nbCompletedSubOperations_;
    uint16_t nbFailedSubOperations_;
    uint16_t nbWarningSubOperations_;

    virtual void Retrieve(const DicomMap &findAnswer) = 0;

    explicit DicomRetrieveScuBaseJob(ServerContext &context);

    DicomRetrieveScuBaseJob(ServerContext &context,
                            const Json::Value &serialized);

  public:
    virtual void AddFindAnswer(const DicomMap &answer);

    void AddQuery(const DicomMap& query);

    void AddFindAnswer(QueryRetrieveHandler &query,
                       size_t i);

    const DicomAssociationParameters &GetParameters() const
    {
      return parameters_;
    }

    void SetLocalAet(const std::string &aet);

    void SetRemoteModality(const RemoteModalityParameters &remote);

    void SetTimeout(uint32_t timeout);

    void SetQueryFormat(DicomToJsonFormat format);

    DicomToJsonFormat GetQueryFormat() const
    {
      return queryFormat_;
    }

    virtual void Stop(JobStopReason reason) ORTHANC_OVERRIDE;

    virtual void GetPublicContent(Json::Value &value) const ORTHANC_OVERRIDE;

    virtual bool Serialize(Json::Value &target) const ORTHANC_OVERRIDE;

    virtual void OnProgressUpdated(uint16_t nbRemainingSubOperations,
                                    uint16_t nbCompletedSubOperations,
                                    uint16_t nbFailedSubOperations,
                                    uint16_t nbWarningSubOperations) ORTHANC_OVERRIDE;

    virtual float GetProgress() const ORTHANC_OVERRIDE;

    virtual bool NeedsProgressUpdateBetweenSteps() const ORTHANC_OVERRIDE
    {
      return true;
    }

  };
}
