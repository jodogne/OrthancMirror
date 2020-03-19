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

#include "../Core/Cache/LeastRecentlyUsedIndex.h"
#include "../Core/DicomFormat/DicomMap.h"

#include "Database/IDatabaseWrapper.h"

#include <boost/thread.hpp>
#include <boost/noncopyable.hpp>

namespace Orthanc
{
  class DatabaseLookup;
  class DicomInstanceToStore;
  class ParsedDicomFile;
  class ServerContext;

  class ServerIndex : public boost::noncopyable
  {
  public:
    typedef std::list<FileInfo> Attachments;
    typedef std::map<std::pair<ResourceType, MetadataType>, std::string>  MetadataMap;

  private:
    class Listener;
    class Transaction;
    class UnstableResourcePayload;
    class MainDicomTagsRegistry;

    bool done_;
    boost::mutex mutex_;
    boost::thread flushThread_;
    boost::thread unstableResourcesMonitorThread_;

    std::unique_ptr<Listener> listener_;
    IDatabaseWrapper& db_;
    LeastRecentlyUsedIndex<int64_t, UnstableResourcePayload>  unstableResources_;

    uint64_t     maximumStorageSize_;
    unsigned int maximumPatients_;
    bool         overwrite_;
    std::unique_ptr<MainDicomTagsRegistry>  mainDicomTagsRegistry_;

    static void FlushThread(ServerIndex* that,
                            unsigned int threadSleep);

    static void UnstableResourcesMonitorThread(ServerIndex* that,
                                               unsigned int threadSleep);

    void MainDicomTagsToJson(Json::Value& result,
                             int64_t resourceId,
                             ResourceType resourceType);

    bool IsRecyclingNeeded(uint64_t instanceSize);

    void Recycle(uint64_t instanceSize,
                 const std::string& newPatientId);

    void StandaloneRecycling();

    void MarkAsUnstable(int64_t id,
                        Orthanc::ResourceType type,
                        const std::string& publicId);

    void LogChange(int64_t internalId,
                   ChangeType changeType,
                   ResourceType resourceType,
                   const std::string& publicId);

    void SignalNewResource(ChangeType changeType,
                           ResourceType level,
                           const std::string& publicId,
                           int64_t internalId);

    uint64_t IncrementGlobalSequenceInternal(GlobalProperty property);

    void NormalizeLookup(std::vector<DatabaseConstraint>& target,
                         const DatabaseLookup& source,
                         ResourceType level) const;

    SeriesStatus GetSeriesStatus(int64_t id,
                                 int64_t expectedNumberOfInstances);

  public:
    ServerIndex(ServerContext& context,
                IDatabaseWrapper& database,
                unsigned int threadSleep);

    ~ServerIndex();

    void Stop();

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

    void SetOverwriteInstances(bool overwrite);

    StoreStatus Store(std::map<MetadataType, std::string>& instanceMetadata,
                      DicomInstanceToStore& instance,
                      const Attachments& attachments);

    void GetGlobalStatistics(/* out */ uint64_t& diskSize,
                             /* out */ uint64_t& uncompressedSize,
                             /* out */ uint64_t& countPatients, 
                             /* out */ uint64_t& countStudies, 
                             /* out */ uint64_t& countSeries, 
                             /* out */ uint64_t& countInstances);

    bool LookupResource(Json::Value& result,
                        const std::string& publicId,
                        ResourceType expectedType);

    bool LookupAttachment(FileInfo& attachment,
                          const std::string& instanceUuid,
                          FileContentType contentType);

    void GetAllUuids(std::list<std::string>& target,
                     ResourceType resourceType);

    void GetAllUuids(std::list<std::string>& target,
                     ResourceType resourceType,
                     size_t since,
                     size_t limit);

    bool DeleteResource(Json::Value& target /* out */,
                        const std::string& uuid,
                        ResourceType expectedType);

    void GetChanges(Json::Value& target,
                    int64_t since,
                    unsigned int maxResults);

    void GetLastChange(Json::Value& target);

    void LogExportedResource(const std::string& publicId,
                             const std::string& remoteModality);

    void GetExportedResources(Json::Value& target,
                              int64_t since,
                              unsigned int maxResults);

    void GetLastExportedResource(Json::Value& target);

    bool IsProtectedPatient(const std::string& publicId);

    void SetProtectedPatient(const std::string& publicId,
                             bool isProtected);

    void GetChildren(std::list<std::string>& result,
                     const std::string& publicId);

    void GetChildInstances(std::list<std::string>& result,
                           const std::string& publicId);

    void SetMetadata(const std::string& publicId,
                     MetadataType type,
                     const std::string& value);

    void DeleteMetadata(const std::string& publicId,
                        MetadataType type);

    void GetAllMetadata(std::map<MetadataType, std::string>& target,
                        const std::string& publicId);

    bool LookupMetadata(std::string& target,
                        const std::string& publicId,
                        MetadataType type);

    void ListAvailableAttachments(std::list<FileContentType>& target,
                                  const std::string& publicId,
                                  ResourceType expectedType);

    bool LookupParent(std::string& target,
                      const std::string& publicId);

    uint64_t IncrementGlobalSequence(GlobalProperty sequence);

    void LogChange(ChangeType changeType,
                   const std::string& publicId);

    void DeleteChanges();

    void DeleteExportedResources();

    void GetResourceStatistics(/* out */ ResourceType& type,
                               /* out */ uint64_t& diskSize, 
                               /* out */ uint64_t& uncompressedSize, 
                               /* out */ unsigned int& countStudies, 
                               /* out */ unsigned int& countSeries, 
                               /* out */ unsigned int& countInstances, 
                               /* out */ uint64_t& dicomDiskSize, 
                               /* out */ uint64_t& dicomUncompressedSize, 
                               const std::string& publicId);

    void LookupIdentifierExact(std::vector<std::string>& result,
                               ResourceType level,
                               const DicomTag& tag,
                               const std::string& value);

    StoreStatus AddAttachment(const FileInfo& attachment,
                              const std::string& publicId);

    void DeleteAttachment(const std::string& publicId,
                          FileContentType type);

    void SetGlobalProperty(GlobalProperty property,
                           const std::string& value);

    bool LookupGlobalProperty(std::string& value,
                              GlobalProperty property);

    std::string GetGlobalProperty(GlobalProperty property,
                                  const std::string& defaultValue);

    bool GetMainDicomTags(DicomMap& result,
                          const std::string& publicId,
                          ResourceType expectedType,
                          ResourceType levelOfInterest);

    // Only applicable at the instance level
    bool GetAllMainDicomTags(DicomMap& result,
                             const std::string& instancePublicId);

    bool LookupResourceType(ResourceType& type,
                            const std::string& publicId);

    unsigned int GetDatabaseVersion();

    bool LookupParent(std::string& target,
                      const std::string& publicId,
                      ResourceType parentType);

    void ReconstructInstance(ParsedDicomFile& dicom);

    void ApplyLookupResources(std::vector<std::string>& resourcesId,
                              std::vector<std::string>* instancesId,  // Can be NULL if not needed
                              const DatabaseLookup& lookup,
                              ResourceType queryLevel,
                              size_t limit);
  };
}
