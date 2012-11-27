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


#pragma once

#include <boost/thread.hpp>
#include "../Core/SQLite/Connection.h"
#include "../Core/DicomFormat/DicomMap.h"
#include "../Core/FileStorage.h"
#include "../Core/DicomFormat/DicomInstanceHasher.h"
#include "ServerEnumerations.h"

#include "DatabaseWrapper.h"


namespace Orthanc
{
  namespace Internals
  {
    class SignalDeletedLevelFunction;
    class ServerIndexListenerTodo;
  }


  class ServerIndex
  {
  private:
    SQLite::Connection db_;
    boost::mutex mutex_;

    std::auto_ptr<Internals::ServerIndexListenerTodo> listener2_;
    std::auto_ptr<DatabaseWrapper> db2_;

    // DO NOT delete the following one, SQLite::Connection will do it automatically
    Internals::SignalDeletedLevelFunction* deletedLevels_;

    void StoreMainDicomTags(const std::string& uuid,
                            const DicomMap& map);

    bool HasPatient(DicomInstanceHasher& hasher);

    void CreatePatient(DicomInstanceHasher& hasher,
                       const DicomMap& dicomSummary);

    bool HasStudy(DicomInstanceHasher& hasher);

    void CreateStudy(DicomInstanceHasher& hasher,
                     const DicomMap& dicomSummary);

    bool HasSeries(DicomInstanceHasher& hasher);

    void CreateSeries(DicomInstanceHasher& hasher,
                      const DicomMap& dicomSummary);

    bool HasInstance(DicomInstanceHasher& hasher);

    void CreateInstance(DicomInstanceHasher& hasher,
                        const DicomMap& dicomSummary,
                        const std::string& fileUuid,
                        uint64_t fileSize,
                        const std::string& jsonUuid, 
                        const std::string& remoteAet);

    void RecordChange(const std::string& resourceType,
                      const std::string& uuid);

    void RemoveInstance(const std::string& uuid);

    void MainDicomTagsToJson2(Json::Value& result,
                              int64_t resourceId);

    bool DeleteInternal(Json::Value& target,
                        const std::string& uuid,
                        const std::string& tableName);

    StoreStatus Store2(const DicomMap& dicomSummary,
                       const std::string& fileUuid,
                       uint64_t uncompressedFileSize,
                       const std::string& jsonUuid,
                       const std::string& remoteAet);

    bool LookupResource(Json::Value& result,
                        const std::string& publicId,
                        ResourceType expectedType);

    SeriesStatus GetSeriesStatus(int id);

  public:
    ServerIndex(FileStorage& fileStorage,
                const std::string& dbPath);

    StoreStatus Store(const DicomMap& dicomSummary,
                      const std::string& fileUuid,
                      uint64_t uncompressedFileSize,
                      const std::string& jsonUuid,
                      const std::string& remoteAet);

    StoreStatus Store(FileStorage& storage,
                      const char* dicomFile,
                      size_t dicomSize,
                      const DicomMap& dicomSummary,
                      const Json::Value& dicomJson,
                      const std::string& remoteAet);

    uint64_t GetTotalCompressedSize();

    uint64_t GetTotalUncompressedSize();

    bool GetInstance(Json::Value& result,
                     const std::string& instanceUuid)
    {
      return LookupResource(result, instanceUuid, ResourceType_Instance);
    }

    bool GetSeries(Json::Value& result,
                   const std::string& seriesUuid)
    {
      return LookupResource(result, seriesUuid, ResourceType_Series);
    }


    bool GetStudy(Json::Value& result,
                  const std::string& studyUuid)
    {
      return LookupResource(result, studyUuid, ResourceType_Study);
    }


    bool GetPatient(Json::Value& result,
                    const std::string& patientUuid)
    {
      return LookupResource(result, patientUuid, ResourceType_Patient);
    }

    bool GetFile(std::string& fileUuid,
                 CompressionType& compressionType,
                 const std::string& instanceUuid,
                 AttachedFileType contentType);

    void GetAllUuids(Json::Value& target,
                     ResourceType resourceType);

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
