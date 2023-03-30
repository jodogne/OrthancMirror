/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2021-2023 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


#include "../../Sources/PrecompiledHeadersServer.h"
#include "OrthancPluginDatabaseV4.h"

#if ORTHANC_ENABLE_PLUGINS != 1
#  error The plugin support is disabled
#endif

#include "../../../OrthancFramework/Sources/Logging.h"
#include "../../../OrthancFramework/Sources/OrthancException.h"
#include "PluginsEnumerations.h"

#include "OrthancDatabasePlugin.pb.h"

#include <cassert>


namespace Orthanc
{
  class OrthancPluginDatabaseV4::Transaction : public IDatabaseWrapper::ITransaction
  {
  private:
    OrthancPluginDatabaseV4&  that_;
    IDatabaseListener&        listener_;
    void*                     transaction_;

    void CheckSuccess(OrthancPluginErrorCode code) const
    {
      that_.CheckSuccess(code);
    }
    
  public:
    Transaction(OrthancPluginDatabaseV4& that,
                IDatabaseListener& listener,
                TransactionType type) :
      that_(that),
      listener_(listener)
    {
#if 0
      CheckSuccess(that.database_.startTransaction(that.database_, &transaction_, type));
      if (transaction_ == NULL)
      {
        throw OrthancException(ErrorCode_DatabasePlugin);
      }
#endif
    }

    
    virtual ~Transaction()
    {
#if 0
      OrthancPluginErrorCode code = that_.database_.destructTransaction(transaction_);
      if (code != OrthancPluginErrorCode_Success)
      {
        // Don't throw exception in destructors
        that_.errorDictionary_.LogError(code, true);
      }
#endif
    }
    

    virtual void Rollback() ORTHANC_OVERRIDE
    {
    }
    

    virtual void Commit(int64_t fileSizeDelta) ORTHANC_OVERRIDE
    {
    }

    
    virtual void AddAttachment(int64_t id,
                               const FileInfo& attachment,
                               int64_t revision) ORTHANC_OVERRIDE
    {
    }


    virtual void ClearChanges() ORTHANC_OVERRIDE
    {
    }

    
    virtual void ClearExportedResources() ORTHANC_OVERRIDE
    {
    }

    
    virtual void DeleteAttachment(int64_t id,
                                  FileContentType attachment) ORTHANC_OVERRIDE
    {
    }

    
    virtual void DeleteMetadata(int64_t id,
                                MetadataType type) ORTHANC_OVERRIDE
    {
    }

    
    virtual void DeleteResource(int64_t id) ORTHANC_OVERRIDE
    {
    }

    
    virtual void GetAllMetadata(std::map<MetadataType, std::string>& target,
                                int64_t id) ORTHANC_OVERRIDE
    {
    }

    
    virtual void GetAllPublicIds(std::list<std::string>& target,
                                 ResourceType resourceType) ORTHANC_OVERRIDE
    {
    }

    
    virtual void GetAllPublicIds(std::list<std::string>& target,
                                 ResourceType resourceType,
                                 size_t since,
                                 size_t limit) ORTHANC_OVERRIDE
    {
    }

    
    virtual void GetChanges(std::list<ServerIndexChange>& target /*out*/,
                            bool& done /*out*/,
                            int64_t since,
                            uint32_t maxResults) ORTHANC_OVERRIDE
    {
    }

    
    virtual void GetChildrenInternalId(std::list<int64_t>& target,
                                       int64_t id) ORTHANC_OVERRIDE
    {
    }

    
    virtual void GetChildrenPublicId(std::list<std::string>& target,
                                     int64_t id) ORTHANC_OVERRIDE
    {
    }

    
    virtual void GetExportedResources(std::list<ExportedResource>& target /*out*/,
                                      bool& done /*out*/,
                                      int64_t since,
                                      uint32_t maxResults) ORTHANC_OVERRIDE
    {
    }


    virtual void GetLastExportedResource(std::list<ExportedResource>& target /*out*/) ORTHANC_OVERRIDE
    {
    }

    
    virtual void GetMainDicomTags(DicomMap& target,
                                  int64_t id) ORTHANC_OVERRIDE
    {
    }

    
    virtual std::string GetPublicId(int64_t resourceId) ORTHANC_OVERRIDE
    {
    }

    
    virtual uint64_t GetResourcesCount(ResourceType resourceType) ORTHANC_OVERRIDE
    {
    }

    
    virtual ResourceType GetResourceType(int64_t resourceId) ORTHANC_OVERRIDE
    {
    }

    
    virtual uint64_t GetTotalCompressedSize() ORTHANC_OVERRIDE
    {
    }

    
    virtual uint64_t GetTotalUncompressedSize() ORTHANC_OVERRIDE
    {
    }

    
    virtual bool IsProtectedPatient(int64_t internalId) ORTHANC_OVERRIDE
    {
    }

    
    virtual void ListAvailableAttachments(std::set<FileContentType>& target,
                                          int64_t id) ORTHANC_OVERRIDE
    {
    }

    
    virtual void LogChange(int64_t internalId,
                           const ServerIndexChange& change) ORTHANC_OVERRIDE
    {
    }

    
    virtual void LogExportedResource(const ExportedResource& resource) ORTHANC_OVERRIDE
    {
    }

    
    virtual bool LookupAttachment(FileInfo& attachment,
                                  int64_t& revision,
                                  int64_t id,
                                  FileContentType contentType) ORTHANC_OVERRIDE
    {
    }

    
    virtual bool LookupGlobalProperty(std::string& target,
                                      GlobalProperty property,
                                      bool shared) ORTHANC_OVERRIDE
    {
    }

    
    virtual bool LookupMetadata(std::string& target,
                                int64_t& revision,
                                int64_t id,
                                MetadataType type) ORTHANC_OVERRIDE
    {
    }

    
    virtual bool LookupParent(int64_t& parentId,
                              int64_t resourceId) ORTHANC_OVERRIDE
    {
    }

    
    virtual bool LookupResource(int64_t& id,
                                ResourceType& type,
                                const std::string& publicId) ORTHANC_OVERRIDE
    {
    }

    
    virtual bool SelectPatientToRecycle(int64_t& internalId) ORTHANC_OVERRIDE
    {
    }

    
    virtual bool SelectPatientToRecycle(int64_t& internalId,
                                        int64_t patientIdToAvoid) ORTHANC_OVERRIDE
    {
    }

    
    virtual void SetGlobalProperty(GlobalProperty property,
                                   bool shared,
                                   const std::string& value) ORTHANC_OVERRIDE
    {
    }

    
    virtual void ClearMainDicomTags(int64_t id) ORTHANC_OVERRIDE
    {
    }

    
    virtual void SetMetadata(int64_t id,
                             MetadataType type,
                             const std::string& value,
                             int64_t revision) ORTHANC_OVERRIDE
    {
    }

    
    virtual void SetProtectedPatient(int64_t internalId, 
                                     bool isProtected) ORTHANC_OVERRIDE
    {
    }


    virtual bool IsDiskSizeAbove(uint64_t threshold) ORTHANC_OVERRIDE
    {
    }

    
    virtual void ApplyLookupResources(std::list<std::string>& resourcesId,
                                      std::list<std::string>* instancesId, // Can be NULL if not needed
                                      const std::vector<DatabaseConstraint>& lookup,
                                      ResourceType queryLevel,
                                      size_t limit) ORTHANC_OVERRIDE
    {
    }

    
    virtual bool CreateInstance(CreateInstanceResult& result, /* out */
                                int64_t& instanceId,          /* out */
                                const std::string& patient,
                                const std::string& study,
                                const std::string& series,
                                const std::string& instance) ORTHANC_OVERRIDE
    {
    }

    
    virtual void SetResourcesContent(const ResourcesContent& content) ORTHANC_OVERRIDE
    {
    }

    
    virtual void GetChildrenMetadata(std::list<std::string>& target,
                                     int64_t resourceId,
                                     MetadataType metadata) ORTHANC_OVERRIDE
    {
    }

    
    virtual int64_t GetLastChangeIndex() ORTHANC_OVERRIDE
    {
    }

    
    virtual bool LookupResourceAndParent(int64_t& id,
                                         ResourceType& type,
                                         std::string& parentPublicId,
                                         const std::string& publicId) ORTHANC_OVERRIDE
    {
    }
  };


  static void CheckSuccess(PluginsErrorDictionary& errorDictionary,
                           OrthancPluginErrorCode code)
  {
    if (code != OrthancPluginErrorCode_Success)
    {
      errorDictionary.LogError(code, true);
      throw OrthancException(static_cast<ErrorCode>(code));
    }
  }


  static void Execute(DatabasePluginMessages::Response& response,
                      const _OrthancPluginRegisterDatabaseBackendV4& database,
                      PluginsErrorDictionary& errorDictionary,
                      const DatabasePluginMessages::Request& request)
  {
    std::string requestSerialized;
    request.SerializeToString(&requestSerialized);

    OrthancPluginMemoryBuffer64 responseSerialized;
    CheckSuccess(errorDictionary, database.operations(
                   &responseSerialized, database.backend,
                   requestSerialized.empty() ? NULL : requestSerialized.c_str(),
                   requestSerialized.size()));

    bool success = response.ParseFromArray(responseSerialized.data, responseSerialized.size);

    if (responseSerialized.size > 0)
    {
      free(responseSerialized.data);
    }

    if (!success)
    {
      throw OrthancException(ErrorCode_DatabasePlugin, "Cannot unserialize protobuf originating from the database plugin");
    }
  }

  static void ExecuteDatabase(DatabasePluginMessages::DatabaseResponse& response,
                              const _OrthancPluginRegisterDatabaseBackendV4& database,
                              PluginsErrorDictionary& errorDictionary,
                              DatabasePluginMessages::DatabaseOperation operation,
                              const DatabasePluginMessages::DatabaseRequest& request)
  {
    DatabasePluginMessages::Request fullRequest;
    fullRequest.set_type(DatabasePluginMessages::REQUEST_DATABASE);
    fullRequest.mutable_database_request()->CopyFrom(request);
    fullRequest.mutable_database_request()->set_operation(operation);

    DatabasePluginMessages::Response fullResponse;
    Execute(fullResponse, database, errorDictionary, fullRequest);
    
    response.CopyFrom(fullResponse.database_response());
  }


  OrthancPluginDatabaseV4::OrthancPluginDatabaseV4(SharedLibrary& library,
                                                   PluginsErrorDictionary&  errorDictionary,
                                                   const _OrthancPluginRegisterDatabaseBackendV4& database,
                                                   const std::string& serverIdentifier) :
    library_(library),
    errorDictionary_(errorDictionary),
    database_(database),
    serverIdentifier_(serverIdentifier),
    open_(false),
    databaseVersion_(0),
    hasFlushToDisk_(false),
    hasRevisionsSupport_(false)
  {
    CLOG(INFO, PLUGINS) << "Identifier of this Orthanc server for the global properties "
                        << "of the custom database: \"" << serverIdentifier << "\"";

    if (database_.backend == NULL ||
        database_.operations == NULL ||
        database_.finalize == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
  }

  
  OrthancPluginDatabaseV4::~OrthancPluginDatabaseV4()
  {
    database_.finalize(database_.backend);
  }

  
  void OrthancPluginDatabaseV4::Open()
  {
    if (open_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    
    {
      DatabasePluginMessages::DatabaseRequest request;
      DatabasePluginMessages::DatabaseResponse response;
      ExecuteDatabase(response, database_, errorDictionary_, DatabasePluginMessages::OPERATION_OPEN, request);
    }

    {
      DatabasePluginMessages::DatabaseRequest request;
      DatabasePluginMessages::DatabaseResponse response;
      ExecuteDatabase(response, database_, errorDictionary_, DatabasePluginMessages::OPERATION_GET_SYSTEM_INFORMATION, request);
      hasFlushToDisk_ = response.get_system_information().supports_flush_to_disk();
      hasRevisionsSupport_ = response.get_system_information().supports_revisions();
    }

    open_ = true;    
  }


  void OrthancPluginDatabaseV4::Close()
  {
    if (!open_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      DatabasePluginMessages::DatabaseRequest request;
      DatabasePluginMessages::DatabaseResponse response;
      ExecuteDatabase(response, database_, errorDictionary_, DatabasePluginMessages::OPERATION_CLOSE, request);
    }
  }
  

  bool OrthancPluginDatabaseV4::HasFlushToDisk() const
  {
    if (!open_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      return hasFlushToDisk_;
    }
  }


  void OrthancPluginDatabaseV4::FlushToDisk()
  {
    if (!open_ ||
        !hasFlushToDisk_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      DatabasePluginMessages::DatabaseRequest request;
      DatabasePluginMessages::DatabaseResponse response;
      ExecuteDatabase(response, database_, errorDictionary_, DatabasePluginMessages::OPERATION_FLUSH_TO_DISK, request);
    }
  }
  

  IDatabaseWrapper::ITransaction* OrthancPluginDatabaseV4::StartTransaction(TransactionType type,
                                                                            IDatabaseListener& listener)
  {
    if (!open_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }

    switch (type)
    {
      case TransactionType_ReadOnly:

      case TransactionType_ReadWrite:

      default:
        throw OrthancException(ErrorCode_InternalError);
    }
  }

  
  unsigned int OrthancPluginDatabaseV4::GetDatabaseVersion()
  {
    if (!open_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      return databaseVersion_;
    }
  }

  
  void OrthancPluginDatabaseV4::Upgrade(unsigned int targetVersion,
                                        IStorageArea& storageArea)
  {
    if (!open_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      // TODO
    }
  }

  
  bool OrthancPluginDatabaseV4::HasRevisionsSupport() const
  {
    if (!open_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      return hasRevisionsSupport_;
    }
  }
}
