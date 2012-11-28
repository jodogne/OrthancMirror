/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012 Medical Physics Department, CHU of Liege,
 * Belgium
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


#include "ServerIndex.h"

using namespace Orthanc;

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "EmbeddedResources.h"
#include "../Core/Toolbox.h"
#include "../Core/Uuid.h"
#include "../Core/DicomFormat/DicomArray.h"
#include "../Core/SQLite/Transaction.h"
#include "FromDcmtkBridge.h"

#include <boost/lexical_cast.hpp>
#include <stdio.h>
#include <glog/logging.h>

namespace Orthanc
{
  namespace Internals
  {
    class ServerIndexListener : public IServerIndexListener
    {
    private:
      FileStorage& fileStorage_;
      bool hasRemainingLevel_;
      ResourceType remainingType_;
      std::string remainingPublicId_;

    public:
      ServerIndexListener(FileStorage& fileStorage) : 
        fileStorage_(fileStorage),
        hasRemainingLevel_(false)
      {
        assert(ResourceType_Patient < ResourceType_Study &&
               ResourceType_Study < ResourceType_Series &&
               ResourceType_Series < ResourceType_Instance);
      }

      void Reset()
      {
        hasRemainingLevel_ = false;
      }

      virtual void SignalRemainingAncestor(ResourceType parentType,
                                           const std::string& publicId)
      {
        LOG(INFO) << "Remaining ancestor \"" << publicId << "\" (" << parentType << ")";

        if (hasRemainingLevel_)
        {
          if (parentType < remainingType_)
          {
            remainingType_ = parentType;
            remainingPublicId_ = publicId;
          }
        }
        else
        {
          hasRemainingLevel_ = true;
          remainingType_ = parentType;
          remainingPublicId_ = publicId;
        }        
      }

      virtual void SignalFileDeleted(const std::string& fileUuid)
      {
        assert(Toolbox::IsUuid(fileUuid));
        fileStorage_.Remove(fileUuid);
      }

      bool HasRemainingLevel() const
      {
        return hasRemainingLevel_;
      }

      ResourceType GetRemainingType() const
      {
        assert(HasRemainingLevel());
        return remainingType_;
      }

      const std::string& GetRemainingPublicId() const
      {
        assert(HasRemainingLevel());
        return remainingPublicId_;
      }                                 
    };
  }


  bool ServerIndex::DeleteInternal(Json::Value& target,
                                   const std::string& uuid,
                                   ResourceType expectedType)
  {
    boost::mutex::scoped_lock scoped_lock(mutex_);

    listener_->Reset();

    std::auto_ptr<SQLite::Transaction> t(db_->StartTransaction());
    t->Begin();

    int64_t id;
    ResourceType type;
    if (!db_->LookupResource(uuid, id, type) ||
        expectedType != type)
    {
      return false;
    }
      
    db_->DeleteResource(id);

    if (listener_->HasRemainingLevel())
    {
      ResourceType type = listener_->GetRemainingType();
      const std::string& uuid = listener_->GetRemainingPublicId();

      target["RemainingAncestor"] = Json::Value(Json::objectValue);
      target["RemainingAncestor"]["Path"] = GetBasePath(type, uuid);
      target["RemainingAncestor"]["Type"] = ToString(type);
      target["RemainingAncestor"]["ID"] = uuid;
    }
    else
    {
      target["RemainingAncestor"] = Json::nullValue;
    }

    t->Commit();

    return true;
  }


  ServerIndex::ServerIndex(FileStorage& fileStorage,
                           const std::string& dbPath)
  {
    listener_.reset(new Internals::ServerIndexListener(fileStorage));

    if (dbPath == ":memory:")
    {
      db_.reset(new DatabaseWrapper(*listener_));
    }
    else
    {
      boost::filesystem::path p = dbPath;

      try
      {
        boost::filesystem::create_directories(p);
      }
      catch (boost::filesystem::filesystem_error)
      {
      }

      db_.reset(new DatabaseWrapper(p.string() + "/index", *listener_));
    }
  }


  StoreStatus ServerIndex::Store(const DicomMap& dicomSummary,
                                 const std::string& fileUuid,
                                 uint64_t uncompressedFileSize,
                                 const std::string& jsonUuid,
                                 const std::string& remoteAet)
  {
    boost::mutex::scoped_lock scoped_lock(mutex_);

    DicomInstanceHasher hasher(dicomSummary);

    try
    {
      std::auto_ptr<SQLite::Transaction> t(db_->StartTransaction());
      t->Begin();

      int64_t patient, study, series, instance;
      ResourceType type;
      bool isNewSeries = false;

      // Do nothing if the instance already exists
      if (db_->LookupResource(hasher.HashInstance(), patient, type))
      {
        assert(type == ResourceType_Instance);
        return StoreStatus_AlreadyStored;
      }

      // Create the instance
      instance = db_->CreateResource(hasher.HashInstance(), ResourceType_Instance);

      DicomMap dicom;
      dicomSummary.ExtractInstanceInformation(dicom);
      db_->SetMainDicomTags(instance, dicom);

      // Create the patient/study/series/instance hierarchy
      if (!db_->LookupResource(hasher.HashSeries(), series, type))
      {
        // This is a new series
        isNewSeries = true;
        series = db_->CreateResource(hasher.HashSeries(), ResourceType_Series);
        dicomSummary.ExtractSeriesInformation(dicom);
        db_->SetMainDicomTags(series, dicom);
        db_->AttachChild(series, instance);

        if (!db_->LookupResource(hasher.HashStudy(), study, type))
        {
          // This is a new study
          study = db_->CreateResource(hasher.HashStudy(), ResourceType_Study);
          dicomSummary.ExtractStudyInformation(dicom);
          db_->SetMainDicomTags(study, dicom);
          db_->AttachChild(study, series);

          if (!db_->LookupResource(hasher.HashPatient(), patient, type))
          {
            // This is a new patient
            patient = db_->CreateResource(hasher.HashPatient(), ResourceType_Patient);
            dicomSummary.ExtractPatientInformation(dicom);
            db_->SetMainDicomTags(patient, dicom);
            db_->AttachChild(patient, study);
          }
          else
          {
            assert(type == ResourceType_Patient);
            db_->AttachChild(patient, study);
          }
        }
        else
        {
          assert(type == ResourceType_Study);
          db_->AttachChild(study, series);
        }
      }
      else
      {
        assert(type == ResourceType_Series);
        db_->AttachChild(series, instance);
      }

      // Attach the files to the newly created instance
      db_->AttachFile(instance, AttachedFileType_Dicom, fileUuid, uncompressedFileSize);
      db_->AttachFile(instance, AttachedFileType_Json, jsonUuid, 0);  // TODO "0"

      // Attach the metadata
      db_->SetMetadata(instance, MetadataType_Instance_ReceptionDate, Toolbox::GetNowIsoString());
      db_->SetMetadata(instance, MetadataType_Instance_RemoteAet, remoteAet);

      const DicomValue* value;
      if ((value = dicomSummary.TestAndGetValue(DICOM_TAG_INSTANCE_NUMBER)) != NULL ||
          (value = dicomSummary.TestAndGetValue(DICOM_TAG_IMAGE_INDEX)) != NULL)
      {
        db_->SetMetadata(instance, MetadataType_Instance_IndexInSeries, value->AsString());
      }

      if (isNewSeries)
      {
        if ((value = dicomSummary.TestAndGetValue(DICOM_TAG_NUMBER_OF_SLICES)) != NULL ||
            (value = dicomSummary.TestAndGetValue(DICOM_TAG_IMAGES_IN_ACQUISITION)) != NULL ||
            (value = dicomSummary.TestAndGetValue(DICOM_TAG_CARDIAC_NUMBER_OF_IMAGES)) != NULL)
        {
          db_->SetMetadata(series, MetadataType_Series_ExpectedNumberOfInstances, value->AsString());
        }
      }

      t->Commit();

      return StoreStatus_Success;
    }
    catch (OrthancException& e)
    {
      LOG(ERROR) << "EXCEPTION2 [" << e.What() << "]" << " " << db_->GetErrorMessage();  
    }

    return StoreStatus_Failure;
  }


  StoreStatus ServerIndex::Store(FileStorage& storage,
                                 const char* dicomFile,
                                 size_t dicomSize,
                                 const DicomMap& dicomSummary,
                                 const Json::Value& dicomJson,
                                 const std::string& remoteAet)
  {
    std::string fileUuid = storage.Create(dicomFile, dicomSize);
    std::string jsonUuid = storage.Create(dicomJson.toStyledString());
    StoreStatus status = Store(dicomSummary, fileUuid, dicomSize, jsonUuid, remoteAet);

    if (status != StoreStatus_Success)
    {
      storage.Remove(fileUuid);
      storage.Remove(jsonUuid);
    }

    switch (status)
    {
    case StoreStatus_Success:
      LOG(INFO) << "New instance stored";
      break;

    case StoreStatus_AlreadyStored:
      LOG(INFO) << "Already stored";
      break;

    case StoreStatus_Failure:
      LOG(ERROR) << "Store failure";
      break;
    }

    return status;
  }

  uint64_t ServerIndex::GetTotalCompressedSize()
  {
    boost::mutex::scoped_lock scoped_lock(mutex_);
    return db_->GetTotalCompressedSize();
  }

  uint64_t ServerIndex::GetTotalUncompressedSize()
  {
    boost::mutex::scoped_lock scoped_lock(mutex_);
    return db_->GetTotalUncompressedSize();
  }


  SeriesStatus ServerIndex::GetSeriesStatus(int id)
  {
    // Get the expected number of instances in this series (from the metadata)
    std::string s = db_->GetMetadata(id, MetadataType_Series_ExpectedNumberOfInstances);

    size_t expected;
    try
    {
      expected = boost::lexical_cast<size_t>(s);
      if (expected < 0)
      {
        return SeriesStatus_Unknown;
      }
    }
    catch (boost::bad_lexical_cast&)
    {
      return SeriesStatus_Unknown;
    }

    // Loop over the instances of this series
    std::list<int64_t> children;
    db_->GetChildrenInternalId(children, id);

    std::set<size_t> instances;
    for (std::list<int64_t>::const_iterator 
           it = children.begin(); it != children.end(); it++)
    {
      // Get the index of this instance in the series
      s = db_->GetMetadata(*it, MetadataType_Instance_IndexInSeries);
      size_t index;
      try
      {
        index = boost::lexical_cast<size_t>(s);
      }
      catch (boost::bad_lexical_cast&)
      {
        return SeriesStatus_Unknown;
      }

      if (index <= 0 || index > expected)
      {
        // Out-of-range instance index
        return SeriesStatus_Inconsistent;
      }

      if (instances.find(index) != instances.end())
      {
        // Twice the same instance index
        return SeriesStatus_Inconsistent;
      }

      instances.insert(index);
    }

    if (instances.size() == expected)
    {
      return SeriesStatus_Complete;
    }
    else
    {
      return SeriesStatus_Missing;
    }
  }



  void ServerIndex::MainDicomTagsToJson(Json::Value& target,
                                        int64_t resourceId)
  {
    DicomMap tags;
    db_->GetMainDicomTags(tags, resourceId);
    target["MainDicomTags"] = Json::objectValue;
    FromDcmtkBridge::ToJson(target["MainDicomTags"], tags);
  }

  bool ServerIndex::LookupResource(Json::Value& result,
                                   const std::string& publicId,
                                   ResourceType expectedType)
  {
    result = Json::objectValue;

    boost::mutex::scoped_lock scoped_lock(mutex_);

    // Lookup for the requested resource
    int64_t id;
    ResourceType type;
    if (!db_->LookupResource(publicId, id, type) ||
        type != expectedType)
    {
      return false;
    }

    // Find the parent resource (if it exists)
    if (type != ResourceType_Patient)
    {
      int64_t parentId;
      if (!db_->LookupParent(parentId, id))
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      std::string parent = db_->GetPublicId(parentId);

      switch (type)
      {
      case ResourceType_Study:
        result["ParentPatient"] = parent;
        break;

      case ResourceType_Series:
        result["ParentStudy"] = parent;
        break;

      case ResourceType_Instance:
        result["ParentSeries"] = parent;
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
      }
    }

    // List the children resources
    std::list<std::string> children;
    db_->GetChildrenPublicId(children, id);

    if (type != ResourceType_Instance)
    {
      Json::Value c = Json::arrayValue;

      for (std::list<std::string>::const_iterator
             it = children.begin(); it != children.end(); it++)
      {
        c.append(*it);
      }

      switch (type)
      {
      case ResourceType_Patient:
        result["Studies"] = c;
        break;

      case ResourceType_Study:
        result["Series"] = c;
        break;

      case ResourceType_Series:
        result["Instances"] = c;
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
      }
    }

    // Set the resource type
    switch (type)
    {
    case ResourceType_Patient:
      result["Type"] = "Patient";
      break;

    case ResourceType_Study:
      result["Type"] = "Study";
      break;

    case ResourceType_Series:
    {
      result["Type"] = "Series";
      result["Status"] = ToString(GetSeriesStatus(id));

      int i;
      if (db_->GetMetadataAsInteger(i, id, MetadataType_Series_ExpectedNumberOfInstances))
        result["ExpectedNumberOfInstances"] = i;
      else
        result["ExpectedNumberOfInstances"] = Json::nullValue;

      break;
    }

    case ResourceType_Instance:
    {
      result["Type"] = "Instance";

      std::string fileUuid;
      uint64_t uncompressedSize;
      if (!db_->LookupFile(id, AttachedFileType_Dicom, fileUuid, uncompressedSize))
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      result["FileSize"] = static_cast<unsigned int>(uncompressedSize);
      result["FileUuid"] = fileUuid;

      int i;
      if (db_->GetMetadataAsInteger(i, id, MetadataType_Instance_IndexInSeries))
        result["IndexInSeries"] = i;
      else
        result["IndexInSeries"] = Json::nullValue;

      break;
    }

    default:
      throw OrthancException(ErrorCode_InternalError);
    }

    // Record the remaining information
    result["ID"] = publicId;
    MainDicomTagsToJson(result, id);

    return true;
  }


  bool ServerIndex::GetFile(std::string& fileUuid,
                            CompressionType& compressionType,
                            const std::string& instanceUuid,
                            AttachedFileType contentType)
  {
    boost::mutex::scoped_lock scoped_lock(mutex_);

    int64_t id;
    ResourceType type;
    if (!db_->LookupResource(instanceUuid, id, type) ||
        type != ResourceType_Instance)
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    uint64_t compressedSize, uncompressedSize;

    return db_->LookupFile(id, contentType, fileUuid, compressedSize, uncompressedSize, compressionType);
  }



  void ServerIndex::GetAllUuids(Json::Value& target,
                                ResourceType resourceType)
  {
    boost::mutex::scoped_lock scoped_lock(mutex_);
    db_->GetAllPublicIds(target, resourceType);
  }


  bool ServerIndex::GetChanges(Json::Value& target,
                               int64_t since,                               
                               unsigned int maxResults)
  {
    boost::mutex::scoped_lock scoped_lock(mutex_);

    db_->GetChanges(target, since, maxResults);

    return true;
  }
}
