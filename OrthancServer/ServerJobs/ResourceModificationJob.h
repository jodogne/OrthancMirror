/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
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

#include "../../Core/JobsEngine/SetOfInstancesJob.h"
#include "../ServerContext.h"

namespace Orthanc
{
  class ResourceModificationJob : public SetOfInstancesJob
  {
  public:
    class Output : public boost::noncopyable
    {
    private:
      boost::mutex  mutex_;
      ResourceType  level_;
      bool          isFirst_;
      std::string   id_;
      std::string   patientId_;

    public:
      Output(ResourceType  level);

      ResourceType GetLevel() const
      {
        return level_;
      }

      void Update(DicomInstanceHasher& hasher);

      bool Format(Json::Value& target);

      bool GetIdentifier(std::string& id);
    };
    
  private:
    ServerContext&                    context_;
    std::auto_ptr<DicomModification>  modification_;
    boost::shared_ptr<Output>         output_;
    bool                              isAnonymization_;
    DicomInstanceOrigin               origin_;

  protected:
    virtual bool HandleInstance(const std::string& instance);
    
  public:
    ResourceModificationJob(ServerContext& context) :
      context_(context),
      isAnonymization_(false)
    {
    }

    ResourceModificationJob(ServerContext& context,
                            const Json::Value& serialized);

    void SetModification(DicomModification* modification,   // Takes ownership
                         bool isAnonymization);

    void SetOutput(boost::shared_ptr<Output>& output);

    void SetOrigin(const DicomInstanceOrigin& origin);

    void SetOrigin(const RestApiCall& call);

    const DicomModification& GetModification() const;

    bool IsAnonymization() const
    {
      return isAnonymization_;
    }

    const DicomInstanceOrigin& GetOrigin() const
    {
      return origin_;
    }

    virtual void ReleaseResources()
    {
    }

    virtual void GetJobType(std::string& target)
    {
      target = "ResourceModification";
    }

    virtual void GetPublicContent(Json::Value& value);
    
    virtual bool Serialize(Json::Value& value);
  };
}
