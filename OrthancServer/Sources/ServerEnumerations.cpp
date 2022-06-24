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


#include "PrecompiledHeadersServer.h"
#include "ServerEnumerations.h"

#include "../../OrthancFramework/Sources/OrthancException.h"
#include "../../OrthancFramework/Sources/EnumerationDictionary.h"
#include "../../OrthancFramework/Sources/Logging.h"
#include "../../OrthancFramework/Sources/Toolbox.h"

#include <boost/thread.hpp>

namespace Orthanc
{
  typedef std::map<FileContentType, std::string>  MimeTypes;

  static boost::mutex enumerationsMutex_;
  static EnumerationDictionary<MetadataType> dictMetadataType_;
  static EnumerationDictionary<FileContentType> dictContentType_;
  static MimeTypes  mimeTypes_;

  void InitializeServerEnumerations()
  {
    boost::mutex::scoped_lock lock(enumerationsMutex_);

    dictMetadataType_.Clear();
    dictContentType_.Clear();
    
    dictMetadataType_.Add(MetadataType_Instance_IndexInSeries, "IndexInSeries");
    dictMetadataType_.Add(MetadataType_Instance_ReceptionDate, "ReceptionDate");
    dictMetadataType_.Add(MetadataType_RemoteAet, "RemoteAET");
    dictMetadataType_.Add(MetadataType_Series_ExpectedNumberOfInstances, "ExpectedNumberOfInstances");
    dictMetadataType_.Add(MetadataType_ModifiedFrom, "ModifiedFrom");
    dictMetadataType_.Add(MetadataType_AnonymizedFrom, "AnonymizedFrom");
    dictMetadataType_.Add(MetadataType_LastUpdate, "LastUpdate");
    dictMetadataType_.Add(MetadataType_Instance_Origin, "Origin");
    dictMetadataType_.Add(MetadataType_Instance_TransferSyntax, "TransferSyntax");
    dictMetadataType_.Add(MetadataType_Instance_SopClassUid, "SopClassUid");
    dictMetadataType_.Add(MetadataType_Instance_RemoteIp, "RemoteIP");
    dictMetadataType_.Add(MetadataType_Instance_CalledAet, "CalledAET");
    dictMetadataType_.Add(MetadataType_Instance_HttpUsername, "HttpUsername");
    dictMetadataType_.Add(MetadataType_Instance_PixelDataOffset, "PixelDataOffset");
    dictMetadataType_.Add(MetadataType_MainDicomTagsSignature, "MainDicomTagsSignature");
    dictMetadataType_.Add(MetadataType_MainDicomSequences, "MainDicomSequences");

    dictContentType_.Add(FileContentType_Dicom, "dicom");
    dictContentType_.Add(FileContentType_DicomAsJson, "dicom-as-json");
    dictContentType_.Add(FileContentType_DicomUntilPixelData, "dicom-until-pixel-data");
  }

  void RegisterUserMetadata(int metadata,
                            const std::string& name)
  {
    boost::mutex::scoped_lock lock(enumerationsMutex_);

    MetadataType type = static_cast<MetadataType>(metadata);

    if (metadata < 0 || 
        !IsUserMetadata(type))
    {
      LOG(ERROR) << "A user content type must have index between "
                 << static_cast<int>(MetadataType_StartUser) << " and "
                 << static_cast<int>(MetadataType_EndUser) << ", but \""
                 << name << "\" has index " << metadata;
        
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    if (dictMetadataType_.Contains(type))
    {
      LOG(ERROR) << "Cannot associate user content type \""
                 << name << "\" with index " << metadata 
                 << ", as this index is already used";
        
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    dictMetadataType_.Add(type, name);
  }

  std::string EnumerationToString(MetadataType type)
  {
    // This function MUST return a "std::string" and not "const
    // char*", as the result is not a static string
    boost::mutex::scoped_lock lock(enumerationsMutex_);
    return dictMetadataType_.Translate(type);
  }

  MetadataType StringToMetadata(const std::string& str)
  {
    boost::mutex::scoped_lock lock(enumerationsMutex_);
    return dictMetadataType_.Translate(str);
  }

  void RegisterUserContentType(int contentType,
                               const std::string& name,
                               const std::string& mime)
  {
    boost::mutex::scoped_lock lock(enumerationsMutex_);

    FileContentType type = static_cast<FileContentType>(contentType);

    if (contentType < 0 || 
        !IsUserContentType(type))
    {
      LOG(ERROR) << "A user content type must have index between "
                 << static_cast<int>(FileContentType_StartUser) << " and "
                 << static_cast<int>(FileContentType_EndUser) << ", but \""
                 << name << "\" has index " << contentType;
        
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    if (dictContentType_.Contains(type))
    {
      LOG(ERROR) << "Cannot associate user content type \""
                 << name << "\" with index " << contentType 
                 << ", as this index is already used";
        
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    dictContentType_.Add(type, name);
    mimeTypes_[type] = mime;
  }

  std::string EnumerationToString(FileContentType type)
  {
    // This function MUST return a "std::string" and not "const
    // char*", as the result is not a static string
    boost::mutex::scoped_lock lock(enumerationsMutex_);
    return dictContentType_.Translate(type);
  }

  std::string GetFileContentMime(FileContentType type)
  {
    if (type >= FileContentType_StartUser &&
        type <= FileContentType_EndUser)
    {
      boost::mutex::scoped_lock lock(enumerationsMutex_);
      
      MimeTypes::const_iterator it = mimeTypes_.find(type);
      if (it != mimeTypes_.end())
      {
        return it->second;
      }
    }

    switch (type)
    {
      case FileContentType_Dicom:
        return EnumerationToString(MimeType_Dicom);

      case FileContentType_DicomAsJson:
        return MIME_JSON_UTF8;

      default:
        return EnumerationToString(MimeType_Binary);
    }
  }

  FileContentType StringToContentType(const std::string& str)
  {
    boost::mutex::scoped_lock lock(enumerationsMutex_);
    return dictContentType_.Translate(str);
  }


  FindStorageAccessMode StringToFindStorageAccessMode(const std::string& value)
  {
    if (value == "Always")
    {
      return FindStorageAccessMode_DiskOnLookupAndAnswer;
    }
    else if (value == "Never")
    {
      return FindStorageAccessMode_DatabaseOnly;
    }
    else if (value == "Answers")
    {
      return FindStorageAccessMode_DiskOnAnswer;
    }
    else
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange,
                             "Configuration option \"StorageAccessOnFind\" "
                             "should be \"Always\", \"Never\" or \"Answers\": " + value);
    }    
  }


  BuiltinDecoderTranscoderOrder StringToBuiltinDecoderTranscoderOrder(const std::string& value)
  {
    if (value == "Before")
    {
      return BuiltinDecoderTranscoderOrder_Before;
    }
    else if (value == "After")
    {
      return BuiltinDecoderTranscoderOrder_After;
    }
    else if (value == "Disabled")
    {
      return BuiltinDecoderTranscoderOrder_Disabled;
    }
    else
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange,
                             "Configuration option \"BuiltinDecoderTranscoderOrder\" "
                             "should be \"After\", \"Before\" or \"Disabled\": " + value);
    }    
  }


  Verbosity StringToVerbosity(const std::string& str)
  {
    if (str == "default")
    {
      return Verbosity_Default;
    }
    else if (str == "verbose")
    {
      return Verbosity_Verbose;
    }
    else if (str == "trace")
    {
      return Verbosity_Trace;
    }
    else
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange,
                             "Verbosity can be \"default\", \"verbose\" or \"trace\": " + str);
    }    
  }
  

  std::string GetBasePath(ResourceType type,
                          const std::string& publicId)
  {
    switch (type)
    {
      case ResourceType_Patient:
        return "/patients/" + publicId;

      case ResourceType_Study:
        return "/studies/" + publicId;

      case ResourceType_Series:
        return "/series/" + publicId;

      case ResourceType_Instance:
        return "/instances/" + publicId;
      
      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }

  const char* EnumerationToString(SeriesStatus status)
  {
    switch (status)
    {
      case SeriesStatus_Complete:
        return "Complete";

      case SeriesStatus_Missing:
        return "Missing";

      case SeriesStatus_Inconsistent:
        return "Inconsistent";

      case SeriesStatus_Unknown:
        return "Unknown";

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }

  const char* EnumerationToString(StoreStatus status)
  {
    switch (status)
    {
      case StoreStatus_Success:
        return "Success";

      case StoreStatus_AlreadyStored:
        return "AlreadyStored";

      case StoreStatus_Failure:
        return "Failure";

      case StoreStatus_FilteredOut:
        return "FilteredOut";

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  const char* EnumerationToString(ChangeType type)
  {
    switch (type)
    {
      case ChangeType_CompletedSeries:
        return "CompletedSeries";

      case ChangeType_NewInstance:
        return "NewInstance";

      case ChangeType_NewPatient:
        return "NewPatient";

      case ChangeType_NewSeries:
        return "NewSeries";

      case ChangeType_NewStudy:
        return "NewStudy";

      case ChangeType_AnonymizedStudy:
        return "AnonymizedStudy";

      case ChangeType_AnonymizedSeries:
        return "AnonymizedSeries";

      case ChangeType_ModifiedStudy:
        return "ModifiedStudy";

      case ChangeType_ModifiedSeries:
        return "ModifiedSeries";

      case ChangeType_AnonymizedPatient:
        return "AnonymizedPatient";

      case ChangeType_ModifiedPatient:
        return "ModifiedPatient";

      case ChangeType_StablePatient:
        return "StablePatient";

      case ChangeType_StableStudy:
        return "StableStudy";

      case ChangeType_StableSeries:
        return "StableSeries";

      case ChangeType_Deleted:
        return "Deleted";

      case ChangeType_NewChildInstance:
        return "NewChildInstance";

      case ChangeType_UpdatedAttachment:
        return "UpdatedAttachment";

      case ChangeType_UpdatedMetadata:
        return "UpdatedMetadata";

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  const char* EnumerationToString(Verbosity verbosity)
  {
    switch (verbosity)
    {
      case Verbosity_Default:
        return "default";
        
      case Verbosity_Verbose:
        return "verbose";
        
      case Verbosity_Trace:
        return "trace";

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }    

  
  bool IsUserMetadata(MetadataType metadata)
  {
    return (metadata >= MetadataType_StartUser &&
            metadata <= MetadataType_EndUser);
  }


  void GetTransferSyntaxGroup(std::set<DicomTransferSyntax>& target,
                              TransferSyntaxGroup source)
  {
    target.clear();

    switch (source)
    {    
      // Transfer syntaxes supported since Orthanc 0.7.2
      case TransferSyntaxGroup_Deflated:
        target.insert(DicomTransferSyntax_DeflatedLittleEndianExplicit);
        break;
        
      case TransferSyntaxGroup_Jpeg:
        target.insert(DicomTransferSyntax_JPEGProcess1);
        target.insert(DicomTransferSyntax_JPEGProcess2_4);
        target.insert(DicomTransferSyntax_JPEGProcess3_5);
        target.insert(DicomTransferSyntax_JPEGProcess6_8);
        target.insert(DicomTransferSyntax_JPEGProcess7_9);
        target.insert(DicomTransferSyntax_JPEGProcess10_12);
        target.insert(DicomTransferSyntax_JPEGProcess11_13);
        target.insert(DicomTransferSyntax_JPEGProcess14);
        target.insert(DicomTransferSyntax_JPEGProcess15);
        target.insert(DicomTransferSyntax_JPEGProcess16_18);
        target.insert(DicomTransferSyntax_JPEGProcess17_19);
        target.insert(DicomTransferSyntax_JPEGProcess20_22);
        target.insert(DicomTransferSyntax_JPEGProcess21_23);
        target.insert(DicomTransferSyntax_JPEGProcess24_26);
        target.insert(DicomTransferSyntax_JPEGProcess25_27);
        target.insert(DicomTransferSyntax_JPEGProcess28);
        target.insert(DicomTransferSyntax_JPEGProcess29);
        target.insert(DicomTransferSyntax_JPEGProcess14SV1);
        break;

      case TransferSyntaxGroup_Jpeg2000:
        target.insert(DicomTransferSyntax_JPEG2000);
        target.insert(DicomTransferSyntax_JPEG2000LosslessOnly);
        target.insert(DicomTransferSyntax_JPEG2000Multicomponent);
        target.insert(DicomTransferSyntax_JPEG2000MulticomponentLosslessOnly);
        break;

      case TransferSyntaxGroup_JpegLossless:
        target.insert(DicomTransferSyntax_JPEGLSLossless);
        target.insert(DicomTransferSyntax_JPEGLSLossy);
        break;

      case TransferSyntaxGroup_Jpip:
        target.insert(DicomTransferSyntax_JPIPReferenced);
        target.insert(DicomTransferSyntax_JPIPReferencedDeflate);
        break;

      case TransferSyntaxGroup_Mpeg2:
        target.insert(DicomTransferSyntax_MPEG2MainProfileAtMainLevel);
        target.insert(DicomTransferSyntax_MPEG2MainProfileAtHighLevel);
        break;

      case TransferSyntaxGroup_Rle:
        target.insert(DicomTransferSyntax_RLELossless);
        break;

      case TransferSyntaxGroup_Mpeg4:
        // New in Orthanc 1.6.0
        target.insert(DicomTransferSyntax_MPEG4BDcompatibleHighProfileLevel4_1);
        target.insert(DicomTransferSyntax_MPEG4HighProfileLevel4_1);
        target.insert(DicomTransferSyntax_MPEG4HighProfileLevel4_2_For2DVideo);
        target.insert(DicomTransferSyntax_MPEG4HighProfileLevel4_2_For3DVideo);
        target.insert(DicomTransferSyntax_MPEG4StereoHighProfileLevel4_2);
        break;

      case TransferSyntaxGroup_H265:
        // New in Orthanc 1.9.0
        target.insert(DicomTransferSyntax_HEVCMainProfileLevel5_1);
        target.insert(DicomTransferSyntax_HEVCMain10ProfileLevel5_1);
        break;
        
      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }
}
