/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include <string>
#include <map>

#include "../../OrthancFramework/Sources/Enumerations.h"
#include "../../OrthancFramework/Sources/DicomFormat/DicomTag.h"

namespace Orthanc
{
  enum Verbosity
  {
    Verbosity_Default,
    Verbosity_Verbose,
    Verbosity_Trace
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
    StoreStatus_Failure,
    StoreStatus_FilteredOut     // Removed by NewInstanceFilter or ReceivedInstanceCallback
  };

  enum DicomTagType
  {
    DicomTagType_Identifier,   // Tag that whose value is stored and indexed in the DB
    DicomTagType_Main,         // Tag that is stored in the DB (but not indexed)
    DicomTagType_Generic       // Tag that is only stored in the JSON files
  };

  namespace Compatibility
  {
    enum IdentifierConstraintType
    {
      IdentifierConstraintType_Equal,
      IdentifierConstraintType_SmallerOrEqual,
      IdentifierConstraintType_GreaterOrEqual,
      IdentifierConstraintType_Wildcard        /* Case sensitive, "*" or "?" are the only allowed wildcards */
    };
  }

  enum FindStorageAccessMode
  {
    FindStorageAccessMode_DatabaseOnly,
    FindStorageAccessMode_DiskOnAnswer,
    FindStorageAccessMode_DiskOnLookupAndAnswer
  };

  enum StoreInstanceMode
  {
    StoreInstanceMode_Default,
    StoreInstanceMode_OverwriteDuplicate,
    StoreInstanceMode_IgnoreDuplicate
  };

  // This enum was previously part of Orthanc framework (until 1.8.2)
  enum TransferSyntaxGroup
  {
    TransferSyntaxGroup_Deflated,
    TransferSyntaxGroup_Jpeg,
    TransferSyntaxGroup_Jpeg2000,
    TransferSyntaxGroup_JpegLossless,
    TransferSyntaxGroup_Jpip,
    TransferSyntaxGroup_Mpeg2,
    TransferSyntaxGroup_Rle,
    TransferSyntaxGroup_Mpeg4,  // New in Orthanc 1.6.0
    TransferSyntaxGroup_H265    // New in Orthanc 1.9.0
  };

  enum TransactionType
  {
    TransactionType_ReadOnly,
    TransactionType_ReadWrite
  };


  /**
   * WARNING: Do not change the explicit values in the enumerations
   * below this point. This would result in incompatible databases
   * between versions of Orthanc!
   **/

  enum GlobalProperty
  {
    GlobalProperty_DatabaseSchemaVersion = 1,   // Unused in the Orthanc core as of Orthanc 0.9.5
    GlobalProperty_FlushSleep = 2,              // Unused in the Orthanc core if Orthanc > 1.9.1
    GlobalProperty_AnonymizationSequence = 3,
    GlobalProperty_JobsRegistry = 5,
    GlobalProperty_GetTotalSizeIsFast = 6,      // New in Orthanc 1.5.2
    GlobalProperty_Modalities = 20,             // New in Orthanc 1.5.0
    GlobalProperty_Peers = 21,                  // New in Orthanc 1.5.0

    // Reserved values for internal use by the database plugins
    GlobalProperty_DatabasePatchLevel = 4,
    GlobalProperty_DatabaseInternal0 = 10,
    GlobalProperty_DatabaseInternal1 = 11,
    GlobalProperty_DatabaseInternal2 = 12,
    GlobalProperty_DatabaseInternal3 = 13,
    GlobalProperty_DatabaseInternal4 = 14,
    GlobalProperty_DatabaseInternal5 = 15,
    GlobalProperty_DatabaseInternal6 = 16,
    GlobalProperty_DatabaseInternal7 = 17,
    GlobalProperty_DatabaseInternal8 = 18,
    GlobalProperty_DatabaseInternal9 = 19
  };

  enum MetadataType
  {
    MetadataType_Instance_IndexInSeries = 1,
    MetadataType_Instance_ReceptionDate = 2,
    MetadataType_RemoteAet = 3,   // Renamed from "MetadataType_Series_RemoteAet" in Orthanc 1.9.0
    MetadataType_Series_ExpectedNumberOfInstances = 4,
    MetadataType_ModifiedFrom = 5,
    MetadataType_AnonymizedFrom = 6,
    MetadataType_LastUpdate = 7,
    MetadataType_Instance_Origin = 8,            // New in Orthanc 0.9.5
    MetadataType_Instance_TransferSyntax = 9,    // New in Orthanc 1.2.0
    MetadataType_Instance_SopClassUid = 10,      // New in Orthanc 1.2.0
    MetadataType_Instance_RemoteIp = 11,         // New in Orthanc 1.4.0
    MetadataType_Instance_CalledAet = 12,        // New in Orthanc 1.4.0
    MetadataType_Instance_HttpUsername = 13,     // New in Orthanc 1.4.0
    MetadataType_Instance_PixelDataOffset = 14,  // New in Orthanc 1.9.0
    MetadataType_MainDicomTagsSignature = 15,    // New in Orthanc 1.11.0
    MetadataType_MainDicomSequences = 16,        // New in Orthanc 1.11.1
    
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
    ChangeType_StableSeries = 14,
    ChangeType_UpdatedAttachment = 15,
    ChangeType_UpdatedMetadata = 16,

    ChangeType_INTERNAL_LastLogged = 4095,

    // The changes below this point are not logged into the database
    ChangeType_Deleted = 4096,
    ChangeType_NewChildInstance = 4097
  };

  enum BuiltinDecoderTranscoderOrder
  {
    BuiltinDecoderTranscoderOrder_Before,
    BuiltinDecoderTranscoderOrder_After,
    BuiltinDecoderTranscoderOrder_Disabled
  };

  enum Warnings
  {
    Warnings_None,
    Warnings_001_TagsBeingReadFromStorage,
    Warnings_002_InconsistentDicomTagsInDb,
  };


  void InitializeServerEnumerations();

  void RegisterUserMetadata(int metadata,
                            const std::string& name);

  MetadataType StringToMetadata(const std::string& str);

  std::string EnumerationToString(MetadataType type);

  void RegisterUserContentType(int contentType,
                               const std::string& name,
                               const std::string& mime);

  FileContentType StringToContentType(const std::string& str);

  FindStorageAccessMode StringToFindStorageAccessMode(const std::string& str);

  BuiltinDecoderTranscoderOrder StringToBuiltinDecoderTranscoderOrder(const std::string& str);

  Verbosity StringToVerbosity(const std::string& str);

  std::string EnumerationToString(FileContentType type);

  std::string GetFileContentMime(FileContentType type);

  std::string GetBasePath(ResourceType type,
                          const std::string& publicId);

  const char* EnumerationToString(SeriesStatus status);

  const char* EnumerationToString(StoreStatus status);

  const char* EnumerationToString(ChangeType type);

  const char* EnumerationToString(Verbosity verbosity);

  bool IsUserMetadata(MetadataType type);

  void GetTransferSyntaxGroup(std::set<DicomTransferSyntax>& target,
                              TransferSyntaxGroup source);
}
