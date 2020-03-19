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

#include "../../Core/Compatibility.h"
#include "../../Core/JobsEngine/IJob.h"
#include "../../Core/TemporaryFile.h"

#include <boost/shared_ptr.hpp>
#include <stdint.h>

namespace Orthanc
{
  class ServerContext;
  
  class ArchiveJob : public IJob
  {
  private:
    class ArchiveIndex;
    class ArchiveIndexVisitor;
    class IArchiveVisitor;
    class MediaIndexVisitor;
    class ResourceIdentifiers;
    class ZipCommands;
    class ZipWriterIterator;
    
    boost::shared_ptr<TemporaryFile>      synchronousTarget_;
    std::unique_ptr<TemporaryFile>        asynchronousTarget_;
    ServerContext&                        context_;
    boost::shared_ptr<ArchiveIndex>       archive_;
    bool                                  isMedia_;
    bool                                  enableExtendedSopClass_;
    std::string                           description_;

    boost::shared_ptr<ZipWriterIterator>  writer_;
    size_t                                currentStep_;
    unsigned int                          instancesCount_;
    uint64_t                              uncompressedSize_;
    std::string                           mediaArchiveId_;

    void FinalizeTarget();
    
  public:
    ArchiveJob(ServerContext& context,
               bool isMedia,
               bool enableExtendedSopClass);
    
    virtual ~ArchiveJob();
    
    void SetSynchronousTarget(boost::shared_ptr<TemporaryFile>& synchronousTarget);

    void SetDescription(const std::string& description);

    const std::string& GetDescription() const
    {
      return description_;
    }

    void AddResource(const std::string& publicId);

    virtual void Reset() ORTHANC_OVERRIDE;

    virtual void Start() ORTHANC_OVERRIDE;

    virtual JobStepResult Step(const std::string& jobId) ORTHANC_OVERRIDE;

    virtual void Stop(JobStopReason reason) ORTHANC_OVERRIDE
    {
    }

    virtual float GetProgress() ORTHANC_OVERRIDE;

    virtual void GetJobType(std::string& target) ORTHANC_OVERRIDE;
    
    virtual void GetPublicContent(Json::Value& value) ORTHANC_OVERRIDE;

    virtual bool Serialize(Json::Value& value) ORTHANC_OVERRIDE
    {
      return false;  // Cannot serialize this kind of job
    }

    virtual bool GetOutput(std::string& output,
                           MimeType& mime,
                           const std::string& key) ORTHANC_OVERRIDE;
  };
}
