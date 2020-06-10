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

#if ORTHANC_ENABLE_PLUGINS == 1

#include "../../Core/SharedLibrary.h"
#include "../../OrthancServer/Database/Compatibility/ICreateInstance.h"
#include "../../OrthancServer/Database/Compatibility/IGetChildrenMetadata.h"
#include "../../OrthancServer/Database/Compatibility/ILookupResources.h"
#include "../../OrthancServer/Database/Compatibility/ILookupResourceAndParent.h"
#include "../../OrthancServer/Database/Compatibility/ISetResourcesContent.h"
#include "../Include/orthanc/OrthancCDatabasePlugin.h"
#include "PluginsErrorDictionary.h"

namespace Orthanc
{
  class OrthancPluginDatabase :
    public IDatabaseWrapper,
    public Compatibility::ICreateInstance,
    public Compatibility::IGetChildrenMetadata,
    public Compatibility::ILookupResources,
    public Compatibility::ILookupResourceAndParent,
    public Compatibility::ISetResourcesContent
  {
  private:
    class Transaction;

    typedef std::pair<int64_t, ResourceType>     AnswerResource;
    typedef std::map<MetadataType, std::string>  AnswerMetadata;

    SharedLibrary&  library_;
    PluginsErrorDictionary&  errorDictionary_;
    _OrthancPluginDatabaseAnswerType type_;
    OrthancPluginDatabaseBackend backend_;
    OrthancPluginDatabaseExtensions extensions_;
    void* payload_;
    IDatabaseListener* listener_;

    bool      fastGetTotalSize_;
    uint64_t  currentDiskSize_;

    std::list<std::string>         answerStrings_;
    std::list<int32_t>             answerInt32_;
    std::list<int64_t>             answerInt64_;
    std::list<AnswerResource>      answerResources_;
    std::list<FileInfo>            answerAttachments_;

    DicomMap*                      answerDicomMap_;
    std::list<ServerIndexChange>*  answerChanges_;
    std::list<ExportedResource>*   answerExportedResources_;
    bool*                          answerDone_;
    std::list<std::string>*        answerMatchingResources_;
    std::list<std::string>*        answerMatchingInstances_;
    AnswerMetadata*                answerMetadata_;

    OrthancPluginDatabaseContext* GetContext()
    {
      return reinterpret_cast<OrthancPluginDatabaseContext*>(this);
    }

    void CheckSuccess(OrthancPluginErrorCode code);

    void ResetAnswers();

    void ForwardAnswers(std::list<int64_t>& target);

    void ForwardAnswers(std::list<std::string>& target);

    bool ForwardSingleAnswer(std::string& target);

    bool ForwardSingleAnswer(int64_t& target);

  public:
    OrthancPluginDatabase(SharedLibrary& library,
                          PluginsErrorDictionary&  errorDictionary,
                          const OrthancPluginDatabaseBackend& backend,
                          const OrthancPluginDatabaseExtensions* extensions,
                          size_t extensionsSize,
                          void *payload);

    virtual void Open() 
      ORTHANC_OVERRIDE;

    virtual void Close() 
      ORTHANC_OVERRIDE
    {
      CheckSuccess(backend_.close(payload_));
    }

    const SharedLibrary& GetSharedLibrary() const
    {
      return library_;
    }

    virtual void AddAttachment(int64_t id,
                               const FileInfo& attachment) 
      ORTHANC_OVERRIDE;

    virtual void AttachChild(int64_t parent,
                             int64_t child) 
      ORTHANC_OVERRIDE;

    virtual void ClearChanges() 
      ORTHANC_OVERRIDE;

    virtual void ClearExportedResources() 
      ORTHANC_OVERRIDE;

    virtual int64_t CreateResource(const std::string& publicId,
                                   ResourceType type) 
      ORTHANC_OVERRIDE;

    virtual void DeleteAttachment(int64_t id,
                                  FileContentType attachment) 
      ORTHANC_OVERRIDE;

    virtual void DeleteMetadata(int64_t id,
                                MetadataType type) 
      ORTHANC_OVERRIDE;

    virtual void DeleteResource(int64_t id) 
      ORTHANC_OVERRIDE;

    virtual void FlushToDisk() 
      ORTHANC_OVERRIDE
    {
    }

    virtual bool HasFlushToDisk() const 
      ORTHANC_OVERRIDE
    {
      return false;
    }

    virtual void GetAllMetadata(std::map<MetadataType, std::string>& target,
                                int64_t id) 
      ORTHANC_OVERRIDE;

    virtual void GetAllPublicIds(std::list<std::string>& target,
                                 ResourceType resourceType) 
      ORTHANC_OVERRIDE;

    virtual void GetAllPublicIds(std::list<std::string>& target,
                                 ResourceType resourceType,
                                 size_t since,
                                 size_t limit) 
      ORTHANC_OVERRIDE;

    virtual void GetChanges(std::list<ServerIndexChange>& target /*out*/,
                            bool& done /*out*/,
                            int64_t since,
                            uint32_t maxResults) 
      ORTHANC_OVERRIDE;

    virtual void GetChildrenInternalId(std::list<int64_t>& target,
                                       int64_t id) 
      ORTHANC_OVERRIDE;

    virtual void GetChildrenPublicId(std::list<std::string>& target,
                                     int64_t id) 
      ORTHANC_OVERRIDE;

    virtual void GetExportedResources(std::list<ExportedResource>& target /*out*/,
                                      bool& done /*out*/,
                                      int64_t since,
                                      uint32_t maxResults) 
      ORTHANC_OVERRIDE;

    virtual void GetLastChange(std::list<ServerIndexChange>& target /*out*/) 
      ORTHANC_OVERRIDE;

    virtual void GetLastExportedResource(std::list<ExportedResource>& target /*out*/) 
      ORTHANC_OVERRIDE;

    virtual void GetMainDicomTags(DicomMap& map,
                                  int64_t id) 
      ORTHANC_OVERRIDE;

    virtual std::string GetPublicId(int64_t resourceId) 
      ORTHANC_OVERRIDE;

    virtual uint64_t GetResourceCount(ResourceType resourceType) 
      ORTHANC_OVERRIDE;

    virtual ResourceType GetResourceType(int64_t resourceId) 
      ORTHANC_OVERRIDE;

    virtual uint64_t GetTotalCompressedSize() 
      ORTHANC_OVERRIDE;
    
    virtual uint64_t GetTotalUncompressedSize() 
      ORTHANC_OVERRIDE;

    virtual bool IsExistingResource(int64_t internalId) 
      ORTHANC_OVERRIDE;

    virtual bool IsProtectedPatient(int64_t internalId) 
      ORTHANC_OVERRIDE;

    virtual void ListAvailableAttachments(std::list<FileContentType>& target,
                                          int64_t id) 
      ORTHANC_OVERRIDE;

    virtual void LogChange(int64_t internalId,
                           const ServerIndexChange& change) 
      ORTHANC_OVERRIDE;

    virtual void LogExportedResource(const ExportedResource& resource) 
      ORTHANC_OVERRIDE;
    
    virtual bool LookupAttachment(FileInfo& attachment,
                                  int64_t id,
                                  FileContentType contentType) 
      ORTHANC_OVERRIDE;

    virtual bool LookupGlobalProperty(std::string& target,
                                      GlobalProperty property) 
      ORTHANC_OVERRIDE;

    virtual bool LookupMetadata(std::string& target,
                                int64_t id,
                                MetadataType type) 
      ORTHANC_OVERRIDE;

    virtual bool LookupParent(int64_t& parentId,
                              int64_t resourceId) 
      ORTHANC_OVERRIDE;

    virtual bool LookupResource(int64_t& id,
                                ResourceType& type,
                                const std::string& publicId) 
      ORTHANC_OVERRIDE;

    virtual bool SelectPatientToRecycle(int64_t& internalId) 
      ORTHANC_OVERRIDE;

    virtual bool SelectPatientToRecycle(int64_t& internalId,
                                        int64_t patientIdToAvoid) 
      ORTHANC_OVERRIDE;

    virtual void SetGlobalProperty(GlobalProperty property,
                                   const std::string& value) 
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

    virtual void SetMetadata(int64_t id,
                             MetadataType type,
                             const std::string& value) 
      ORTHANC_OVERRIDE;

    virtual void SetProtectedPatient(int64_t internalId, 
                                     bool isProtected) 
      ORTHANC_OVERRIDE;

    virtual IDatabaseWrapper::ITransaction* StartTransaction() 
      ORTHANC_OVERRIDE;

    virtual void SetListener(IDatabaseListener& listener) 
      ORTHANC_OVERRIDE
    {
      listener_ = &listener;
    }

    virtual unsigned int GetDatabaseVersion() 
      ORTHANC_OVERRIDE;

    virtual void Upgrade(unsigned int targetVersion,
                         IStorageArea& storageArea) 
      ORTHANC_OVERRIDE;

    void AnswerReceived(const _OrthancPluginDatabaseAnswer& answer);

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
      ORTHANC_OVERRIDE;

    // From the "ILookupResources" interface
    virtual void GetAllInternalIds(std::list<int64_t>& target,
                                   ResourceType resourceType) 
      ORTHANC_OVERRIDE;

    // From the "ILookupResources" interface
    virtual void LookupIdentifier(std::list<int64_t>& result,
                                  ResourceType level,
                                  const DicomTag& tag,
                                  Compatibility::IdentifierConstraintType type,
                                  const std::string& value)
      ORTHANC_OVERRIDE;
    
    // From the "ILookupResources" interface
    virtual void LookupIdentifierRange(std::list<int64_t>& result,
                                       ResourceType level,
                                       const DicomTag& tag,
                                       const std::string& start,
                                       const std::string& end)
      ORTHANC_OVERRIDE;

    virtual void SetResourcesContent(const Orthanc::ResourcesContent& content)
      ORTHANC_OVERRIDE;

    virtual void GetChildrenMetadata(std::list<std::string>& target,
                                     int64_t resourceId,
                                     MetadataType metadata)
      ORTHANC_OVERRIDE;

    virtual int64_t GetLastChangeIndex() ORTHANC_OVERRIDE;
  
    virtual void TagMostRecentPatient(int64_t patient) ORTHANC_OVERRIDE;

    virtual bool LookupResourceAndParent(int64_t& id,
                                         ResourceType& type,
                                         std::string& parentPublicId,
                                         const std::string& publicId)
      ORTHANC_OVERRIDE;
  };
}

#endif
