/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
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

#include "../Core/DicomFormat/DicomMap.h"
#include "../Core/DicomFormat/DicomTag.h"
#include "../Core/Enumerations.h"
#include "../Core/FileStorage/FileInfo.h"
#include "../Core/SQLite/Connection.h"
#include "../OrthancServer/ExportedResource.h"
#include "../OrthancServer/ServerIndexChange.h"
#include "ServerEnumerations.h"

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
  };
}

