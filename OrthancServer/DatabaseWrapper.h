/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
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

#include "../Core/SQLite/Connection.h"
#include "../Core/SQLite/Transaction.h"
#include "DatabaseWrapperBase.h"

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
  class DatabaseWrapper : public IDatabaseWrapper
  {
  private:
    IDatabaseListener* listener_;
    SQLite::Connection db_;
    DatabaseWrapperBase base_;
    Internals::SignalRemainingAncestor* signalRemainingAncestor_;
    unsigned int version_;

    void ClearTable(const std::string& tableName);

  public:
    DatabaseWrapper(const std::string& path);

    DatabaseWrapper();

    virtual void Open();

    virtual void Close()
    {
      db_.Close();
    }

    virtual void SetListener(IDatabaseListener& listener);

    virtual void SetGlobalProperty(GlobalProperty property,
                                   const std::string& value)
    {
      base_.SetGlobalProperty(property, value);
    }

    virtual bool LookupGlobalProperty(std::string& target,
                                      GlobalProperty property)
    {
      return base_.LookupGlobalProperty(target, property);
    }

    virtual int64_t CreateResource(const std::string& publicId,
                                   ResourceType type)
    {
      return base_.CreateResource(publicId, type);
    }

    virtual bool LookupResource(int64_t& id,
                                ResourceType& type,
                                const std::string& publicId)
    {
      return base_.LookupResource(id, type, publicId);
    }

    virtual bool LookupParent(int64_t& parentId,
                              int64_t resourceId);

    virtual std::string GetPublicId(int64_t resourceId);

    virtual ResourceType GetResourceType(int64_t resourceId);

    virtual void AttachChild(int64_t parent,
                             int64_t child)
    {
      base_.AttachChild(parent, child);
    }

    virtual void DeleteResource(int64_t id);

    virtual void SetMetadata(int64_t id,
                             MetadataType type,
                             const std::string& value)
    {
      base_.SetMetadata(id, type, value);
    }

    virtual void DeleteMetadata(int64_t id,
                                MetadataType type)
    {
      base_.DeleteMetadata(id, type);
    }

    virtual bool LookupMetadata(std::string& target,
                                int64_t id,
                                MetadataType type)
    {
      return base_.LookupMetadata(target, id, type);
    }

    virtual void ListAvailableMetadata(std::list<MetadataType>& target,
                                       int64_t id)
    {
      base_.ListAvailableMetadata(target, id);
    }

    virtual void AddAttachment(int64_t id,
                               const FileInfo& attachment)
    {
      base_.AddAttachment(id, attachment);
    }

    virtual void DeleteAttachment(int64_t id,
                                  FileContentType attachment)
    {
      base_.DeleteAttachment(id, attachment);
    }

    virtual void ListAvailableAttachments(std::list<FileContentType>& target,
                                          int64_t id)
    {
      return base_.ListAvailableAttachments(target, id);
    }

    virtual bool LookupAttachment(FileInfo& attachment,
                                  int64_t id,
                                  FileContentType contentType)
    {
      return base_.LookupAttachment(attachment, id, contentType);
    }

    virtual void ClearMainDicomTags(int64_t id)
    {
      base_.ClearMainDicomTags(id);
    }

    virtual void SetMainDicomTag(int64_t id,
                                 const DicomTag& tag,
                                 const std::string& value)
    {
      base_.SetMainDicomTag(id, tag, value);
    }

    virtual void SetIdentifierTag(int64_t id,
                                 const DicomTag& tag,
                                 const std::string& value)
    {
      base_.SetIdentifierTag(id, tag, value);
    }

    virtual void GetMainDicomTags(DicomMap& map,
                                  int64_t id)
    {
      base_.GetMainDicomTags(map, id);
    }

    virtual void GetChildrenPublicId(std::list<std::string>& target,
                                     int64_t id)
    {
      base_.GetChildrenPublicId(target, id);
    }

    virtual void GetChildrenInternalId(std::list<int64_t>& target,
                                       int64_t id)
    {
      base_.GetChildrenInternalId(target, id);
    }

    virtual void LogChange(int64_t internalId,
                           const ServerIndexChange& change)
    {
      base_.LogChange(internalId, change);
    }

    virtual void GetChanges(std::list<ServerIndexChange>& target /*out*/,
                            bool& done /*out*/,
                            int64_t since,
                            uint32_t maxResults);

    virtual void GetLastChange(std::list<ServerIndexChange>& target /*out*/);

    virtual void LogExportedResource(const ExportedResource& resource)
    {
      base_.LogExportedResource(resource);
    }
    
    virtual void GetExportedResources(std::list<ExportedResource>& target /*out*/,
                                      bool& done /*out*/,
                                      int64_t since,
                                      uint32_t maxResults)
    {
      base_.GetExportedResources(target, done, since, maxResults);
    }

    virtual void GetLastExportedResource(std::list<ExportedResource>& target /*out*/)
    {
      base_.GetLastExportedResource(target);
    }

    virtual uint64_t GetTotalCompressedSize()
    {
      return base_.GetTotalCompressedSize();
    }
    
    virtual uint64_t GetTotalUncompressedSize()
    {
      return base_.GetTotalUncompressedSize();
    }

    virtual uint64_t GetResourceCount(ResourceType resourceType)
    {
      return base_.GetResourceCount(resourceType);
    }

    virtual void GetAllInternalIds(std::list<int64_t>& target,
                                   ResourceType resourceType)
    {
      base_.GetAllInternalIds(target, resourceType);
    }

    virtual void GetAllPublicIds(std::list<std::string>& target,
                                 ResourceType resourceType)
    {
      base_.GetAllPublicIds(target, resourceType);
    }

    virtual void GetAllPublicIds(std::list<std::string>& target,
                                 ResourceType resourceType,
                                 size_t since,
                                 size_t limit)
    {
      base_.GetAllPublicIds(target, resourceType, since, limit);
    }

    virtual bool SelectPatientToRecycle(int64_t& internalId)
    {
      return base_.SelectPatientToRecycle(internalId);
    }

    virtual bool SelectPatientToRecycle(int64_t& internalId,
                                        int64_t patientIdToAvoid)
    {
      return base_.SelectPatientToRecycle(internalId, patientIdToAvoid);
    }

    virtual bool IsProtectedPatient(int64_t internalId)
    {
      return base_.IsProtectedPatient(internalId);
    }

    virtual void SetProtectedPatient(int64_t internalId, 
                                     bool isProtected)
    {
      base_.SetProtectedPatient(internalId, isProtected);
    }

    virtual SQLite::ITransaction* StartTransaction()
    {
      return new SQLite::Transaction(db_);
    }

    virtual void FlushToDisk()
    {
      db_.FlushToDisk();
    }

    virtual bool HasFlushToDisk() const
    {
      return true;
    }

    virtual void ClearChanges()
    {
      ClearTable("Changes");
    }

    virtual void ClearExportedResources()
    {
      ClearTable("ExportedResources");
    }

    virtual bool IsExistingResource(int64_t internalId)
    {
      return base_.IsExistingResource(internalId);
    }

    virtual void LookupIdentifier(std::list<int64_t>& result,
                                  ResourceType level,
                                  const DicomTag& tag,
                                  IdentifierConstraintType type,
                                  const std::string& value)
    {
      base_.LookupIdentifier(result, level, tag, type, value);
    }

    virtual void GetAllMetadata(std::map<MetadataType, std::string>& target,
                                int64_t id);

    virtual unsigned int GetDatabaseVersion()
    {
      return version_;
    }

    virtual void Upgrade(unsigned int targetVersion,
                         IStorageArea& storageArea);



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

  };
}
