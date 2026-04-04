/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include <string>
#include <boost/noncopyable.hpp>
#include "../../../OrthancFramework/Sources/Compatibility.h"
#include "../../../OrthancFramework/Sources/MultiThreading/BlockingSharedMessageQueue.h"
#include "../../../OrthancFramework/Sources/Enumerations.h"
#include "../../../OrthancFramework/Sources/MultiThreading/Semaphore.h"
#include "../../../OrthancFramework/Sources/FileStorage/FileInfo.h"


namespace Orthanc
{
  class ServerContext;

  class ORTHANC_PUBLIC ThreadedInstancesLoader : public boost::noncopyable
  {
    boost::condition_variable           condInstanceAvailable_;
    std::map<std::string, boost::shared_ptr<std::string> >  availableInstances_;
    boost::mutex                        availableInstancesMutex_;
    Semaphore                           availableInstancesSemaphore_;
    BlockingSharedMessageQueue          instancesToPreload_;
    std::vector<boost::thread*>         threads_;
    bool                                loadersShouldStop_;
    std::string                         nameForLogs4Char_;

  protected:
    ServerContext&                        context_;
    bool                                  transcode_;
    DicomTransferSyntax                   transferSyntax_;
    unsigned int                          lossyQuality_;

  public:
    ThreadedInstancesLoader(ServerContext& context, size_t threadCount, bool transcode, DicomTransferSyntax transferSyntax, unsigned int lossyQuality, const std::string& nameForLogs4Char);

    ~ThreadedInstancesLoader();

    void Clear(bool isAbort);

    static void PreloaderWorkerThread(ThreadedInstancesLoader* that);

    void PrepareDicom(const std::string& instanceId, const FileInfo& fileInfo);

    void GetDicom(std::string& dicom, const std::string& instanceId);

  protected:
    bool TranscodeDicom(std::string& transcodedBuffer, const std::string& sourceBuffer, const std::string& instanceId);
  };
}
 