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

#include "IDatabaseWrapper.h"

#include "../../Core/SQLite/Connection.h"
#include "Compatibility/ICreateInstance.h"
#include "Compatibility/IGetChildrenMetadata.h"
#include "Compatibility/ILookupResourceAndParent.h"
#include "Compatibility/ISetResourcesContent.h"

namespace Orthanc
{
  namespace Internals
  {
    class SignalRemainingAncestor;
  }

  /**
   * This class manages an instance of the Orthanc SQLite database. It
   * translates low-level requests into SQL statements. Mutual
   * exclusion MUST be implemented at a higher level.
   **/
  class SQLiteDatabaseWrapper :
    public IDatabaseWrapper,
    public Compatibility::ICreateInstance,
    public Compatibility::IGetChildrenMetadata,
    public Compatibility::ILookupResourceAndParent,
    public Compatibility::ISetResourcesContent
  {
  private:
    class Transaction;
    class LookupFormatter;

    IDatabaseListener* listener_;
    SQLite::Connection db_;
    Internals::SignalRemainingAncestor* signalRemainingAncestor_;
    unsigned int version_;

    void GetChangesInternal(std::list<ServerIndexChange>& target,
                            bool& done,
                            SQLite::Statement& s,
                            uint32_t maxResults);

    void GetExportedResourcesInternal(std::list<ExportedResource>& target,
                                      bool& done,
                                      SQLite::Statement& s,
                                      uint32_t maxResults);

    void ClearTable(const std::string& tableName);

    // Unused => could be removed
    int GetGlobalIntegerProperty(GlobalProperty property,
                                 int defaultValue);

  public:
    SQLiteDatabaseWrapper(const std::string& path);

    SQLiteDatabaseWrapper();

    virtual void Open()
      ORTHANC_OVERRIDE;

    virtual void Close()
      ORTHANC_OVERRIDE
    {
      db_.Close();
    }

    virtual void SetListener(IDatabaseListener& listener)
      ORTHANC_OVERRIDE;

    virtual bool LookupParent(int64_t& parentId,
                              int64_t resourceId)
      ORTHANC_OVERRIDE;

    virtual std::string GetPublicId(int64_t resourceId)
      ORTHANC_OVERRIDE;

    virtual ResourceType GetResourceType(int64_t resourceId)
      ORTHANC_OVERRIDE;

    virtual void DeleteResource(int64_t id)
      ORTHANC_OVERRIDE;

    virtual void GetChanges(std::list<ServerIndexChange>& target /*out*/,
                            bool& done /*out*/,
                            int64_t since,
                            uint32_t maxResults)
      ORTHANC_OVERRIDE;

    virtual void GetLastChange(std::list<ServerIndexChange>& target /*out*/)
      ORTHANC_OVERRIDE;

    virtual IDatabaseWrapper::ITransaction* StartTransaction()
      ORTHANC_OVERRIDE;

    virtual void FlushToDisk()
      ORTHANC_OVERRIDE
    {
      db_.FlushToDisk();
    }

    virtual bool HasFlushToDisk() const
      ORTHANC_OVERRIDE
    {
      return true;
    }

    virtual void ClearChanges()
      ORTHANC_OVERRIDE
    {
      ClearTable("Changes");
    }

    virtual void ClearExportedResources()
      ORTHANC_OVERRIDE
    {
      ClearTable("ExportedResources");
    }

    virtual void GetAllMetadata(std::map<MetadataType, std::string>& target,
                                int64_t id)
      ORTHANC_OVERRIDE;

    virtual unsigned int GetDatabaseVersion()
      ORTHANC_OVERRIDE
    {
      return version_;
    }

    virtual void Upgrade(unsigned int targetVersion,
                         IStorageArea& storageArea)
      ORTHANC_OVERRIDE;


    /**
     * The methods declared below are for unit testing only!
     **/

    const char* GetErrorMessage() const
    {
      return db_.GetErrorMessage();
    }

    void GetChildren(std::list<std::string>& childrenPublicIds,
                     int64_t id);

    int64_t GetTableRecordCount(const std::string& table);
    
    bool GetParentPublicId(std::string& target,
                           int64_t id);



    /**
     * Until Orthanc 1.4.0, the methods below were part of the
     * "DatabaseWrapperBase" class, that is now placed in the
     * graveyard.
     **/

    virtual void SetGlobalProperty(GlobalProperty property,
                                   const std::string& value)
      ORTHANC_OVERRIDE;

    virtual bool LookupGlobalProperty(std::string& target,
                                      GlobalProperty property)
      ORTHANC_OVERRIDE;

    virtual int64_t CreateResource(const std::string& publicId,
                                   ResourceType type)
      ORTHANC_OVERRIDE;

    virtual bool LookupResource(int64_t& id,
                                ResourceType& type,
                                const std::string& publicId)
      ORTHANC_OVERRIDE;

    virtual void AttachChild(int64_t parent,
                             int64_t child)
      ORTHANC_OVERRIDE;

    virtual void SetMetadata(int64_t id,
                             MetadataType type,
                             const std::string& value)
      ORTHANC_OVERRIDE;

    virtual void DeleteMetadata(int64_t id,
                                MetadataType type)
      ORTHANC_OVERRIDE;

    virtual bool LookupMetadata(std::string& target,
                                int64_t id,
                                MetadataType type)
      ORTHANC_OVERRIDE;

    virtual void AddAttachment(int64_t id,
                               const FileInfo& attachment)
      ORTHANC_OVERRIDE;

    virtual void DeleteAttachment(int64_t id,
                                  FileContentType attachment)
      ORTHANC_OVERRIDE;

    virtual void ListAvailableAttachments(std::list<FileContentType>& target,
                                          int64_t id)
      ORTHANC_OVERRIDE;

    virtual bool LookupAttachment(FileInfo& attachment,
                                  int64_t id,
                                  FileContentType contentType)
      ORTHANC_OVERRIDE;

    virtual void ClearMainDicomTags(int64_t id)
      ORTHANC_OVERRIDE;

    virtual void SetMainDicomTag(int64_t id,
                                 const DicomTag& tag,
                                 const std::string& value)
      ORTHANC_OVERRIDE;

    virtual void SetIdentifierTag(int64_t id,
                                  const DicomTag& tag,
                                  const std::string& value)
      ORTHANC_OVERRIDE;

    virtual void GetMainDicomTags(DicomMap& map,
                                  int64_t id)
      ORTHANC_OVERRIDE;

    virtual void GetChildrenPublicId(std::list<std::string>& target,
                                     int64_t id)
      ORTHANC_OVERRIDE;

    virtual void GetChildrenInternalId(std::list<int64_t>& target,
                                       int64_t id)
      ORTHANC_OVERRIDE;

    virtual void LogChange(int64_t internalId,
                           const ServerIndexChange& change)
      ORTHANC_OVERRIDE;

    virtual void LogExportedResource(const ExportedResource& resource)
      ORTHANC_OVERRIDE;
    
    virtual void GetExportedResources(std::list<ExportedResource>& target /*out*/,
                                      bool& done /*out*/,
                                      int64_t since,
                                      uint32_t maxResults)
      ORTHANC_OVERRIDE;

    virtual void GetLastExportedResource(std::list<ExportedResource>& target /*out*/)
      ORTHANC_OVERRIDE;

    virtual uint64_t GetTotalCompressedSize()
      ORTHANC_OVERRIDE;
    
    virtual uint64_t GetTotalUncompressedSize()
      ORTHANC_OVERRIDE;

    virtual uint64_t GetResourceCount(ResourceType resourceType)
      ORTHANC_OVERRIDE;

    virtual void GetAllPublicIds(std::list<std::string>& target,
                                 ResourceType resourceType)
      ORTHANC_OVERRIDE;

    virtual void GetAllPublicIds(std::list<std::string>& target,
                                 ResourceType resourceType,
                                 size_t since,
                                 size_t limit)
      ORTHANC_OVERRIDE;

    virtual bool SelectPatientToRecycle(int64_t& internalId)
      ORTHANC_OVERRIDE;

    virtual bool SelectPatientToRecycle(int64_t& internalId,
                                        int64_t patientIdToAvoid)
      ORTHANC_OVERRIDE;

    virtual bool IsProtectedPatient(int64_t internalId)
      ORTHANC_OVERRIDE;

    virtual void SetProtectedPatient(int64_t internalId, 
                                     bool isProtected)
      ORTHANC_OVERRIDE;

    virtual bool IsExistingResource(int64_t internalId)
      ORTHANC_OVERRIDE;

    virtual bool IsDiskSizeAbove(uint64_t threshold)
      ORTHANC_OVERRIDE;

    virtual void ApplyLookupResources(std::list<std::string>& resourcesId,
                                      std::list<std::string>* instancesId,
                                      const std::vector<DatabaseConstraint>& lookup,
                                      ResourceType queryLevel,
                                      size_t limit)
      ORTHANC_OVERRIDE;

    virtual bool CreateInstance(CreateInstanceResult& result,
                                int64_t& instanceId,
                                const std::string& patient,
                                const std::string& study,
                                const std::string& series,
                                const std::string& instance)
      ORTHANC_OVERRIDE
    {
      return ICreateInstance::Apply
        (*this, result, instanceId, patient, study, series, instance);
    }

    virtual void SetResourcesContent(const Orthanc::ResourcesContent& content)
      ORTHANC_OVERRIDE
    {
      ISetResourcesContent::Apply(*this, content);
    }

    virtual void GetChildrenMetadata(std::list<std::string>& target,
                                     int64_t resourceId,
                                     MetadataType metadata)
      ORTHANC_OVERRIDE
    {
      IGetChildrenMetadata::Apply(*this, target, resourceId, metadata);
    }

    virtual int64_t GetLastChangeIndex() ORTHANC_OVERRIDE;

    virtual void TagMostRecentPatient(int64_t patient) ORTHANC_OVERRIDE;

    virtual bool LookupResourceAndParent(int64_t& id,
                                         ResourceType& type,
                                         std::string& parentPublicId,
                                         const std::string& publicId)
      ORTHANC_OVERRIDE
    {
      return ILookupResourceAndParent::Apply(*this, id, type, parentPublicId, publicId);
    }
  };
}
