/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2024 Osimis S.A., Belgium
 * Copyright (C) 2021-2024 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

  class ExpandedResource : public boost::noncopyable
  {
  private:
    std::string                         id_;
    ResourceType                        level_;
    DicomMap                            tags_;  // all main tags and main sequences from DB

  public:
    std::string                         mainDicomTagsSignature_;
    std::string                         parentId_;
    std::list<std::string>              childrenIds_;
    std::map<MetadataType, std::string> metadata_;
    std::string                         anonymizedFrom_;
    std::string                         modifiedFrom_;
    std::string                         lastUpdate_;
    std::set<DicomTag>                  missingRequestedTags_;

    // for patients/studies/series
    bool                                isStable_;

    // for series only
    int                                 expectedNumberOfInstances_;
    std::string                         status_;

    // for instances only
    size_t                              fileSize_;
    std::string                         fileUuid_;
    int                                 indexInSeries_;

    // New in Orthanc 1.12.0
    std::set<std::string>               labels_;

  public:
    // TODO - Cleanup
    ExpandedResource() :
      level_(ResourceType_Instance),
      isStable_(false),
      expectedNumberOfInstances_(0),
      fileSize_(0),
      indexInSeries_(0)
    {
    }

    void SetResource(ResourceType level,
                     const std::string& id)
    {
      level_ = level;
      id_ = id;
    }

    const std::string& GetPublicId() const
    {
      return id_;
    }

    ResourceType GetLevel() const
    {
      return level_;
    }

    DicomMap& GetMainDicomTags()
    {
      return tags_;
    }

    const DicomMap& GetMainDicomTags() const
    {
      return tags_;
    }
  };

  enum ExpandResourceFlags
  {
    ExpandResourceFlags_None                    = 0,
    // used to fetch from DB and for output
    ExpandResourceFlags_IncludeMetadata         = (1 << 0),
    ExpandResourceFlags_IncludeChildren         = (1 << 1),
    ExpandResourceFlags_IncludeMainDicomTags    = (1 << 2),
    ExpandResourceFlags_IncludeLabels           = (1 << 3),

    // only used for output
    ExpandResourceFlags_IncludeAllMetadata      = (1 << 4),  // new in Orthanc 1.12.4
    ExpandResourceFlags_IncludeIsStable         = (1 << 5),  // new in Orthanc 1.12.4

    ExpandResourceFlags_DefaultExtract = (ExpandResourceFlags_IncludeMetadata |
                                          ExpandResourceFlags_IncludeChildren |
                                          ExpandResourceFlags_IncludeMainDicomTags |
                                          ExpandResourceFlags_IncludeLabels),

    ExpandResourceFlags_DefaultOutput = (ExpandResourceFlags_IncludeMetadata |
                                         ExpandResourceFlags_IncludeChildren |
                                         ExpandResourceFlags_IncludeMainDicomTags |
                                         ExpandResourceFlags_IncludeLabels |
                                         ExpandResourceFlags_IncludeIsStable)
  };

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

      void ApplyLookupResources(std::list<std::string>& resourcesId,
                                std::list<std::string>* instancesId, // Can be NULL if not needed
                                const std::vector<DatabaseConstraint>& lookup,
                                ResourceType queryLevel,
                                const std::set<std::string>& labels,  // New in Orthanc 1.12.0
                                LabelsConstraint labelsConstraint,    // New in Orthanc 1.12.0
                                uint32_t limit)
      {
        return transaction_.ApplyLookupResources(resourcesId, instancesId, lookup, queryLevel,
                                                 labels, labelsConstraint, limit);
      }

      void GetAllMetadata(std::map<MetadataType, std::string>& target,
                          int64_t id)
      {
        transaction_.GetAllMetadata(target, id);
      }

      void GetAllPublicIds(std::list<std::string>& target,
                           ResourceType resourceType)
      {
        return transaction_.GetAllPublicIds(target, resourceType);
      }

      void GetAllPublicIds(std::list<std::string>& target,
                           ResourceType resourceType,
                           size_t since,
                           uint32_t limit)
      {
        return transaction_.GetAllPublicIds(target, resourceType, since, limit);
      }  

      void GetChanges(std::list<ServerIndexChange>& target /*out*/,
                      bool& done /*out*/,
                      int64_t since,
                      uint32_t limit)
      {
        transaction_.GetChanges(target, done, since, limit);
      }

      void GetChildrenInternalId(std::list<int64_t>& target,
                                 int64_t id)
      {
        transaction_.GetChildrenInternalId(target, id);
      }

      void GetChildrenPublicId(std::list<std::string>& target,
                               int64_t id)
      {
        transaction_.GetChildrenPublicId(target, id);
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
      
      bool LookupResourceAndParent(int64_t& id,
                                   ResourceType& type,
                                   std::string& parentPublicId,
                                   const std::string& publicId)
      {
        return transaction_.LookupResourceAndParent(id, type, parentPublicId, publicId);
      }

      void ListLabels(std::set<std::string>& target,
                      int64_t id)
      {
        transaction_.ListLabels(target, id);
      }

      void ListAllLabels(std::set<std::string>& target)
      {
        transaction_.ListAllLabels(target);
      }

      void ExecuteFind(FindResponse& response,
                       const FindRequest& request)
      {
        transaction_.ExecuteFind(response, request);
      }

      void ExecuteFind(std::list<std::string>& identifiers,
                       const FindRequest& request)
      {
        transaction_.ExecuteFind(identifiers, request);
      }

      void ExecuteExpand(FindResponse& response,
                         const FindRequest& request,
                         const std::string& identifier)
      {
        transaction_.ExecuteExpand(response, request, identifier);
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

      bool HasReachedMaxStorageSize(uint64_t maximumStorageSize,
                                    uint64_t addedInstanceSize);

      bool HasReachedMaxPatientCount(unsigned int maximumPatientCount,
                                     const std::string& patientId);
      
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

    void NormalizeLookup(std::vector<DatabaseConstraint>& target,
                         const DatabaseLookup& source,
                         ResourceType level) const;

    void ApplyInternal(IReadOnlyOperations* readOperations,
                       IReadWriteOperations* writeOperations);

  protected:
    void StandaloneRecycling(MaxStorageMode maximumStorageMode,
                             uint64_t maximumStorageSize,
                             unsigned int maximumPatientCount);

  public:
    explicit StatelessDatabaseOperations(IDatabaseWrapper& database);

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

    bool ExpandResource(ExpandedResource& target,
                        const std::string& publicId,
                        ResourceType level,
                        const std::set<DicomTag>& requestedTags,
                        ExpandResourceFlags expandFlags);

    void GetAllMetadata(std::map<MetadataType, std::string>& target,
                        const std::string& publicId,
                        ResourceType level);

    void GetAllUuids(std::list<std::string>& target,
                     ResourceType resourceType);

    void GetAllUuids(std::list<std::string>& target,
                     ResourceType resourceType,
                     size_t since,
                     uint32_t limit);

    void GetGlobalStatistics(/* out */ uint64_t& diskSize,
                             /* out */ uint64_t& uncompressedSize,
                             /* out */ uint64_t& countPatients, 
                             /* out */ uint64_t& countStudies, 
                             /* out */ uint64_t& countSeries, 
                             /* out */ uint64_t& countInstances);

    bool LookupAttachment(FileInfo& attachment,
                          int64_t& revision,
                          const std::string& instancePublicId,
                          FileContentType contentType);

    void GetChanges(Json::Value& target,
                    int64_t since,
                    uint32_t limit);

    void GetLastChange(Json::Value& target);

    void GetExportedResources(Json::Value& target,
                              int64_t since,
                              uint32_t limit);

    void GetLastExportedResource(Json::Value& target);

    bool IsProtectedPatient(const std::string& publicId);

    void GetChildren(std::list<std::string>& result,
                     const std::string& publicId);

    void GetChildInstances(std::list<std::string>& result,
                           const std::string& publicId);

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

    // Only applicable at the instance level
    bool GetAllMainDicomTags(DicomMap& result,
                             const std::string& instancePublicId);

    bool LookupResourceType(ResourceType& type,
                            const std::string& publicId);

    bool LookupParent(std::string& target,
                      const std::string& publicId,
                      ResourceType parentType);

    void ApplyLookupResources(std::vector<std::string>& resourcesId,
                              std::vector<std::string>* instancesId,  // Can be NULL if not needed
                              const DatabaseLookup& lookup,
                              ResourceType queryLevel,
                              const std::set<std::string>& labels,
                              LabelsConstraint labelsConstraint,
                              uint32_t limit);

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
  };
}
