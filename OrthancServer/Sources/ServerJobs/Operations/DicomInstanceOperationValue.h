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

#include "../../../../OrthancFramework/Sources/Compatibility.h"  // For ORTHANC_OVERRIDE
#include "../../../../OrthancFramework/Sources/JobsEngine/Operations/IJobOperationValue.h"

namespace Orthanc
{
  class ServerContext;
  
  class DicomInstanceOperationValue : public IJobOperationValue
  {
  private:
    ServerContext&   context_;
    std::string      id_;

  public:
    DicomInstanceOperationValue(ServerContext& context,
                                const std::string& id) :
      context_(context),
      id_(id)
    {
    }

    virtual Type GetType() const ORTHANC_OVERRIDE
    {
      return Type_DicomInstance;
    }
    
    ServerContext& GetServerContext() const
    {
      return context_;
    }

    const std::string& GetId() const
    {
      return id_;
    }

    void ReadDicom(std::string& dicom) const;

    virtual IJobOperationValue* Clone() const ORTHANC_OVERRIDE
    {
      return new DicomInstanceOperationValue(context_, id_);
    }

    virtual void Serialize(Json::Value& target) const ORTHANC_OVERRIDE
    {
      target = Json::objectValue;
      target["Type"] = "DicomInstance";
      target["ID"] = id_;
    }
  };
}
