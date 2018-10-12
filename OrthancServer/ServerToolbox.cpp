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
#include "ServerToolbox.h"

#include "../Core/DicomFormat/DicomArray.h"
#include "../Core/FileStorage/StorageAccessor.h"
#include "../Core/Logging.h"
#include "../Core/OrthancException.h"

#include <cassert>

namespace Orthanc
{
  namespace ServerToolbox
  {
    static const DicomTag patientIdentifiers[] = 
    {
      DICOM_TAG_PATIENT_ID,
      DICOM_TAG_PATIENT_NAME,
      DICOM_TAG_PATIENT_BIRTH_DATE
    };

    static const DicomTag studyIdentifiers[] = 
    {
      DICOM_TAG_PATIENT_ID,
      DICOM_TAG_PATIENT_NAME,
      DICOM_TAG_PATIENT_BIRTH_DATE,
      DICOM_TAG_STUDY_INSTANCE_UID,
      DICOM_TAG_ACCESSION_NUMBER,
      DICOM_TAG_STUDY_DESCRIPTION,
      DICOM_TAG_STUDY_DATE
    };

    static const DicomTag seriesIdentifiers[] = 
    {
      DICOM_TAG_SERIES_INSTANCE_UID
    };

    static const DicomTag instanceIdentifiers[] = 
    {
      DICOM_TAG_SOP_INSTANCE_UID
    };


    void SimplifyTags(Json::Value& target,
                      const Json::Value& source,
                      DicomToJsonFormat format)
    {
      assert(source.isObject());

      target = Json::objectValue;
      Json::Value::Members members = source.getMemberNames();

      for (size_t i = 0; i < members.size(); i++)
      {
        const Json::Value& v = source[members[i]];
        const std::string& type = v["Type"].asString();

        std::string name;
        switch (format)
        {
          case DicomToJsonFormat_Human:
            name = v["Name"].asString();
            break;

          case DicomToJsonFormat_Short:
            name = members[i];
            break;

          default:
            throw OrthancException(ErrorCode_ParameterOutOfRange);
        }

        if (type == "String")
        {
          target[name] = v["Value"].asString();
        }
        else if (type == "TooLong" ||
                 type == "Null")
        {
          target[name] = Json::nullValue;
        }
        else if (type == "Sequence")
        {
          const Json::Value& array = v["Value"];
          assert(array.isArray());

          Json::Value children = Json::arrayValue;
          for (Json::Value::ArrayIndex i = 0; i < array.size(); i++)
          {
            Json::Value c;
            SimplifyTags(c, array[i], format);
            children.append(c);
          }

          target[name] = children;
        }
        else
        {
          assert(0);
        }
      }
    }


    static void StoreMainDicomTagsInternal(IDatabaseWrapper& database,
                                           int64_t resource,
                                           const DicomMap& tags)
    {
      DicomArray flattened(tags);

      for (size_t i = 0; i < flattened.GetSize(); i++)
      {
        const DicomElement& element = flattened.GetElement(i);
        const DicomTag& tag = element.GetTag();
        const DicomValue& value = element.GetValue();
        if (!value.IsNull() && 
            !value.IsBinary())
        {
          database.SetMainDicomTag(resource, tag, element.GetValue().GetContent());
        }
      }
    }


    static void StoreIdentifiers(IDatabaseWrapper& database,
                                 int64_t resource,
                                 ResourceType level,
                                 const DicomMap& map)
    {
      const DicomTag* tags;
      size_t size;

      LoadIdentifiers(tags, size, level);

      for (size_t i = 0; i < size; i++)
      {
        const DicomValue* value = map.TestAndGetValue(tags[i]);
        if (value != NULL &&
            !value->IsNull() &&
            !value->IsBinary())
        {
          std::string s = NormalizeIdentifier(value->GetContent());
          database.SetIdentifierTag(resource, tags[i], s);
        }
      }
    }


    void StoreMainDicomTags(IDatabaseWrapper& database,
                            int64_t resource,
                            ResourceType level,
                            const DicomMap& dicomSummary)
    {
      // WARNING: The database should be locked with a transaction!

      StoreIdentifiers(database, resource, level, dicomSummary);

      DicomMap tags;

      switch (level)
      {
        case ResourceType_Patient:
          dicomSummary.ExtractPatientInformation(tags);
          break;

        case ResourceType_Study:
          // Duplicate the patient tags at the study level (new in Orthanc 0.9.5 - db v6)
          dicomSummary.ExtractPatientInformation(tags);
          StoreMainDicomTagsInternal(database, resource, tags);

          dicomSummary.ExtractStudyInformation(tags);
          break;

        case ResourceType_Series:
          dicomSummary.ExtractSeriesInformation(tags);
          break;

        case ResourceType_Instance:
          dicomSummary.ExtractInstanceInformation(tags);
          break;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }

      StoreMainDicomTagsInternal(database, resource, tags);
    }


    bool FindOneChildInstance(int64_t& result,
                              IDatabaseWrapper& database,
                              int64_t resource,
                              ResourceType type)
    {
      for (;;)
      {
        if (type == ResourceType_Instance)
        {
          result = resource;
          return true;
        }

        std::list<int64_t> children;
        database.GetChildrenInternalId(children, resource);
        if (children.empty())
        {
          return false;
        }

        resource = children.front();
        type = GetChildResourceType(type);    
      }
    }


    void ReconstructMainDicomTags(IDatabaseWrapper& database,
                                  IStorageArea& storageArea,
                                  ResourceType level)
    {
      // WARNING: The database should be locked with a transaction!

      // TODO: This function might consume much memory if level ==
      // ResourceType_Instance. To improve this, first download the
      // list of studies, then remove the instances for each single
      // study (check out OrthancRestApi::InvalidateTags for an
      // example). Take this improvement into consideration for the
      // next upgrade of the database schema.

      const char* plural = NULL;

      switch (level)
      {
        case ResourceType_Patient:
          plural = "patients";
          break;

        case ResourceType_Study:
          plural = "studies";
          break;

        case ResourceType_Series:
          plural = "series";
          break;

        case ResourceType_Instance:
          plural = "instances";
          break;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }

      LOG(WARNING) << "Upgrade: Reconstructing the main DICOM tags of all the " << plural << "...";

      std::list<std::string> resources;
      database.GetAllPublicIds(resources, level);

      for (std::list<std::string>::const_iterator
             it = resources.begin(); it != resources.end(); ++it)
      {
        // Locate the resource and one of its child instances
        int64_t resource, instance;
        ResourceType tmp;

        if (!database.LookupResource(resource, tmp, *it) ||
            tmp != level ||
            !FindOneChildInstance(instance, database, resource, level))
        {
          LOG(ERROR) << "Cannot find an instance for " << EnumerationToString(level) 
                     << " with identifier " << *it;
          throw OrthancException(ErrorCode_InternalError);
        }

        // Get the DICOM file attached to some instances in the resource
        FileInfo attachment;
        if (!database.LookupAttachment(attachment, instance, FileContentType_Dicom))
        {
          LOG(ERROR) << "Cannot retrieve the DICOM file associated with instance " << database.GetPublicId(instance);
          throw OrthancException(ErrorCode_InternalError);
        }

        try
        {
          // Read and parse the content of the DICOM file
          StorageAccessor accessor(storageArea);

          std::string content;
          accessor.Read(content, attachment);

          ParsedDicomFile dicom(content);

          // Update the tags of this resource
          DicomMap dicomSummary;
          dicom.ExtractDicomSummary(dicomSummary);

          database.ClearMainDicomTags(resource);
          StoreMainDicomTags(database, resource, level, dicomSummary);
        }
        catch (OrthancException&)
        {
          LOG(ERROR) << "Cannot decode the DICOM file with UUID " << attachment.GetUuid()
                     << " associated with instance " << database.GetPublicId(instance);
          throw;
        }
      }
    }


    void LoadIdentifiers(const DicomTag*& tags,
                         size_t& size,
                         ResourceType level)
    {
      switch (level)
      {
        case ResourceType_Patient:
          tags = patientIdentifiers;
          size = sizeof(patientIdentifiers) / sizeof(DicomTag);
          break;

        case ResourceType_Study:
          tags = studyIdentifiers;
          size = sizeof(studyIdentifiers) / sizeof(DicomTag);
          break;

        case ResourceType_Series:
          tags = seriesIdentifiers;
          size = sizeof(seriesIdentifiers) / sizeof(DicomTag);
          break;

        case ResourceType_Instance:
          tags = instanceIdentifiers;
          size = sizeof(instanceIdentifiers) / sizeof(DicomTag);
          break;

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
    }


    std::string NormalizeIdentifier(const std::string& value)
    {
      std::string t;
      t.reserve(value.size());

      for (size_t i = 0; i < value.size(); i++)
      {
        if (value[i] == '%' ||
            value[i] == '_')
        {
          t.push_back(' ');  // These characters might break wildcard queries in SQL
        }
        else if (isascii(value[i]) &&
                 !iscntrl(value[i]) &&
                 (!isspace(value[i]) || value[i] == ' '))
        {
          t.push_back(value[i]);
        }
      }

      Toolbox::ToUpperCase(t);

      return Toolbox::StripSpaces(t);
    }


    bool IsIdentifier(const DicomTag& tag,
                      ResourceType level)
    {
      const DicomTag* tags;
      size_t size;

      LoadIdentifiers(tags, size, level);

      for (size_t i = 0; i < size; i++)
      {
        if (tag == tags[i])
        {
          return true;
        }
      }

      return false;
    }

    
    void ReconstructResource(ServerContext& context,
                             const std::string& resource)
    {
      LOG(WARNING) << "Reconstructing resource " << resource;
      
      std::list<std::string> instances;
      context.GetIndex().GetChildInstances(instances, resource);

      for (std::list<std::string>::const_iterator 
             it = instances.begin(); it != instances.end(); ++it)
      {
        ServerContext::DicomCacheLocker locker(context, *it);

        Json::Value dicomAsJson;
        locker.GetDicom().DatasetToJson(dicomAsJson);

        std::string s = dicomAsJson.toStyledString();
        context.AddAttachment(*it, FileContentType_DicomAsJson, s.c_str(), s.size());

        context.GetIndex().ReconstructInstance(locker.GetDicom());
      }
    }
  }
}
