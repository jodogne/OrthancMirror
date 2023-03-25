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

#include "../../../OrthancFramework/Sources/JobsEngine/SetOfInstancesJob.h"

namespace Orthanc
{
  class ServerContext;
  
  class CleaningInstancesJob : public SetOfInstancesJob
  {
  private:
    ServerContext&  context_;
    bool            keepSource_;
    
  protected:
    virtual bool HandleTrailingStep() ORTHANC_OVERRIDE;
    
  public:
    CleaningInstancesJob(ServerContext& context,
                         bool keepSource) :
      context_(context),
      keepSource_(keepSource)
    {
    }

    CleaningInstancesJob(ServerContext& context,
                         const Json::Value& serialized,
                         bool defaultKeepSource);

    ServerContext& GetContext() const
    {
      return context_;
    }
    
    bool IsKeepSource() const
    {
      return keepSource_;
    }
    
    void SetKeepSource(bool keep);

    virtual bool Serialize(Json::Value& target) ORTHANC_OVERRIDE;

    virtual void Start() ORTHANC_OVERRIDE;
  };
}
