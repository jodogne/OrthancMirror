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


#pragma once

#include "../../../../OrthancFramework/Sources/DicomFormat/DicomMap.h"
#include "../../../../OrthancFramework/Sources/DicomFormat/DicomTag.h"
#include "../../../../OrthancFramework/Sources/Enumerations.h"
#include "../../../../OrthancFramework/Sources/FileStorage/FileInfo.h"
#include "../../../../OrthancFramework/Sources/SQLite/Connection.h"
#include "../../../Sources/ExportedResource.h"
#include "../../../Sources/ServerIndexChange.h"
#include "../../../Sources/ServerEnumerations.h"

#include <list>


namespace Orthanc
{
  /**
   * This class is shared between the Orthanc core and the sample
   * database plugin whose code is in
   * "../Plugins/Samples/DatabasePlugin".
   **/
  class DatabaseWrapperBase
  {
  private:
    SQLite::Connection&  db_;

    ErrorCode GetChangesInternal(std::list<ServerIndexChange>& target,
                                 bool& done,
                                 SQLite::Statement& s,
                                 uint32_t maxResults);

    void GetExportedResourcesInternal(std::list<ExportedResource>& target,
                                      bool& done,
                                      SQLite::Statement& s,
                                      uint32_t maxResults);

  public:
    DatabaseWrapperBase(SQLite::Connection& db) : db_(db)
    {
    }

    void SetGlobalProperty(GlobalProperty property,
                           const std::string& value);

    bool LookupGlobalProperty(std::string& target,
                              GlobalProperty property);

    int64_t CreateResource(const std::string& publicId,
                           ResourceType type);

    bool LookupResource(int64_t& id,
                        ResourceType& type,
                        const std::string& publicId);

    ErrorCode LookupParent(bool& found,
                           int64_t& parentId,
                           int64_t resourceId);

    bool GetPublicId(std::string& result,
                     int64_t resourceId);

    ErrorCode GetResourceType(ResourceType& result,
                              int64_t resourceId);

    void AttachChild(int64_t parent,
                     int64_t child);

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

    void AddAttachment(int64_t id,
                       const FileInfo& attachment);

    void DeleteAttachment(int64_t id,
                          FileContentType attachment);

    void ListAvailableAttachments(std::list<FileContentType>& target,
                                  int64_t id);

    bool LookupAttachment(FileInfo& attachment,
                          int64_t id,
                          FileContentType contentType);


    void ClearMainDicomTags(int64_t id);


    void SetMainDicomTag(int64_t id,
                         const DicomTag& tag,
                         const std::string& value);

    void SetIdentifierTag(int64_t id,
                          const DicomTag& tag,
                          const std::string& value);

    void GetMainDicomTags(DicomMap& map,
                          int64_t id);

    void GetChildrenPublicId(std::list<std::string>& target,
                             int64_t id);

    void GetChildrenInternalId(std::list<int64_t>& target,
                               int64_t id);

    void LogChange(int64_t internalId,
                   const ServerIndexChange& change);

    ErrorCode GetChanges(std::list<ServerIndexChange>& target,
                         bool& done,
                         int64_t since,
                         uint32_t maxResults);

    ErrorCode GetLastChange(std::list<ServerIndexChange>& target);

    void LogExportedResource(const ExportedResource& resource);

    void GetExportedResources(std::list<ExportedResource>& target,
                              bool& done,
                              int64_t since,
                              uint32_t maxResults);
    
    void GetLastExportedResource(std::list<ExportedResource>& target);
    
    uint64_t GetTotalCompressedSize();
    
    uint64_t GetTotalUncompressedSize();

    void GetAllInternalIds(std::list<int64_t>& target,
                           ResourceType resourceType);

    void GetAllPublicIds(std::list<std::string>& target,
                         ResourceType resourceType);

    void GetAllPublicIds(std::list<std::string>& target,
                         ResourceType resourceType,
                         size_t since,
                         size_t limit);

    uint64_t GetResourceCount(ResourceType resourceType);

    bool SelectPatientToRecycle(int64_t& internalId);

    bool SelectPatientToRecycle(int64_t& internalId,
                                int64_t patientIdToAvoid);

    bool IsProtectedPatient(int64_t internalId);

    void SetProtectedPatient(int64_t internalId, 
                             bool isProtected);

    bool IsExistingResource(int64_t internalId);

    void LookupIdentifier(std::list<int64_t>& result,
                          ResourceType level,
                          const DicomTag& tag,
                          IdentifierConstraintType type,
                          const std::string& value);

    void LookupIdentifierRange(std::list<int64_t>& result,
                               ResourceType level,
                               const DicomTag& tag,
                               const std::string& start,
                               const std::string& end);
  };
}

