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
    class ServerIndexListenerTodo : public IServerIndexListener
    {
    private:
      FileStorage& fileStorage_;
      bool hasRemainingLevel_;
      ResourceType remainingType_;
      std::string remainingPublicId_;

    public:
      ServerIndexListenerTodo(FileStorage& fileStorage) : 
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


    class DeleteFromFileStorageFunction : public SQLite::IScalarFunction
    {
    private:
      FileStorage& fileStorage_;

    public:
      DeleteFromFileStorageFunction(FileStorage& fileStorage) : 
        fileStorage_(fileStorage)
      {
      }

      virtual const char* GetName() const
      {
        return "DeleteFromFileStorage";
      }

      virtual unsigned int GetCardinality() const
      {
        return 1;
      }

      virtual void Compute(SQLite::FunctionContext& context)
      {
        std::string fileUuid = context.GetStringValue(0);
        LOG(INFO) << "Removing file [" << fileUuid << "]";

        if (Toolbox::IsUuid(fileUuid))
        {
          fileStorage_.Remove(fileUuid);
        }
      }
    };


    class SignalDeletedLevelFunction : public SQLite::IScalarFunction
    {
    private:
      int remainingLevel_;
      std::string remainingLevelUuid_;

    public:
      void Clear()
      {
        remainingLevel_ = std::numeric_limits<int>::max();
      }

      bool HasRemainingLevel() const
      {
        return (remainingLevel_ != 0 && 
                remainingLevel_ !=  std::numeric_limits<int>::max());
      }

      const std::string& GetRemainingLevelUuid() const
      {
        assert(HasRemainingLevel());
        return remainingLevelUuid_;
      }

      const char* GetRemainingLevelType() const
      {
        assert(HasRemainingLevel());
        switch (remainingLevel_)
        {
        case 1:
          return "patient";
        case 2:
          return "study";
        case 3:
          return "series";
        default:
          throw OrthancException(ErrorCode_InternalError);
        }
      }

      virtual const char* GetName() const
      {
        return "SignalDeletedLevel";
      }

      virtual unsigned int GetCardinality() const
      {
        return 2;
      }

      virtual void Compute(SQLite::FunctionContext& context)
      {
        int level = context.GetIntValue(0);
        if (level < remainingLevel_)
        {
          remainingLevel_ = level;
          remainingLevelUuid_ = context.GetStringValue(1);
        }

        //printf("deleted level [%d] [%s]\n", level, context.GetStringValue(1).c_str());
      }
    };
  }


  void ServerIndex::StoreMainDicomTags(const std::string& uuid,
                                       const DicomMap& map)
  {
    DicomArray flattened(map);
    for (size_t i = 0; i < flattened.GetSize(); i++)
    {
      SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO MainDicomTags VALUES(?, ?, ?, ?)");
      s.BindString(0, uuid);
      s.BindInt(1, flattened.GetElement(i).GetTag().GetGroup());
      s.BindInt(2, flattened.GetElement(i).GetTag().GetElement());
      s.BindString(3, flattened.GetElement(i).GetValue().AsString());
      s.Run();
    }
  }

  bool ServerIndex::HasInstance(DicomInstanceHasher& hasher)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT uuid FROM Instances WHERE dicomInstance=?");
    s.BindString(0, hasher.GetInstanceUid());
    return s.Step();
  }


  void ServerIndex::RecordChange(const std::string& resourceType,
                                 const std::string& uuid)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO Changes VALUES(NULL, ?, ?)");
    s.BindString(0, resourceType);
    s.BindString(1, uuid);
    s.Run();
  }


  void ServerIndex::CreateInstance(DicomInstanceHasher& hasher,
                                   const DicomMap& dicomSummary,
                                   const std::string& fileUuid,
                                   uint64_t fileSize,
                                   const std::string& jsonUuid, 
                                   const std::string& remoteAet)
  {
    SQLite::Statement s2(db_, SQLITE_FROM_HERE, "INSERT INTO Resources VALUES(?, ?)");
    s2.BindString(0, hasher.HashInstance());
    s2.BindInt(1, ResourceType_Instance);
    s2.Run();

    SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO Instances VALUES(?, ?, ?, ?, ?, ?, ?, ?)");
    s.BindString(0, hasher.HashInstance());
    s.BindString(1, hasher.HashSeries());
    s.BindString(2, hasher.GetInstanceUid());
    s.BindString(3, fileUuid);
    s.BindInt64(4, fileSize);
    s.BindString(5, jsonUuid);
    s.BindString(6, remoteAet);

    const DicomValue* indexInSeries;
    if ((indexInSeries = dicomSummary.TestAndGetValue(DICOM_TAG_INSTANCE_NUMBER)) != NULL ||
        (indexInSeries = dicomSummary.TestAndGetValue(DICOM_TAG_IMAGE_INDEX)) != NULL)
    {
      s.BindInt(7, boost::lexical_cast<unsigned int>(indexInSeries->AsString()));
    }
    else
    {
      s.BindNull(7);
    }

    s.Run();

    RecordChange("instances", hasher.HashInstance());

    DicomMap dicom;
    dicomSummary.ExtractInstanceInformation(dicom);
    StoreMainDicomTags(hasher.HashInstance(), dicom);
  }

  void ServerIndex::RemoveInstance(const std::string& uuid)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "DELETE FROM Instances WHERE uuid=?");
    s.BindString(0, uuid);
    s.Run();
  }

  bool ServerIndex::HasSeries(DicomInstanceHasher& hasher)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT uuid FROM Series WHERE dicomSeries=?");
    s.BindString(0, hasher.GetSeriesUid());
    return s.Step();
  }

  void ServerIndex::CreateSeries(DicomInstanceHasher& hasher,
                                 const DicomMap& dicomSummary)
  {
    SQLite::Statement s2(db_, SQLITE_FROM_HERE, "INSERT INTO Resources VALUES(?, ?)");
    s2.BindString(0, hasher.HashSeries());
    s2.BindInt(1, ResourceType_Series);
    s2.Run();

    SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO Series VALUES(?, ?, ?, ?)");
    s.BindString(0, hasher.HashSeries());
    s.BindString(1, hasher.HashStudy());
    s.BindString(2, hasher.GetSeriesUid());

    const DicomValue* expectedNumberOfInstances;
    if (//(expectedNumberOfInstances = dicomSummary.TestAndGetValue(DICOM_TAG_NUMBER_OF_FRAMES)) != NULL ||
      (expectedNumberOfInstances = dicomSummary.TestAndGetValue(DICOM_TAG_NUMBER_OF_SLICES)) != NULL ||
      //(expectedNumberOfInstances = dicomSummary.TestAndGetValue(DICOM_TAG_CARDIAC_NUMBER_OF_IMAGES)) != NULL ||
      (expectedNumberOfInstances = dicomSummary.TestAndGetValue(DICOM_TAG_IMAGES_IN_ACQUISITION)) != NULL)
    {
      s.BindInt(3, boost::lexical_cast<unsigned int>(expectedNumberOfInstances->AsString()));
    }
    else
    {
      s.BindNull(3);
    }

    s.Run();

    RecordChange("series", hasher.HashSeries());

    DicomMap dicom;
    dicomSummary.ExtractSeriesInformation(dicom);
    StoreMainDicomTags(hasher.HashSeries(), dicom);
  }

  bool ServerIndex::HasStudy(DicomInstanceHasher& hasher)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT uuid FROM Studies WHERE dicomStudy=?");
    s.BindString(0, hasher.GetStudyUid());
    return s.Step();
  }

  void ServerIndex::CreateStudy(DicomInstanceHasher& hasher,
                                const DicomMap& dicomSummary)
  {
    SQLite::Statement s2(db_, SQLITE_FROM_HERE, "INSERT INTO Resources VALUES(?, ?)");
    s2.BindString(0, hasher.HashStudy());
    s2.BindInt(1, ResourceType_Study);
    s2.Run();

    SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO Studies VALUES(?, ?, ?)");
    s.BindString(0, hasher.HashStudy());
    s.BindString(1, hasher.HashPatient());
    s.BindString(2, hasher.GetStudyUid());
    s.Run();

    RecordChange("studies", hasher.HashStudy());

    DicomMap dicom;
    dicomSummary.ExtractStudyInformation(dicom);
    StoreMainDicomTags(hasher.HashStudy(), dicom);
  }



  bool ServerIndex::HasPatient(DicomInstanceHasher& hasher)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT uuid FROM Patients WHERE dicomPatientId=?");
    s.BindString(0,hasher.GetPatientId());
    return s.Step();
  }

  void ServerIndex::CreatePatient(DicomInstanceHasher& hasher, 
                                  const DicomMap& dicomSummary)
  {
    std::string dicomPatientId = dicomSummary.GetValue(DICOM_TAG_PATIENT_ID).AsString();

    SQLite::Statement s2(db_, SQLITE_FROM_HERE, "INSERT INTO Resources VALUES(?, ?)");
    s2.BindString(0, hasher.HashPatient());
    s2.BindInt(1, ResourceType_Patient);
    s2.Run();

    SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO Patients VALUES(?, ?)");
    s.BindString(0, hasher.HashPatient());
    s.BindString(1, dicomPatientId);
    s.Run();

    RecordChange("patients", hasher.HashPatient());

    DicomMap dicom;
    dicomSummary.ExtractPatientInformation(dicom);
    StoreMainDicomTags(hasher.HashPatient(), dicom);
  }


  bool ServerIndex::DeleteInternal(Json::Value& target,
                                   const std::string& uuid,
                                   const std::string& tableName)
  {
    boost::mutex::scoped_lock scoped_lock(mutex_);


    {
      listener2_->Reset();

      std::auto_ptr<SQLite::Transaction> t(db2_->StartTransaction());
      t->Begin();

      int64_t id;
      ResourceType type;
      if (!db2_->LookupResource(uuid, id, type))
      {
        return false;
      }
      
      db2_->DeleteResource(id);

      if (listener2_->HasRemainingLevel())
      {
        ResourceType type = listener2_->GetRemainingType();
        const std::string& uuid = listener2_->GetRemainingPublicId();

        target["RemainingAncestor"] = Json::Value(Json::objectValue);
        target["RemainingAncestor"]["Path"] = std::string(GetBasePath(type)) + "/" + uuid;
        target["RemainingAncestor"]["Type"] = ToString(type);
        target["RemainingAncestor"]["ID"] = uuid;
      }
      else
      {
        target["RemainingAncestor"] = Json::nullValue;
      }

      std::cout << target << std::endl;

      t->Commit();

      return true;
    }



    deletedLevels_->Clear();

    SQLite::Statement s(db_, "DELETE FROM " + tableName + " WHERE uuid=?");
    s.BindString(0, uuid);

    if (!s.Run())
    {
      return false;
    }

    if (db_.GetLastChangeCount() == 0)
    {
      // Nothing was deleted, inexistent UUID
      return false;
    }
    
    if (deletedLevels_->HasRemainingLevel())
    {
      std::string type(deletedLevels_->GetRemainingLevelType());
      const std::string& uuid = deletedLevels_->GetRemainingLevelUuid();

      target["RemainingAncestor"] = Json::Value(Json::objectValue);
      target["RemainingAncestor"]["Path"] = "/" + type + "/" + uuid;
      target["RemainingAncestor"]["Type"] = type;
      target["RemainingAncestor"]["ID"] = uuid;
    }
    else
    {
      target["RemainingAncestor"] = Json::nullValue;
    }

    return true;
  }


  ServerIndex::ServerIndex(FileStorage& fileStorage,
                           const std::string& dbPath)
  {
    listener2_.reset(new Internals::ServerIndexListenerTodo(fileStorage));

    if (dbPath == ":memory:")
    {
      db_.OpenInMemory();
      db2_.reset(new DatabaseWrapper(*listener2_));
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

      db2_.reset(new DatabaseWrapper(p.string() + "/index2", *listener2_));

      p /= "index";
      db_.Open(p.string());
    }

    db_.Register(new Internals::DeleteFromFileStorageFunction(fileStorage));
    deletedLevels_ = (Internals::SignalDeletedLevelFunction*) 
      db_.Register(new Internals::SignalDeletedLevelFunction);

    if (!db_.DoesTableExist("GlobalProperties"))
    {
      LOG(INFO) << "Creating the database";
      std::string query;
      EmbeddedResources::GetFileResource(query, EmbeddedResources::PREPARE_DATABASE);
      db_.Execute(query);
    }
  }


  StoreStatus ServerIndex::Store2(const DicomMap& dicomSummary,
                                  const std::string& fileUuid,
                                  uint64_t uncompressedFileSize,
                                  const std::string& jsonUuid,
                                  const std::string& remoteAet)
  {
    boost::mutex::scoped_lock scoped_lock(mutex_);

    DicomInstanceHasher hasher(dicomSummary);

    try
    {
      std::auto_ptr<SQLite::Transaction> t(db2_->StartTransaction());
      t->Begin();

      int64_t patient, study, series, instance;
      ResourceType type;
      bool isNewSeries = false;

      // Do nothing if the instance already exists
      if (db2_->LookupResource(hasher.HashInstance(), patient, type))
      {
        assert(type == ResourceType_Instance);
        return StoreStatus_AlreadyStored;
      }

      // Create the instance
      instance = db2_->CreateResource(hasher.HashInstance(), ResourceType_Instance);

      DicomMap dicom;
      dicomSummary.ExtractInstanceInformation(dicom);
      db2_->SetMainDicomTags(instance, dicom);

      // Create the patient/study/series/instance hierarchy
      if (!db2_->LookupResource(hasher.HashSeries(), series, type))
      {
        // This is a new series
        isNewSeries = true;
        series = db2_->CreateResource(hasher.HashSeries(), ResourceType_Series);
        dicomSummary.ExtractSeriesInformation(dicom);
        db2_->SetMainDicomTags(series, dicom);
        db2_->AttachChild(series, instance);

        if (!db2_->LookupResource(hasher.HashStudy(), study, type))
        {
          // This is a new study
          study = db2_->CreateResource(hasher.HashStudy(), ResourceType_Study);
          dicomSummary.ExtractStudyInformation(dicom);
          db2_->SetMainDicomTags(study, dicom);
          db2_->AttachChild(study, series);

          if (!db2_->LookupResource(hasher.HashPatient(), patient, type))
          {
            // This is a new patient
            patient = db2_->CreateResource(hasher.HashPatient(), ResourceType_Patient);
            dicomSummary.ExtractPatientInformation(dicom);
            db2_->SetMainDicomTags(patient, dicom);
            db2_->AttachChild(patient, study);
          }
          else
          {
            assert(type == ResourceType_Patient);
            db2_->AttachChild(patient, study);
          }
        }
        else
        {
          assert(type == ResourceType_Study);
          db2_->AttachChild(study, series);
        }
      }
      else
      {
        assert(type == ResourceType_Series);
        db2_->AttachChild(series, instance);
      }

      // Attach the files to the newly created instance
      db2_->AttachFile(instance, AttachedFileType_Dicom, fileUuid, uncompressedFileSize);
      db2_->AttachFile(instance, AttachedFileType_Json, jsonUuid, 0);  // TODO "0"

      // Attach the metadata
      db2_->SetMetadata(instance, MetadataType_Instance_ReceptionDate, Toolbox::GetNowIsoString());
      db2_->SetMetadata(instance, MetadataType_Instance_RemoteAet, remoteAet);

      const DicomValue* value;
      if ((value = dicomSummary.TestAndGetValue(DICOM_TAG_INSTANCE_NUMBER)) != NULL ||
          (value = dicomSummary.TestAndGetValue(DICOM_TAG_IMAGE_INDEX)) != NULL)
      {
        db2_->SetMetadata(instance, MetadataType_Instance_IndexInSeries, value->AsString());
      }

      if (isNewSeries)
      {
        if ((value = dicomSummary.TestAndGetValue(DICOM_TAG_NUMBER_OF_SLICES)) != NULL ||
            (value = dicomSummary.TestAndGetValue(DICOM_TAG_IMAGES_IN_ACQUISITION)) != NULL ||
            (value = dicomSummary.TestAndGetValue(DICOM_TAG_CARDIAC_NUMBER_OF_IMAGES)) != NULL)
        {
          db2_->SetMetadata(series, MetadataType_Series_ExpectedNumberOfInstances, value->AsString());
        }
      }

      t->Commit();
    }
    catch (OrthancException& e)
    {
      LOG(ERROR) << "EXCEPTION2 [" << e.What() << "]" << " " << db_.GetErrorMessage();  
    }

    return StoreStatus_Failure;
  }


  StoreStatus ServerIndex::Store(const DicomMap& dicomSummary,
                                 const std::string& fileUuid,
                                 uint64_t uncompressedFileSize,
                                 const std::string& jsonUuid,
                                 const std::string& remoteAet)
  {
    Store2(dicomSummary, fileUuid, uncompressedFileSize, jsonUuid, remoteAet);

    boost::mutex::scoped_lock scoped_lock(mutex_);

    DicomInstanceHasher hasher(dicomSummary);

    try
    {
      SQLite::Transaction t(db_);
      t.Begin();

      if (HasInstance(hasher))
      {
        return StoreStatus_AlreadyStored;
        // TODO: Check consistency?
      }

      if (HasPatient(hasher))
      {
        // TODO: Check consistency?
      }
      else
      {
        CreatePatient(hasher, dicomSummary);
      }

      if (HasStudy(hasher))
      {
        // TODO: Check consistency?
      }
      else
      {
        CreateStudy(hasher, dicomSummary);
      }

      if (HasSeries(hasher))
      {
        // TODO: Check consistency?
      }
      else
      {
        CreateSeries(hasher, dicomSummary);
      }

      CreateInstance(hasher, dicomSummary, fileUuid, 
                     uncompressedFileSize, jsonUuid, remoteAet);
      
      t.Commit();
      return StoreStatus_Success;
      //t.Rollback();
    }
    catch (OrthancException& e)
    {
      LOG(ERROR) << "EXCEPTION [" << e.What() << "]" << " " << db_.GetErrorMessage();  
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
    return db2_->GetTotalCompressedSize();
  }

  uint64_t ServerIndex::GetTotalUncompressedSize()
  {
    boost::mutex::scoped_lock scoped_lock(mutex_);
    return db2_->GetTotalUncompressedSize();
  }


  SeriesStatus ServerIndex::GetSeriesStatus(int id)
  {
    // Get the expected number of instances in this series (from the metadata)
    std::string s = db2_->GetMetadata(id, MetadataType_Series_ExpectedNumberOfInstances);

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
    db2_->GetChildrenInternalId(children, id);

    std::set<size_t> instances;
    for (std::list<int64_t>::const_iterator 
           it = children.begin(); it != children.end(); it++)
    {
      // Get the index of this instance in the series
      s = db2_->GetMetadata(*it, MetadataType_Instance_IndexInSeries);
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



  void ServerIndex::MainDicomTagsToJson2(Json::Value& target,
                                         int64_t resourceId)
  {
    DicomMap tags;
    db2_->GetMainDicomTags(tags, resourceId);
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
    if (!db2_->LookupResource(publicId, id, type) ||
        type != expectedType)
    {
      return false;
    }

    // Find the parent resource (if it exists)
    if (type != ResourceType_Patient)
    {
      int64_t parentId;
      if (!db2_->LookupParent(parentId, id))
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      std::string parent = db2_->GetPublicId(parentId);

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
    db2_->GetChildrenPublicId(children, id);

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
      if (db2_->GetMetadataAsInteger(i, id, MetadataType_Series_ExpectedNumberOfInstances))
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
      if (!db2_->LookupFile(id, AttachedFileType_Dicom, fileUuid, uncompressedSize))
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      result["FileSize"] = static_cast<unsigned int>(uncompressedSize);
      result["FileUuid"] = fileUuid;

      int i;
      if (db2_->GetMetadataAsInteger(i, id, MetadataType_Instance_IndexInSeries))
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
    MainDicomTagsToJson2(result, id);

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
    if (!db2_->LookupResource(instanceUuid, id, type) ||
        type != ResourceType_Instance)
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    uint64_t compressedSize, uncompressedSize;

    return db2_->LookupFile(id, contentType, fileUuid, compressedSize, uncompressedSize, compressionType);
  }



  void ServerIndex::GetAllUuids(Json::Value& target,
                                ResourceType resourceType)
  {
    boost::mutex::scoped_lock scoped_lock(mutex_);
    db2_->GetAllPublicIds(target, resourceType);
  }


  bool ServerIndex::GetChanges(Json::Value& target,
                               int64_t since,
                               const std::string& filter,
                               unsigned int maxResults)
  {
    assert(target.type() == Json::objectValue);
    boost::mutex::scoped_lock scoped_lock(mutex_);

    if (filter.size() != 0 &&
        filter != "instances" &&
        filter != "series" &&
        filter != "studies" &&
        filter != "patients")
    {
      return false;
    }

    std::auto_ptr<SQLite::Statement> s;
    if (filter.size() == 0)
    {    
      s.reset(new SQLite::Statement(db_, SQLITE_FROM_HERE, "SELECT * FROM Changes WHERE seq>? "
                                    "ORDER BY seq LIMIT ?"));
      s->BindInt64(0, since);
      s->BindInt(1, maxResults);
    }
    else
    {
      s.reset(new SQLite::Statement(db_, SQLITE_FROM_HERE, "SELECT * FROM Changes WHERE seq>? "
                                    "AND basePath=? ORDER BY seq LIMIT ?"));
      s->BindInt64(0, since);
      s->BindString(1, filter);
      s->BindInt(2, maxResults);
    }
    
    int64_t lastSeq = 0;
    Json::Value results(Json::arrayValue);
    while (s->Step())
    {
      int64_t seq = s->ColumnInt64(0);
      std::string basePath = s->ColumnString(1);
      std::string uuid = s->ColumnString(2);

      if (filter.size() == 0 ||
          filter == basePath)
      {
        Json::Value change(Json::objectValue);
        change["Seq"] = static_cast<int>(seq);   // TODO JsonCpp in 64bit
        change["BasePath"] = basePath;
        change["ID"] = uuid;
        results.append(change);
      }

      if (seq > lastSeq)
      {
        lastSeq = seq;
      }
    }

    target["Results"] = results;
    target["LastSeq"] = static_cast<int>(lastSeq);   // TODO JsonCpp in 64bit

    return true;
  }

}
