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

#include <orthanc/OrthancCppDatabasePlugin.h>

#include "../../../Core/SQLite/Connection.h"
#include "../../../Core/SQLite/Transaction.h"
#include "../../../OrthancServer/DatabaseWrapperBase.h"
#include "../../Engine/PluginsEnumerations.h"

#include <memory>

class Database : public OrthancPlugins::IDatabaseBackend
{
private:
  class SignalRemainingAncestor;

  std::string                   path_;
  Orthanc::SQLite::Connection   db_;
  Orthanc::DatabaseWrapperBase  base_;
  SignalRemainingAncestor*      signalRemainingAncestor_;

  std::auto_ptr<Orthanc::SQLite::Transaction>  transaction_;

public:
  Database(const std::string& path);

  virtual void Open();

  virtual void Close();

  virtual void AddAttachment(int64_t id,
                             const OrthancPluginAttachment& attachment);

  virtual void AttachChild(int64_t parent,
                           int64_t child)
  {
    base_.AttachChild(parent, child);
  }

  virtual void ClearChanges()
  {
    db_.Execute("DELETE FROM Changes");    
  }

  virtual void ClearExportedResources()
  {
    db_.Execute("DELETE FROM ExportedResources");    
  }

  virtual int64_t CreateResource(const char* publicId,
                                 OrthancPluginResourceType type)
  {
    return base_.CreateResource(publicId, Orthanc::Plugins::Convert(type));
  }

  virtual void DeleteAttachment(int64_t id,
                                int32_t attachment)
  {
    base_.DeleteAttachment(id, static_cast<Orthanc::FileContentType>(attachment));
  }

  virtual void DeleteMetadata(int64_t id,
                              int32_t metadataType)
  {
    base_.DeleteMetadata(id, static_cast<Orthanc::MetadataType>(metadataType));
  }

  virtual void DeleteResource(int64_t id);

  virtual void GetAllInternalIds(std::list<int64_t>& target,
                                 OrthancPluginResourceType resourceType)
  {
    base_.GetAllInternalIds(target, Orthanc::Plugins::Convert(resourceType));
  }

  virtual void GetAllPublicIds(std::list<std::string>& target,
                               OrthancPluginResourceType resourceType)
  {
    base_.GetAllPublicIds(target, Orthanc::Plugins::Convert(resourceType));
  }

  virtual void GetAllPublicIds(std::list<std::string>& target,
                               OrthancPluginResourceType resourceType,
                               uint64_t since,
                               uint64_t limit)
  {
    base_.GetAllPublicIds(target, Orthanc::Plugins::Convert(resourceType), since, limit);
  }

  virtual void GetChanges(bool& done /*out*/,
                          int64_t since,
                          uint32_t maxResults);

  virtual void GetChildrenInternalId(std::list<int64_t>& target /*out*/,
                                     int64_t id)
  {
    base_.GetChildrenInternalId(target, id);
  }

  virtual void GetChildrenPublicId(std::list<std::string>& target /*out*/,
                                   int64_t id)
  {
    base_.GetChildrenPublicId(target, id);
  }

  virtual void GetExportedResources(bool& done /*out*/,
                                    int64_t since,
                                    uint32_t maxResults);

  virtual void GetLastChange();

  virtual void GetLastExportedResource();

  virtual void GetMainDicomTags(int64_t id);

  virtual std::string GetPublicId(int64_t resourceId);

  virtual uint64_t GetResourceCount(OrthancPluginResourceType resourceType)
  {
    return base_.GetResourceCount(Orthanc::Plugins::Convert(resourceType));
  }

  virtual OrthancPluginResourceType GetResourceType(int64_t resourceId);

  virtual uint64_t GetTotalCompressedSize()
  {
    return base_.GetTotalCompressedSize();
  }
    
  virtual uint64_t GetTotalUncompressedSize()
  {
    return base_.GetTotalUncompressedSize();
  }

  virtual bool IsExistingResource(int64_t internalId)
  {
    return base_.IsExistingResource(internalId);
  }

  virtual bool IsProtectedPatient(int64_t internalId)
  {
    return base_.IsProtectedPatient(internalId);
  }

  virtual void ListAvailableMetadata(std::list<int32_t>& target /*out*/,
                                     int64_t id);

  virtual void ListAvailableAttachments(std::list<int32_t>& target /*out*/,
                                        int64_t id);

  virtual void LogChange(const OrthancPluginChange& change);

  virtual void LogExportedResource(const OrthancPluginExportedResource& resource);
    
  virtual bool LookupAttachment(int64_t id,
                                int32_t contentType);

  virtual bool LookupGlobalProperty(std::string& target /*out*/,
                                    int32_t property)
  {
    return base_.LookupGlobalProperty(target, static_cast<Orthanc::GlobalProperty>(property));
  }

  virtual void LookupIdentifier(std::list<int64_t>& target /*out*/,
                                OrthancPluginResourceType level,
                                uint16_t group,
                                uint16_t element,
                                OrthancPluginIdentifierConstraint constraint,
                                const char* value)
  {
    base_.LookupIdentifier(target, Orthanc::Plugins::Convert(level),
                           Orthanc::DicomTag(group, element), 
                           Orthanc::Plugins::Convert(constraint), value);
  }

  virtual bool LookupMetadata(std::string& target /*out*/,
                              int64_t id,
                              int32_t metadataType)
  {
    return base_.LookupMetadata(target, id, static_cast<Orthanc::MetadataType>(metadataType));
  }

  virtual bool LookupParent(int64_t& parentId /*out*/,
                            int64_t resourceId);

  virtual bool LookupResource(int64_t& id /*out*/,
                              OrthancPluginResourceType& type /*out*/,
                              const char* publicId);

  virtual bool SelectPatientToRecycle(int64_t& internalId /*out*/)
  {
    return base_.SelectPatientToRecycle(internalId);
  }

  virtual bool SelectPatientToRecycle(int64_t& internalId /*out*/,
                                      int64_t patientIdToAvoid)
  {
    return base_.SelectPatientToRecycle(internalId, patientIdToAvoid);
  }


  virtual void SetGlobalProperty(int32_t property,
                                 const char* value)
  {
    base_.SetGlobalProperty(static_cast<Orthanc::GlobalProperty>(property), value);
  }

  virtual void SetMainDicomTag(int64_t id,
                               uint16_t group,
                               uint16_t element,
                               const char* value)
  {
    base_.SetMainDicomTag(id, Orthanc::DicomTag(group, element), value);
  }

  virtual void SetIdentifierTag(int64_t id,
                                uint16_t group,
                                uint16_t element,
                                const char* value)
  {
    base_.SetIdentifierTag(id, Orthanc::DicomTag(group, element), value);
  }

  virtual void SetMetadata(int64_t id,
                           int32_t metadataType,
                           const char* value)
  {
    base_.SetMetadata(id, static_cast<Orthanc::MetadataType>(metadataType), value);
  }

  virtual void SetProtectedPatient(int64_t internalId, 
                                   bool isProtected)
  {
    base_.SetProtectedPatient(internalId, isProtected);
  }

  virtual void StartTransaction();

  virtual void RollbackTransaction();

  virtual void CommitTransaction();

  virtual uint32_t GetDatabaseVersion();

  virtual void UpgradeDatabase(uint32_t  targetVersion,
                               OrthancPluginStorageArea* storageArea);

  virtual void ClearMainDicomTags(int64_t internalId)
  {
    base_.ClearMainDicomTags(internalId);
  }
};
