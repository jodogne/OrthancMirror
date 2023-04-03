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
#include "../../Sources/Database/ResourcesContent.h"
#include "../../Sources/Database/VoidDatabaseListener.h"
#include "PluginsEnumerations.h"

#include "OrthancDatabasePlugin.pb.h"  // Auto-generated file

#include <cassert>


namespace Orthanc
{
  static void CheckSuccess(PluginsErrorDictionary& errorDictionary,
                           OrthancPluginErrorCode code)
  {
    if (code != OrthancPluginErrorCode_Success)
    {
      errorDictionary.LogError(code, true);
      throw OrthancException(static_cast<ErrorCode>(code));
    }
  }


  static ResourceType Convert(DatabasePluginMessages::ResourceType type)
  {
    switch (type)
    {
      case DatabasePluginMessages::RESOURCE_PATIENT:
        return ResourceType_Patient;

      case DatabasePluginMessages::RESOURCE_STUDY:
        return ResourceType_Study;

      case DatabasePluginMessages::RESOURCE_SERIES:
        return ResourceType_Series;

      case DatabasePluginMessages::RESOURCE_INSTANCE:
        return ResourceType_Instance;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }

    
  static DatabasePluginMessages::ResourceType Convert(ResourceType type)
  {
    switch (type)
    {
      case ResourceType_Patient:
        return DatabasePluginMessages::RESOURCE_PATIENT;

      case ResourceType_Study:
        return DatabasePluginMessages::RESOURCE_STUDY;

      case ResourceType_Series:
        return DatabasePluginMessages::RESOURCE_SERIES;

      case ResourceType_Instance:
        return DatabasePluginMessages::RESOURCE_INSTANCE;

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }

    
  static FileInfo Convert(const DatabasePluginMessages::FileInfo& source)
  {
    return FileInfo(source.uuid(),
                    static_cast<FileContentType>(source.content_type()),
                    source.uncompressed_size(),
                    source.uncompressed_hash(),
                    static_cast<CompressionType>(source.compression_type()),
                    source.compressed_size(),
                    source.compressed_hash());
  }


  static ServerIndexChange Convert(const DatabasePluginMessages::ServerIndexChange& source)
  {
    return ServerIndexChange(source.seq(),
                             static_cast<ChangeType>(source.change_type()),
                             Convert(source.resource_type()),
                             source.public_id(),
                             source.date());
  }


  static ExportedResource Convert(const DatabasePluginMessages::ExportedResource& source)
  {
    return ExportedResource(source.seq(),
                            Convert(source.resource_type()),
                            source.public_id(),
                            source.modality(),
                            source.date(),
                            source.patient_id(),
                            source.study_instance_uid(),
                            source.series_instance_uid(),
                            source.sop_instance_uid());
  }


  static void Execute(DatabasePluginMessages::Response& response,
                      const OrthancPluginDatabaseV4& database,
                      const DatabasePluginMessages::Request& request)
  {
    std::string requestSerialized;
    request.SerializeToString(&requestSerialized);

    OrthancPluginMemoryBuffer64 responseSerialized;
    CheckSuccess(database.GetErrorDictionary(), database.GetDefinition().operations(
                   &responseSerialized, database.GetDefinition().backend,
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
                              const OrthancPluginDatabaseV4& database,
                              DatabasePluginMessages::DatabaseOperation operation,
                              const DatabasePluginMessages::DatabaseRequest& request)
  {
    DatabasePluginMessages::Request fullRequest;
    fullRequest.set_type(DatabasePluginMessages::REQUEST_DATABASE);
    fullRequest.mutable_database_request()->CopyFrom(request);
    fullRequest.mutable_database_request()->set_operation(operation);

    DatabasePluginMessages::Response fullResponse;
    Execute(fullResponse, database, fullRequest);
    
    response.CopyFrom(fullResponse.database_response());
  }

  
  class OrthancPluginDatabaseV4::Transaction : public IDatabaseWrapper::ITransaction
  {
  private:
    OrthancPluginDatabaseV4&  database_;
    IDatabaseListener&        listener_;
    void*                     transaction_;
    
    void ExecuteTransaction(DatabasePluginMessages::TransactionResponse& response,
                            DatabasePluginMessages::TransactionOperation operation,
                            const DatabasePluginMessages::TransactionRequest& request)
    {
      DatabasePluginMessages::Request fullRequest;
      fullRequest.set_type(DatabasePluginMessages::REQUEST_TRANSACTION);
      fullRequest.mutable_transaction_request()->CopyFrom(request);
      fullRequest.mutable_transaction_request()->set_transaction(reinterpret_cast<intptr_t>(transaction_));
      fullRequest.mutable_transaction_request()->set_operation(operation);

      DatabasePluginMessages::Response fullResponse;
      Execute(fullResponse, database_, fullRequest);
    
      response.CopyFrom(fullResponse.transaction_response());
    }
    
    
    void ExecuteTransaction(DatabasePluginMessages::TransactionResponse& response,
                            DatabasePluginMessages::TransactionOperation operation)
    {
      DatabasePluginMessages::TransactionRequest request;    // Ignored
      ExecuteTransaction(response, operation, request);
    }
    
    
    void ExecuteTransaction(DatabasePluginMessages::TransactionOperation operation,
                            const DatabasePluginMessages::TransactionRequest& request)
    {
      DatabasePluginMessages::TransactionResponse response;  // Ignored
      ExecuteTransaction(response, operation, request);
    }
    
    
    void ExecuteTransaction(DatabasePluginMessages::TransactionOperation operation)
    {
      DatabasePluginMessages::TransactionResponse response;  // Ignored
      DatabasePluginMessages::TransactionRequest request;    // Ignored
      ExecuteTransaction(response, operation, request);
    }


  public:
    Transaction(OrthancPluginDatabaseV4& database,
                IDatabaseListener& listener,
                TransactionType type) :
      database_(database),
      listener_(listener),
      transaction_(NULL)
    {
      DatabasePluginMessages::DatabaseRequest request;

      switch (type)
      {
        case TransactionType_ReadOnly:
          request.mutable_start_transaction()->set_type(DatabasePluginMessages::TRANSACTION_READ_ONLY);
          break;

        case TransactionType_ReadWrite:
          request.mutable_start_transaction()->set_type(DatabasePluginMessages::TRANSACTION_READ_WRITE);
          break;

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }

      DatabasePluginMessages::DatabaseResponse response;
      ExecuteDatabase(response, database, DatabasePluginMessages::OPERATION_START_TRANSACTION, request);

      transaction_ = reinterpret_cast<void*>(response.start_transaction().transaction());

      if (transaction_ == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }
    }

    
    virtual ~Transaction()
    {
      try
      {
        DatabasePluginMessages::DatabaseRequest request;
        request.mutable_finalize_transaction()->set_transaction(reinterpret_cast<intptr_t>(transaction_));

        DatabasePluginMessages::DatabaseResponse response;
        ExecuteDatabase(response, database_, DatabasePluginMessages::OPERATION_FINALIZE_TRANSACTION, request);
      }
      catch (OrthancException& e)
      {
        // Destructors must not throw exceptions
        LOG(ERROR) << "Cannot finalize the database engine: " << e.What();
      }
    }
    

    virtual void Rollback() ORTHANC_OVERRIDE
    {
      ExecuteTransaction(DatabasePluginMessages::OPERATION_ROLLBACK);
    }
    

    virtual void Commit(int64_t fileSizeDelta) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_commit()->set_file_size_delta(fileSizeDelta);

      ExecuteTransaction(DatabasePluginMessages::OPERATION_COMMIT, request);
    }

    
    virtual void AddAttachment(int64_t id,
                               const FileInfo& attachment,
                               int64_t revision) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_add_attachment()->set_id(id);
      request.mutable_add_attachment()->mutable_attachment()->set_uuid(attachment.GetUuid());
      request.mutable_add_attachment()->mutable_attachment()->set_content_type(attachment.GetContentType());
      request.mutable_add_attachment()->mutable_attachment()->set_uncompressed_size(attachment.GetUncompressedSize());
      request.mutable_add_attachment()->mutable_attachment()->set_uncompressed_hash(attachment.GetUncompressedMD5());
      request.mutable_add_attachment()->mutable_attachment()->set_compression_type(attachment.GetCompressionType());
      request.mutable_add_attachment()->mutable_attachment()->set_compressed_size(attachment.GetCompressedSize());
      request.mutable_add_attachment()->mutable_attachment()->set_compressed_hash(attachment.GetCompressedMD5());        
      request.mutable_add_attachment()->set_revision(revision);

      ExecuteTransaction(DatabasePluginMessages::OPERATION_ADD_ATTACHMENT, request);
    }


    virtual void ClearChanges() ORTHANC_OVERRIDE
    {
      ExecuteTransaction(DatabasePluginMessages::OPERATION_CLEAR_CHANGES);
    }

    
    virtual void ClearExportedResources() ORTHANC_OVERRIDE
    {
      ExecuteTransaction(DatabasePluginMessages::OPERATION_CLEAR_EXPORTED_RESOURCES);
    }


    virtual void DeleteAttachment(int64_t id,
                                  FileContentType attachment) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_delete_attachment()->set_id(id);
      request.mutable_delete_attachment()->set_type(attachment);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_DELETE_ATTACHMENT, request);

      listener_.SignalAttachmentDeleted(Convert(response.delete_attachment().deleted_attachment()));
    }

    
    virtual void DeleteMetadata(int64_t id,
                                MetadataType type) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_delete_metadata()->set_id(id);
      request.mutable_delete_metadata()->set_type(type);

      ExecuteTransaction(DatabasePluginMessages::OPERATION_DELETE_METADATA, request);
    }

    
    virtual void DeleteResource(int64_t id) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_delete_resource()->set_id(id);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_DELETE_RESOURCE, request);

      for (int i = 0; i < response.delete_resource().deleted_attachments().size(); i++)
      {
        listener_.SignalAttachmentDeleted(Convert(response.delete_resource().deleted_attachments(i)));
      }

      for (int i = 0; i < response.delete_resource().deleted_resources().size(); i++)
      {
        listener_.SignalResourceDeleted(Convert(response.delete_resource().deleted_resources(i).level()),
                                        response.delete_resource().deleted_resources(i).public_id());
      }

      if (response.delete_resource().is_remaining_ancestor())
      {
        listener_.SignalRemainingAncestor(Convert(response.delete_resource().remaining_ancestor().level()),
                                          response.delete_resource().remaining_ancestor().public_id());
      }
    }

    
    virtual void GetAllMetadata(std::map<MetadataType, std::string>& target,
                                int64_t id) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_get_all_metadata()->set_id(id);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_ALL_METADATA, request);

      target.clear();
      for (int i = 0; i < response.get_all_metadata().metadata().size(); i++)
      {
        MetadataType key = static_cast<MetadataType>(response.get_all_metadata().metadata(i).type());
          
        if (target.find(key) == target.end())
        {
          target[key] = response.get_all_metadata().metadata(i).value();
        }
        else
        {
          throw OrthancException(ErrorCode_DatabasePlugin);
        }
      }
    }

    
    virtual void GetAllPublicIds(std::list<std::string>& target,
                                 ResourceType resourceType) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_get_all_public_ids()->set_resource_type(Convert(resourceType));

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_ALL_PUBLIC_IDS, request);

      target.clear();
      for (int i = 0; i < response.get_all_public_ids().ids().size(); i++)
      {
        target.push_back(response.get_all_public_ids().ids(i));
      }
    }

    
    virtual void GetAllPublicIds(std::list<std::string>& target,
                                 ResourceType resourceType,
                                 int64_t since,
                                 uint32_t limit) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_get_all_public_ids_with_limits()->set_resource_type(Convert(resourceType));
      request.mutable_get_all_public_ids_with_limits()->set_since(since);
      request.mutable_get_all_public_ids_with_limits()->set_limit(limit);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_ALL_PUBLIC_IDS_WITH_LIMITS, request);

      target.clear();
      for (int i = 0; i < response.get_all_public_ids_with_limits().ids().size(); i++)
      {
        target.push_back(response.get_all_public_ids_with_limits().ids(i));
      }
    }

    
    virtual void GetChanges(std::list<ServerIndexChange>& target /*out*/,
                            bool& done /*out*/,
                            int64_t since,
                            uint32_t limit) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_get_changes()->set_since(since);
      request.mutable_get_changes()->set_limit(limit);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_CHANGES, request);

      done = response.get_changes().done();
        
      target.clear();
      for (int i = 0; i < response.get_changes().changes().size(); i++)
      {
        target.push_back(Convert(response.get_changes().changes(i)));
      }
    }

    
    virtual void GetChildrenInternalId(std::list<int64_t>& target,
                                       int64_t id) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_get_children_internal_id()->set_id(id);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_CHILDREN_INTERNAL_ID, request);

      target.clear();
      for (int i = 0; i < response.get_children_internal_id().ids().size(); i++)
      {
        target.push_back(response.get_children_internal_id().ids(i));
      }
    }

    
    virtual void GetChildrenPublicId(std::list<std::string>& target,
                                     int64_t id) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_get_children_public_id()->set_id(id);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_CHILDREN_PUBLIC_ID, request);

      target.clear();
      for (int i = 0; i < response.get_children_public_id().ids().size(); i++)
      {
        target.push_back(response.get_children_public_id().ids(i));
      }
    }

    
    virtual void GetExportedResources(std::list<ExportedResource>& target /*out*/,
                                      bool& done /*out*/,
                                      int64_t since,
                                      uint32_t limit) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_get_exported_resources()->set_since(since);
      request.mutable_get_exported_resources()->set_limit(limit);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_EXPORTED_RESOURCES, request);

      done = response.get_exported_resources().done();
        
      target.clear();
      for (int i = 0; i < response.get_exported_resources().resources().size(); i++)
      {
        target.push_back(Convert(response.get_exported_resources().resources(i)));
      }
    }

    
    virtual void GetLastChange(std::list<ServerIndexChange>& target /*out*/) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_LAST_CHANGE);

      target.clear();
      if (response.get_last_change().found())
      {
        target.push_back(Convert(response.get_last_change().change()));
      }
    }

    
    virtual void GetLastExportedResource(std::list<ExportedResource>& target /*out*/) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_LAST_EXPORTED_RESOURCE);

      target.clear();
      if (response.get_last_exported_resource().found())
      {
        target.push_back(Convert(response.get_last_exported_resource().resource()));
      }
    }

    
    virtual void GetMainDicomTags(DicomMap& target,
                                  int64_t id) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_get_main_dicom_tags()->set_id(id);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_MAIN_DICOM_TAGS, request);

      target.Clear();

      for (int i = 0; i < response.get_main_dicom_tags().tags().size(); i++)
      {
        const DatabasePluginMessages::GetMainDicomTags_Response_Tag& tag = response.get_main_dicom_tags().tags(i);
        if (tag.group() > 0xffffu ||
            tag.element() > 0xffffu)
        {
          throw OrthancException(ErrorCode_ParameterOutOfRange);
        }
        else
        {
          target.SetValue(tag.group(), tag.element(), tag.value(), false);
        }
      }
    }

    
    virtual std::string GetPublicId(int64_t resourceId) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_get_public_id()->set_id(resourceId);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_PUBLIC_ID, request);
      return response.get_public_id().id();
    }

    
    virtual uint64_t GetResourcesCount(ResourceType resourceType) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_get_resources_count()->set_type(Convert(resourceType));

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_RESOURCES_COUNT, request);
      return response.get_resources_count().count();
    }

    
    virtual ResourceType GetResourceType(int64_t resourceId) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_get_resource_type()->set_id(resourceId);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_RESOURCE_TYPE, request);
      return Convert(response.get_resource_type().type());
    }

    
    virtual uint64_t GetTotalCompressedSize() ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_TOTAL_COMPRESSED_SIZE);
      return response.get_total_compressed_size().size();
    }

    
    virtual uint64_t GetTotalUncompressedSize() ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_TOTAL_UNCOMPRESSED_SIZE);
      return response.get_total_uncompressed_size().size();
    }

    
    virtual bool IsProtectedPatient(int64_t internalId) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_is_protected_patient()->set_patient_id(internalId);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_IS_PROTECTED_PATIENT, request);
      return response.is_protected_patient().protected_patient();
    }

    
    virtual void ListAvailableAttachments(std::set<FileContentType>& target,
                                          int64_t id) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_list_available_attachments()->set_id(id);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_LIST_AVAILABLE_ATTACHMENTS, request);

      target.clear();
      for (int i = 0; i < response.list_available_attachments().attachments().size(); i++)
      {
        FileContentType attachment = static_cast<FileContentType>(response.list_available_attachments().attachments(i));

        if (target.find(attachment) == target.end())
        {
          target.insert(attachment);
        }
        else
        {
          throw OrthancException(ErrorCode_DatabasePlugin);
        }
      }
    }

    
    virtual void LogChange(int64_t internalId,
                           const ServerIndexChange& change) ORTHANC_OVERRIDE
    {
      // TODO => Simplify "IDatabaseWrapper"
      
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_log_change()->set_change_type(change.GetChangeType());
      request.mutable_log_change()->set_resource_id(internalId);
      request.mutable_log_change()->set_resource_type(Convert(change.GetResourceType()));
      request.mutable_log_change()->set_date(change.GetDate());

      ExecuteTransaction(DatabasePluginMessages::OPERATION_LOG_CHANGE, request);
    }

    
    virtual void LogExportedResource(const ExportedResource& resource) ORTHANC_OVERRIDE
    {
      // TODO: "seq" is ignored, could be simplified in "ExportedResource"
      
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_log_exported_resource()->set_resource_type(Convert(resource.GetResourceType()));
      request.mutable_log_exported_resource()->set_public_id(resource.GetPublicId());
      request.mutable_log_exported_resource()->set_modality(resource.GetModality());
      request.mutable_log_exported_resource()->set_date(resource.GetDate());
      request.mutable_log_exported_resource()->set_patient_id(resource.GetPatientId());
      request.mutable_log_exported_resource()->set_study_instance_uid(resource.GetStudyInstanceUid());
      request.mutable_log_exported_resource()->set_series_instance_uid(resource.GetSeriesInstanceUid());
      request.mutable_log_exported_resource()->set_sop_instance_uid(resource.GetSopInstanceUid());

      ExecuteTransaction(DatabasePluginMessages::OPERATION_LOG_EXPORTED_RESOURCE, request);
    }

    
    virtual bool LookupAttachment(FileInfo& attachment,
                                  int64_t& revision,
                                  int64_t id,
                                  FileContentType contentType) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_lookup_attachment()->set_id(id);
      request.mutable_lookup_attachment()->set_content_type(contentType);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_LOOKUP_ATTACHMENT, request);

      if (response.lookup_attachment().found())
      {
        attachment = Convert(response.lookup_attachment().attachment());
        revision = response.lookup_attachment().revision();
        return true;
      }
      else
      {
        return false;
      }
    }

    
    virtual bool LookupGlobalProperty(std::string& target,
                                      GlobalProperty property,
                                      bool shared) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_lookup_global_property()->set_server_id(shared ? "" : database_.GetServerIdentifier());
      request.mutable_lookup_global_property()->set_property(property);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_LOOKUP_GLOBAL_PROPERTY, request);

      if (response.lookup_global_property().found())
      {
        target = response.lookup_global_property().value();
        return true;
      }
      else
      {
        return false;
      }
    }

    
    virtual bool LookupMetadata(std::string& target,
                                int64_t& revision,
                                int64_t id,
                                MetadataType type) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_lookup_metadata()->set_id(id);
      request.mutable_lookup_metadata()->set_metadata_type(type);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_LOOKUP_METADATA, request);

      if (response.lookup_metadata().found())
      {
        target = response.lookup_metadata().value();
        revision = response.lookup_metadata().revision();
        return true;
      }
      else
      {
        return false;
      }
    }

    
    virtual bool LookupParent(int64_t& parentId,
                              int64_t resourceId) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_lookup_parent()->set_id(resourceId);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_LOOKUP_PARENT, request);

      if (response.lookup_parent().found())
      {
        parentId = response.lookup_parent().parent();
        return true;
      }
      else
      {
        return false;
      }
    }

    
    virtual bool LookupResource(int64_t& id,
                                ResourceType& type,
                                const std::string& publicId) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_lookup_resource()->set_public_id(publicId);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_LOOKUP_RESOURCE, request);

      if (response.lookup_resource().found())
      {
        id = response.lookup_resource().internal_id();
        type = Convert(response.lookup_resource().type());
        return true;
      }
      else
      {
        return false;
      }
    }

    
    virtual bool SelectPatientToRecycle(int64_t& internalId) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_SELECT_PATIENT_TO_RECYCLE);

      if (response.select_patient_to_recycle().found())
      {
        internalId = response.select_patient_to_recycle().patient_id();
        return true;
      }
      else
      {
        return false;
      }
    }

    
    virtual bool SelectPatientToRecycle(int64_t& internalId,
                                        int64_t patientIdToAvoid) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_select_patient_to_recycle_with_avoid()->set_patient_id_to_avoid(patientIdToAvoid);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_SELECT_PATIENT_TO_RECYCLE_WITH_AVOID, request);

      if (response.select_patient_to_recycle_with_avoid().found())
      {
        internalId = response.select_patient_to_recycle_with_avoid().patient_id();
        return true;
      }
      else
      {
        return false;
      }
    }

    
    virtual void SetGlobalProperty(GlobalProperty property,
                                   bool shared,
                                   const std::string& value) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_set_global_property()->set_server_id(shared ? "" : database_.GetServerIdentifier());
      request.mutable_set_global_property()->set_property(property);
      request.mutable_set_global_property()->set_value(value);

      ExecuteTransaction(DatabasePluginMessages::OPERATION_SET_GLOBAL_PROPERTY, request);
    }

    
    virtual void ClearMainDicomTags(int64_t id) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_clear_main_dicom_tags()->set_id(id);

      ExecuteTransaction(DatabasePluginMessages::OPERATION_CLEAR_MAIN_DICOM_TAGS, request);
    }

    
    virtual void SetMetadata(int64_t id,
                             MetadataType type,
                             const std::string& value,
                             int64_t revision) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_set_metadata()->set_id(id);
      request.mutable_set_metadata()->set_metadata_type(type);
      request.mutable_set_metadata()->set_value(value);
      request.mutable_set_metadata()->set_revision(revision);

      ExecuteTransaction(DatabasePluginMessages::OPERATION_SET_METADATA, request);
    }

    
    virtual void SetProtectedPatient(int64_t internalId, 
                                     bool isProtected) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_set_protected_patient()->set_patient_id(internalId);
      request.mutable_set_protected_patient()->set_protected_patient(isProtected);

      ExecuteTransaction(DatabasePluginMessages::OPERATION_SET_PROTECTED_PATIENT, request);
    }


    virtual bool IsDiskSizeAbove(uint64_t threshold) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_is_disk_size_above()->set_threshold(threshold);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_IS_DISK_SIZE_ABOVE, request);

      return response.is_disk_size_above().result();
    }

    
    virtual void ApplyLookupResources(std::list<std::string>& resourcesId,
                                      std::list<std::string>* instancesId, // Can be NULL if not needed
                                      const std::vector<DatabaseConstraint>& lookup,
                                      ResourceType queryLevel,
                                      uint32_t limit) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_lookup_resources()->set_query_level(Convert(queryLevel));
      request.mutable_lookup_resources()->set_limit(limit);
      request.mutable_lookup_resources()->set_retrieve_instances_ids(instancesId != NULL);

      request.mutable_lookup_resources()->mutable_lookup()->Reserve(lookup.size());
      
      for (size_t i = 0; i < lookup.size(); i++)
      {
        DatabasePluginMessages::DatabaseConstraint* constraint = request.mutable_lookup_resources()->add_lookup();
        constraint->set_level(Convert(lookup[i].GetLevel()));
        constraint->set_tag_group(lookup[i].GetTag().GetGroup());
        constraint->set_tag_element(lookup[i].GetTag().GetElement());
        constraint->set_is_identifier_tag(lookup[i].IsIdentifier());
        constraint->set_is_case_sensitive(lookup[i].IsCaseSensitive());
        constraint->set_is_mandatory(lookup[i].IsMandatory());

        constraint->mutable_values()->Reserve(lookup[i].GetValuesCount());
        for (size_t j = 0; j < lookup[i].GetValuesCount(); j++)
        {
          constraint->add_values(lookup[i].GetValue(j));
        }

        switch (lookup[i].GetConstraintType())
        {
          case ConstraintType_Equal:
            constraint->set_type(DatabasePluginMessages::CONSTRAINT_EQUAL);
            break;
            
          case ConstraintType_SmallerOrEqual:
            constraint->set_type(DatabasePluginMessages::CONSTRAINT_SMALLER_OR_EQUAL);
            break;
            
          case ConstraintType_GreaterOrEqual:
            constraint->set_type(DatabasePluginMessages::CONSTRAINT_GREATER_OR_EQUAL);
            break;
            
          case ConstraintType_Wildcard:
            constraint->set_type(DatabasePluginMessages::CONSTRAINT_WILDCARD);
            break;
            
          case ConstraintType_List:
            constraint->set_type(DatabasePluginMessages::CONSTRAINT_LIST);
            break;

          default:
            throw OrthancException(ErrorCode_ParameterOutOfRange);
        }
      }
      
      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_LOOKUP_RESOURCES, request);

      for (int i = 0; i < response.lookup_resources().resources_ids().size(); i++)
      {
        resourcesId.push_back(response.lookup_resources().resources_ids(i));
      }
      
      if (instancesId != NULL)
      {
        if (response.lookup_resources().resources_ids().size() != response.lookup_resources().instances_ids().size())
        {
          throw OrthancException(ErrorCode_DatabasePlugin);
        }
        else
        {
          for (int i = 0; i < response.lookup_resources().instances_ids().size(); i++)
          {
            instancesId->push_back(response.lookup_resources().instances_ids(i));
          }
        }
      }
    }

    
    virtual bool CreateInstance(CreateInstanceResult& result, /* out */
                                int64_t& instanceId,          /* out */
                                const std::string& patient,
                                const std::string& study,
                                const std::string& series,
                                const std::string& instance) ORTHANC_OVERRIDE
    {
      // TODO: "CreateInstanceResult" => constructor and getters
      
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_create_instance()->set_patient(patient);
      request.mutable_create_instance()->set_study(study);
      request.mutable_create_instance()->set_series(series);
      request.mutable_create_instance()->set_instance(instance);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_CREATE_INSTANCE, request);

      instanceId = response.create_instance().instance_id();

      if (response.create_instance().is_new_instance())
      {
        result.isNewPatient_ = response.create_instance().is_new_patient();
        result.isNewStudy_ = response.create_instance().is_new_study();
        result.isNewSeries_ = response.create_instance().is_new_series();
        result.patientId_ = response.create_instance().patient_id();
        result.studyId_ = response.create_instance().study_id();
        result.seriesId_ = response.create_instance().series_id();
        return true;
      }
      else
      {
        return false;
      }
    }

    
    virtual void SetResourcesContent(const ResourcesContent& content) ORTHANC_OVERRIDE
    {
      // TODO: "ResourcesContent" => getters
      
      DatabasePluginMessages::TransactionRequest request;

      request.mutable_set_resources_content()->mutable_tags()->Reserve(content.GetListTags().size());
      for (ResourcesContent::ListTags::const_iterator it = content.GetListTags().begin(); it != content.GetListTags().end(); ++it)
      {
        DatabasePluginMessages::SetResourcesContent_Request_Tag* tag = request.mutable_set_resources_content()->add_tags();
        tag->set_resource_id(it->resourceId_);
        tag->set_is_identifier(it->isIdentifier_);
        tag->set_group(it->tag_.GetGroup());
        tag->set_element(it->tag_.GetElement());
        tag->set_value(it->value_);
      }
      
      request.mutable_set_resources_content()->mutable_metadata()->Reserve(content.GetListMetadata().size());
      for (ResourcesContent::ListMetadata::const_iterator it = content.GetListMetadata().begin(); it != content.GetListMetadata().end(); ++it)
      {
        DatabasePluginMessages::SetResourcesContent_Request_Metadata* metadata = request.mutable_set_resources_content()->add_metadata();
        metadata->set_resource_id(it->resourceId_);
        metadata->set_metadata(it->metadata_);
        metadata->set_value(it->value_);
      }

      ExecuteTransaction(DatabasePluginMessages::OPERATION_SET_RESOURCES_CONTENT, request);
    }

    
    virtual void GetChildrenMetadata(std::list<std::string>& target,
                                     int64_t resourceId,
                                     MetadataType metadata) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_get_children_metadata()->set_id(resourceId);
      request.mutable_get_children_metadata()->set_metadata(metadata);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_CHILDREN_METADATA, request);

      for (int i = 0; i < response.get_children_metadata().values().size(); i++)
      {
        target.push_back(response.get_children_metadata().values(i));
      }
    }

    
    virtual int64_t GetLastChangeIndex() ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_GET_LAST_CHANGE_INDEX);
      return response.get_last_change_index().result();
    }

    
    virtual bool LookupResourceAndParent(int64_t& id,
                                         ResourceType& type,
                                         std::string& parentPublicId,
                                         const std::string& publicId) ORTHANC_OVERRIDE
    {
      DatabasePluginMessages::TransactionRequest request;
      request.mutable_lookup_resource_and_parent()->set_public_id(publicId);

      DatabasePluginMessages::TransactionResponse response;
      ExecuteTransaction(response, DatabasePluginMessages::OPERATION_LOOKUP_RESOURCE_AND_PARENT, request);

      if (response.lookup_resource_and_parent().found())
      {
        id = response.lookup_resource_and_parent().id();
        type = Convert(response.lookup_resource_and_parent().type());

        switch (type)
        {
          case ResourceType_Patient:
            if (!response.lookup_resource_and_parent().parent_public_id().empty())
            {
              throw OrthancException(ErrorCode_DatabasePlugin);
            }
            break;
            
          case ResourceType_Study:
          case ResourceType_Series:
          case ResourceType_Instance:
            if (response.lookup_resource_and_parent().parent_public_id().empty())
            {
              throw OrthancException(ErrorCode_DatabasePlugin);
            }
            else
            {
              parentPublicId = response.lookup_resource_and_parent().parent_public_id();
            }
            break;
            
          default:
            throw OrthancException(ErrorCode_ParameterOutOfRange);
        }
        
        return true;
      }
      else
      {
        return false;
      }
    }
  };


  OrthancPluginDatabaseV4::OrthancPluginDatabaseV4(SharedLibrary& library,
                                                   PluginsErrorDictionary&  errorDictionary,
                                                   const _OrthancPluginRegisterDatabaseBackendV4& database,
                                                   const std::string& serverIdentifier) :
    library_(library),
    errorDictionary_(errorDictionary),
    definition_(database),
    serverIdentifier_(serverIdentifier),
    open_(false),
    databaseVersion_(0),
    hasFlushToDisk_(false),
    hasRevisionsSupport_(false)
  {
    CLOG(INFO, PLUGINS) << "Identifier of this Orthanc server for the global properties "
                        << "of the custom database: \"" << serverIdentifier << "\"";

    if (definition_.backend == NULL ||
        definition_.operations == NULL ||
        definition_.finalize == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
  }

  
  OrthancPluginDatabaseV4::~OrthancPluginDatabaseV4()
  {
    definition_.finalize(definition_.backend);
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
      ExecuteDatabase(response, *this, DatabasePluginMessages::OPERATION_OPEN, request);
    }

    {
      DatabasePluginMessages::DatabaseRequest request;
      DatabasePluginMessages::DatabaseResponse response;
      ExecuteDatabase(response, *this, DatabasePluginMessages::OPERATION_GET_SYSTEM_INFORMATION, request);
      databaseVersion_ = response.get_system_information().database_version();
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
      ExecuteDatabase(response, *this, DatabasePluginMessages::OPERATION_CLOSE, request);
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
      ExecuteDatabase(response, *this, DatabasePluginMessages::OPERATION_FLUSH_TO_DISK, request);
    }
  }
  

  IDatabaseWrapper::ITransaction* OrthancPluginDatabaseV4::StartTransaction(TransactionType type,
                                                                            IDatabaseListener& listener)
  {
    if (!open_)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      return new Transaction(*this, listener, type);
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
      VoidDatabaseListener listener;
      Transaction transaction(*this, listener, TransactionType_ReadWrite);

      try
      {
        DatabasePluginMessages::DatabaseRequest request;
        request.mutable_upgrade()->set_target_version(targetVersion);
        request.mutable_upgrade()->set_storage_area(reinterpret_cast<intptr_t>(&storageArea));
        
        DatabasePluginMessages::DatabaseResponse response;

        ExecuteDatabase(response, *this, DatabasePluginMessages::OPERATION_UPGRADE, request);
        transaction.Commit(0);
      }
      catch (OrthancException& e)
      {
        transaction.Rollback();
        throw;
      }
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
