/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include "../../../OrthancFramework/Sources/DicomFormat/DicomMap.h"

#include "../DicomInstanceOrigin.h"
#include "IDatabaseWrapper.h"
#include "MainDicomTagsRegistry.h"

#include <boost/shared_ptr.hpp>
#include <boost/thread/shared_mutex.hpp>


namespace Orthanc
{
  class DatabaseLookup;
  class ParsedDicomFile;
  struct ServerIndexChange;

  class StatelessDatabaseOperations : public boost::noncopyable
  {
  public:
    typedef std::list<FileInfo> Attachments;
    typedef std::map<std::pair<ResourceType, MetadataType>, std::string>  MetadataMap;

    enum LabelOperation
    {
      LabelOperation_Add,
      LabelOperation_Remove
    };

    class ITransactionContext : public IDatabaseListener
    {
    public:
      virtual ~ITransactionContext()
      {
      }

      virtual void Commit() = 0;

      virtual int64_t GetCompressedSizeDelta() = 0;

      virtual bool IsUnstableResource(Orthanc::ResourceType type,
                                      int64_t id) = 0;

      virtual bool LookupRemainingLevel(std::string& remainingPublicId /* out */,
                                        ResourceType& remainingLevel   /* out */) = 0;

      virtual void MarkAsUnstable(Orthanc::ResourceType type,
                                  int64_t id,
                                  const std::string& publicId) = 0;

      virtual void SignalAttachmentsAdded(uint64_t compressedSize) = 0;

      virtual void SignalChange(const ServerIndexChange& change) = 0;
    };

    
    class ITransactionContextFactory : public boost::noncopyable
    {
    public:
      virtual ~ITransactionContextFactory()
      {
      }

      // WARNING: This method can be invoked from several threads concurrently
      virtual ITransactionContext* Create() = 0;
    };


    class ReadOnlyTransaction : public boost::noncopyable
    {
    private:
      ITransactionContext&  context_;

    protected:
      IDatabaseWrapper::ITransaction&  transaction_;
      
    public:
      explicit ReadOnlyTransaction(IDatabaseWrapper::ITransaction& transaction,
                                   ITransactionContext& context) :
        context_(context),
        transaction_(transaction)
      {
      }

      ITransactionContext& GetTransactionContext()
      {
        return context_;
      }

      /**
       * Higher-level constructions
       **/

      SeriesStatus GetSeriesStatus(int64_t id,
                                   int64_t expectedNumberOfInstances);

      
      /**
       * Read-only methods from "IDatabaseWrapper"
       **/

      void GetAllMetadata(std::map<MetadataType, std::string>& target,
                          int64_t id)
      {
        transaction_.GetAllMetadata(target, id);
      }

      void GetChanges(std::list<ServerIndexChange>& target /*out*/,
                      bool& done /*out*/,
                      int64_t since,
                      uint32_t limit)
      {
        transaction_.GetChanges(target, done, since, limit);
      }

      void GetChangesExtended(std::list<ServerIndexChange>& target /*out*/,
                              bool& done /*out*/,
                              int64_t since,
                              int64_t to,
                              uint32_t limit,
                              const std::set<ChangeType>& filterType)
      {
        transaction_.GetChangesExtended(target, done, since, to, limit, filterType);
      }

      void GetChildrenInternalId(std::list<int64_t>& target,
                                 int64_t id)
      {
        transaction_.GetChildrenInternalId(target, id);
      }

      void GetExportedResources(std::list<ExportedResource>& target /*out*/,
                                bool& done /*out*/,
                                int64_t since,
                                uint32_t limit)
      {
        return transaction_.GetExportedResources(target, done, since, limit);
      }

      void GetLastChange(std::list<ServerIndexChange>& target /*out*/)
      {
        transaction_.GetLastChange(target);
      }

      void GetLastExportedResource(std::list<ExportedResource>& target /*out*/)
      {
        return transaction_.GetLastExportedResource(target);
      }

      int64_t GetLastChangeIndex()
      {
        return transaction_.GetLastChangeIndex();
      }

      void GetMainDicomTags(DicomMap& map,
                            int64_t id)
      {
        transaction_.GetMainDicomTags(map, id);
      }

      std::string GetPublicId(int64_t resourceId)
      {
        return transaction_.GetPublicId(resourceId);
      }
      
      uint64_t GetResourcesCount(ResourceType resourceType)
      {
        return transaction_.GetResourcesCount(resourceType);
      }
      
      ResourceType GetResourceType(int64_t resourceId)
      {
        return transaction_.GetResourceType(resourceId);
      }

      uint64_t GetTotalCompressedSize()
      {
        return transaction_.GetTotalCompressedSize();
      }
    
      uint64_t GetTotalUncompressedSize()
      {
        return transaction_.GetTotalUncompressedSize();
      }
      
      bool IsProtectedPatient(int64_t internalId)
      {
        return transaction_.IsProtectedPatient(internalId);
      }

      void ListAvailableAttachments(std::set<FileContentType>& target,
                                    int64_t id)
      {
        transaction_.ListAvailableAttachments(target, id);
      }

      bool LookupAttachment(FileInfo& attachment,
                            int64_t& revision,
                            int64_t id,
                            FileContentType contentType)
      {
        return transaction_.LookupAttachment(attachment, revision, id, contentType);
      }
      
      bool LookupGlobalProperty(std::string& target,
                                GlobalProperty property,
                                bool shared)
      {
        return transaction_.LookupGlobalProperty(target, property, shared);
      }

      bool LookupMetadata(std::string& target,
                          int64_t& revision,
                          int64_t id,
                          MetadataType type)
      {
        return transaction_.LookupMetadata(target, revision, id, type);
      }

      bool LookupParent(int64_t& parentId,
                        int64_t resourceId)
      {
        return transaction_.LookupParent(parentId, resourceId);
      }
        
      bool LookupResource(int64_t& id,
                          ResourceType& type,
                          const std::string& publicId)
      {
        return transaction_.LookupResource(id, type, publicId);
      }

      void ListAllLabels(std::set<std::string>& target)
      {
        transaction_.ListAllLabels(target);
      }

      bool HasReachedMaxStorageSize(uint64_t maximumStorageSize,
                                    uint64_t addedInstanceSize);

      bool HasReachedMaxPatientCount(unsigned int maximumPatientCount,
                                     const std::string& patientId);

      void ExecuteCount(uint64_t& count,
                        const FindRequest& request,
                        const IDatabaseWrapper::Capabilities& capabilities)
      {
        transaction_.ExecuteCount(count, request, capabilities);
      }

      void ExecuteFind(FindResponse& response,
                       const FindRequest& request,
                       const IDatabaseWrapper::Capabilities& capabilities)
      {
        transaction_.ExecuteFind(response, request, capabilities);
      }

      void ExecuteFind(std::list<std::string>& identifiers,
                       const IDatabaseWrapper::Capabilities& capabilities,
                       const FindRequest& request)
      {
        transaction_.ExecuteFind(identifiers, capabilities, request);
      }

      void ExecuteExpand(FindResponse& response,
                         const IDatabaseWrapper::Capabilities& capabilities,
                         const FindRequest& request,
                         const std::string& identifier)
      {
        transaction_.ExecuteExpand(response, capabilities, request, identifier);
      }
    };


    class ReadWriteTransaction : public ReadOnlyTransaction
    {
    public:
      ReadWriteTransaction(IDatabaseWrapper::ITransaction& transaction,
                           ITransactionContext& context) :
        ReadOnlyTransaction(transaction, context)
      {
      }

      void AddAttachment(int64_t id,
                         const FileInfo& attachment,
                         int64_t revision)
      {
        transaction_.AddAttachment(id, attachment, revision);
      }
      
      void ClearChanges()
      {
        transaction_.ClearChanges();
      }

      void ClearExportedResources()
      {
        transaction_.ClearExportedResources();
      }

      void ClearMainDicomTags(int64_t id)
      {
        return transaction_.ClearMainDicomTags(id);
      }

      bool CreateInstance(IDatabaseWrapper::CreateInstanceResult& result, /* out */
                          int64_t& instanceId,          /* out */
                          const std::string& patient,
                          const std::string& study,
                          const std::string& series,
                          const std::string& instance)
      {
        return transaction_.CreateInstance(result, instanceId, patient, study, series, instance);
      }

      void DeleteAttachment(int64_t id,
                            FileContentType attachment)
      {
        return transaction_.DeleteAttachment(id, attachment);
      }
      
      void DeleteMetadata(int64_t id,
                          MetadataType type)
      {
        transaction_.DeleteMetadata(id, type);
      }

      void DeleteResource(int64_t id)
      {
        transaction_.DeleteResource(id);
      }

      void LogChange(int64_t internalId,
                     ChangeType changeType,
                     ResourceType resourceType,
                     const std::string& publicId);

      void LogExportedResource(const ExportedResource& resource)
      {
        transaction_.LogExportedResource(resource);
      }

      void SetGlobalProperty(GlobalProperty property,
                             bool shared,
                             const std::string& value)
      {
        transaction_.SetGlobalProperty(property, shared, value);
      }

      int64_t IncrementGlobalProperty(GlobalProperty sequence,
                                      bool shared,
                                      int64_t increment)
      {
        return transaction_.IncrementGlobalProperty(sequence, shared, increment);
      }

      void UpdateAndGetStatistics(int64_t& patientsCount,
                                  int64_t& studiesCount,
                                  int64_t& seriesCount,
                                  int64_t& instancesCount,
                                  int64_t& compressedSize,
                                  int64_t& uncompressedSize)
      {
        return transaction_.UpdateAndGetStatistics(patientsCount, studiesCount, seriesCount, instancesCount, compressedSize, uncompressedSize);
      }

      void SetMetadata(int64_t id,
                       MetadataType type,
                       const std::string& value,
                       int64_t revision)
      {
        return transaction_.SetMetadata(id, type, value, revision);
      }

      void SetProtectedPatient(int64_t internalId, 
                               bool isProtected)
      {
        transaction_.SetProtectedPatient(internalId, isProtected);
      }

      void SetResourcesContent(const ResourcesContent& content)
      {
        transaction_.SetResourcesContent(content);
      }

      void Recycle(uint64_t maximumStorageSize,
                   unsigned int maximumPatients,
                   uint64_t addedInstanceSize,
                   const std::string& newPatientId);

      bool IsRecyclingNeeded(uint64_t maximumStorageSize,
                             unsigned int maximumPatients,
                             uint64_t addedInstanceSize,
                             const std::string& newPatientId);

      void AddLabel(int64_t id,
                    const std::string& label)
      {
        transaction_.AddLabel(id, label);
      }

      void RemoveLabel(int64_t id,
                    const std::string& label)
      {
        transaction_.RemoveLabel(id, label);
      }
    };


    class IReadOnlyOperations : public boost::noncopyable
    {
    public:
      virtual ~IReadOnlyOperations()
      {
      }

      virtual void Apply(ReadOnlyTransaction& transaction) = 0;
    };


    class IReadWriteOperations : public boost::noncopyable
    {
    public:
      virtual ~IReadWriteOperations()
      {
      }

      virtual void Apply(ReadWriteTransaction& transaction) = 0;
    };
    

  private:
    class Transaction;

    IDatabaseWrapper&                            db_;
    boost::shared_ptr<MainDicomTagsRegistry>     mainDicomTagsRegistry_;  // "shared_ptr" because of PImpl

    // Mutex to protect the configuration options
    boost::shared_mutex                          mutex_;
    std::unique_ptr<ITransactionContextFactory>  factory_;
    unsigned int                                 maxRetries_;
    bool                                         readOnly_;

    void ApplyInternal(IReadOnlyOperations* readOperations,
                       IReadWriteOperations* writeOperations);

    const FindResponse::Resource &ExecuteSingleResource(FindResponse &response,
                                                        const FindRequest &request);

  protected:
    void StandaloneRecycling(MaxStorageMode maximumStorageMode,
                             uint64_t maximumStorageSize,
                             unsigned int maximumPatientCount);

    bool IsReadOnly()
    {
      return readOnly_;
    }

  public:
    explicit StatelessDatabaseOperations(IDatabaseWrapper& database, bool readOnly);

    void SetTransactionContextFactory(ITransactionContextFactory* factory /* takes ownership */);

    // Only used to handle "ErrorCode_DatabaseCannotSerialize" in the
    // case of collision between multiple writers
    void SetMaxDatabaseRetries(unsigned int maxRetries);
    
    // It is assumed that "GetDatabaseVersion()" can run out of a
    // database transaction
    unsigned int GetDatabaseVersion()
    {
      return db_.GetDatabaseVersion();
    }

    const IDatabaseWrapper::Capabilities GetDatabaseCapabilities() const
    {
      return db_.GetDatabaseCapabilities();
    }

    void FlushToDisk();


    void Apply(IReadOnlyOperations& operations);
  
    void Apply(IReadWriteOperations& operations);

    void GetAllMetadata(std::map<MetadataType, std::string>& target,
                        const std::string& publicId,
                        ResourceType level);

    void GetAllUuids(std::list<std::string>& target,
                     ResourceType resourceType);

    void GetGlobalStatistics(/* out */ uint64_t& diskSize,
                             /* out */ uint64_t& uncompressedSize,
                             /* out */ uint64_t& countPatients, 
                             /* out */ uint64_t& countStudies, 
                             /* out */ uint64_t& countSeries, 
                             /* out */ uint64_t& countInstances);

    bool LookupAttachment(FileInfo& attachment,
                          int64_t& revision,
                          ResourceType level,
                          const std::string& publicId,
                          FileContentType contentType);

    void GetChanges(Json::Value& target,
                    int64_t since,
                    uint32_t limit);

    void GetChangesExtended(Json::Value& target,
                            int64_t since,
                            int64_t to,
                            uint32_t limit,
                            const std::set<ChangeType>& filterType);

    void GetLastChange(Json::Value& target);

    bool HasExtendedChanges();

    bool HasFindSupport();
    
    void GetExportedResources(Json::Value& target,
                              int64_t since,
                              uint32_t limit);

    void GetLastExportedResource(Json::Value& target);

    bool IsProtectedPatient(const std::string& publicId);

    void GetChildren(std::list<std::string>& result,
                     ResourceType level,
                     const std::string& publicId);

    // Always prefer this flavor, which is more efficient than the flavor without "level"
    void GetChildInstances(std::list<std::string>& result,
                           const std::string& publicId,
                           ResourceType level);

    void GetChildInstances(std::list<std::string>& result,
                           const std::string& publicId);

    bool LookupMetadata(std::string& target,
                        const std::string& publicId,
                        ResourceType expectedType,
                        MetadataType type);

    bool LookupMetadata(std::string& target,
                        int64_t& revision,
                        const std::string& publicId,
                        ResourceType expectedType,
                        MetadataType type);

    void ListAvailableAttachments(std::set<FileContentType>& target,
                                  const std::string& publicId,
                                  ResourceType expectedType);

    bool LookupParent(std::string& target,
                      const std::string& publicId);

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

    bool LookupGlobalProperty(std::string& value,
                              GlobalProperty property,
                              bool shared);

    std::string GetGlobalProperty(GlobalProperty property,
                                  bool shared,
                                  const std::string& defaultValue);

    bool GetMainDicomTags(DicomMap& result,
                          const std::string& publicId,
                          ResourceType expectedType,
                          ResourceType levelOfInterest);

    // Only applicable at the instance level, retrieves tags from patient/study/series levels
    bool GetAllMainDicomTags(DicomMap& result,
                             const std::string& instancePublicId);

    bool LookupResourceType(ResourceType& type,
                            const std::string& publicId);

    bool LookupParent(std::string& target,
                      const std::string& publicId,
                      ResourceType parentType);

    bool DeleteResource(Json::Value& remainingAncestor /* out */,
                        const std::string& uuid,
                        ResourceType expectedType);

    void LogExportedResource(const std::string& publicId,
                             const std::string& remoteModality);

    void SetProtectedPatient(const std::string& publicId,
                             bool isProtected);

    void SetMetadata(int64_t& newRevision /*out*/,
                     const std::string& publicId,
                     MetadataType type,
                     const std::string& value,
                     bool hasOldRevision,
                     int64_t oldRevision,
                     const std::string& oldMD5);

    // Same as "SetMetadata()", but doesn't care about revisions
    void OverwriteMetadata(const std::string& publicId,
                           MetadataType type,
                           const std::string& value);

    bool DeleteMetadata(const std::string& publicId,
                        MetadataType type,
                        bool hasRevision,
                        int64_t revision,
                        const std::string& md5);

    uint64_t IncrementGlobalSequence(GlobalProperty sequence,
                                     bool shared);

    void DeleteChanges();

    void DeleteExportedResources();

    void SetGlobalProperty(GlobalProperty property,
                           bool shared,
                           const std::string& value);

    bool DeleteAttachment(const std::string& publicId,
                          FileContentType type,
                          bool hasRevision,
                          int64_t revision,
                          const std::string& md5);

    void LogChange(int64_t internalId,
                   ChangeType changeType,
                   const std::string& publicId,
                   ResourceType level);

    void ReconstructInstance(const ParsedDicomFile& dicom, 
                             bool limitToThisLevelDicomTags, 
                             ResourceType limitToLevel_);

    StoreStatus Store(std::map<MetadataType, std::string>& instanceMetadata,
                      const DicomMap& dicomSummary,
                      const Attachments& attachments,
                      const MetadataMap& metadata,
                      const DicomInstanceOrigin& origin,
                      bool overwrite,
                      bool hasTransferSyntax,
                      DicomTransferSyntax transferSyntax,
                      bool hasPixelDataOffset,
                      uint64_t pixelDataOffset,
                      ValueRepresentation pixelDataVR,
                      MaxStorageMode maximumStorageMode,
                      uint64_t maximumStorageSize,
                      unsigned int maximumPatients,
                      bool isReconstruct);

    StoreStatus AddAttachment(int64_t& newRevision /*out*/,
                              const FileInfo& attachment,
                              const std::string& publicId,
                              uint64_t maximumStorageSize,
                              unsigned int maximumPatients,
                              bool hasOldRevision,
                              int64_t oldRevision,
                              const std::string& oldMd5);

    void ListLabels(std::set<std::string>& target,
                    const std::string& publicId,
                    ResourceType level);

    void ListAllLabels(std::set<std::string>& target);

    void ModifyLabel(const std::string& publicId,
                     ResourceType level,
                     const std::string& label,
                     LabelOperation operation);

    void AddLabels(const std::string& publicId,
                   ResourceType level,
                   const std::set<std::string>& labels);

    bool HasLabelsSupport();

    void ExecuteFind(FindResponse& response,
                     const FindRequest& request);

    void ExecuteCount(uint64_t& count,
                      const FindRequest& request);
  };
}
