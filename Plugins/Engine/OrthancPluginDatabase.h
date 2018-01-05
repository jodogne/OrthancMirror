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

#if ORTHANC_ENABLE_PLUGINS == 1

#include "../../Core/SharedLibrary.h"
#include "../../OrthancServer/IDatabaseWrapper.h"
#include "../Include/orthanc/OrthancCDatabasePlugin.h"
#include "PluginsErrorDictionary.h"

namespace Orthanc
{
  class OrthancPluginDatabase : public IDatabaseWrapper
  {
  private:
    class Transaction;

    typedef std::pair<int64_t, ResourceType>  AnswerResource;

    SharedLibrary&  library_;
    PluginsErrorDictionary&  errorDictionary_;
    _OrthancPluginDatabaseAnswerType type_;
    OrthancPluginDatabaseBackend backend_;
    OrthancPluginDatabaseExtensions extensions_;
    void* payload_;
    IDatabaseListener* listener_;

    std::list<std::string>         answerStrings_;
    std::list<int32_t>             answerInt32_;
    std::list<int64_t>             answerInt64_;
    std::list<AnswerResource>      answerResources_;
    std::list<FileInfo>            answerAttachments_;

    DicomMap*                      answerDicomMap_;
    std::list<ServerIndexChange>*  answerChanges_;
    std::list<ExportedResource>*   answerExportedResources_;
    bool*                          answerDone_;

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
    {
      CheckSuccess(backend_.open(payload_));
    }

    virtual void Close()
    {
      CheckSuccess(backend_.close(payload_));
    }

    const SharedLibrary& GetSharedLibrary() const
    {
      return library_;
    }

    virtual void AddAttachment(int64_t id,
                               const FileInfo& attachment);

    virtual void AttachChild(int64_t parent,
                             int64_t child);

    virtual void ClearChanges();

    virtual void ClearExportedResources();

    virtual int64_t CreateResource(const std::string& publicId,
                                   ResourceType type);

    virtual void DeleteAttachment(int64_t id,
                                  FileContentType attachment);

    virtual void DeleteMetadata(int64_t id,
                                MetadataType type);

    virtual void DeleteResource(int64_t id);

    virtual void FlushToDisk()
    {
    }

    virtual bool HasFlushToDisk() const
    {
      return false;
    }

    virtual void GetAllMetadata(std::map<MetadataType, std::string>& target,
                                int64_t id);

    virtual void GetAllInternalIds(std::list<int64_t>& target,
                                   ResourceType resourceType);

    virtual void GetAllPublicIds(std::list<std::string>& target,
                                 ResourceType resourceType);

    virtual void GetAllPublicIds(std::list<std::string>& target,
                                 ResourceType resourceType,
                                 size_t since,
                                 size_t limit);

    virtual void GetChanges(std::list<ServerIndexChange>& target /*out*/,
                            bool& done /*out*/,
                            int64_t since,
                            uint32_t maxResults);

    virtual void GetChildrenInternalId(std::list<int64_t>& target,
                                       int64_t id);

    virtual void GetChildrenPublicId(std::list<std::string>& target,
                                     int64_t id);

    virtual void GetExportedResources(std::list<ExportedResource>& target /*out*/,
                                      bool& done /*out*/,
                                      int64_t since,
                                      uint32_t maxResults);

    virtual void GetLastChange(std::list<ServerIndexChange>& target /*out*/);

    virtual void GetLastExportedResource(std::list<ExportedResource>& target /*out*/);

    virtual void GetMainDicomTags(DicomMap& map,
                                  int64_t id);

    virtual std::string GetPublicId(int64_t resourceId);

    virtual uint64_t GetResourceCount(ResourceType resourceType);

    virtual ResourceType GetResourceType(int64_t resourceId);

    virtual uint64_t GetTotalCompressedSize();
    
    virtual uint64_t GetTotalUncompressedSize();

    virtual bool IsExistingResource(int64_t internalId);

    virtual bool IsProtectedPatient(int64_t internalId);

    virtual void ListAvailableMetadata(std::list<MetadataType>& target,
                                       int64_t id);

    virtual void ListAvailableAttachments(std::list<FileContentType>& target,
                                          int64_t id);

    virtual void LogChange(int64_t internalId,
                           const ServerIndexChange& change);

    virtual void LogExportedResource(const ExportedResource& resource);
    
    virtual bool LookupAttachment(FileInfo& attachment,
                                  int64_t id,
                                  FileContentType contentType);

    virtual bool LookupGlobalProperty(std::string& target,
                                      GlobalProperty property);

    virtual void LookupIdentifier(std::list<int64_t>& result,
                                  ResourceType level,
                                  const DicomTag& tag,
                                  IdentifierConstraintType type,
                                  const std::string& value);

    virtual bool LookupMetadata(std::string& target,
                                int64_t id,
                                MetadataType type);

    virtual bool LookupParent(int64_t& parentId,
                              int64_t resourceId);

    virtual bool LookupResource(int64_t& id,
                                ResourceType& type,
                                const std::string& publicId);

    virtual bool SelectPatientToRecycle(int64_t& internalId);

    virtual bool SelectPatientToRecycle(int64_t& internalId,
                                        int64_t patientIdToAvoid);

    virtual void SetGlobalProperty(GlobalProperty property,
                                   const std::string& value);

    virtual void ClearMainDicomTags(int64_t id);

    virtual void SetMainDicomTag(int64_t id,
                                 const DicomTag& tag,
                                 const std::string& value);

    virtual void SetIdentifierTag(int64_t id,
                                  const DicomTag& tag,
                                  const std::string& value);

    virtual void SetMetadata(int64_t id,
                             MetadataType type,
                             const std::string& value);

    virtual void SetProtectedPatient(int64_t internalId, 
                                     bool isProtected);

    virtual SQLite::ITransaction* StartTransaction();

    virtual void SetListener(IDatabaseListener& listener)
    {
      listener_ = &listener;
    }

    virtual unsigned int GetDatabaseVersion();

    virtual void Upgrade(unsigned int targetVersion,
                         IStorageArea& storageArea);

    void AnswerReceived(const _OrthancPluginDatabaseAnswer& answer);
  };
}

#endif
