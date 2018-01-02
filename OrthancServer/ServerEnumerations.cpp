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


#include "PrecompiledHeadersServer.h"
#include "ServerEnumerations.h"

#include "../Core/OrthancException.h"
#include "../Core/EnumerationDictionary.h"
#include "../Core/Logging.h"
#include "../Core/Toolbox.h"

#include <boost/thread.hpp>

namespace Orthanc
{
  typedef std::map<FileContentType, std::string>  MimeTypes;

  static boost::mutex enumerationsMutex_;
  static Toolbox::EnumerationDictionary<MetadataType> dictMetadataType_;
  static Toolbox::EnumerationDictionary<FileContentType> dictContentType_;
  static MimeTypes  mimeTypes_;

  void InitializeServerEnumerations()
  {
    boost::mutex::scoped_lock lock(enumerationsMutex_);

    dictMetadataType_.Clear();
    dictContentType_.Clear();
    
    dictMetadataType_.Add(MetadataType_Instance_IndexInSeries, "IndexInSeries");
    dictMetadataType_.Add(MetadataType_Instance_ReceptionDate, "ReceptionDate");
    dictMetadataType_.Add(MetadataType_Instance_RemoteAet, "RemoteAET");
    dictMetadataType_.Add(MetadataType_Series_ExpectedNumberOfInstances, "ExpectedNumberOfInstances");
    dictMetadataType_.Add(MetadataType_ModifiedFrom, "ModifiedFrom");
    dictMetadataType_.Add(MetadataType_AnonymizedFrom, "AnonymizedFrom");
    dictMetadataType_.Add(MetadataType_LastUpdate, "LastUpdate");
    dictMetadataType_.Add(MetadataType_Instance_Origin, "Origin");
    dictMetadataType_.Add(MetadataType_Instance_TransferSyntax, "TransferSyntax");
    dictMetadataType_.Add(MetadataType_Instance_SopClassUid, "SopClassUid");

    dictContentType_.Add(FileContentType_Dicom, "dicom");
    dictContentType_.Add(FileContentType_DicomAsJson, "dicom-as-json");
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
        return "application/dicom";

      case FileContentType_DicomAsJson:
        return "application/json";

      default:
        return "application/octet-stream";
    }
  }

  FileContentType StringToContentType(const std::string& str)
  {
    boost::mutex::scoped_lock lock(enumerationsMutex_);
    return dictContentType_.Translate(str);
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

  
  bool IsUserMetadata(MetadataType metadata)
  {
    return (metadata >= MetadataType_StartUser &&
            metadata <= MetadataType_EndUser);
  }
}
