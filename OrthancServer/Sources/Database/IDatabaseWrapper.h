/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2024 Orthanc Team SRL, Belgium
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
#include "../../../OrthancFramework/Sources/FileStorage/FileInfo.h"
#include "../../../OrthancFramework/Sources/FileStorage/IStorageArea.h"
#include "../ExportedResource.h"
#include "../Search/ISqlLookupFormatter.h"
#include "../ServerIndexChange.h"
#include "FindRequest.h"
#include "FindResponse.h"
#include "IDatabaseListener.h"

#include <list>
#include <boost/noncopyable.hpp>
#include <set>

namespace Orthanc
{
  class DatabaseConstraint;
  class ResourcesContent;

  class IDatabaseWrapper : public boost::noncopyable
  {
  public:
    class Capabilities
    {
    private:
      bool hasFlushToDisk_;
      bool hasRevisionsSupport_;
      bool hasLabelsSupport_;
      bool hasAtomicIncrementGlobalProperty_;
      bool hasUpdateAndGetStatistics_;
      bool hasMeasureLatency_;

    public:
      Capabilities() :
        hasFlushToDisk_(false),
        hasRevisionsSupport_(false),
        hasLabelsSupport_(false),
        hasAtomicIncrementGlobalProperty_(false),
        hasUpdateAndGetStatistics_(false),
        hasMeasureLatency_(false)
      {
      }

      void SetFlushToDisk(bool value)
      {
        hasFlushToDisk_ = value;
      }

      bool HasFlushToDisk() const
      {
        return hasFlushToDisk_;
      }

      void SetRevisionsSupport(bool value)
      {
        hasRevisionsSupport_ = value;
      }

      bool HasRevisionsSupport() const
      {
        return hasRevisionsSupport_;
      }

      void SetLabelsSupport(bool value)
      {
        hasLabelsSupport_ = value;
      }

      bool HasLabelsSupport() const
      {
        return hasLabelsSupport_;
      }

      void SetAtomicIncrementGlobalProperty(bool value)
      {
        hasAtomicIncrementGlobalProperty_ = value;
      }

      bool HasAtomicIncrementGlobalProperty() const
      {
        return hasAtomicIncrementGlobalProperty_;
      }

      void SetUpdateAndGetStatistics(bool value)
      {
        hasUpdateAndGetStatistics_ = value;
      }

      bool HasUpdateAndGetStatistics() const
      {
        return hasUpdateAndGetStatistics_;
      }

      void SetMeasureLatency(bool value)
      {
        hasMeasureLatency_ = value;
      }

      bool HasMeasureLatency() const
      {
        return hasMeasureLatency_;
      }
    };


    struct CreateInstanceResult : public boost::noncopyable
    {
      bool     isNewPatient_;
      bool     isNewStudy_;
      bool     isNewSeries_;
      int64_t  patientId_;
      int64_t  studyId_;
      int64_t  seriesId_;
    };


    class ITransaction : public boost::noncopyable
    {
    public:
      virtual ~ITransaction()
      {
      }

      virtual void Rollback() = 0;

      // The "fileSizeDelta" is used for older database plugins that
      // have no fast way to compute the size of all the stored
      // attachments (cf. "fastGetTotalSize_")
      virtual void Commit(int64_t fileSizeDelta) = 0;

      // A call to "AddAttachment()" guarantees that this attachment
      // is not already existing. This is different from
      // "SetMetadata()" that might have to replace an older value.
      virtual void AddAttachment(int64_t id,
                                 const FileInfo& attachment,
                                 int64_t revision) = 0;

      virtual void ClearChanges() = 0;

      virtual void ClearExportedResources() = 0;

      virtual void DeleteAttachment(int64_t id,
                                    FileContentType attachment) = 0;

      virtual void DeleteMetadata(int64_t id,
                                  MetadataType type) = 0;

      virtual void DeleteResource(int64_t id) = 0;

      virtual void GetAllMetadata(std::map<MetadataType, std::string>& target,
                                  int64_t id) = 0;

      virtual void GetAllPublicIds(std::list<std::string>& target,
                                   ResourceType resourceType) = 0;

      virtual void GetAllPublicIds(std::list<std::string>& target,
                                   ResourceType resourceType,
                                   int64_t since,
                                   uint32_t limit) = 0;

      virtual void GetChanges(std::list<ServerIndexChange>& target /*out*/,
                              bool& done /*out*/,
                              int64_t since,
                              uint32_t limit) = 0;

      virtual void GetChildrenInternalId(std::list<int64_t>& target,
                                         int64_t id) = 0;

      virtual void GetChildrenPublicId(std::list<std::string>& target,
                                       int64_t id) = 0;

      virtual void GetExportedResources(std::list<ExportedResource>& target /*out*/,
                                        bool& done /*out*/,
                                        int64_t since,
                                        uint32_t limit) = 0;

      virtual void GetLastChange(std::list<ServerIndexChange>& target /*out*/) = 0;

      virtual void GetLastExportedResource(std::list<ExportedResource>& target /*out*/) = 0;

      virtual void GetMainDicomTags(DicomMap& map,
                                    int64_t id) = 0;

      virtual std::string GetPublicId(int64_t resourceId) = 0;

      virtual uint64_t GetResourcesCount(ResourceType resourceType) = 0;

      virtual ResourceType GetResourceType(int64_t resourceId) = 0;

      virtual uint64_t GetTotalCompressedSize() = 0;
    
      virtual uint64_t GetTotalUncompressedSize() = 0;

      virtual bool IsProtectedPatient(int64_t internalId) = 0;

      virtual void ListAvailableAttachments(std::set<FileContentType>& target,
                                            int64_t id) = 0;

      virtual void LogChange(ChangeType changeType,
                             ResourceType resourceType,
                             int64_t internalId,
                             const std::string& publicId,  /* only for compatibility with V1 and V2 plugins */
                             const std::string& date) = 0;

      virtual void LogExportedResource(const ExportedResource& resource) = 0;
    
      virtual bool LookupAttachment(FileInfo& attachment,
                                    int64_t& revision,
                                    int64_t id,
                                    FileContentType contentType) = 0;

      /**
       * If "shared" is "true", the property is shared by all the
       * Orthanc servers that access the same database. If "shared" is
       * "false", the property is private to the server (cf. the
       * "DatabaseServerIdentifier" configuration option).
       **/
      virtual bool LookupGlobalProperty(std::string& target,
                                        GlobalProperty property,
                                        bool shared) = 0;

      virtual bool LookupMetadata(std::string& target,
                                  int64_t& revision,
                                  int64_t id,
                                  MetadataType type) = 0;

      virtual bool LookupParent(int64_t& parentId,
                                int64_t resourceId) = 0;

      virtual bool LookupResource(int64_t& id,
                                  ResourceType& type,
                                  const std::string& publicId) = 0;

      virtual bool SelectPatientToRecycle(int64_t& internalId) = 0;

      virtual bool SelectPatientToRecycle(int64_t& internalId,
                                          int64_t patientIdToAvoid) = 0;

      virtual void SetGlobalProperty(GlobalProperty property,
                                     bool shared,
                                     const std::string& value) = 0;

      virtual void ClearMainDicomTags(int64_t id) = 0;

      virtual void SetMetadata(int64_t id,
                               MetadataType type,
                               const std::string& value,
                               int64_t revision) = 0;

      virtual void SetProtectedPatient(int64_t internalId, 
                                       bool isProtected) = 0;


      /**
       * Primitives introduced in Orthanc 1.5.2
       **/
    
      virtual bool IsDiskSizeAbove(uint64_t threshold) = 0;
    
      virtual void ApplyLookupResources(std::list<std::string>& resourcesId,
                                        std::list<std::string>* instancesId, // Can be NULL if not needed
                                        const std::vector<DatabaseConstraint>& lookup,
                                        ResourceType queryLevel,
                                        const std::set<std::string>& labels,
                                        LabelsConstraint labelsConstraint,
                                        uint32_t limit) = 0;

      // Returns "true" iff. the instance is new and has been inserted
      // into the database. If "false" is returned, the content of
      // "result" is undefined, but "instanceId" must be properly
      // set. This method must also tag the parent patient as the most
      // recent in the patient recycling order if it is not protected
      // (so as to fix issue #58).
      virtual bool CreateInstance(CreateInstanceResult& result, /* out */
                                  int64_t& instanceId,          /* out */
                                  const std::string& patient,
                                  const std::string& study,
                                  const std::string& series,
                                  const std::string& instance) = 0;

      // It is guaranteed that the resources to be modified have no main
      // DICOM tags, and no DICOM identifiers associated with
      // them. However, some metadata might be already existing, and
      // have to be overwritten.
      virtual void SetResourcesContent(const ResourcesContent& content) = 0;

      virtual void GetChildrenMetadata(std::list<std::string>& target,
                                       int64_t resourceId,
                                       MetadataType metadata) = 0;

      virtual int64_t GetLastChangeIndex() = 0;


      /**
       * Primitives introduced in Orthanc 1.5.4
       **/

      virtual bool LookupResourceAndParent(int64_t& id,
                                           ResourceType& type,
                                           std::string& parentPublicId,
                                           const std::string& publicId) = 0;


      /**
       * Primitives introduced in Orthanc 1.12.0
       **/

      virtual void AddLabel(int64_t resource,
                            const std::string& label) = 0;

      virtual void RemoveLabel(int64_t resource,
                               const std::string& label) = 0;

      // List the labels of one single resource
      virtual void ListLabels(std::set<std::string>& target,
                              int64_t resource) = 0;

      // List all the labels that are present in any resource
      virtual void ListAllLabels(std::set<std::string>& target) = 0;

      virtual int64_t IncrementGlobalProperty(GlobalProperty property,
                                              int64_t increment,
                                              bool shared) = 0;

      virtual void UpdateAndGetStatistics(int64_t& patientsCount,
                                          int64_t& studiesCount,
                                          int64_t& seriesCount,
                                          int64_t& instancesCount,
                                          int64_t& compressedSize,
                                          int64_t& uncompressedSize) = 0;

      /**
       * Primitives introduced in Orthanc 1.12.4
       **/

      // This is only implemented if "HasIntegratedFind()" is "true"
      virtual void ExecuteFind(FindResponse& response,
                               const FindRequest& request) = 0;

      // This is only implemented if "HasIntegratedFind()" is "false"
      virtual void ExecuteFind(std::list<std::string>& identifiers,
                               const FindRequest& request) = 0;

      /**
       * This is only implemented if "HasIntegratedFind()" is
       * "false". In this flavor, the resource of interest might have
       * been deleted, as the expansion is not done in the same
       * transaction as the "ExecuteFind()". In such cases, the
       * wrapper should not throw an exception, but simply ignore the
       * request to expand the resource (i.e., "response" must not be
       * modified).
       **/
      virtual void ExecuteExpand(FindResponse& response,
                                 const FindRequest& request,
                                 const std::string& identifier) = 0;
    };


    virtual ~IDatabaseWrapper()
    {
    }

    virtual void Open() = 0;

    virtual void Close() = 0;

    virtual void FlushToDisk() = 0;

    virtual ITransaction* StartTransaction(TransactionType type,
                                           IDatabaseListener& listener) = 0;

    virtual unsigned int GetDatabaseVersion() = 0;

    virtual void Upgrade(unsigned int targetVersion,
                         IStorageArea& storageArea) = 0;

    virtual const Capabilities GetDatabaseCapabilities() const = 0;

    virtual uint64_t MeasureLatency() = 0;

    // Returns "true" iff. the database engine supports the
    // simultaneous find and expansion of resources.
    virtual bool HasIntegratedFind() const = 0;
  };
}
