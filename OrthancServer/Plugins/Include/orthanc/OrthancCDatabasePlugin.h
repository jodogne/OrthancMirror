/**
 * @ingroup CInterface
 **/

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
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/



#pragma once

#include "OrthancCPlugin.h"


/** @{ */

#ifdef __cplusplus
extern "C"
{
#endif


  /**
   * Opaque structure that represents the context of a custom database engine.
   * @ingroup Callbacks
   **/
  typedef struct _OrthancPluginDatabaseContext_t OrthancPluginDatabaseContext;


  /**
   * Opaque structure that represents a transaction of a custom database engine.
   * New in Orthanc 1.9.2.
   * @ingroup Callbacks
   **/
  typedef struct _OrthancPluginDatabaseTransaction_t OrthancPluginDatabaseTransaction;


/*<! @cond Doxygen_Suppress */
  typedef enum
  {
    _OrthancPluginDatabaseAnswerType_None = 0,

    /* Events */
    _OrthancPluginDatabaseAnswerType_DeletedAttachment = 1,
    _OrthancPluginDatabaseAnswerType_DeletedResource = 2,
    _OrthancPluginDatabaseAnswerType_RemainingAncestor = 3,

    /* Return value */
    _OrthancPluginDatabaseAnswerType_Attachment = 10,
    _OrthancPluginDatabaseAnswerType_Change = 11,
    _OrthancPluginDatabaseAnswerType_DicomTag = 12,
    _OrthancPluginDatabaseAnswerType_ExportedResource = 13,
    _OrthancPluginDatabaseAnswerType_Int32 = 14,
    _OrthancPluginDatabaseAnswerType_Int64 = 15,
    _OrthancPluginDatabaseAnswerType_Resource = 16,
    _OrthancPluginDatabaseAnswerType_String = 17,
    _OrthancPluginDatabaseAnswerType_MatchingResource = 18,  /* New in Orthanc 1.5.2 */
    _OrthancPluginDatabaseAnswerType_Metadata = 19,          /* New in Orthanc 1.5.4 */

    _OrthancPluginDatabaseAnswerType_INTERNAL = 0x7fffffff
  } _OrthancPluginDatabaseAnswerType;


  typedef struct
  {
    const char* uuid;
    int32_t     contentType;
    uint64_t    uncompressedSize;
    const char* uncompressedHash;
    int32_t     compressionType;
    uint64_t    compressedSize;
    const char* compressedHash;
  } OrthancPluginAttachment;

  typedef struct
  {
    uint16_t     group;
    uint16_t     element;
    const char*  value;
  } OrthancPluginDicomTag;

  typedef struct
  {
    int64_t                    seq;
    int32_t                    changeType;
    OrthancPluginResourceType  resourceType;
    const char*                publicId;
    const char*                date;
  } OrthancPluginChange;

  typedef struct
  {
    int64_t                    seq;
    OrthancPluginResourceType  resourceType;
    const char*                publicId;
    const char*                modality;
    const char*                date;
    const char*                patientId;
    const char*                studyInstanceUid;
    const char*                seriesInstanceUid;
    const char*                sopInstanceUid;
  } OrthancPluginExportedResource;

  typedef struct   /* New in Orthanc 1.5.2 */
  {
    OrthancPluginResourceType    level;
    uint16_t                     tagGroup;
    uint16_t                     tagElement;
    uint8_t                      isIdentifierTag;
    uint8_t                      isCaseSensitive;
    uint8_t                      isMandatory;
    OrthancPluginConstraintType  type;
    uint32_t                     valuesCount;
    const char* const*           values;
  } OrthancPluginDatabaseConstraint;

  typedef struct   /* New in Orthanc 1.5.2 */
  {
    const char*  resourceId;
    const char*  someInstanceId;  /* Can be NULL if not requested */
  } OrthancPluginMatchingResource;

  typedef struct   /* New in Orthanc 1.5.2 */
  {
    /* Mandatory field */
    uint8_t  isNewInstance;
    int64_t  instanceId;

    /* The following fields must only be set if "isNewInstance" is "true" */
    uint8_t  isNewPatient;
    uint8_t  isNewStudy;
    uint8_t  isNewSeries;
    int64_t  patientId;
    int64_t  studyId;
    int64_t  seriesId;
  } OrthancPluginCreateInstanceResult;

  typedef struct  /* New in Orthanc 1.5.2 */
  {
    int64_t      resource;
    uint16_t     group;
    uint16_t     element;
    const char*  value;
  } OrthancPluginResourcesContentTags;
    
  typedef struct  /* New in Orthanc 1.5.2 */
  {
    int64_t      resource;
    int32_t      metadata;
    const char*  value;
  } OrthancPluginResourcesContentMetadata;


  typedef struct
  {
    OrthancPluginDatabaseContext* database;
    _OrthancPluginDatabaseAnswerType  type;
    int32_t      valueInt32;
    uint32_t     valueUint32;
    int64_t      valueInt64;
    const char  *valueString;
    const void  *valueGeneric;
  } _OrthancPluginDatabaseAnswer;

  ORTHANC_PLUGIN_INLINE void OrthancPluginDatabaseAnswerString(
    OrthancPluginContext*          context,
    OrthancPluginDatabaseContext*  database,
    const char*                    value)
  {
    _OrthancPluginDatabaseAnswer params;
    memset(&params, 0, sizeof(params));
    params.database = database;
    params.type = _OrthancPluginDatabaseAnswerType_String;
    params.valueString = value;
    context->InvokeService(context, _OrthancPluginService_DatabaseAnswer, &params);
  }

  ORTHANC_PLUGIN_INLINE void OrthancPluginDatabaseAnswerChange(
    OrthancPluginContext*          context,
    OrthancPluginDatabaseContext*  database,
    const OrthancPluginChange*     change)
  {
    _OrthancPluginDatabaseAnswer params;
    memset(&params, 0, sizeof(params));

    params.database = database;
    params.type = _OrthancPluginDatabaseAnswerType_Change;
    params.valueUint32 = 0;
    params.valueGeneric = change;

    context->InvokeService(context, _OrthancPluginService_DatabaseAnswer, &params);
  }

  ORTHANC_PLUGIN_INLINE void OrthancPluginDatabaseAnswerChangesDone(
    OrthancPluginContext*          context,
    OrthancPluginDatabaseContext*  database)
  {
    _OrthancPluginDatabaseAnswer params;
    memset(&params, 0, sizeof(params));

    params.database = database;
    params.type = _OrthancPluginDatabaseAnswerType_Change;
    params.valueUint32 = 1;
    params.valueGeneric = NULL;

    context->InvokeService(context, _OrthancPluginService_DatabaseAnswer, &params);
  }

  ORTHANC_PLUGIN_INLINE void OrthancPluginDatabaseAnswerInt32(
    OrthancPluginContext*          context,
    OrthancPluginDatabaseContext*  database,
    int32_t                        value)
  {
    _OrthancPluginDatabaseAnswer params;
    memset(&params, 0, sizeof(params));
    params.database = database;
    params.type = _OrthancPluginDatabaseAnswerType_Int32;
    params.valueInt32 = value;
    context->InvokeService(context, _OrthancPluginService_DatabaseAnswer, &params);
  }

  ORTHANC_PLUGIN_INLINE void OrthancPluginDatabaseAnswerInt64(
    OrthancPluginContext*          context,
    OrthancPluginDatabaseContext*  database,
    int64_t                        value)
  {
    _OrthancPluginDatabaseAnswer params;
    memset(&params, 0, sizeof(params));
    params.database = database;
    params.type = _OrthancPluginDatabaseAnswerType_Int64;
    params.valueInt64 = value;
    context->InvokeService(context, _OrthancPluginService_DatabaseAnswer, &params);
  }

  ORTHANC_PLUGIN_INLINE void OrthancPluginDatabaseAnswerExportedResource(
    OrthancPluginContext*                 context,
    OrthancPluginDatabaseContext*         database,
    const OrthancPluginExportedResource*  exported)
  {
    _OrthancPluginDatabaseAnswer params;
    memset(&params, 0, sizeof(params));

    params.database = database;
    params.type = _OrthancPluginDatabaseAnswerType_ExportedResource;
    params.valueUint32 = 0;
    params.valueGeneric = exported;
    context->InvokeService(context, _OrthancPluginService_DatabaseAnswer, &params);
  }

  ORTHANC_PLUGIN_INLINE void OrthancPluginDatabaseAnswerExportedResourcesDone(
    OrthancPluginContext*                 context,
    OrthancPluginDatabaseContext*         database)
  {
    _OrthancPluginDatabaseAnswer params;
    memset(&params, 0, sizeof(params));

    params.database = database;
    params.type = _OrthancPluginDatabaseAnswerType_ExportedResource;
    params.valueUint32 = 1;
    params.valueGeneric = NULL;
    context->InvokeService(context, _OrthancPluginService_DatabaseAnswer, &params);
  }

  ORTHANC_PLUGIN_INLINE void OrthancPluginDatabaseAnswerDicomTag(
    OrthancPluginContext*          context,
    OrthancPluginDatabaseContext*  database,
    const OrthancPluginDicomTag*   tag)
  {
    _OrthancPluginDatabaseAnswer params;
    memset(&params, 0, sizeof(params));
    params.database = database;
    params.type = _OrthancPluginDatabaseAnswerType_DicomTag;
    params.valueGeneric = tag;
    context->InvokeService(context, _OrthancPluginService_DatabaseAnswer, &params);
  }

  ORTHANC_PLUGIN_INLINE void OrthancPluginDatabaseAnswerAttachment(
    OrthancPluginContext*          context,
    OrthancPluginDatabaseContext*  database,
    const OrthancPluginAttachment* attachment)
  {
    _OrthancPluginDatabaseAnswer params;
    memset(&params, 0, sizeof(params));
    params.database = database;
    params.type = _OrthancPluginDatabaseAnswerType_Attachment;
    params.valueGeneric = attachment;
    context->InvokeService(context, _OrthancPluginService_DatabaseAnswer, &params);
  }

  ORTHANC_PLUGIN_INLINE void OrthancPluginDatabaseAnswerResource(
    OrthancPluginContext*          context,
    OrthancPluginDatabaseContext*  database,
    int64_t                        id,
    OrthancPluginResourceType      resourceType)
  {
    _OrthancPluginDatabaseAnswer params;
    memset(&params, 0, sizeof(params));
    params.database = database;
    params.type = _OrthancPluginDatabaseAnswerType_Resource;
    params.valueInt64 = id;
    params.valueInt32 = (int32_t) resourceType;
    context->InvokeService(context, _OrthancPluginService_DatabaseAnswer, &params);
  }

  ORTHANC_PLUGIN_INLINE void OrthancPluginDatabaseAnswerMatchingResource(
    OrthancPluginContext*                 context,
    OrthancPluginDatabaseContext*         database,
    const OrthancPluginMatchingResource*  match)
  {
    _OrthancPluginDatabaseAnswer params;
    memset(&params, 0, sizeof(params));
    params.database = database;
    params.type = _OrthancPluginDatabaseAnswerType_MatchingResource;
    params.valueGeneric = match;
    context->InvokeService(context, _OrthancPluginService_DatabaseAnswer, &params);
  }

  ORTHANC_PLUGIN_INLINE void OrthancPluginDatabaseAnswerMetadata(
    OrthancPluginContext*          context,
    OrthancPluginDatabaseContext*  database,
    int64_t                        resourceId,
    int32_t                        type,
    const char*                    value)
  {
    OrthancPluginResourcesContentMetadata metadata;
    _OrthancPluginDatabaseAnswer params;
    metadata.resource = resourceId;
    metadata.metadata = type;
    metadata.value = value;
    memset(&params, 0, sizeof(params));
    params.database = database;
    params.type = _OrthancPluginDatabaseAnswerType_Metadata;
    params.valueGeneric = &metadata;
    context->InvokeService(context, _OrthancPluginService_DatabaseAnswer, &params);
  }

  ORTHANC_PLUGIN_INLINE void OrthancPluginDatabaseSignalDeletedAttachment(
    OrthancPluginContext*          context,
    OrthancPluginDatabaseContext*  database,
    const OrthancPluginAttachment* attachment)
  {
    _OrthancPluginDatabaseAnswer params;
    memset(&params, 0, sizeof(params));
    params.database = database;
    params.type = _OrthancPluginDatabaseAnswerType_DeletedAttachment;
    params.valueGeneric = attachment;
    context->InvokeService(context, _OrthancPluginService_DatabaseAnswer, &params);
  }

  ORTHANC_PLUGIN_INLINE void OrthancPluginDatabaseSignalDeletedResource(
    OrthancPluginContext*          context,
    OrthancPluginDatabaseContext*  database,
    const char*                    publicId,
    OrthancPluginResourceType      resourceType)
  {
    _OrthancPluginDatabaseAnswer params;
    memset(&params, 0, sizeof(params));
    params.database = database;
    params.type = _OrthancPluginDatabaseAnswerType_DeletedResource;
    params.valueString = publicId;
    params.valueInt32 = (int32_t) resourceType;
    context->InvokeService(context, _OrthancPluginService_DatabaseAnswer, &params);
  }

  ORTHANC_PLUGIN_INLINE void OrthancPluginDatabaseSignalRemainingAncestor(
    OrthancPluginContext*          context,
    OrthancPluginDatabaseContext*  database,
    const char*                    ancestorId,
    OrthancPluginResourceType      ancestorType)
  {
    _OrthancPluginDatabaseAnswer params;
    memset(&params, 0, sizeof(params));
    params.database = database;
    params.type = _OrthancPluginDatabaseAnswerType_RemainingAncestor;
    params.valueString = ancestorId;
    params.valueInt32 = (int32_t) ancestorType;
    context->InvokeService(context, _OrthancPluginService_DatabaseAnswer, &params);
  }





  typedef struct
  {
    OrthancPluginErrorCode  (*addAttachment) (
      /* inputs */
      void* payload,
      int64_t id,
      const OrthancPluginAttachment* attachment);
                             
    OrthancPluginErrorCode  (*attachChild) (
      /* inputs */
      void* payload,
      int64_t parent,
      int64_t child);
                             
    OrthancPluginErrorCode  (*clearChanges) (
      /* inputs */
      void* payload);
                             
    OrthancPluginErrorCode  (*clearExportedResources) (
      /* inputs */
      void* payload);

    OrthancPluginErrorCode  (*createResource) (
      /* outputs */
      int64_t* id, 
      /* inputs */
      void* payload,
      const char* publicId,
      OrthancPluginResourceType resourceType);           
                   
    OrthancPluginErrorCode  (*deleteAttachment) (
      /* inputs */
      void* payload,
      int64_t id,
      int32_t contentType);
   
    OrthancPluginErrorCode  (*deleteMetadata) (
      /* inputs */
      void* payload,
      int64_t id,
      int32_t metadataType);
   
    OrthancPluginErrorCode  (*deleteResource) (
      /* inputs */
      void* payload,
      int64_t id);    

    /* Output: Use OrthancPluginDatabaseAnswerString() */
    OrthancPluginErrorCode  (*getAllPublicIds) (
      /* outputs */
      OrthancPluginDatabaseContext* context,
      /* inputs */
      void* payload,
      OrthancPluginResourceType resourceType);

    /* Output: Use OrthancPluginDatabaseAnswerChange() and
     * OrthancPluginDatabaseAnswerChangesDone() */
    OrthancPluginErrorCode  (*getChanges) (
      /* outputs */
      OrthancPluginDatabaseContext* context,
      /* inputs */
      void* payload,
      int64_t since,
      uint32_t maxResult);

    /* Output: Use OrthancPluginDatabaseAnswerInt64() */
    OrthancPluginErrorCode  (*getChildrenInternalId) (
      /* outputs */
      OrthancPluginDatabaseContext* context,
      /* inputs */
      void* payload,
      int64_t id);
                   
    /* Output: Use OrthancPluginDatabaseAnswerString() */
    OrthancPluginErrorCode  (*getChildrenPublicId) (
      /* outputs */
      OrthancPluginDatabaseContext* context,
      /* inputs */
      void* payload,
      int64_t id);

    /* Output: Use OrthancPluginDatabaseAnswerExportedResource() and
     * OrthancPluginDatabaseAnswerExportedResourcesDone() */
    OrthancPluginErrorCode  (*getExportedResources) (
      /* outputs */
      OrthancPluginDatabaseContext* context,
      /* inputs */
      void* payload,
      int64_t  since,
      uint32_t  maxResult);
                   
    /* Output: Use OrthancPluginDatabaseAnswerChange() */
    OrthancPluginErrorCode  (*getLastChange) (
      /* outputs */
      OrthancPluginDatabaseContext* context,
      /* inputs */
      void* payload);

    /* Output: Use OrthancPluginDatabaseAnswerExportedResource() */
    OrthancPluginErrorCode  (*getLastExportedResource) (
      /* outputs */
      OrthancPluginDatabaseContext* context,
      /* inputs */
      void* payload);
                   
    /* Output: Use OrthancPluginDatabaseAnswerDicomTag() */
    OrthancPluginErrorCode  (*getMainDicomTags) (
      /* outputs */
      OrthancPluginDatabaseContext* context,
      /* inputs */
      void* payload,
      int64_t id);
                   
    /* Output: Use OrthancPluginDatabaseAnswerString() */
    OrthancPluginErrorCode  (*getPublicId) (
      /* outputs */
      OrthancPluginDatabaseContext* context,
      /* inputs */
      void* payload,
      int64_t id);

    OrthancPluginErrorCode  (*getResourceCount) (
      /* outputs */
      uint64_t* target,
      /* inputs */
      void* payload,
      OrthancPluginResourceType  resourceType);
                   
    OrthancPluginErrorCode  (*getResourceType) (
      /* outputs */
      OrthancPluginResourceType* resourceType,
      /* inputs */
      void* payload,
      int64_t id);

    OrthancPluginErrorCode  (*getTotalCompressedSize) (
      /* outputs */
      uint64_t* target,
      /* inputs */
      void* payload);
                   
    OrthancPluginErrorCode  (*getTotalUncompressedSize) (
      /* outputs */
      uint64_t* target,
      /* inputs */
      void* payload);
                   
    OrthancPluginErrorCode  (*isExistingResource) (
      /* outputs */
      int32_t* existing,
      /* inputs */
      void* payload,
      int64_t id);

    OrthancPluginErrorCode  (*isProtectedPatient) (
      /* outputs */
      int32_t* isProtected,
      /* inputs */
      void* payload,
      int64_t id);

    /* Output: Use OrthancPluginDatabaseAnswerInt32() */
    OrthancPluginErrorCode  (*listAvailableMetadata) (
      /* outputs */
      OrthancPluginDatabaseContext* context,
      /* inputs */
      void* payload,
      int64_t id);
                   
    /* Output: Use OrthancPluginDatabaseAnswerInt32() */
    OrthancPluginErrorCode  (*listAvailableAttachments) (
      /* outputs */
      OrthancPluginDatabaseContext* context,
      /* inputs */
      void* payload,
      int64_t id);

    OrthancPluginErrorCode  (*logChange) (
      /* inputs */
      void* payload,
      const OrthancPluginChange* change);
                   
    OrthancPluginErrorCode  (*logExportedResource) (
      /* inputs */
      void* payload,
      const OrthancPluginExportedResource* exported);
                   
    /* Output: Use OrthancPluginDatabaseAnswerAttachment() */
    OrthancPluginErrorCode  (*lookupAttachment) (
      /* outputs */
      OrthancPluginDatabaseContext* context,
      /* inputs */
      void* payload,
      int64_t id,
      int32_t contentType);

    /* Output: Use OrthancPluginDatabaseAnswerString() */
    OrthancPluginErrorCode  (*lookupGlobalProperty) (
      /* outputs */
      OrthancPluginDatabaseContext* context,
      /* inputs */
      void* payload,
      int32_t property);

    /* Use "OrthancPluginDatabaseExtensions::lookupIdentifier3" 
       instead of this function as of Orthanc 0.9.5 (db v6), can be set to NULL.
       Output: Use OrthancPluginDatabaseAnswerInt64() */
    OrthancPluginErrorCode  (*lookupIdentifier) (
      /* outputs */
      OrthancPluginDatabaseContext* context,
      /* inputs */
      void* payload,
      const OrthancPluginDicomTag* tag);

    /* Unused starting with Orthanc 0.9.5 (db v6), can be set to NULL.
       Output: Use OrthancPluginDatabaseAnswerInt64() */
    OrthancPluginErrorCode  (*lookupIdentifier2) (
      /* outputs */
      OrthancPluginDatabaseContext* context,
      /* inputs */
      void* payload,
      const char* value);

    /* Output: Use OrthancPluginDatabaseAnswerString() */
    OrthancPluginErrorCode  (*lookupMetadata) (
      /* outputs */
      OrthancPluginDatabaseContext* context,
      /* inputs */
      void* payload,
      int64_t id,
      int32_t metadata);

    /* Output: Use OrthancPluginDatabaseAnswerInt64() */
    OrthancPluginErrorCode  (*lookupParent) (
      /* outputs */
      OrthancPluginDatabaseContext* context,
      /* inputs */
      void* payload,
      int64_t id);

    /* Output: Use OrthancPluginDatabaseAnswerResource() */
    OrthancPluginErrorCode  (*lookupResource) (
      /* outputs */
      OrthancPluginDatabaseContext* context,
      /* inputs */
      void* payload,
      const char* publicId);

    /* Output: Use OrthancPluginDatabaseAnswerInt64() */
    OrthancPluginErrorCode  (*selectPatientToRecycle) (
      /* outputs */
      OrthancPluginDatabaseContext* context,
      /* inputs */
      void* payload);

    /* Output: Use OrthancPluginDatabaseAnswerInt64() */
    OrthancPluginErrorCode  (*selectPatientToRecycle2) (
      /* outputs */
      OrthancPluginDatabaseContext* context,
      /* inputs */
      void* payload,
      int64_t patientIdToAvoid);

    OrthancPluginErrorCode  (*setGlobalProperty) (
      /* inputs */
      void* payload,
      int32_t property,
      const char* value);

    OrthancPluginErrorCode  (*setMainDicomTag) (
      /* inputs */
      void* payload,
      int64_t id,
      const OrthancPluginDicomTag* tag);

    OrthancPluginErrorCode  (*setIdentifierTag) (
      /* inputs */
      void* payload,
      int64_t id,
      const OrthancPluginDicomTag* tag);

    OrthancPluginErrorCode  (*setMetadata) (
      /* inputs */
      void* payload,
      int64_t id,
      int32_t metadata,
      const char* value);

    OrthancPluginErrorCode  (*setProtectedPatient) (
      /* inputs */
      void* payload,
      int64_t id,
      int32_t isProtected);

    OrthancPluginErrorCode  (*startTransaction) (
      /* inputs */
      void* payload);

    OrthancPluginErrorCode  (*rollbackTransaction) (
      /* inputs */
      void* payload);

    OrthancPluginErrorCode  (*commitTransaction) (
      /* inputs */
      void* payload);

    OrthancPluginErrorCode  (*open) (
      /* inputs */
      void* payload);

    OrthancPluginErrorCode  (*close) (
      /* inputs */
      void* payload);

  } OrthancPluginDatabaseBackend;


  typedef struct
  {
    /**
     * Base extensions since Orthanc 1.0.0
     **/
    
    /* Output: Use OrthancPluginDatabaseAnswerString() */
    OrthancPluginErrorCode  (*getAllPublicIdsWithLimit) (
      /* outputs */
      OrthancPluginDatabaseContext* context,
      /* inputs */
      void* payload,
      OrthancPluginResourceType resourceType,
      uint64_t since,
      uint64_t limit);

    OrthancPluginErrorCode  (*getDatabaseVersion) (
      /* outputs */
      uint32_t* version,
      /* inputs */
      void* payload);

    OrthancPluginErrorCode  (*upgradeDatabase) (
      /* inputs */
      void* payload,
      uint32_t targetVersion,
      OrthancPluginStorageArea* storageArea);
 
    OrthancPluginErrorCode  (*clearMainDicomTags) (
      /* inputs */
      void* payload,
      int64_t id);

    /* Output: Use OrthancPluginDatabaseAnswerInt64() */
    OrthancPluginErrorCode  (*getAllInternalIds) (
      /* outputs */
      OrthancPluginDatabaseContext* context,
      /* inputs */
      void* payload,
      OrthancPluginResourceType resourceType);

    /* Output: Use OrthancPluginDatabaseAnswerInt64() */
    OrthancPluginErrorCode  (*lookupIdentifier3) (
      /* outputs */
      OrthancPluginDatabaseContext* context,
      /* inputs */
      void* payload,
      OrthancPluginResourceType resourceType,
      const OrthancPluginDicomTag* tag,
      OrthancPluginIdentifierConstraint constraint);


    /**
     * Extensions since Orthanc 1.4.0
     **/
    
    /* Output: Use OrthancPluginDatabaseAnswerInt64() */
    OrthancPluginErrorCode  (*lookupIdentifierRange) (
      /* outputs */
      OrthancPluginDatabaseContext* context,
      /* inputs */
      void* payload,
      OrthancPluginResourceType resourceType,
      uint16_t group,
      uint16_t element,
      const char* start,
      const char* end);

    
    /**
     * Extensions since Orthanc 1.5.2
     **/
    
    /* Ouput: Use OrthancPluginDatabaseAnswerMatchingResource */
    OrthancPluginErrorCode  (*lookupResources) (
      /* outputs */
      OrthancPluginDatabaseContext* context,
      /* inputs */
      void* payload,
      uint32_t constraintsCount,
      const OrthancPluginDatabaseConstraint* constraints,
      OrthancPluginResourceType queryLevel,
      uint32_t limit,
      uint8_t requestSomeInstance);
    
    OrthancPluginErrorCode  (*createInstance) (
      /* output */
      OrthancPluginCreateInstanceResult* output,
      /* inputs */
      void* payload,
      const char* hashPatient,
      const char* hashStudy,
      const char* hashSeries,
      const char* hashInstance);

    OrthancPluginErrorCode  (*setResourcesContent) (
      /* inputs */
      void* payload,
      uint32_t countIdentifierTags,
      const OrthancPluginResourcesContentTags* identifierTags,
      uint32_t countMainDicomTags,
      const OrthancPluginResourcesContentTags* mainDicomTags,
      uint32_t countMetadata,
      const OrthancPluginResourcesContentMetadata* metadata);

    /* Ouput: Use OrthancPluginDatabaseAnswerString */
    OrthancPluginErrorCode  (*getChildrenMetadata) (
      /* outputs */
      OrthancPluginDatabaseContext* context,
      /* inputs */
      void* payload,
      int64_t resourceId,
      int32_t metadata);

    OrthancPluginErrorCode  (*getLastChangeIndex) (
      /* outputs */
      int64_t* target,
      /* inputs */
      void* payload);
                   
    OrthancPluginErrorCode  (*tagMostRecentPatient) (
      /* inputs */
      void* payload,
      int64_t patientId);
                   
    
    /**
     * Extensions since Orthanc 1.5.4
     **/

    /* Ouput: Use OrthancPluginDatabaseAnswerMetadata */
    OrthancPluginErrorCode  (*getAllMetadata) (
      /* outputs */
      OrthancPluginDatabaseContext* context,
      /* inputs */
      void* payload,
      int64_t resourceId);
    
    /* Ouput: Use OrthancPluginDatabaseAnswerString to send 
       the public ID of the parent (if the resource is not a patient) */
    OrthancPluginErrorCode  (*lookupResourceAndParent) (
      /* outputs */
      OrthancPluginDatabaseContext* context,
      uint8_t* isExisting,
      int64_t* id,
      OrthancPluginResourceType* type,
      
      /* inputs */
      void* payload,
      const char* publicId);

  } OrthancPluginDatabaseExtensions;

/*<! @endcond */


  typedef struct
  {
    OrthancPluginDatabaseContext**       result;
    const OrthancPluginDatabaseBackend*  backend;
    void*                                payload;
  } _OrthancPluginRegisterDatabaseBackend;

  /**
   * Register a custom database back-end (for legacy plugins).
   *
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param backend The callbacks of the custom database engine.
   * @param payload Pointer containing private information for the database engine.
   * @return The context of the database engine (it must not be manually freed).
   * @ingroup Callbacks
   * @deprecated
   * @see OrthancPluginRegisterDatabaseBackendV2
   **/
  ORTHANC_PLUGIN_INLINE OrthancPluginDatabaseContext* OrthancPluginRegisterDatabaseBackend(
    OrthancPluginContext*                context,
    const OrthancPluginDatabaseBackend*  backend,
    void*                                payload)
  {
    OrthancPluginDatabaseContext* result = NULL;
    _OrthancPluginRegisterDatabaseBackend params;

    if (sizeof(int32_t) != sizeof(_OrthancPluginDatabaseAnswerType))
    {
      return NULL;
    }

    memset(&params, 0, sizeof(params));
    params.backend = backend;
    params.result = &result;
    params.payload = payload;

    if (context->InvokeService(context, _OrthancPluginService_RegisterDatabaseBackend, &params) ||
        result == NULL)
    {
      /* Error */
      return NULL;
    }
    else
    {
      return result;
    }
  }


  typedef struct
  {
    OrthancPluginDatabaseContext**          result;
    const OrthancPluginDatabaseBackend*     backend;
    void*                                   payload;
    const OrthancPluginDatabaseExtensions*  extensions;
    uint32_t                                extensionsSize;
  } _OrthancPluginRegisterDatabaseBackendV2;


  /**
   * Register a custom database back-end.
   *
   * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
   * @param backend The callbacks of the custom database engine.
   * @param payload Pointer containing private information for the database engine.
   * @param extensions Extensions to the base database SDK that was shipped until Orthanc 0.9.3.
   * @return The context of the database engine (it must not be manually freed).
   * @ingroup Callbacks
   **/
  ORTHANC_PLUGIN_INLINE OrthancPluginDatabaseContext* OrthancPluginRegisterDatabaseBackendV2(
    OrthancPluginContext*                   context,
    const OrthancPluginDatabaseBackend*     backend,
    const OrthancPluginDatabaseExtensions*  extensions,
    void*                                   payload)
  {
    OrthancPluginDatabaseContext* result = NULL;
    _OrthancPluginRegisterDatabaseBackendV2 params;

    if (sizeof(int32_t) != sizeof(_OrthancPluginDatabaseAnswerType))
    {
      return NULL;
    }

    memset(&params, 0, sizeof(params));
    params.backend = backend;
    params.result = &result;
    params.payload = payload;
    params.extensions = extensions;
    params.extensionsSize = sizeof(OrthancPluginDatabaseExtensions);

    if (context->InvokeService(context, _OrthancPluginService_RegisterDatabaseBackendV2, &params) ||
        result == NULL)
    {
      /* Error */
      return NULL;
    }
    else
    {
      return result;
    }
  }



  /**
   * New interface starting with Orthanc 1.9.2
   **/

/*<! @cond Doxygen_Suppress */
  typedef enum
  {
    OrthancPluginDatabaseTransactionType_ReadOnly = 1,
    OrthancPluginDatabaseTransactionType_ReadWrite = 2,
    OrthancPluginDatabaseTransactionType_INTERNAL = 0x7fffffff
  } OrthancPluginDatabaseTransactionType;


  typedef enum
  {
    OrthancPluginDatabaseEventType_DeletedAttachment = 1,
    OrthancPluginDatabaseEventType_DeletedResource = 2,
    OrthancPluginDatabaseEventType_RemainingAncestor = 3,
    OrthancPluginDatabaseEventType_INTERNAL = 0x7fffffff
  } OrthancPluginDatabaseEventType;


  typedef struct
  {
    OrthancPluginDatabaseEventType type;

    union
    {
      struct
      {
        /* For ""DeletedResource" and "RemainingAncestor" */
        OrthancPluginResourceType  level;
        const char*                publicId;
      } resource;

      /* For "DeletedAttachment" */
      OrthancPluginAttachment  attachment;
      
    } content;
    
  } OrthancPluginDatabaseEvent;

  
  typedef struct
  {
    /**
     * Functions to read the answers inside a transaction
     **/
    
    OrthancPluginErrorCode (*readAnswersCount) (OrthancPluginDatabaseTransaction* transaction,
                                                uint32_t* target /* out */);

    OrthancPluginErrorCode (*readAnswerAttachment) (OrthancPluginDatabaseTransaction* transaction,
                                                    OrthancPluginAttachment* target /* out */,
                                                    uint32_t index);

    OrthancPluginErrorCode (*readAnswerChange) (OrthancPluginDatabaseTransaction* transaction,
                                                OrthancPluginChange* target /* out */,
                                                uint32_t index);

    OrthancPluginErrorCode (*readAnswerDicomTag) (OrthancPluginDatabaseTransaction* transaction,
                                                  uint16_t* group,
                                                  uint16_t* element,
                                                  const char** value,
                                                  uint32_t index);

    OrthancPluginErrorCode (*readAnswerExportedResource) (OrthancPluginDatabaseTransaction* transaction,
                                                          OrthancPluginExportedResource* target /* out */,
                                                          uint32_t index);

    OrthancPluginErrorCode (*readAnswerInt32) (OrthancPluginDatabaseTransaction* transaction,
                                               int32_t* target /* out */,
                                               uint32_t index);

    OrthancPluginErrorCode (*readAnswerInt64) (OrthancPluginDatabaseTransaction* transaction,
                                               int64_t* target /* out */,
                                               uint32_t index);

    OrthancPluginErrorCode (*readAnswerMatchingResource) (OrthancPluginDatabaseTransaction* transaction,
                                                          OrthancPluginMatchingResource* target /* out */,
                                                          uint32_t index);
    
    OrthancPluginErrorCode (*readAnswerMetadata) (OrthancPluginDatabaseTransaction* transaction,
                                                  int32_t* metadata /* out */,
                                                  const char** value /* out */,
                                                  uint32_t index);

    OrthancPluginErrorCode (*readAnswerString) (OrthancPluginDatabaseTransaction* transaction,
                                                const char** target /* out */,
                                                uint32_t index);
    
    OrthancPluginErrorCode (*readEventsCount) (OrthancPluginDatabaseTransaction* transaction,
                                               uint32_t* target /* out */);

    OrthancPluginErrorCode (*readEvent) (OrthancPluginDatabaseTransaction* transaction,
                                         OrthancPluginDatabaseEvent* event /* out */,
                                         uint32_t index);

    
    
    /**
     * Functions to access the global database object
     * (cf. "IDatabaseWrapper" class in Orthanc)
     **/

    OrthancPluginErrorCode (*open) (void* database);

    OrthancPluginErrorCode (*close) (void* database);

    OrthancPluginErrorCode (*destructDatabase) (void* database);

    OrthancPluginErrorCode (*getDatabaseVersion) (void* database,
                                                  uint32_t* target /* out */);

    OrthancPluginErrorCode (*upgradeDatabase) (void* database,
                                               OrthancPluginStorageArea* storageArea,
                                               uint32_t targetVersion);

    OrthancPluginErrorCode (*startTransaction) (void* database,
                                                OrthancPluginDatabaseTransaction** target /* out */,
                                                OrthancPluginDatabaseTransactionType type);

    OrthancPluginErrorCode (*destructTransaction) (OrthancPluginDatabaseTransaction* transaction);


    /**
     * Functions to run operations within a database transaction
     * (cf. "IDatabaseWrapper::ITransaction" class in Orthanc)
     **/

    OrthancPluginErrorCode (*rollback) (OrthancPluginDatabaseTransaction* transaction);
    
    OrthancPluginErrorCode (*commit) (OrthancPluginDatabaseTransaction* transaction,
                                      int64_t fileSizeDelta);
    
    OrthancPluginErrorCode (*addAttachment) (OrthancPluginDatabaseTransaction* transaction,
                                             int64_t id,
                                             const OrthancPluginAttachment* attachment);

    OrthancPluginErrorCode (*clearChanges) (OrthancPluginDatabaseTransaction* transaction);
    
    OrthancPluginErrorCode (*clearExportedResources) (OrthancPluginDatabaseTransaction* transaction);
    
    OrthancPluginErrorCode (*clearMainDicomTags) (OrthancPluginDatabaseTransaction* transaction,
                                                  int64_t resourceId);

    OrthancPluginErrorCode (*createInstance) (OrthancPluginDatabaseTransaction* transaction,
                                              OrthancPluginCreateInstanceResult* target /* out */,
                                              const char* hashPatient,
                                              const char* hashStudy,
                                              const char* hashSeries,
                                              const char* hashInstance);

    OrthancPluginErrorCode (*deleteAttachment) (OrthancPluginDatabaseTransaction* transaction,
                                                int64_t id,
                                                int32_t contentType);
    
    OrthancPluginErrorCode (*deleteMetadata) (OrthancPluginDatabaseTransaction* transaction,
                                              int64_t id,
                                              int32_t metadataType);

    OrthancPluginErrorCode (*deleteResource) (OrthancPluginDatabaseTransaction* transaction,
                                              int64_t id);

    /* Answers are read using "readAnswerMetadata()" */
    OrthancPluginErrorCode (*getAllMetadata) (OrthancPluginDatabaseTransaction* transaction,
                                              int64_t id);
    
    /* Answers are read using "readAnswerString()" */
    OrthancPluginErrorCode (*getAllPublicIds) (OrthancPluginDatabaseTransaction* transaction,
                                               OrthancPluginResourceType resourceType);
    
    /* Answers are read using "readAnswerString()" */
    OrthancPluginErrorCode (*getAllPublicIdsWithLimit) (OrthancPluginDatabaseTransaction* transaction,
                                                        OrthancPluginResourceType resourceType,
                                                        uint64_t since,
                                                        uint64_t limit);

    /* Answers are read using "readAnswerChange()" */
    OrthancPluginErrorCode (*getChanges) (OrthancPluginDatabaseTransaction* transaction,
                                          uint8_t* targetDone /* out */,
                                          int64_t since,
                                          uint32_t maxResults);
    
    /* Answers are read using "readAnswerInt64()" */
    OrthancPluginErrorCode (*getChildrenInternalId) (OrthancPluginDatabaseTransaction* transaction,
                                                     int64_t id);
    
    /* Answers are read using "readAnswerString()" */
    OrthancPluginErrorCode  (*getChildrenMetadata) (OrthancPluginDatabaseTransaction* transaction,
                                                    int64_t resourceId,
                                                    int32_t metadata);

    /* Answers are read using "readAnswerString()" */
    OrthancPluginErrorCode (*getChildrenPublicId) (OrthancPluginDatabaseTransaction* transaction,
                                                   int64_t id);

    /* Answers are read using "readAnswerExportedResource()" */
    OrthancPluginErrorCode (*getExportedResources) (OrthancPluginDatabaseTransaction* transaction,
                                                    uint8_t* targetDone /* out */,
                                                    int64_t since,
                                                    uint32_t maxResults);
    
    /* Answer is read using "readAnswerChange()" */
    OrthancPluginErrorCode (*getLastChange) (OrthancPluginDatabaseTransaction* transaction);
    
    OrthancPluginErrorCode (*getLastChangeIndex) (OrthancPluginDatabaseTransaction* transaction,
                                                  int64_t* target /* out */);
    
    /* Answer is read using "readAnswerExportedResource()" */
    OrthancPluginErrorCode (*getLastExportedResource) (OrthancPluginDatabaseTransaction* transaction);
    
    /* Answers are read using "readAnswerDicomTag()" */
    OrthancPluginErrorCode (*getMainDicomTags) (OrthancPluginDatabaseTransaction* transaction,
                                                int64_t id);
    
    /* Answer is read using "readAnswerString()" */
    OrthancPluginErrorCode (*getPublicId) (OrthancPluginDatabaseTransaction* transaction,
                                           int64_t internalId);
    
    OrthancPluginErrorCode (*getResourcesCount) (OrthancPluginDatabaseTransaction* transaction,
                                                 uint64_t* target /* out */,
                                                 OrthancPluginResourceType resourceType);
    
    OrthancPluginErrorCode (*getResourceType) (OrthancPluginDatabaseTransaction* transaction,
                                               OrthancPluginResourceType* target /* out */,
                                               uint64_t resourceId);
    
    OrthancPluginErrorCode (*getTotalCompressedSize) (OrthancPluginDatabaseTransaction* transaction,
                                                      uint64_t* target /* out */);
    
    OrthancPluginErrorCode (*getTotalUncompressedSize) (OrthancPluginDatabaseTransaction* transaction,
                                                        uint64_t* target /* out */);
    
    OrthancPluginErrorCode (*isDiskSizeAbove) (OrthancPluginDatabaseTransaction* transaction,
                                               uint8_t* target /* out */,
                                               uint64_t threshold);
    
    OrthancPluginErrorCode (*isExistingResource) (OrthancPluginDatabaseTransaction* transaction,
                                                  uint8_t* target /* out */,
                                                  int64_t resourceId);
    
    OrthancPluginErrorCode (*isProtectedPatient) (OrthancPluginDatabaseTransaction* transaction,
                                                  uint8_t* target /* out */,
                                                  int64_t resourceId);
    
    /* Answers are read using "readAnswerInt32()" */
    OrthancPluginErrorCode (*listAvailableAttachments) (OrthancPluginDatabaseTransaction* transaction,
                                                        int64_t internalId);

    OrthancPluginErrorCode (*logChange) (OrthancPluginDatabaseTransaction* transaction,
                                         int32_t changeType,
                                         int64_t resourceId,
                                         OrthancPluginResourceType resourceType,
                                         const char* date);

    OrthancPluginErrorCode (*logExportedResource) (OrthancPluginDatabaseTransaction* transaction,
                                                   OrthancPluginResourceType resourceType,
                                                   const char* publicId,
                                                   const char* modality,
                                                   const char* date,
                                                   const char* patientId,
                                                   const char* studyInstanceUid,
                                                   const char* seriesInstanceUid,
                                                   const char* sopInstanceUid);

    /* Answer is read using "readAnswerAttachment()" */
    OrthancPluginErrorCode (*lookupAttachment) (OrthancPluginDatabaseTransaction* transaction,
                                                int64_t resourceId,
                                                int32_t contentType);

    /* Answer is read using "readAnswerString()" */
    OrthancPluginErrorCode (*lookupGlobalProperty) (OrthancPluginDatabaseTransaction* transaction,
                                                    int32_t property);
    
    /* Answer is read using "readAnswerString()" */
    OrthancPluginErrorCode (*lookupMetadata) (OrthancPluginDatabaseTransaction* transaction,
                                              int64_t id,
                                              int32_t metadata);
    
    /* Answer is read using "readAnswerInt64()" -- TODO */
    OrthancPluginErrorCode (*lookupParent) (OrthancPluginDatabaseTransaction* transaction,
                                            int64_t id);
    
    OrthancPluginErrorCode (*lookupResource) (OrthancPluginDatabaseTransaction* transaction,
                                              uint8_t* isExisting /* out */,
                                              int64_t* id /* out */,
                                              OrthancPluginResourceType* type /* out */,
                                              const char* publicId);
    
    /* Answers are read using "readAnswerMatchingResource()" */
    OrthancPluginErrorCode  (*lookupResources) (OrthancPluginDatabaseTransaction* transaction,
                                                uint32_t constraintsCount,
                                                const OrthancPluginDatabaseConstraint* constraints,
                                                OrthancPluginResourceType queryLevel,
                                                uint32_t limit,
                                                uint8_t requestSomeInstanceId);

    /* The public ID of the parent resource is read using "readAnswerString()" */
    OrthancPluginErrorCode (*lookupResourceAndParent) (OrthancPluginDatabaseTransaction* transaction,
                                                       uint8_t* isExisting /* out */,
                                                       int64_t* id /* out */,
                                                       OrthancPluginResourceType* type /* out */,
                                                       const char* publicId);

    /* Answer is read using "readAnswerInt64()" -- TODO */
    OrthancPluginErrorCode (*selectPatientToRecycle) (OrthancPluginDatabaseTransaction* transaction);
    
    /* Answer is read using "readAnswerInt64()" -- TODO */
    OrthancPluginErrorCode (*selectPatientToRecycle2) (OrthancPluginDatabaseTransaction* transaction,
                                                       int64_t patientIdToAvoid);

    OrthancPluginErrorCode (*setGlobalProperty) (OrthancPluginDatabaseTransaction* transaction,
                                                 int32_t property,
                                                 const char* value);

    OrthancPluginErrorCode (*setMetadata) (OrthancPluginDatabaseTransaction* transaction,
                                           int64_t id,
                                           int32_t metadata,
                                           const char* value);
    
    OrthancPluginErrorCode (*setProtectedPatient) (OrthancPluginDatabaseTransaction* transaction,
                                                   int64_t id,
                                                   uint8_t isProtected);

    OrthancPluginErrorCode  (*setResourcesContent) (OrthancPluginDatabaseTransaction* transaction,
                                                    uint32_t countIdentifierTags,
                                                    const OrthancPluginResourcesContentTags* identifierTags,
                                                    uint32_t countMainDicomTags,
                                                    const OrthancPluginResourcesContentTags* mainDicomTags,
                                                    uint32_t countMetadata,
                                                    const OrthancPluginResourcesContentMetadata* metadata);
    

  } OrthancPluginDatabaseBackendV3;

/*<! @endcond */
  

  typedef struct
  {
    const OrthancPluginDatabaseBackendV3*  backend;
    uint32_t                               backendSize;
    void*                                  database;
  } _OrthancPluginRegisterDatabaseBackendV3;


  ORTHANC_PLUGIN_INLINE OrthancPluginErrorCode OrthancPluginRegisterDatabaseBackendV3(
    OrthancPluginContext*                  context,
    const OrthancPluginDatabaseBackendV3*  backend,
    uint32_t                               backendSize,
    void*                                  database)
  {
    _OrthancPluginRegisterDatabaseBackendV3 params;

    if (sizeof(int32_t) != sizeof(_OrthancPluginDatabaseAnswerType))
    {
      return OrthancPluginErrorCode_Plugin;
    }

    memset(&params, 0, sizeof(params));
    params.backend = backend;
    params.backendSize = sizeof(OrthancPluginDatabaseBackendV3);
    params.database = database;

    return context->InvokeService(context, _OrthancPluginService_RegisterDatabaseBackendV3, &params);
  }
  
#ifdef  __cplusplus
}
#endif


/** @} */

