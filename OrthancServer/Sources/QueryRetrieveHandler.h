/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include "../../OrthancFramework/Sources/DicomNetworking/DicomFindAnswers.h"
#include "../../OrthancFramework/Sources/DicomNetworking/RemoteModalityParameters.h"

namespace Orthanc
{
  class ServerContext;
  
  class QueryRetrieveHandler : public IDynamicObject
  {
  private:
    ServerContext&             context_;
    std::string                localAet_;
    bool                       done_;
    RemoteModalityParameters   modality_;
    ResourceType               level_;
    DicomMap                   query_;
    DicomFindAnswers           answers_;
    std::string                modalityName_;
    bool                       findNormalized_;
    uint32_t                   timeout_;  // New in Orthanc 1.9.1

    void Invalidate();

  public:
    explicit QueryRetrieveHandler(ServerContext& context);

    void SetModality(const std::string& symbolicName);

    const RemoteModalityParameters& GetRemoteModality() const
    {
      return modality_;
    }

    void SetLocalAet(const std::string& localAet);

    const std::string& GetLocalAet() const
    {
      return localAet_;
    }

    const std::string& GetModalitySymbolicName() const
    {
      return modalityName_;
    }

    void SetLevel(ResourceType level);

    ResourceType GetLevel() const
    {
      return level_;
    }

    void SetQuery(const DicomTag& tag,
                  const std::string& value);

    const DicomMap& GetQuery() const
    {
      return query_;
    }

    void CopyStringTag(const DicomMap& from,
                       const DicomTag& tag);

    void Run();

    size_t GetAnswersCount();

    void GetAnswer(DicomMap& target,
                   size_t i);

    bool IsFindNormalized() const
    {
      return findNormalized_;
    }

    void SetFindNormalized(bool normalized);

    void SetTimeout(uint32_t seconds)
    {
      timeout_ = seconds;
    }

    uint32_t GetTimeout() const
    {
      return timeout_;
    }

    bool HasTimeout() const
    {
      return timeout_ != 0;
    }
  };
}
