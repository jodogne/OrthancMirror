/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2014 Medical Physics Department, CHU of Liege,
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

#include "../Core/SQLite/Connection.h"
#include "../Core/SQLite/Transaction.h"
#include "../Core/DicomFormat/DicomInstanceHasher.h"
#include "../Core/FileStorage/FileInfo.h"
#include "IServerIndexListener.h"

#include <list>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace Orthanc
{
  namespace Internals
  {
    class SignalRemainingAncestor;
  }

  /**
   * This class manages an instance of the Orthanc SQLite database. It
   * translates low-level requests into SQL statements. Mutual
   * exclusion MUST be implemented at a higher level.
   **/
  class DatabaseWrapper
  {
  private:
    IServerIndexListener& listener_;
    SQLite::Connection db_;
    Internals::SignalRemainingAncestor* signalRemainingAncestor_;

    void Open();

    void GetChangesInternal(Json::Value& target,
                            SQLite::Statement& s,
                            int64_t since,
                            unsigned int maxResults);

    void GetExportedResourcesInternal(Json::Value& target,
                                      SQLite::Statement& s,
                                      int64_t since,
                                      unsigned int maxResults);

  public:
    void SetGlobalProperty(GlobalProperty property,
                           const std::string& value);

    bool LookupGlobalProperty(std::string& target,
                              GlobalProperty property);

    std::string GetGlobalProperty(GlobalProperty property,
                                  const std::string& defaultValue = "");

    int64_t CreateResource(const std::string& publicId,
                           ResourceType type);

    bool LookupResource(const std::string& publicId,
                        int64_t& id,
                        ResourceType& type);

    bool LookupParent(int64_t& parentId,
                      int64_t resourceId);

    std::string GetPublicId(int64_t resourceId);

    ResourceType GetResourceType(int64_t resourceId);

    void AttachChild(int64_t parent,
                     int64_t child);

    void GetChildren(Json::Value& childrenPublicIds,
                     int64_t id);

    void DeleteResource(int64_t id);

    void SetMetadata(int64_t id,
                     MetadataType type,
                     const std::string& value);

    void DeleteMetadata(int64_t id,
                        MetadataType type);

    bool LookupMetadata(std::string& target,
                        int64_t id,
                        MetadataType type);

    void ListAvailableMetadata(std::list<MetadataType>& target,
                               int64_t id);

    std::string GetMetadata(int64_t id,
                            MetadataType type,
                            const std::string& defaultValue = "");

    bool GetMetadataAsInteger(int& result,
                              int64_t id,
                              MetadataType type);

    void AddAttachment(int64_t id,
                       const FileInfo& attachment);

    void DeleteAttachment(int64_t id,
                          FileContentType attachment);

    void ListAvailableAttachments(std::list<FileContentType>& result,
                                  int64_t id);

    bool LookupAttachment(FileInfo& attachment,
                          int64_t id,
                          FileContentType contentType);

    void SetMainDicomTags(int64_t id,
                          const DicomMap& tags);

    void GetMainDicomTags(DicomMap& map,
                          int64_t id);

    bool GetParentPublicId(std::string& result,
                           int64_t id);

    void GetChildrenPublicId(std::list<std::string>& result,
                             int64_t id);

    void GetChildrenInternalId(std::list<int64_t>& result,
                               int64_t id);

    void LogChange(int64_t internalId,
                   ChangeType changeType,
                   ResourceType resourceType,
                   const std::string& publicId)
    {
      ServerIndexChange change(changeType, resourceType, publicId);
      LogChange(internalId, change);
    }

    void LogChange(int64_t internalId,
                   const ServerIndexChange& change);

    void GetChanges(Json::Value& target,
                    int64_t since,
                    unsigned int maxResults);

    void GetLastChange(Json::Value& target);

    void LogExportedResource(ResourceType resourceType,
                             const std::string& publicId,
                             const std::string& remoteModality,
                             const std::string& patientId,
                             const std::string& studyInstanceUid,
                             const std::string& seriesInstanceUid,
                             const std::string& sopInstanceUid,
                             const boost::posix_time::ptime& date = 
                             boost::posix_time::second_clock::local_time());
    
    void GetExportedResources(Json::Value& target,
                              int64_t since,
                              unsigned int maxResults);

    void GetLastExportedResource(Json::Value& target);

    // For unit testing only!
    int64_t GetTableRecordCount(const std::string& table);
    
    uint64_t GetTotalCompressedSize();
    
    uint64_t GetTotalUncompressedSize();

    uint64_t GetResourceCount(ResourceType resourceType);

    void GetAllPublicIds(Json::Value& target,
                         ResourceType resourceType);

    bool SelectPatientToRecycle(int64_t& internalId);

    bool SelectPatientToRecycle(int64_t& internalId,
                                int64_t patientIdToAvoid);

    bool IsProtectedPatient(int64_t internalId);

    void SetProtectedPatient(int64_t internalId, 
                             bool isProtected);

    DatabaseWrapper(const std::string& path,
                    IServerIndexListener& listener);

    DatabaseWrapper(IServerIndexListener& listener);

    SQLite::Transaction* StartTransaction()
    {
      return new SQLite::Transaction(db_);
    }

    const char* GetErrorMessage() const
    {
      return db_.GetErrorMessage();
    }

    void FlushToDisk()
    {
      db_.FlushToDisk();
    }

    uint64_t IncrementGlobalSequence(GlobalProperty property);

    void ClearTable(const std::string& tableName);

    bool IsExistingResource(int64_t internalId);

    void LookupIdentifier(std::list<int64_t>& result,
                          const DicomTag& tag,
                          const std::string& value);

    void LookupIdentifier(std::list<int64_t>& result,
                          const std::string& value);

    void GetAllMetadata(std::map<MetadataType, std::string>& result,
                        int64_t id);
  };
}
