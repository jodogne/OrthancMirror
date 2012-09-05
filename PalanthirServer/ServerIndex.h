/**
 * Palanthir - A Lightweight, RESTful DICOM Store
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


#pragma once

#include <boost/thread.hpp>
#include "../Core/SQLite/Connection.h"
#include "../Core/DicomFormat/DicomMap.h"
#include "../Core/FileStorage.h"


namespace Palanthir
{
  enum SeriesStatus
  {
    SeriesStatus_Complete,
    SeriesStatus_Missing,
    SeriesStatus_Inconsistent,
    SeriesStatus_Unknown
  };


  enum StoreStatus
  {
    StoreStatus_Success,
    StoreStatus_AlreadyStored,
    StoreStatus_Failure
  };


  namespace Internals
  {
    class SignalDeletedLevelFunction;
  }


  class ServerIndex
  {
  private:
    SQLite::Connection db_;
    boost::mutex mutex_;

    // DO NOT delete the following one, SQLite::Connection will do it automatically
    Internals::SignalDeletedLevelFunction* deletedLevels_;

    void StoreMainDicomTags(const std::string& uuid,
                            const DicomMap& map);

    bool GetMainDicomStringTag(std::string& result,
                               const std::string& uuid,
                               const DicomTag& tag);

    bool GetMainDicomIntTag(int& result,
                            const std::string& uuid,
                            const DicomTag& tag);

    bool HasInstance(std::string& instanceUuid,
                     const std::string& dicomInstance);

    void RecordChange(const std::string& resourceType,
                      const std::string& uuid);

    std::string CreateInstance(const std::string& parentSeriesUuid,
                               const std::string& dicomInstance,
                               const DicomMap& dicomSummary,
                               const std::string& fileUuid,
                               uint64_t fileSize,
                               const std::string& jsonUuid, 
                               const std::string& distantAet);

    void RemoveInstance(const std::string& uuid);

    bool HasSeries(std::string& seriesUuid,
                   const std::string& dicomSeries);

    std::string CreateSeries(const std::string& parentStudyUuid,
                             const std::string& dicomSeries,
                             const DicomMap& dicomSummary);

    bool HasStudy(std::string& studyUuid,
                  const std::string& dicomStudy);

    std::string CreateStudy(const std::string& parentPatientUuid,
                            const std::string& dicomStudy,
                            const DicomMap& dicomSummary);

    bool HasPatient(std::string& patientUuid,
                    const std::string& dicomPatientId);

    std::string CreatePatient(const std::string& patientId,
                              const DicomMap& dicomSummary);

    void GetMainDicomTags(DicomMap& map,
                          const std::string& uuid);

    void MainDicomTagsToJson(Json::Value& target,
                             const std::string& uuid);

    bool DeleteInternal(Json::Value& target,
                        const std::string& uuid,
                        const std::string& tableName);

  public:
    ServerIndex(const std::string& storagePath);

    StoreStatus Store(std::string& instanceUuid,
                      const DicomMap& dicomSummary,
                      const std::string& fileUuid,
                      uint64_t uncompressedFileSize,
                      const std::string& jsonUuid,
                      const std::string& distantAet);

    StoreStatus Store(std::string& instanceUuid,
                      FileStorage& storage,
                      const char* dicomFile,
                      size_t dicomSize,
                      const DicomMap& dicomSummary,
                      const Json::Value& dicomJson,
                      const std::string& distantAet);

    uint64_t GetTotalSize();

    SeriesStatus GetSeriesStatus(const std::string& seriesUuid);


    bool GetInstance(Json::Value& result,
                     const std::string& instanceUuid);

    bool GetSeries(Json::Value& result,
                   const std::string& seriesUuid);

    bool GetStudy(Json::Value& result,
                  const std::string& studyUuid);

    bool GetPatient(Json::Value& result,
                    const std::string& patientUuid);

    bool GetJsonFile(std::string& fileUuid,
                     const std::string& instanceUuid);

    bool GetDicomFile(std::string& fileUuid,
                      const std::string& instanceUuid);

    void GetAllUuids(Json::Value& target,
                     const std::string& tableName);

    bool DeletePatient(Json::Value& target,
                       const std::string& patientUuid)
    {
      return DeleteInternal(target, patientUuid, "Patients");
    }

    bool DeleteStudy(Json::Value& target,
                     const std::string& studyUuid)
    {
      return DeleteInternal(target, studyUuid, "Studies");
    }

    bool DeleteSeries(Json::Value& target,
                      const std::string& seriesUuid)
    {
      return DeleteInternal(target, seriesUuid, "Series");
    }

    bool DeleteInstance(Json::Value& target,
                        const std::string& instanceUuid)
    {
      return DeleteInternal(target, instanceUuid, "Instances");
    }

    bool GetChanges(Json::Value& target,
                    int64_t since,
                    const std::string& filter,
                    unsigned int maxResults);
  };
}
