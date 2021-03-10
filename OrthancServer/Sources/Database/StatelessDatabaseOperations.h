/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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

#include "../../../OrthancFramework/Sources/DicomFormat/DicomMap.h"

#include "IDatabaseWrapper.h"
#include "../DicomInstanceOrigin.h"

#include <boost/thread/mutex.hpp>   // TODO - REMOVE


namespace Orthanc
{
  class DatabaseLookup;
  class ParsedDicomFile;
  class ServerIndexChange;

  class StatelessDatabaseOperations : public boost::noncopyable
  {
  public:
    typedef std::list<FileInfo> Attachments;
    typedef std::map<std::pair<ResourceType, MetadataType>, std::string>  MetadataMap;

    class ITransactionContext : public boost::noncopyable
    {
    public:
      virtual ~ITransactionContext()
      {
      }

      virtual void Commit() = 0;

      virtual int64_t GetCompressedSizeDelta() = 0;

      virtual bool IsUnstableResource(int64_t id) = 0;

      virtual bool LookupRemainingLevel(std::string& remainingPublicId /* out */,
                                        ResourceType& remainingLevel   /* out */) = 0;

      virtual void MarkAsUnstable(int64_t id,
                                  Orthanc::ResourceType type,
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

      virtual ITransactionContext* Create() = 0;
    };


    class ReadOnlyTransaction : public boost::noncopyable
    {
    private:
      ITransactionContext&  context_;
      
    protected:
      IDatabaseWrapper&  db_;
      
    public:
      explicit ReadOnlyTransaction(IDatabaseWrapper& db,
                                   ITransactionContext& context) :
        context_(context),
        db_(db)
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
                                size_t limit)
      {
        return db_.ApplyLookupResources(resourcesId, instancesId, lookup, queryLevel, limit);
      }

      void GetAllMetadata(std::map<MetadataType, std::string>& target,
                          int64_t id)
      {
        db_.GetAllMetadata(target, id);
      }

      void GetAllPublicIds(std::list<std::string>& target,
                           ResourceType resourceType)
      {
        return db_.GetAllPublicIds(target, resourceType);
      }

      void GetAllPublicIds(std::list<std::string>& target,
                           ResourceType resourceType,
                           size_t since,
                           size_t limit)
      {
        return db_.GetAllPublicIds(target, resourceType, since, limit);
      }  

      void GetChanges(std::list<ServerIndexChange>& target /*out*/,
                      bool& done /*out*/,
                      int64_t since,
                      uint32_t maxResults)
      {
        db_.GetChanges(target, done, since, maxResults);
      }

      void GetChildrenInternalId(std::list<int64_t>& target,
                                 int64_t id)
      {
        db_.GetChildrenInternalId(target, id);
      }

      void GetChildrenPublicId(std::list<std::string>& target,
                               int64_t id)
      {
        db_.GetChildrenPublicId(target, id);
      }

      void GetExportedResources(std::list<ExportedResource>& target /*out*/,
                                bool& done /*out*/,
                                int64_t since,
                                uint32_t maxResults)
      {
        return db_.GetExportedResources(target, done, since, maxResults);
      }

      void GetLastChange(std::list<ServerIndexChange>& target /*out*/)
      {
        db_.GetLastChange(target);
      }

      void GetLastExportedResource(std::list<ExportedResource>& target /*out*/)
      {
        return db_.GetLastExportedResource(target);
      }

      int64_t GetLastChangeIndex()
      {
        return db_.GetLastChangeIndex();
      }

      void GetMainDicomTags(DicomMap& map,
                            int64_t id)
      {
        db_.GetMainDicomTags(map, id);
      }

      std::string GetPublicId(int64_t resourceId)
      {
        return db_.GetPublicId(resourceId);
      }
      
      uint64_t GetResourceCount(ResourceType resourceType)
      {
        return db_.GetResourceCount(resourceType);
      }
      
      ResourceType GetResourceType(int64_t resourceId)
      {
        return db_.GetResourceType(resourceId);
      }

      uint64_t GetTotalCompressedSize()
      {
        return db_.GetTotalCompressedSize();
      }
    
      uint64_t GetTotalUncompressedSize()
      {
        return db_.GetTotalUncompressedSize();
      }
      
      bool IsProtectedPatient(int64_t internalId)
      {
        return db_.IsProtectedPatient(internalId);
      }

      void ListAvailableAttachments(std::set<FileContentType>& target,
                                    int64_t id)
      {
        db_.ListAvailableAttachments(target, id);
      }

      bool LookupAttachment(FileInfo& attachment,
                            int64_t id,
                            FileContentType contentType)
      {
        return db_.LookupAttachment(attachment, id, contentType);
      }
      
      bool LookupGlobalProperty(std::string& target,
                                GlobalProperty property)
      {
        return db_.LookupGlobalProperty(target, property);
      }

      bool LookupMetadata(std::string& target,
                          int64_t id,
                          MetadataType type)
      {
        return db_.LookupMetadata(target, id, type);
      }

      bool LookupParent(int64_t& parentId,
                        int64_t resourceId)
      {
        return db_.LookupParent(parentId, resourceId);
      }
        
      bool LookupResource(int64_t& id,
                          ResourceType& type,
                          const std::string& publicId)
      {
        return db_.LookupResource(id, type, publicId);
      }
      
      bool LookupResourceAndParent(int64_t& id,
                                   ResourceType& type,
                                   std::string& parentPublicId,
                                   const std::string& publicId)
      {
        return db_.LookupResourceAndParent(id, type, parentPublicId, publicId);
      }
    };


    class ReadWriteTransaction : public ReadOnlyTransaction
    {
    public:
      ReadWriteTransaction(IDatabaseWrapper& db,
                           ITransactionContext& context) :
        ReadOnlyTransaction(db, context)
      {
      }

      void AddAttachment(int64_t id,
                         const FileInfo& attachment)
      {
        db_.AddAttachment(id, attachment);
      }
      
      void ClearChanges()
      {
        db_.ClearChanges();
      }

      void ClearExportedResources()
      {
        db_.ClearExportedResources();
      }

      void ClearMainDicomTags(int64_t id)
      {
        return db_.ClearMainDicomTags(id);
      }

      bool CreateInstance(IDatabaseWrapper::CreateInstanceResult& result, /* out */
                          int64_t& instanceId,          /* out */
                          const std::string& patient,
                          const std::string& study,
                          const std::string& series,
                          const std::string& instance)
      {
        return db_.CreateInstance(result, instanceId, patient, study, series, instance);
      }

      void DeleteAttachment(int64_t id,
                            FileContentType attachment)
      {
        return db_.DeleteAttachment(id, attachment);
      }
      
      void DeleteMetadata(int64_t id,
                          MetadataType type)
      {
        db_.DeleteMetadata(id, type);
      }

      void DeleteResource(int64_t id)
      {
        db_.DeleteResource(id);
      }

      void LogChange(int64_t internalId,
                     ChangeType changeType,
                     ResourceType resourceType,
                     const std::string& publicId);

      void LogExportedResource(const ExportedResource& resource)
      {
        db_.LogExportedResource(resource);
      }

      void SetGlobalProperty(GlobalProperty property,
                             const std::string& value)
      {
        db_.SetGlobalProperty(property, value);
      }

      void SetMetadata(int64_t id,
                       MetadataType type,
                       const std::string& value)
      {
        return db_.SetMetadata(id, type, value);
      }

      void SetProtectedPatient(int64_t internalId, 
                               bool isProtected)
      {
        db_.SetProtectedPatient(internalId, isProtected);
      }

      void SetResourcesContent(const ResourcesContent& content)
      {
        db_.SetResourcesContent(content);
      }

      void Recycle(uint64_t maximumStorageSize,
                   unsigned int maximumPatients,
                   uint64_t addedInstanceSize,
                   const std::string& newPatientId);
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
    class MainDicomTagsRegistry;
    class Transaction;

    IDatabaseWrapper&                            db_;
    boost::mutex                                 databaseMutex_;  // TODO - REMOVE
    std::unique_ptr<ITransactionContextFactory>  factory_;
    unsigned int                                 maxRetries_;
    std::unique_ptr<MainDicomTagsRegistry>       mainDicomTagsRegistry_;
    bool                                         hasFlushToDisk_;

    void NormalizeLookup(std::vector<DatabaseConstraint>& target,
                         const DatabaseLookup& source,
                         ResourceType level) const;

    void ApplyInternal(IReadOnlyOperations* readOperations,
                       IReadWriteOperations* writeOperations);

  protected:
    void StandaloneRecycling(uint64_t maximumStorageSize,
                             unsigned int maximumPatientCount);

  public:
    StatelessDatabaseOperations(IDatabaseWrapper& database);

    void SetTransactionContextFactory(ITransactionContextFactory* factory /* takes ownership */);
    
    // It is assumed that "GetDatabaseVersion()" can run out of a
    // database transaction
    unsigned int GetDatabaseVersion()
    {
      return db_.GetDatabaseVersion();
    }

    void FlushToDisk();

    bool HasFlushToDisk() const
    {
      return hasFlushToDisk_;
    }

    void Apply(IReadOnlyOperations& operations);
  
    void Apply(IReadWriteOperations& operations);

    bool ExpandResource(Json::Value& target,
                        const std::string& publicId,
                        ResourceType level);

    void GetAllMetadata(std::map<MetadataType, std::string>& target,
                        const std::string& publicId,
                        ResourceType level);

    void GetAllUuids(std::list<std::string>& target,
                     ResourceType resourceType);

    void GetAllUuids(std::list<std::string>& target,
                     ResourceType resourceType,
                     size_t since,
                     size_t limit);

    void GetGlobalStatistics(/* out */ uint64_t& diskSize,
                             /* out */ uint64_t& uncompressedSize,
                             /* out */ uint64_t& countPatients, 
                             /* out */ uint64_t& countStudies, 
                             /* out */ uint64_t& countSeries, 
                             /* out */ uint64_t& countInstances);

    bool LookupAttachment(FileInfo& attachment,
                          const std::string& instancePublicId,
                          FileContentType contentType);

    void GetChanges(Json::Value& target,
                    int64_t since,
                    unsigned int maxResults);

    void GetLastChange(Json::Value& target);

    void GetExportedResources(Json::Value& target,
                              int64_t since,
                              unsigned int maxResults);

    void GetLastExportedResource(Json::Value& target);

    bool IsProtectedPatient(const std::string& publicId);

    void GetChildren(std::list<std::string>& result,
                     const std::string& publicId);

    void GetChildInstances(std::list<std::string>& result,
                           const std::string& publicId);

    bool LookupMetadata(std::string& target,
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

    bool LookupParent(std::string& target,
                      const std::string& publicId,
                      ResourceType parentType);

    void ApplyLookupResources(std::vector<std::string>& resourcesId,
                              std::vector<std::string>* instancesId,  // Can be NULL if not needed
                              const DatabaseLookup& lookup,
                              ResourceType queryLevel,
                              size_t limit);

    bool DeleteResource(Json::Value& target /* out */,
                        const std::string& uuid,
                        ResourceType expectedType);

    void LogExportedResource(const std::string& publicId,
                             const std::string& remoteModality);

    void SetProtectedPatient(const std::string& publicId,
                             bool isProtected);

    void SetMetadata(const std::string& publicId,
                     MetadataType type,
                     const std::string& value);

    void DeleteMetadata(const std::string& publicId,
                        MetadataType type);

    uint64_t IncrementGlobalSequence(GlobalProperty sequence);

    void DeleteChanges();

    void DeleteExportedResources();

    void SetGlobalProperty(GlobalProperty property,
                           const std::string& value);

    void DeleteAttachment(const std::string& publicId,
                          FileContentType type);

    void LogChange(int64_t internalId,
                   ChangeType changeType,
                   const std::string& publicId,
                   ResourceType level);

    void ReconstructInstance(const ParsedDicomFile& dicom);

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
                      uint64_t maximumStorageSize,
                      unsigned int maximumPatients);

    StoreStatus AddAttachment(const FileInfo& attachment,
                              const std::string& publicId,
                              uint64_t maximumStorageSize,
                              unsigned int maximumPatients);
  };
}
