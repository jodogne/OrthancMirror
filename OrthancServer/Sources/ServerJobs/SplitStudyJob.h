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

#include "../../../OrthancFramework/Sources/DicomFormat/DicomTag.h"
#include "../DicomInstanceOrigin.h"
#include "CleaningInstancesJob.h"

namespace Orthanc
{
  class ServerContext;
  
  class SplitStudyJob : public CleaningInstancesJob
  {
  private:
    typedef std::map<std::string, std::string>  SeriesUidMap;
    typedef std::map<DicomTag, std::string>     Replacements;
    
    
    std::set<DicomTag>     allowedTags_;
    std::string            sourceStudy_;
    std::string            targetStudy_;
    std::string            targetStudyUid_;
    SeriesUidMap           seriesUidMap_;
    DicomInstanceOrigin    origin_;
    Replacements           replacements_;
    std::set<DicomTag>     removals_;

    void CheckAllowedTag(const DicomTag& tag) const;
    
    void Setup();

    // Make setter methods private to prevent incorrect calls
    using SetOfInstancesJob::AddParentResource;
    using SetOfInstancesJob::AddInstance;
    
  protected:
    virtual bool HandleInstance(const std::string& instance) ORTHANC_OVERRIDE;

  public:
    SplitStudyJob(ServerContext& context,
                  const std::string& sourceStudy);

    SplitStudyJob(ServerContext& context,
                  const Json::Value& serialized);

    const std::string& GetSourceStudy() const
    {
      return sourceStudy_;
    }

    const std::string& GetTargetStudy() const
    {
      return targetStudy_;
    }

    const std::string& GetTargetStudyUid() const
    {
      return targetStudyUid_;
    }

    void AddSourceSeries(const std::string& series);

    void AddSourceInstance(const std::string& instance);  // New in Orthanc 1.9.4
    
    bool LookupTargetSeriesUid(std::string& uid,
                               const std::string& series) const;

    void Replace(const DicomTag& tag,
                 const std::string& value);
    
    bool LookupReplacement(std::string& value,
                           const DicomTag& tag) const;

    void Remove(const DicomTag& tag);
    
    bool IsRemoved(const DicomTag& tag) const
    {
      return removals_.find(tag) != removals_.end();
    }

    void SetOrigin(const DicomInstanceOrigin& origin);

    void SetOrigin(const RestApiCall& call);

    const DicomInstanceOrigin& GetOrigin() const
    {
      return origin_;
    }

    virtual void Stop(JobStopReason reason) ORTHANC_OVERRIDE
    {
    }

    virtual void GetJobType(std::string& target) ORTHANC_OVERRIDE
    {
      target = "SplitStudy";
    }

    virtual void GetPublicContent(Json::Value& value) ORTHANC_OVERRIDE;

    virtual bool Serialize(Json::Value& target) ORTHANC_OVERRIDE;
  };
}
