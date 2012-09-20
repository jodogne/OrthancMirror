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

namespace Orthanc
{
  namespace Internals
  {
    class DeleteFromFileStorageFunction : public SQLite::IScalarFunction
    {
    private:
      FileStorage fileStorage_;

    public:
      DeleteFromFileStorageFunction(const std::string& path) :
        fileStorage_(path)
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
        printf("Removing file [%s]\n", fileUuid.c_str());
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
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO MainDicomTags VALUES(?, ?, ?, ?)");

    DicomArray flattened(map);
    for (size_t i = 0; i < flattened.GetSize(); i++)
    {
      s.Reset();
      s.BindString(0, uuid);
      s.BindInt(1, flattened.GetElement(i).GetTag().GetGroup());
      s.BindInt(2, flattened.GetElement(i).GetTag().GetElement());
      s.BindString(3, flattened.GetElement(i).GetValue().AsString());
      s.Run();
    }
  }

  bool ServerIndex::GetMainDicomStringTag(std::string& result,
                                          const std::string& uuid,
                                          const DicomTag& tag)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, 
                        "SELECT * FROM MainDicomTags WHERE uuid=? AND tagGroup=? AND tagElement=?");
    s.BindString(0, uuid);
    s.BindInt(1, tag.GetGroup());
    s.BindInt(2, tag.GetElement());
    if (!s.Step())
    {
      return false;
    }

    result = s.ColumnString(0);
    return true;
  }

  bool ServerIndex::GetMainDicomIntTag(int& result,
                                       const std::string& uuid,
                                       const DicomTag& tag)
  {
    std::string s;
    if (!GetMainDicomStringTag(s, uuid, tag))
    {
      return false;
    }

    try
    {
      result = boost::lexical_cast<int>(s);
      return true;
    }
    catch (boost::bad_lexical_cast)
    {
      return false;
    }
  }

  bool ServerIndex::HasInstance(std::string& instanceUuid,
                                const std::string& dicomInstance)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT uuid FROM Instances WHERE dicomInstance=?");
    s.BindString(0, dicomInstance);
    if (s.Step())
    {
      instanceUuid = s.ColumnString(0);
      return true;
    }
    else
    {
      return false;
    }
  }


  void ServerIndex::RecordChange(const std::string& resourceType,
                                 const std::string& uuid)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO Changes VALUES(NULL, ?, ?)");
    s.BindString(0, resourceType);
    s.BindString(1, uuid);
    s.Run();
  }


  std::string ServerIndex::CreateInstance(const std::string& parentSeriesUuid,
                                          const std::string& dicomInstance,
                                          const DicomMap& dicomSummary,
                                          const std::string& fileUuid,
                                          uint64_t fileSize,
                                          const std::string& jsonUuid, 
                                          const std::string& distantAet)
  {
    std::string instanceUuid = Toolbox::GenerateUuid();

    SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO Instances VALUES(?, ?, ?, ?, ?, ?, ?)");
    s.BindString(0, instanceUuid);
    s.BindString(1, parentSeriesUuid);
    s.BindString(2, dicomInstance);
    s.BindString(3, fileUuid);
    s.BindInt64(4, fileSize);
    s.BindString(5, jsonUuid);
    s.BindString(6, distantAet);
    s.Run();

    RecordChange("instances", instanceUuid);

    DicomMap dicom;
    dicomSummary.ExtractInstanceInformation(dicom);
    StoreMainDicomTags(instanceUuid, dicom);

    return instanceUuid;
  }

  void ServerIndex::RemoveInstance(const std::string& uuid)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "DELETE FROM Instances WHERE uuid=?");
    s.BindString(0, uuid);
    s.Run();
  }

  bool ServerIndex::HasSeries(std::string& seriesUuid,
                              const std::string& dicomSeries)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT uuid FROM Series WHERE dicomSeries=?");
    s.BindString(0, dicomSeries);
    if (s.Step())
    {
      seriesUuid = s.ColumnString(0);
      return true;
    }
    else
    {
      return false;
    }
  }

  std::string ServerIndex::CreateSeries(const std::string& parentStudyUuid,
                                        const std::string& dicomSeries,
                                        const DicomMap& dicomSummary)
  {
    std::string seriesUuid = Toolbox::GenerateUuid();

    SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO Series VALUES(?, ?, ?, ?)");
    s.BindString(0, seriesUuid);
    s.BindString(1, parentStudyUuid);
    s.BindString(2, dicomSeries);
    s.BindNull(3);
    s.Run();

    RecordChange("series", seriesUuid);

    DicomMap dicom;
    dicomSummary.ExtractSeriesInformation(dicom);
    StoreMainDicomTags(seriesUuid, dicom);

    return seriesUuid;
  }

  bool ServerIndex::HasStudy(std::string& studyUuid,
                             const std::string& dicomStudy)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT uuid FROM Studies WHERE dicomStudy=?");
    s.BindString(0, dicomStudy);
    if (s.Step())
    {
      studyUuid = s.ColumnString(0);
      return true;
    }
    else
    {
      return false;
    }
  }

  std::string ServerIndex::CreateStudy(const std::string& parentPatientUuid,
                                       const std::string& dicomStudy,
                                       const DicomMap& dicomSummary)
  {
    std::string studyUuid = Toolbox::GenerateUuid();

    SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO Studies VALUES(?, ?, ?)");
    s.BindString(0, studyUuid);
    s.BindString(1, parentPatientUuid);
    s.BindString(2, dicomStudy);
    s.Run();

    RecordChange("studies", studyUuid);

    DicomMap dicom;
    dicomSummary.ExtractStudyInformation(dicom);
    StoreMainDicomTags(studyUuid, dicom);

    return studyUuid;
  }



  bool ServerIndex::HasPatient(std::string& patientUuid,
                               const std::string& dicomPatientId)
  {
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT uuid FROM Patients WHERE dicomPatientId=?");
    s.BindString(0, dicomPatientId);
    if (s.Step())
    {
      patientUuid = s.ColumnString(0);
      return true;
    }
    else
    {
      return false;
    }
  }

  std::string ServerIndex::CreatePatient(const std::string& patientId,
                                         const DicomMap& dicomSummary)
  {
    std::string patientUuid = Toolbox::GenerateUuid();
    std::string dicomPatientId = dicomSummary.GetValue(DicomTag::PATIENT_ID).AsString();

    SQLite::Statement s(db_, SQLITE_FROM_HERE, "INSERT INTO Patients VALUES(?, ?)");
    s.BindString(0, patientUuid);
    s.BindString(1, dicomPatientId);
    s.Run();

    RecordChange("patients", patientUuid);

    DicomMap dicom;
    dicomSummary.ExtractPatientInformation(dicom);
    StoreMainDicomTags(patientUuid, dicom);

    return patientUuid;
  }


  void ServerIndex::GetMainDicomTags(DicomMap& map,
                                     const std::string& uuid)
  {
    map.Clear();

    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT * FROM MainDicomTags WHERE uuid=?");
    s.BindString(0, uuid);
    while (s.Step())
    {
      map.SetValue(s.ColumnInt(1),
                   s.ColumnInt(2),
                   s.ColumnString(3));
    }
  }

  void ServerIndex::MainDicomTagsToJson(Json::Value& target,
                                        const std::string& uuid)
  {
    DicomMap map;
    GetMainDicomTags(map, uuid);
    target["MainDicomTags"] = Json::objectValue;
    FromDcmtkBridge::ToJson(target["MainDicomTags"], map);
  }


  bool ServerIndex::DeleteInternal(Json::Value& target,
                                   const std::string& uuid,
                                   const std::string& tableName)
  {
    boost::mutex::scoped_lock scoped_lock(mutex_);

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


  ServerIndex::ServerIndex(const std::string& storagePath)
  {
    boost::filesystem::path p = storagePath;

    try
    {
      boost::filesystem::create_directories(storagePath);
    }
    catch (boost::filesystem::filesystem_error)
    {
    }

    p /= "index";
    db_.Open(p.string());
    db_.Register(new Internals::DeleteFromFileStorageFunction(storagePath));
    deletedLevels_ = (Internals::SignalDeletedLevelFunction*) 
      db_.Register(new Internals::SignalDeletedLevelFunction);

    if (!db_.DoesTableExist("GlobalProperties"))
    {
      printf("Creating the database\n");
      std::string query;
      EmbeddedResources::GetFileResource(query, EmbeddedResources::PREPARE_DATABASE);
      db_.Execute(query);
    }
  }


  StoreStatus ServerIndex::Store(std::string& instanceUuid,
                                 const DicomMap& dicomSummary,
                                 const std::string& fileUuid,
                                 uint64_t uncompressedFileSize,
                                 const std::string& jsonUuid,
                                 const std::string& distantAet)
  {
    boost::mutex::scoped_lock scoped_lock(mutex_);

    std::string dicomPatientId = dicomSummary.GetValue(DicomTag::PATIENT_ID).AsString();
    std::string dicomInstance = dicomSummary.GetValue(DicomTag::SOP_INSTANCE_UID).AsString();
    std::string dicomSeries = dicomSummary.GetValue(DicomTag::SERIES_INSTANCE_UID).AsString();
    std::string dicomStudy = dicomSummary.GetValue(DicomTag::STUDY_INSTANCE_UID).AsString();

    try
    {
      SQLite::Transaction t(db_);
      t.Begin();

      if (HasInstance(instanceUuid, dicomInstance))
      {
        return StoreStatus_AlreadyStored;
        // TODO: Check consistency?
      }

      std::string patientUuid;
      if (HasPatient(patientUuid, dicomPatientId))
      {
        // TODO: Check consistency?
      }
      else
      {
        patientUuid = CreatePatient(dicomPatientId, dicomSummary);
      }

      std::string studyUuid;
      if (HasStudy(studyUuid, dicomStudy))
      {
        // TODO: Check consistency?
      }
      else
      {
        studyUuid = CreateStudy(patientUuid, dicomStudy, dicomSummary);
      }

      std::string seriesUuid;
      if (HasSeries(seriesUuid, dicomSeries))
      {
        // TODO: Check consistency?
      }
      else
      {
        seriesUuid = CreateSeries(studyUuid, dicomSeries, dicomSummary);
      }

      instanceUuid = CreateInstance(seriesUuid, dicomInstance, dicomSummary, fileUuid, 
                                    uncompressedFileSize, jsonUuid, distantAet);
      
      t.Commit();
      return StoreStatus_Success;
      //t.Rollback();
    }
    catch (OrthancException& e)
    {
      std::cout << "EXCEPT2 [" << e.What() << "]" << " " << db_.GetErrorMessage() << std::endl;  
    }

    return StoreStatus_Failure;
  }


  StoreStatus ServerIndex::Store(std::string& instanceUuid,
                                 FileStorage& storage,
                                 const char* dicomFile,
                                 size_t dicomSize,
                                 const DicomMap& dicomSummary,
                                 const Json::Value& dicomJson,
                                 const std::string& distantAet)
  {
    std::string fileUuid = storage.Create(dicomFile, dicomSize);
    std::string jsonUuid = storage.Create(dicomJson.toStyledString());
    StoreStatus status = Store(instanceUuid, dicomSummary, fileUuid, 
                               dicomSize, jsonUuid, distantAet);

    if (status != StoreStatus_Success)
    {
      storage.Remove(fileUuid);
      storage.Remove(jsonUuid);
    }

    switch (status)
    {
    case StoreStatus_Success:
      std::cout << "New instance stored: " << GetTotalSize() << std::endl;
      break;

    case StoreStatus_AlreadyStored:
      std::cout << "Already stored" << std::endl;
      break;

    case StoreStatus_Failure:
      std::cout << "Store failure" << std::endl;
      break;
    }

    return status;
  }

  uint64_t ServerIndex::GetTotalSize()
  {
    boost::mutex::scoped_lock scoped_lock(mutex_);
    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT SUM(fileSize) FROM Instances");
    s.Run();
    return s.ColumnInt64(0);
  }


  SeriesStatus ServerIndex::GetSeriesStatus(const std::string& seriesUuid)
  {
    int numberOfSlices;
    if (!GetMainDicomIntTag(numberOfSlices, seriesUuid, DicomTag::NUMBER_OF_SLICES) ||
        numberOfSlices < 0)
    {
      return SeriesStatus_Unknown;
    }

    // Loop over the instances of the series
    //std::set<

    // TODO
    return SeriesStatus_Unknown;
  }





  bool ServerIndex::GetInstance(Json::Value& result,
                                const std::string& instanceUuid)
  {
    assert(result.type() == Json::objectValue);
    boost::mutex::scoped_lock scoped_lock(mutex_);

    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT parentSeries, dicomInstance, fileSize, fileUuid FROM Instances WHERE uuid=?");
    s.BindString(0, instanceUuid);
    if (!s.Step())
    {
      return false;
    }
    else
    {
      result["ID"] = instanceUuid;
      result["ParentSeries"] = s.ColumnString(0);
      result["FileSize"] = s.ColumnInt(2);   // TODO switch to 64bit with JsonCpp 0.6?
      result["FileUuid"] = s.ColumnString(3);
      MainDicomTagsToJson(result, instanceUuid);
      return true;
    }
  }


  bool ServerIndex::GetSeries(Json::Value& result,
                              const std::string& seriesUuid)
  {
    assert(result.type() == Json::objectValue);
    boost::mutex::scoped_lock scoped_lock(mutex_);

    SQLite::Statement s1(db_, SQLITE_FROM_HERE, "SELECT parentStudy, dicomSeries FROM Series WHERE uuid=?");
    s1.BindString(0, seriesUuid);
    if (!s1.Step())
    {
      return false;
    }

    result["ID"] = seriesUuid;
    result["ParentStudy"] = s1.ColumnString(0);
    MainDicomTagsToJson(result, seriesUuid);

    Json::Value instances(Json::arrayValue);
    SQLite::Statement s2(db_, SQLITE_FROM_HERE, "SELECT uuid FROM Instances WHERE parentSeries=?");
    s2.BindString(0, seriesUuid);
    while (s2.Step())
    {
      instances.append(s2.ColumnString(0));
    }
      
    result["Instances"] = instances;

    return true;
  }


  bool ServerIndex::GetStudy(Json::Value& result,
                             const std::string& studyUuid)
  {
    assert(result.type() == Json::objectValue);
    boost::mutex::scoped_lock scoped_lock(mutex_);

    SQLite::Statement s1(db_, SQLITE_FROM_HERE, "SELECT parentPatient, dicomStudy FROM Studies WHERE uuid=?");
    s1.BindString(0, studyUuid);
    if (!s1.Step())
    {
      return false;
    }

    result["ID"] = studyUuid;
    result["ParentPatient"] = s1.ColumnString(0);
    MainDicomTagsToJson(result, studyUuid);

    Json::Value series(Json::arrayValue);
    SQLite::Statement s2(db_, SQLITE_FROM_HERE, "SELECT uuid FROM Series WHERE parentStudy=?");
    s2.BindString(0, studyUuid);
    while (s2.Step())
    {
      series.append(s2.ColumnString(0));
    }
      
    result["Series"] = series;
    return true;
  }


  bool ServerIndex::GetPatient(Json::Value& result,
                               const std::string& patientUuid)
  {
    assert(result.type() == Json::objectValue);
    boost::mutex::scoped_lock scoped_lock(mutex_);

    SQLite::Statement s1(db_, SQLITE_FROM_HERE, "SELECT dicomPatientId FROM Patients WHERE uuid=?");
    s1.BindString(0, patientUuid);
    if (!s1.Step())
    {
      return false;
    }

    result["ID"] = patientUuid;
    MainDicomTagsToJson(result, patientUuid);

    Json::Value studies(Json::arrayValue);
    SQLite::Statement s2(db_, SQLITE_FROM_HERE, "SELECT uuid FROM Studies WHERE parentPatient=?");
    s2.BindString(0, patientUuid);
    while (s2.Step())
    {
      studies.append(s2.ColumnString(0));
    }
      
    result["Studies"] = studies;
    return true;
  }


  bool ServerIndex::GetJsonFile(std::string& fileUuid,
                                const std::string& instanceUuid)
  {
    boost::mutex::scoped_lock scoped_lock(mutex_);

    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT jsonUuid FROM Instances WHERE uuid=?");
    s.BindString(0, instanceUuid);
    if (s.Step())
    {
      fileUuid = s.ColumnString(0);
      return true;
    }
    else
    {
      return false;
    }
  }

  bool ServerIndex::GetDicomFile(std::string& fileUuid,
                                 const std::string& instanceUuid)
  {
    boost::mutex::scoped_lock scoped_lock(mutex_);

    SQLite::Statement s(db_, SQLITE_FROM_HERE, "SELECT fileUuid FROM Instances WHERE uuid=?");
    s.BindString(0, instanceUuid);
    if (s.Step())
    {
      fileUuid = s.ColumnString(0);
      return true;
    }
    else
    {
      return false;
    }
  }


  void ServerIndex::GetAllUuids(Json::Value& target,
                                const std::string& tableName)
  {
    assert(target.type() == Json::arrayValue);
    boost::mutex::scoped_lock scoped_lock(mutex_);

    std::string query = "SELECT uuid FROM " + tableName;
    SQLite::Statement s(db_, query);
    while (s.Step())
    {
      target.append(s.ColumnString(0));
    }
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
