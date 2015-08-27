/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
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

#include "OrthancCDatabasePlugin.h"

#include <stdexcept>
#include <list>
#include <string>

namespace OrthancPlugins
{
//! @cond Doxygen_Suppress
  // This class mimics "boost::noncopyable"
  class NonCopyable
  {
  private:
    NonCopyable(const NonCopyable&);

    NonCopyable& operator= (const NonCopyable&);

  protected:
    NonCopyable()
    {
    }

    ~NonCopyable()
    {
    }
  };
//! @endcond



  class DatabaseBackendOutput : public NonCopyable
  {
    friend class DatabaseBackendAdapter;

  private:
    enum AllowedAnswers
    {
      AllowedAnswers_All,
      AllowedAnswers_None,
      AllowedAnswers_Attachment,
      AllowedAnswers_Change,
      AllowedAnswers_DicomTag,
      AllowedAnswers_ExportedResource
    };

    OrthancPluginContext*         context_;
    OrthancPluginDatabaseContext* database_;
    AllowedAnswers                allowedAnswers_;

    void SetAllowedAnswers(AllowedAnswers allowed)
    {
      allowedAnswers_ = allowed;
    }

  public:
    DatabaseBackendOutput(OrthancPluginContext*         context,
                          OrthancPluginDatabaseContext* database) :
      context_(context),
      database_(database),
      allowedAnswers_(AllowedAnswers_All /* for unit tests */)
    {
    }

    void LogError(const std::string& message)
    {
      OrthancPluginLogError(context_, message.c_str());
    }

    void LogWarning(const std::string& message)
    {
      OrthancPluginLogWarning(context_, message.c_str());
    }

    void LogInfo(const std::string& message)
    {
      OrthancPluginLogInfo(context_, message.c_str());
    }

    void SignalDeletedAttachment(const std::string& uuid,
                                 int32_t            contentType,
                                 uint64_t           uncompressedSize,
                                 const std::string& uncompressedHash,
                                 int32_t            compressionType,
                                 uint64_t           compressedSize,
                                 const std::string& compressedHash)
    {
      OrthancPluginAttachment attachment;
      attachment.uuid = uuid.c_str();
      attachment.contentType = contentType;
      attachment.uncompressedSize = uncompressedSize;
      attachment.uncompressedHash = uncompressedHash.c_str();
      attachment.compressionType = compressionType;
      attachment.compressedSize = compressedSize;
      attachment.compressedHash = compressedHash.c_str();

      OrthancPluginDatabaseSignalDeletedAttachment(context_, database_, &attachment);
    }

    void SignalDeletedResource(const std::string& publicId,
                               OrthancPluginResourceType resourceType)
    {
      OrthancPluginDatabaseSignalDeletedResource(context_, database_, publicId.c_str(), resourceType);
    }

    void SignalRemainingAncestor(const std::string& ancestorId,
                                 OrthancPluginResourceType ancestorType)
    {
      OrthancPluginDatabaseSignalRemainingAncestor(context_, database_, ancestorId.c_str(), ancestorType);
    }

    void AnswerAttachment(const std::string& uuid,
                          int32_t            contentType,
                          uint64_t           uncompressedSize,
                          const std::string& uncompressedHash,
                          int32_t            compressionType,
                          uint64_t           compressedSize,
                          const std::string& compressedHash)
    {
      if (allowedAnswers_ != AllowedAnswers_All &&
          allowedAnswers_ != AllowedAnswers_Attachment)
      {
        throw std::runtime_error("Cannot answer with an attachment in the current state");
      }

      OrthancPluginAttachment attachment;
      attachment.uuid = uuid.c_str();
      attachment.contentType = contentType;
      attachment.uncompressedSize = uncompressedSize;
      attachment.uncompressedHash = uncompressedHash.c_str();
      attachment.compressionType = compressionType;
      attachment.compressedSize = compressedSize;
      attachment.compressedHash = compressedHash.c_str();

      OrthancPluginDatabaseAnswerAttachment(context_, database_, &attachment);
    }

    void AnswerChange(int64_t                    seq,
                      int32_t                    changeType,
                      OrthancPluginResourceType  resourceType,
                      const std::string&         publicId,
                      const std::string&         date)
    {
      if (allowedAnswers_ != AllowedAnswers_All &&
          allowedAnswers_ != AllowedAnswers_Change)
      {
        throw std::runtime_error("Cannot answer with a change in the current state");
      }

      OrthancPluginChange change;
      change.seq = seq;
      change.changeType = changeType;
      change.resourceType = resourceType;
      change.publicId = publicId.c_str();
      change.date = date.c_str();

      OrthancPluginDatabaseAnswerChange(context_, database_, &change);
    }

    void AnswerDicomTag(uint16_t group,
                        uint16_t element,
                        const std::string& value)
    {
      if (allowedAnswers_ != AllowedAnswers_All &&
          allowedAnswers_ != AllowedAnswers_DicomTag)
      {
        throw std::runtime_error("Cannot answer with a DICOM tag in the current state");
      }

      OrthancPluginDicomTag tag;
      tag.group = group;
      tag.element = element;
      tag.value = value.c_str();

      OrthancPluginDatabaseAnswerDicomTag(context_, database_, &tag);
    }

    void AnswerExportedResource(int64_t                    seq,
                                OrthancPluginResourceType  resourceType,
                                const std::string&         publicId,
                                const std::string&         modality,
                                const std::string&         date,
                                const std::string&         patientId,
                                const std::string&         studyInstanceUid,
                                const std::string&         seriesInstanceUid,
                                const std::string&         sopInstanceUid)
    {
      if (allowedAnswers_ != AllowedAnswers_All &&
          allowedAnswers_ != AllowedAnswers_ExportedResource)
      {
        throw std::runtime_error("Cannot answer with an exported resource in the current state");
      }

      OrthancPluginExportedResource exported;
      exported.seq = seq;
      exported.resourceType = resourceType;
      exported.publicId = publicId.c_str();
      exported.modality = modality.c_str();
      exported.date = date.c_str();
      exported.patientId = patientId.c_str();
      exported.studyInstanceUid = studyInstanceUid.c_str();
      exported.seriesInstanceUid = seriesInstanceUid.c_str();
      exported.sopInstanceUid = sopInstanceUid.c_str();

      OrthancPluginDatabaseAnswerExportedResource(context_, database_, &exported);
    }
  };



  class IDatabaseBackend : public NonCopyable
  {
    friend class DatabaseBackendAdapter;

  private:
    DatabaseBackendOutput*  output_;

    void Finalize()
    {
      if (output_ != NULL)
      {
        delete output_;
        output_ = NULL;
      }
    }

  protected:
    DatabaseBackendOutput& GetOutput()
    {
      return *output_;
    }

  public:
    IDatabaseBackend() : output_(NULL)
    {
    }

    virtual ~IDatabaseBackend()
    {
      Finalize();
    }

    // This takes the ownership
    void RegisterOutput(DatabaseBackendOutput* output)
    {
      Finalize();
      output_ = output;
    }

    virtual void Open() = 0;

    virtual void Close() = 0;

    virtual void AddAttachment(int64_t id,
                               const OrthancPluginAttachment& attachment) = 0;

    virtual void AttachChild(int64_t parent,
                             int64_t child) = 0;

    virtual void ClearChanges() = 0;

    virtual void ClearExportedResources() = 0;

    virtual int64_t CreateResource(const char* publicId,
                                   OrthancPluginResourceType type) = 0;

    virtual void DeleteAttachment(int64_t id,
                                  int32_t attachment) = 0;

    virtual void DeleteMetadata(int64_t id,
                                int32_t metadataType) = 0;

    virtual void DeleteResource(int64_t id) = 0;

    virtual void GetAllPublicIds(std::list<std::string>& target,
                                 OrthancPluginResourceType resourceType) = 0;

    /* Use GetOutput().AnswerChange() */
    virtual void GetChanges(bool& done /*out*/,
                            int64_t since,
                            uint32_t maxResults) = 0;

    virtual void GetChildrenInternalId(std::list<int64_t>& target /*out*/,
                                       int64_t id) = 0;

    virtual void GetChildrenPublicId(std::list<std::string>& target /*out*/,
                                     int64_t id) = 0;

    /* Use GetOutput().AnswerExportedResource() */
    virtual void GetExportedResources(bool& done /*out*/,
                                      int64_t since,
                                      uint32_t maxResults) = 0;

    /* Use GetOutput().AnswerChange() */
    virtual void GetLastChange() = 0;

    /* Use GetOutput().AnswerExportedResource() */
    virtual void GetLastExportedResource() = 0;

    /* Use GetOutput().AnswerDicomTag() */
    virtual void GetMainDicomTags(int64_t id) = 0;

    virtual std::string GetPublicId(int64_t resourceId) = 0;

    virtual uint64_t GetResourceCount(OrthancPluginResourceType resourceType) = 0;

    virtual OrthancPluginResourceType GetResourceType(int64_t resourceId) = 0;

    virtual uint64_t GetTotalCompressedSize() = 0;
    
    virtual uint64_t GetTotalUncompressedSize() = 0;

    virtual bool IsExistingResource(int64_t internalId) = 0;

    virtual bool IsProtectedPatient(int64_t internalId) = 0;

    virtual void ListAvailableMetadata(std::list<int32_t>& target /*out*/,
                                       int64_t id) = 0;

    virtual void ListAvailableAttachments(std::list<int32_t>& target /*out*/,
                                          int64_t id) = 0;

    virtual void LogChange(const OrthancPluginChange& change) = 0;

    virtual void LogExportedResource(const OrthancPluginExportedResource& resource) = 0;
    
    /* Use GetOutput().AnswerAttachment() */
    virtual bool LookupAttachment(int64_t id,
                                  int32_t contentType) = 0;

    virtual bool LookupGlobalProperty(std::string& target /*out*/,
                                      int32_t property) = 0;

    /**
     * "Identifiers" are necessarily one of the following tags:
     * PatientID (0x0010, 0x0020), StudyInstanceUID (0x0020, 0x000d),
     * SeriesInstanceUID (0x0020, 0x000e), SOPInstanceUID (0x0008,
     * 0x0018) or AccessionNumber (0x0008, 0x0050).
     **/
    virtual void LookupIdentifier(std::list<int64_t>& target /*out*/,
                                  uint16_t group,
                                  uint16_t element,
                                  const char* value) = 0;

    virtual void LookupIdentifier(std::list<int64_t>& target /*out*/,
                                  const char* value) = 0;

    virtual bool LookupMetadata(std::string& target /*out*/,
                                int64_t id,
                                int32_t metadataType) = 0;

    virtual bool LookupParent(int64_t& parentId /*out*/,
                              int64_t resourceId) = 0;

    virtual bool LookupResource(int64_t& id /*out*/,
                                OrthancPluginResourceType& type /*out*/,
                                const char* publicId) = 0;

    virtual bool SelectPatientToRecycle(int64_t& internalId /*out*/) = 0;

    virtual bool SelectPatientToRecycle(int64_t& internalId /*out*/,
                                        int64_t patientIdToAvoid) = 0;

    virtual void SetGlobalProperty(int32_t property,
                                   const char* value) = 0;

    virtual void SetMainDicomTag(int64_t id,
                                 uint16_t group,
                                 uint16_t element,
                                 const char* value) = 0;

    virtual void SetIdentifierTag(int64_t id,
                                  uint16_t group,
                                  uint16_t element,
                                  const char* value) = 0;

    virtual void SetMetadata(int64_t id,
                             int32_t metadataType,
                             const char* value) = 0;

    virtual void SetProtectedPatient(int64_t internalId, 
                                     bool isProtected) = 0;

    virtual void StartTransaction() = 0;

    virtual void RollbackTransaction() = 0;

    virtual void CommitTransaction() = 0;
  };



  /**
   * @brief Bridge between C and C++ database engines.
   * 
   * Class creating the bridge between the C low-level primitives for
   * custom database engines, and the high-level IDatabaseBackend C++
   * interface.
   **/
  class DatabaseBackendAdapter
  {
  private:
    // This class cannot be instantiated
    DatabaseBackendAdapter()
    {
    }

    static void LogError(IDatabaseBackend* backend,
                         const std::runtime_error& e)
    {
      backend->GetOutput().LogError("Exception in database back-end: " + std::string(e.what()));
    }


    static int32_t  AddAttachment(void* payload,
                                  int64_t id,
                                  const OrthancPluginAttachment* attachment)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        backend->AddAttachment(id, *attachment);
        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }

                             
    static int32_t  AttachChild(void* payload,
                                int64_t parent,
                                int64_t child)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        backend->AttachChild(parent, child);
        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }
          
                   
    static int32_t  ClearChanges(void* payload)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        backend->ClearChanges();
        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }
                             

    static int32_t  ClearExportedResources(void* payload)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        backend->ClearExportedResources();
        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }


    static int32_t  CreateResource(int64_t* id, 
                                   void* payload,
                                   const char* publicId,
                                   OrthancPluginResourceType resourceType)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        *id = backend->CreateResource(publicId, resourceType);
        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }
          
         
    static int32_t  DeleteAttachment(void* payload,
                                     int64_t id,
                                     int32_t contentType)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        backend->DeleteAttachment(id, contentType);
        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }
   

    static int32_t  DeleteMetadata(void* payload,
                                   int64_t id,
                                   int32_t metadataType)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        backend->DeleteMetadata(id, metadataType);
        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }
   

    static int32_t  DeleteResource(void* payload,
                                   int64_t id)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        backend->DeleteResource(id);
        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }


    static int32_t  GetAllPublicIds(OrthancPluginDatabaseContext* context,
                                    void* payload,
                                    OrthancPluginResourceType resourceType)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        std::list<std::string> ids;
        backend->GetAllPublicIds(ids, resourceType);

        for (std::list<std::string>::const_iterator
               it = ids.begin(); it != ids.end(); ++it)
        {
          OrthancPluginDatabaseAnswerString(backend->GetOutput().context_,
                                            backend->GetOutput().database_,
                                            it->c_str());
        }

        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }


    static int32_t  GetChanges(OrthancPluginDatabaseContext* context,
                               void* payload,
                               int64_t since,
                               uint32_t maxResult)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_Change);

      try
      {
        bool done;
        backend->GetChanges(done, since, maxResult);
        
        if (done)
        {
          OrthancPluginDatabaseAnswerChangesDone(backend->GetOutput().context_,
                                                 backend->GetOutput().database_);
        }

        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }


    static int32_t  GetChildrenInternalId(OrthancPluginDatabaseContext* context,
                                          void* payload,
                                          int64_t id)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        std::list<int64_t> target;
        backend->GetChildrenInternalId(target, id);

        for (std::list<int64_t>::const_iterator
               it = target.begin(); it != target.end(); ++it)
        {
          OrthancPluginDatabaseAnswerInt64(backend->GetOutput().context_,
                                           backend->GetOutput().database_, *it);
        }

        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }
          
         
    static int32_t  GetChildrenPublicId(OrthancPluginDatabaseContext* context,
                                        void* payload,
                                        int64_t id)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        std::list<std::string> ids;
        backend->GetChildrenPublicId(ids, id);

        for (std::list<std::string>::const_iterator
               it = ids.begin(); it != ids.end(); ++it)
        {
          OrthancPluginDatabaseAnswerString(backend->GetOutput().context_,
                                            backend->GetOutput().database_,
                                            it->c_str());
        }

        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }


    static int32_t  GetExportedResources(OrthancPluginDatabaseContext* context,
                                         void* payload,
                                         int64_t  since,
                                         uint32_t  maxResult)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_ExportedResource);

      try
      {
        bool done;
        backend->GetExportedResources(done, since, maxResult);

        if (done)
        {
          OrthancPluginDatabaseAnswerExportedResourcesDone(backend->GetOutput().context_,
                                                           backend->GetOutput().database_);
        }
        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }
          
         
    static int32_t  GetLastChange(OrthancPluginDatabaseContext* context,
                                  void* payload)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_Change);

      try
      {
        backend->GetLastChange();
        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }


    static int32_t  GetLastExportedResource(OrthancPluginDatabaseContext* context,
                                            void* payload)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_ExportedResource);

      try
      {
        backend->GetLastExportedResource();
        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }
    
               
    static int32_t  GetMainDicomTags(OrthancPluginDatabaseContext* context,
                                     void* payload,
                                     int64_t id)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_DicomTag);

      try
      {
        backend->GetMainDicomTags(id);
        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }
          
         
    static int32_t  GetPublicId(OrthancPluginDatabaseContext* context,
                                void* payload,
                                int64_t id)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        std::string s = backend->GetPublicId(id);
        OrthancPluginDatabaseAnswerString(backend->GetOutput().context_,
                                          backend->GetOutput().database_,
                                          s.c_str());

        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }


    static int32_t  GetResourceCount(uint64_t* target,
                                     void* payload,
                                     OrthancPluginResourceType  resourceType)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        *target = backend->GetResourceCount(resourceType);
        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }
                   

    static int32_t  GetResourceType(OrthancPluginResourceType* resourceType,
                                    void* payload,
                                    int64_t id)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        *resourceType = backend->GetResourceType(id);
        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }


    static int32_t  GetTotalCompressedSize(uint64_t* target,
                                           void* payload)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        *target = backend->GetTotalCompressedSize();
        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }
          
         
    static int32_t  GetTotalUncompressedSize(uint64_t* target,
                                             void* payload)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        *target = backend->GetTotalUncompressedSize();
        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }
                   

    static int32_t  IsExistingResource(int32_t* existing,
                                       void* payload,
                                       int64_t id)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        *existing = backend->IsExistingResource(id);
        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }


    static int32_t  IsProtectedPatient(int32_t* isProtected,
                                       void* payload,
                                       int64_t id)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        *isProtected = backend->IsProtectedPatient(id);
        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }


    static int32_t  ListAvailableMetadata(OrthancPluginDatabaseContext* context,
                                          void* payload,
                                          int64_t id)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        std::list<int32_t> target;
        backend->ListAvailableMetadata(target, id);

        for (std::list<int32_t>::const_iterator
               it = target.begin(); it != target.end(); ++it)
        {
          OrthancPluginDatabaseAnswerInt32(backend->GetOutput().context_,
                                           backend->GetOutput().database_,
                                           *it);
        }

        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }
          
         
    static int32_t  ListAvailableAttachments(OrthancPluginDatabaseContext* context,
                                             void* payload,
                                             int64_t id)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        std::list<int32_t> target;
        backend->ListAvailableAttachments(target, id);

        for (std::list<int32_t>::const_iterator
               it = target.begin(); it != target.end(); ++it)
        {
          OrthancPluginDatabaseAnswerInt32(backend->GetOutput().context_,
                                           backend->GetOutput().database_,
                                           *it);
        }

        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }


    static int32_t  LogChange(void* payload,
                              const OrthancPluginChange* change)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        backend->LogChange(*change);
        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }
          
         
    static int32_t  LogExportedResource(void* payload,
                                        const OrthancPluginExportedResource* exported)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        backend->LogExportedResource(*exported);
        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }
          
         
    static int32_t  LookupAttachment(OrthancPluginDatabaseContext* context,
                                     void* payload,
                                     int64_t id,
                                     int32_t contentType)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_Attachment);

      try
      {
        backend->LookupAttachment(id, contentType);
        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }


    static int32_t  LookupGlobalProperty(OrthancPluginDatabaseContext* context,
                                         void* payload,
                                         int32_t property)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        std::string s;
        if (backend->LookupGlobalProperty(s, property))
        {
          OrthancPluginDatabaseAnswerString(backend->GetOutput().context_,
                                            backend->GetOutput().database_,
                                            s.c_str());
        }

        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }


    static int32_t  LookupIdentifier(OrthancPluginDatabaseContext* context,
                                     void* payload,
                                     const OrthancPluginDicomTag* tag)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        std::list<int64_t> target;
        backend->LookupIdentifier(target, tag->group, tag->element, tag->value);

        for (std::list<int64_t>::const_iterator
               it = target.begin(); it != target.end(); ++it)
        {
          OrthancPluginDatabaseAnswerInt64(backend->GetOutput().context_,
                                           backend->GetOutput().database_, *it);
        }

        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }


    static int32_t  LookupIdentifier2(OrthancPluginDatabaseContext* context,
                                      void* payload,
                                      const char* value)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        std::list<int64_t> target;
        backend->LookupIdentifier(target, value);

        for (std::list<int64_t>::const_iterator
               it = target.begin(); it != target.end(); ++it)
        {
          OrthancPluginDatabaseAnswerInt64(backend->GetOutput().context_,
                                           backend->GetOutput().database_, *it);
        }

        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }


    static int32_t  LookupMetadata(OrthancPluginDatabaseContext* context,
                                   void* payload,
                                   int64_t id,
                                   int32_t metadata)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        std::string s;
        if (backend->LookupMetadata(s, id, metadata))
        {
          OrthancPluginDatabaseAnswerString(backend->GetOutput().context_,
                                            backend->GetOutput().database_, s.c_str());
        }

        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }


    static int32_t  LookupParent(OrthancPluginDatabaseContext* context,
                                 void* payload,
                                 int64_t id)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        int64_t parent;
        if (backend->LookupParent(parent, id))
        {
          OrthancPluginDatabaseAnswerInt64(backend->GetOutput().context_,
                                           backend->GetOutput().database_, parent);
        }

        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }


    static int32_t  LookupResource(OrthancPluginDatabaseContext* context,
                                   void* payload,
                                   const char* publicId)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        int64_t id;
        OrthancPluginResourceType type;
        if (backend->LookupResource(id, type, publicId))
        {
          OrthancPluginDatabaseAnswerResource(backend->GetOutput().context_,
                                              backend->GetOutput().database_, 
                                              id, type);
        }

        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }


    static int32_t  SelectPatientToRecycle(OrthancPluginDatabaseContext* context,
                                           void* payload)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        int64_t id;
        if (backend->SelectPatientToRecycle(id))
        {
          OrthancPluginDatabaseAnswerInt64(backend->GetOutput().context_,
                                           backend->GetOutput().database_, id);
        }

        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }


    static int32_t  SelectPatientToRecycle2(OrthancPluginDatabaseContext* context,
                                            void* payload,
                                            int64_t patientIdToAvoid)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        int64_t id;
        if (backend->SelectPatientToRecycle(id, patientIdToAvoid))
        {
          OrthancPluginDatabaseAnswerInt64(backend->GetOutput().context_,
                                           backend->GetOutput().database_, id);
        }

        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }


    static int32_t  SetGlobalProperty(void* payload,
                                      int32_t property,
                                      const char* value)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        backend->SetGlobalProperty(property, value);
        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }


    static int32_t  SetMainDicomTag(void* payload,
                                    int64_t id,
                                    const OrthancPluginDicomTag* tag)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        backend->SetMainDicomTag(id, tag->group, tag->element, tag->value);
        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }


    static int32_t  SetIdentifierTag(void* payload,
                                    int64_t id,
                                    const OrthancPluginDicomTag* tag)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        backend->SetIdentifierTag(id, tag->group, tag->element, tag->value);
        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }


    static int32_t  SetMetadata(void* payload,
                                int64_t id,
                                int32_t metadata,
                                const char* value)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        backend->SetMetadata(id, metadata, value);
        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }


    static int32_t  SetProtectedPatient(void* payload,
                                        int64_t id,
                                        int32_t isProtected)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        backend->SetProtectedPatient(id, (isProtected != 0));
        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }


    static int32_t StartTransaction(void* payload)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        backend->StartTransaction();
        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }


    static int32_t RollbackTransaction(void* payload)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        backend->RollbackTransaction();
        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }


    static int32_t CommitTransaction(void* payload)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        backend->CommitTransaction();
        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }


    static int32_t Open(void* payload)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        backend->Open();
        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }


    static int32_t Close(void* payload)
    {
      IDatabaseBackend* backend = reinterpret_cast<IDatabaseBackend*>(payload);
      backend->GetOutput().SetAllowedAnswers(DatabaseBackendOutput::AllowedAnswers_None);

      try
      {
        backend->Close();
        return 0;
      }
      catch (std::runtime_error& e)
      {
        LogError(backend, e);
        return -1;
      }
    }

    
  public:
    /**
     * Register a custom database back-end written in C++.
     *
     * @param context The Orthanc plugin context, as received by OrthancPluginInitialize().
     * @param backend Your custom database engine.
     **/

    static void Register(OrthancPluginContext* context,
                         IDatabaseBackend& backend)
    {
      OrthancPluginDatabaseBackend  params;
      memset(&params, 0, sizeof(params));

      params.addAttachment = AddAttachment;
      params.attachChild = AttachChild;
      params.clearChanges = ClearChanges;
      params.clearExportedResources = ClearExportedResources;
      params.createResource = CreateResource;
      params.deleteAttachment = DeleteAttachment;
      params.deleteMetadata = DeleteMetadata;
      params.deleteResource = DeleteResource;
      params.getAllPublicIds = GetAllPublicIds;
      params.getChanges = GetChanges;
      params.getChildrenInternalId = GetChildrenInternalId;
      params.getChildrenPublicId = GetChildrenPublicId;
      params.getExportedResources = GetExportedResources;
      params.getLastChange = GetLastChange;
      params.getLastExportedResource = GetLastExportedResource;
      params.getMainDicomTags = GetMainDicomTags;
      params.getPublicId = GetPublicId;
      params.getResourceCount = GetResourceCount;
      params.getResourceType = GetResourceType;
      params.getTotalCompressedSize = GetTotalCompressedSize;
      params.getTotalUncompressedSize = GetTotalUncompressedSize;
      params.isExistingResource = IsExistingResource;
      params.isProtectedPatient = IsProtectedPatient;
      params.listAvailableMetadata = ListAvailableMetadata;
      params.listAvailableAttachments = ListAvailableAttachments;
      params.logChange = LogChange;
      params.logExportedResource = LogExportedResource;
      params.lookupAttachment = LookupAttachment;
      params.lookupGlobalProperty = LookupGlobalProperty;
      params.lookupIdentifier = LookupIdentifier;
      params.lookupIdentifier2 = LookupIdentifier2;
      params.lookupMetadata = LookupMetadata;
      params.lookupParent = LookupParent;
      params.lookupResource = LookupResource;
      params.selectPatientToRecycle = SelectPatientToRecycle;
      params.selectPatientToRecycle2 = SelectPatientToRecycle2;
      params.setGlobalProperty = SetGlobalProperty;
      params.setMainDicomTag = SetMainDicomTag;
      params.setIdentifierTag = SetIdentifierTag;
      params.setMetadata = SetMetadata;
      params.setProtectedPatient = SetProtectedPatient;
      params.startTransaction = StartTransaction;
      params.rollbackTransaction = RollbackTransaction;
      params.commitTransaction = CommitTransaction;
      params.open = Open;
      params.close = Close;

      OrthancPluginDatabaseContext* database = OrthancPluginRegisterDatabaseBackend(context, &params, &backend);
      if (!context)
      {
        throw std::runtime_error("Unable to register the database backend");
      }

      backend.RegisterOutput(new DatabaseBackendOutput(context, database));
    }
  };
}
