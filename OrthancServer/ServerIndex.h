/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2013 Medical Physics Department, CHU of Liege,
 * Belgium
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

#include <boost/thread.hpp>
#include <boost/noncopyable.hpp>
#include "../Core/Cache/LeastRecentlyUsedIndex.h"
#include "../Core/SQLite/Connection.h"
#include "../Core/DicomFormat/DicomMap.h"
#include "../Core/DicomFormat/DicomInstanceHasher.h"
#include "ServerEnumerations.h"

#include "DatabaseWrapper.h"


namespace Orthanc
{
  class ServerContext;

  namespace Internals
  {
    class ServerIndexListener;
  }

  class ServerIndex : public boost::noncopyable
  {
  private:
    class Transaction;
    struct UnstableResourcePayload;

    bool done_;
    boost::mutex mutex_;
    boost::thread flushThread_;
    boost::thread unstableResourcesMonitorThread_;

    std::auto_ptr<Internals::ServerIndexListener> listener_;
    std::auto_ptr<DatabaseWrapper> db_;
    LeastRecentlyUsedIndex<int64_t, UnstableResourcePayload>  unstableResources_;

    uint64_t currentStorageSize_;
    uint64_t maximumStorageSize_;
    unsigned int maximumPatients_;

    static void FlushThread(ServerIndex* that);

    static void UnstableResourcesMonitorThread(ServerIndex* that);

    void MainDicomTagsToJson(Json::Value& result,
                             int64_t resourceId);

    SeriesStatus GetSeriesStatus(int id);

    bool IsRecyclingNeeded(uint64_t instanceSize);

    void Recycle(uint64_t instanceSize,
                 const std::string& newPatientId);

    void StandaloneRecycling();

    void MarkAsUnstable(int64_t id,
                        Orthanc::ResourceType type);

  public:
    typedef std::list<FileInfo> Attachments;

    ServerIndex(ServerContext& context,
                const std::string& dbPath);

    ~ServerIndex();

    uint64_t GetMaximumStorageSize() const
    {
      return maximumStorageSize_;
    }

    uint64_t GetMaximumPatientCount() const
    {
      return maximumPatients_;
    }

    // "size == 0" means no limit on the storage size
    void SetMaximumStorageSize(uint64_t size);

    // "count == 0" means no limit on the number of patients
    void SetMaximumPatientCount(unsigned int count);

    StoreStatus Store(const DicomMap& dicomSummary,
                      const Attachments& attachments,
                      const std::string& remoteAet);

    void ComputeStatistics(Json::Value& target);                        

    bool LookupResource(Json::Value& result,
                        const std::string& publicId,
                        ResourceType expectedType);

    bool LookupAttachment(FileInfo& attachment,
                          const std::string& instanceUuid,
                          FileContentType contentType);

    void GetAllUuids(Json::Value& target,
                     ResourceType resourceType);

    bool DeleteResource(Json::Value& target,
                        const std::string& uuid,
                        ResourceType expectedType);

    bool GetChanges(Json::Value& target,
                    int64_t since,
                    unsigned int maxResults);

    bool GetLastChange(Json::Value& target);

    void LogExportedResource(const std::string& publicId,
                             const std::string& remoteModality);

    bool GetExportedResources(Json::Value& target,
                              int64_t since,
                              unsigned int maxResults);

    bool GetLastExportedResource(Json::Value& target);

    bool IsProtectedPatient(const std::string& publicId);

    void SetProtectedPatient(const std::string& publicId,
                             bool isProtected);

    void GetChildInstances(std::list<std::string>& result,
                           const std::string& publicId);

    void SetMetadata(const std::string& publicId,
                     MetadataType type,
                     const std::string& value);

    void DeleteMetadata(const std::string& publicId,
                        MetadataType type);

    bool LookupMetadata(std::string& target,
                        const std::string& publicId,
                        MetadataType type);

    bool ListAvailableMetadata(std::list<MetadataType>& target,
                               const std::string& publicId);

    bool LookupParent(std::string& target,
                      const std::string& publicId);

    uint64_t IncrementGlobalSequence(GlobalProperty sequence);

    void LogChange(ChangeType changeType,
                   const std::string& publicId);

    void DeleteChanges();

    void DeleteExportedResources();

    void GetStatistics(Json::Value& target,
                       const std::string& publicId);
  };
}
