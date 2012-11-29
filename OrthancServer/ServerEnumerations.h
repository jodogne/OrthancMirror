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

#include <string>

namespace Orthanc
{
  enum GlobalProperty
  {
    GlobalProperty_FlushSleep = 1
  };

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

  enum ResourceType
  {
    ResourceType_Patient = 1,
    ResourceType_Study = 2,
    ResourceType_Series = 3,
    ResourceType_Instance = 4
  };

  enum CompressionType
  {
    CompressionType_None = 1,
    CompressionType_Zlib = 2
  };

  enum MetadataType
  {
    MetadataType_Instance_IndexInSeries = 2,
    MetadataType_Instance_ReceptionDate = 4,
    MetadataType_Instance_RemoteAet = 1,
    MetadataType_Series_ExpectedNumberOfInstances = 3
  };

  enum ChangeType
  {
    ChangeType_CompletedSeries = 1,
    ChangeType_NewInstance = 3,
    ChangeType_NewPatient = 4,
    ChangeType_NewSeries = 2,
    ChangeType_NewStudy = 5
  };

  enum AttachedFileType
  {
    AttachedFileType_Dicom = 1,
    AttachedFileType_Json = 2
  };

  const char* ToString(ResourceType type);

  std::string GetBasePath(ResourceType type,
                          const std::string& publicId);

  const char* ToString(SeriesStatus status);

  const char* ToString(StoreStatus status);

  const char* ToString(ChangeType type);
}
