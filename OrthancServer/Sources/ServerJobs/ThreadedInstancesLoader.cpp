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

#include "ThreadedInstancesLoader.h"

#include "../ServerContext.h"
#include "../../../OrthancFramework/Sources/Logging.h"
#include "../../../OrthancFramework/Sources/DicomParsing/IDicomTranscoder.h"
#include "../../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"

static boost::mutex loaderThreadsCounterMutex;
static uint32_t loaderThreadsCounter = 0;


namespace Orthanc
{
  class InstanceToPreload : public Orthanc::IDynamicObject
  {
  private:
    std::string id_;
    FileInfo    fileInfo_;

  public:
    InstanceToPreload(const std::string& id,
                      const FileInfo& fileInfo) :
      id_(id),
      fileInfo_(fileInfo)
    {
    }

    const std::string& GetId() const
    {
      return id_;
    }

    const FileInfo& GetFileInfo() const
    {
      return fileInfo_;
    }
  };


  ThreadedInstancesLoader::ThreadedInstancesLoader(ServerContext& context,
                                                   size_t threadCount,
                                                   bool transcode,
                                                   DicomTransferSyntax transferSyntax,
                                                   unsigned int lossyQuality,
                                                   const std::string& nameForLogs4Char) :
    context_(context),
    transcode_(transcode),
    transferSyntax_(transferSyntax),
    lossyQuality_(lossyQuality),
    nameForLogs4Char_(nameForLogs4Char),
    availableInstancesSemaphore_(3 * threadCount),
    instancesToPreload_(0), // no limit on the message queue, the flow control is performed by the availableInstancesSemaphore_
    loadersShouldStop_(false)
  {
    assert(nameForLogs4Char_.size() <= 4);

    if (threadCount < 1)
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    for (size_t i = 0; i < threadCount; i++)
    {
      threads_.push_back(new boost::thread(PreloaderWorkerThread, this));
    }
  }


  ThreadedInstancesLoader::~ThreadedInstancesLoader()
  {
    try
    {
      ThreadedInstancesLoader::Clear(false);
    }
    catch (const OrthancException& e)
    {
      // Don't throw exceptions in destructors
      LOG(ERROR) << "Exception: " << e.What();
    }
  }


  void ThreadedInstancesLoader::Clear(bool isAbort)
  {
    if (threads_.size() > 0)
    {
      loadersShouldStop_ = true; // no need to protect this by a mutex. This is the only "writer" and all loaders are "readers"

      if (isAbort)
      {
        LOG(INFO) << "Cancelling the loader threads";
        instancesToPreload_.Clear();
      }
      else
      {
        LOG(INFO) << "Waiting for loader threads to complete";
      }

      // unlock the loaders if they are waiting on this message queue (this happens when the job completes sucessfully)
      for (size_t i = 0; i < threads_.size(); i++)
      {
        instancesToPreload_.Enqueue(NULL);
      }

      // unlock the loaders if they are waiting for room in the availableInstances (this happens when the job is interrupted)
      availableInstancesSemaphore_.Release(threads_.size());

      for (size_t i = 0; i < threads_.size(); i++)
      {
        if (threads_[i]->joinable())
        {
          threads_[i]->join();
        }
        delete threads_[i];
      }

      threads_.clear();
      availableInstances_.clear();

      LOG(INFO) << "Waiting for loader threads to complete - done";
    }
  }


  void ThreadedInstancesLoader::PreloaderWorkerThread(ThreadedInstancesLoader* that)
  {
    {
      boost::mutex::scoped_lock lock(loaderThreadsCounterMutex);
      Logging::SetCurrentThreadName(that->nameForLogs4Char_ + std::string("-LOAD-") +
                                    boost::lexical_cast<std::string>(loaderThreadsCounter++));
      loaderThreadsCounter %= 1000000;
    }

    LOG(INFO) << "Loader thread has started";

    while (true)
    {
      that->availableInstancesSemaphore_.Acquire(1); // reserve the slot early (since the instances are ordered, it is important that a single worker does not push dozens of small instances while we are waiting for a slot for a big instance)

      std::unique_ptr<InstanceToPreload> instanceToPreload(dynamic_cast<InstanceToPreload*>(that->instancesToPreload_.Dequeue(0)));
      if (instanceToPreload.get() == NULL || that->loadersShouldStop_)  // that's the signal to exit the thread
      {
        LOG(INFO) << "Loader thread has completed";
        return;
      }

      const std::string& instanceId = instanceToPreload->GetId();

      try
      {
        boost::shared_ptr<std::string> dicomContent(new std::string());
        that->context_.ReadAttachment(*dicomContent, instanceToPreload->GetFileInfo(), true);

        if (that->transcode_)
        {
          boost::shared_ptr<std::string> transcodedDicom(new std::string());
          if (that->TranscodeDicom(*transcodedDicom, *dicomContent, instanceId))
          {
            dicomContent = transcodedDicom;
          }
        }

        {
          boost::mutex::scoped_lock lock(that->availableInstancesMutex_);
          that->availableInstances_[instanceId] = dicomContent;
          that->condInstanceAvailable_.notify_one();
        }
      }
      catch (OrthancException& e)
      {
        LOG(ERROR) << "Failed to load instance " << instanceId << " error: " << e.GetDetails();
        boost::mutex::scoped_lock lock(that->availableInstancesMutex_);
        // store a NULL result to notify that we could not read the instance
        that->availableInstances_[instanceId] = boost::shared_ptr<std::string>();
        that->condInstanceAvailable_.notify_one();
      }
      catch (...)
      {
        LOG(ERROR) << "Failed to load instance " << instanceId << " unknown error";
        boost::mutex::scoped_lock lock(that->availableInstancesMutex_);
        // store a NULL result to notify that we could not read the instance
        that->availableInstances_[instanceId] = boost::shared_ptr<std::string>();
        that->condInstanceAvailable_.notify_one();
      }
    }
  }


  void ThreadedInstancesLoader::PreloadDicomInstance(const std::string& instanceId,
                                                     const FileInfo& fileInfo)
  {
    instancesToPreload_.Enqueue(new InstanceToPreload(instanceId, fileInfo));
  }


  void ThreadedInstancesLoader::WaitDicomInstance(std::string& dicom,
                                                  const std::string& instanceId)
  {
    boost::mutex::scoped_lock lock(availableInstancesMutex_);

    // wait for this instance to be available but this might not be the one we are waiting for !
    while (availableInstances_.find(instanceId) == availableInstances_.end())
    {
      condInstanceAvailable_.wait(lock);
    }

    boost::shared_ptr<std::string> dicomContent;

    // this is the instance we were waiting for
    dicomContent = availableInstances_[instanceId];
    availableInstances_.erase(instanceId);
    availableInstancesSemaphore_.Release(1);

    if (dicomContent.get() == NULL)  // there has been an error while reading the file
    {
      throw OrthancException(ErrorCode_InexistentItem);
    }

    dicom.swap(*dicomContent);
  }


  bool ThreadedInstancesLoader::TranscodeDicom(std::string& transcodedBuffer,
                                               const std::string& sourceBuffer,
                                               const std::string& instanceId)
  {
    if (transcode_)
    {
      std::set<DicomTransferSyntax> syntaxes;
      syntaxes.insert(transferSyntax_);

      IDicomTranscoder::DicomImage source, transcoded;
      source.SetExternalBuffer(sourceBuffer);

      if (context_.Transcode(transcoded, source, syntaxes, TranscodingSopInstanceUidMode_AllowNew, lossyQuality_))
      {
        transcodedBuffer.assign(reinterpret_cast<const char*>(transcoded.GetBufferData()), transcoded.GetBufferSize());
        return true;
      }
      else
      {
        LOG(INFO) << "Cannot transcode instance " << instanceId
                  << " to transfer syntax: " << GetTransferSyntaxUid(transferSyntax_);
      }
    }

    return false;
  }
}
