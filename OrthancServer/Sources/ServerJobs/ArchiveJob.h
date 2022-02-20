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

#include "../../../OrthancFramework/Sources/Compatibility.h"
#include "../../../OrthancFramework/Sources/Compression/ZipWriter.h"
#include "../../../OrthancFramework/Sources/JobsEngine/IJob.h"
#include "../../../OrthancFramework/Sources/TemporaryFile.h"

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
    class InstanceLoader;
    class SynchronousInstanceLoader;
    class ThreadedInstanceLoader;

    std::unique_ptr<ZipWriter::IOutputStream>  synchronousTarget_;  // Only valid before "Start()"
    std::unique_ptr<TemporaryFile>        asynchronousTarget_;
    ServerContext&                        context_;
    std::unique_ptr<InstanceLoader>       instanceLoader_;
    boost::shared_ptr<ArchiveIndex>       archive_;
    bool                                  isMedia_;
    bool                                  enableExtendedSopClass_;
    std::string                           description_;

    boost::shared_ptr<ZipWriterIterator>  writer_;
    size_t                                currentStep_;
    unsigned int                          instancesCount_;
    uint64_t                              uncompressedSize_;
    uint64_t                              archiveSize_;
    std::string                           mediaArchiveId_;

    // New in Orthanc 1.7.0
    bool                 transcode_;
    DicomTransferSyntax  transferSyntax_;

    // New in Orthanc 1.10.0
    unsigned int         loaderThreads_;

    void FinalizeTarget();
    
  public:
    ArchiveJob(ServerContext& context,
               bool isMedia,
               bool enableExtendedSopClass);
    
    virtual ~ArchiveJob();

    void AcquireSynchronousTarget(ZipWriter::IOutputStream* synchronousTarget);

    void SetDescription(const std::string& description);

    const std::string& GetDescription() const
    {
      return description_;
    }

    void AddResource(const std::string& publicId);

    void SetTranscode(DicomTransferSyntax transferSyntax);

    void SetLoaderThreads(unsigned int loaderThreads);

    virtual void Reset() ORTHANC_OVERRIDE;

    virtual void Start() ORTHANC_OVERRIDE;

    virtual JobStepResult Step(const std::string& jobId) ORTHANC_OVERRIDE;

    virtual void Stop(JobStopReason reason) ORTHANC_OVERRIDE;

    virtual float GetProgress() ORTHANC_OVERRIDE;

    virtual void GetJobType(std::string& target) ORTHANC_OVERRIDE;
    
    virtual void GetPublicContent(Json::Value& value) ORTHANC_OVERRIDE;

    virtual bool Serialize(Json::Value& value) ORTHANC_OVERRIDE
    {
      return false;  // Cannot serialize this kind of job
    }

    virtual bool GetOutput(std::string& output,
                           MimeType& mime,
                           std::string& filename,
                           const std::string& key) ORTHANC_OVERRIDE;
  };
}
