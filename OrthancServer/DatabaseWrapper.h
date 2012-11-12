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

#include "../Core/SQLite/Connection.h"
#include "../Core/SQLite/Transaction.h"
#include "../Core/DicomFormat/DicomInstanceHasher.h"
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

  public:
    void SetGlobalProperty(const std::string& name,
                           const std::string& value);

    bool LookupGlobalProperty(std::string& target,
                              const std::string& name);

    std::string GetGlobalProperty(const std::string& name,
                                  const std::string& defaultValue = "");

    int64_t CreateResource(const std::string& publicId,
                           ResourceType type);

    bool LookupResource(const std::string& publicId,
                        int64_t& id,
                        ResourceType& type);

    void AttachChild(int64_t parent,
                     int64_t child);

    void DeleteResource(int64_t id);

    void SetMetadata(int64_t id,
                     MetadataType type,
                     const std::string& value);

    bool LookupMetadata(std::string& target,
                        int64_t id,
                        MetadataType type);

    std::string GetMetadata(int64_t id,
                            MetadataType type,
                            const std::string& defaultValue = "");

    void AttachFile(int64_t id,
                    const std::string& name,
                    const std::string& fileUuid,
                    uint64_t compressedSize,
                    uint64_t uncompressedSize,
                    CompressionType compressionType);

    void AttachFile(int64_t id,
                    const std::string& name,
                    const std::string& fileUuid,
                    uint64_t fileSize)
    {
      AttachFile(id, name, fileUuid, fileSize, fileSize, CompressionType_None);
    }

    bool LookupFile(int64_t id,
                    const std::string& name,
                    std::string& fileUuid,
                    uint64_t& compressedSize,
                    uint64_t& uncompressedSize,
                    CompressionType& compressionType);

    void SetMainDicomTags(int64_t id,
                          const DicomMap& tags);

    void GetMainDicomTags(DicomMap& map,
                          int64_t id);

    bool GetParentPublicId(std::string& result,
                           int64_t id);

    void GetChildrenPublicId(std::list<std::string>& result,
                             int64_t id);

    void LogChange(ChangeType changeType,
                   int64_t internalId,
                   ResourceType resourceType,
                   const boost::posix_time::ptime& date = boost::posix_time::second_clock::local_time());

    void LogExportedInstance(const std::string& remoteModality,
                             DicomInstanceHasher& hasher,
                             const boost::posix_time::ptime& date = boost::posix_time::second_clock::local_time());
    
    int64_t GetTableRecordCount(const std::string& table);
    
    uint64_t GetTotalCompressedSize();
    
    uint64_t GetTotalUncompressedSize();

    DatabaseWrapper(const std::string& path,
                    IServerIndexListener& listener);

    DatabaseWrapper(IServerIndexListener& listener);

    SQLite::Transaction* StartTransaction()
    {
      return new SQLite::Transaction(db_);
    }
  };
}
