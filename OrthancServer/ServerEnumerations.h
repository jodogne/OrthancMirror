/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2013 Medical Physics Department, CHU of Liege,
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

#include "../Core/Enumerations.h"

namespace Orthanc
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
    StoreStatus_Failure,
    StoreStatus_FilteredOut     // Removed by NewInstanceFilter
  };

  enum ModalityManufacturer
  {
    ModalityManufacturer_Generic,
    ModalityManufacturer_ClearCanvas
  };

  enum DicomRequestType
  {
    DicomRequestType_Echo,
    DicomRequestType_Find,
    DicomRequestType_Get,
    DicomRequestType_Move,
    DicomRequestType_Store
  };


  /**
   * WARNING: Do not change the explicit values in the enumerations
   * below this point. This would result in incompatible databases
   * between versions of Orthanc!
   **/

  enum GlobalProperty
  {
    GlobalProperty_DatabaseSchemaVersion = 1,
    GlobalProperty_FlushSleep = 2,
    GlobalProperty_AnonymizationSequence = 3
  };

  enum MetadataType
  {
    MetadataType_Instance_IndexInSeries = 1,
    MetadataType_Instance_ReceptionDate = 2,
    MetadataType_Instance_RemoteAet = 3,
    MetadataType_Series_ExpectedNumberOfInstances = 4,
    MetadataType_ModifiedFrom = 5,
    MetadataType_AnonymizedFrom = 6,
    MetadataType_LastUpdate = 7,

    // Make sure that the value "65535" can be stored into this enumeration
    MetadataType_StartUser = 1024,
    MetadataType_EndUser = 65535
  };

  enum ChangeType
  {
    ChangeType_CompletedSeries = 1,
    ChangeType_NewInstance = 2,
    ChangeType_NewPatient = 3,
    ChangeType_NewSeries = 4,
    ChangeType_NewStudy = 5,
    ChangeType_AnonymizedStudy = 6,
    ChangeType_AnonymizedSeries = 7,
    ChangeType_ModifiedStudy = 8,
    ChangeType_ModifiedSeries = 9,
    ChangeType_AnonymizedPatient = 10,
    ChangeType_ModifiedPatient = 11,
    ChangeType_StablePatient = 12,
    ChangeType_StableStudy = 13,
    ChangeType_StableSeries = 14
  };

  void InitializeServerEnumerations();

  void RegisterUserMetadata(int metadata,
                            const std::string name);

  std::string GetBasePath(ResourceType type,
                          const std::string& publicId);

  MetadataType StringToMetadata(const std::string& str);

  std::string EnumerationToString(MetadataType type);

  const char* EnumerationToString(SeriesStatus status);

  const char* EnumerationToString(StoreStatus status);

  const char* EnumerationToString(ChangeType type);

  const char* EnumerationToString(ModalityManufacturer manufacturer);

  const char* EnumerationToString(DicomRequestType type);

  ModalityManufacturer StringToModalityManufacturer(const std::string& manufacturer);

  ResourceType GetParentResourceType(ResourceType type);

  ResourceType GetChildResourceType(ResourceType type);
}
