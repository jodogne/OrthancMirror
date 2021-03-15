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

#if ORTHANC_ENABLE_PLUGINS == 1

#include "../../../OrthancFramework/Sources/SharedLibrary.h"
#include "../../Sources/Database/Compatibility/ICreateInstance.h"
#include "../../Sources/Database/Compatibility/IGetChildrenMetadata.h"
#include "../../Sources/Database/Compatibility/ILookupResources.h"
#include "../../Sources/Database/Compatibility/ILookupResourceAndParent.h"
#include "../../Sources/Database/Compatibility/ISetResourcesContent.h"
#include "../Include/orthanc/OrthancCDatabasePlugin.h"
#include "PluginsErrorDictionary.h"

namespace Orthanc
{
  class OrthancPluginDatabase :
    public IDatabaseWrapper
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
    bool                           answerDoneIgnored_;
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

    virtual void Open() ORTHANC_OVERRIDE;

    virtual void Close() ORTHANC_OVERRIDE
    {
      CheckSuccess(backend_.close(payload_));
    }

    const SharedLibrary& GetSharedLibrary() const
    {
      return library_;
    }

    virtual void FlushToDisk() ORTHANC_OVERRIDE
    {
    }

    virtual bool HasFlushToDisk() const ORTHANC_OVERRIDE
    {
      return false;
    }

    virtual IDatabaseWrapper::ITransaction* StartTransaction(TransactionType type,
                                                             IDatabaseListener& listener)
      ORTHANC_OVERRIDE;

    virtual unsigned int GetDatabaseVersion() ORTHANC_OVERRIDE;

    virtual void Upgrade(unsigned int targetVersion,
                         IStorageArea& storageArea) ORTHANC_OVERRIDE;    

    void AnswerReceived(const _OrthancPluginDatabaseAnswer& answer);
  };
}

#endif
