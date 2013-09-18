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

#include "ServerEnumerations.h"

#include "../Core/OrthancException.h"
#include "../Core/EnumerationDictionary.h"
#include "../Core/Toolbox.h"

#include <boost/thread.hpp>

namespace Orthanc
{
  static boost::mutex enumerationsMutex_;
  static Toolbox::EnumerationDictionary<MetadataType> dictMetadataType_;

  void InitializeServerEnumerations()
  {
    boost::mutex::scoped_lock lock(enumerationsMutex_);
    
    dictMetadataType_.Add(MetadataType_Instance_IndexInSeries, "IndexInSeries");
    dictMetadataType_.Add(MetadataType_Instance_ReceptionDate, "ReceptionDate");
    dictMetadataType_.Add(MetadataType_Instance_RemoteAet, "RemoteAET");
    dictMetadataType_.Add(MetadataType_Series_ExpectedNumberOfInstances, "ExpectedNumberOfInstances");
    dictMetadataType_.Add(MetadataType_ModifiedFrom, "ModifiedFrom");
    dictMetadataType_.Add(MetadataType_AnonymizedFrom, "AnonymizedFrom");
    dictMetadataType_.Add(MetadataType_LastUpdate, "LastUpdate");
  }

  void RegisterUserMetadata(int metadata,
                            const std::string name)
  {
    boost::mutex::scoped_lock lock(enumerationsMutex_);

    if (metadata < static_cast<int>(MetadataType_StartUser) ||
        metadata > static_cast<int>(MetadataType_EndUser))
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }

    dictMetadataType_.Add(static_cast<MetadataType>(metadata), name);
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

  const char* EnumerationToString(ResourceType type)
  {
    switch (type)
    {
      case ResourceType_Patient:
        return "Patient";

      case ResourceType_Study:
        return "Study";

      case ResourceType_Series:
        return "Series";

      case ResourceType_Instance:
        return "Instance";
      
      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
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

      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  ResourceType GetParentResourceType(ResourceType type)
  {
    switch (type)
    {
      case ResourceType_Study:
        return ResourceType_Patient;

      case ResourceType_Series:
        return ResourceType_Study;

      case ResourceType_Instance:
        return ResourceType_Series;
      
      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  ResourceType GetChildResourceType(ResourceType type)
  {
    switch (type)
    {
      case ResourceType_Patient:
        return ResourceType_Study;

      case ResourceType_Study:
        return ResourceType_Series;

      case ResourceType_Series:
        return ResourceType_Instance;
      
      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  const char* EnumerationToString(ModalityManufacturer manufacturer)
  {
    switch (manufacturer)
    {
      case ModalityManufacturer_Generic:
        return "Generic";

      case ModalityManufacturer_ClearCanvas:
        return "ClearCanvas";
      
      default:
        throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  ModalityManufacturer StringToModalityManufacturer(const std::string& manufacturer)
  {
    if (manufacturer == "Generic")
    {
      return ModalityManufacturer_Generic;
    }
    else if (manufacturer == "ClearCanvas")
    {
      return ModalityManufacturer_ClearCanvas;
    }
    else
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }


  ResourceType StringToResourceType(const char* type)
  {
    std::string s(type);
    Toolbox::ToUpperCase(s);

    if (s == "PATIENT")
    {
      return ResourceType_Patient;
    }
    else if (s == "STUDY")
    {
      return ResourceType_Study;
    }
    else if (s == "SERIES")
    {
      return ResourceType_Series;
    }
    else if (s == "INSTANCE" || s == "IMAGE")
    {
      return ResourceType_Instance;
    }
    else
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }
  }
}
