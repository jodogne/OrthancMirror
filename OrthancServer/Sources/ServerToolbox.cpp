/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2024 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2024 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
#include "ServerToolbox.h"

#include "../../OrthancFramework/Sources/DicomParsing/ParsedDicomFile.h"
#include "../../OrthancFramework/Sources/FileStorage/StorageAccessor.h"
#include "../../OrthancFramework/Sources/FileStorage/StorageCache.h"
#include "../../OrthancFramework/Sources/Logging.h"
#include "../../OrthancFramework/Sources/OrthancException.h"
#include "Database/IDatabaseWrapper.h"
#include "Database/ResourcesContent.h"
#include "OrthancConfiguration.h"
#include "ServerContext.h"

#include <cassert>

namespace Orthanc
{
  static const DicomTag PATIENT_IDENTIFIERS[] = 
  {
    DICOM_TAG_PATIENT_ID,
    DICOM_TAG_PATIENT_NAME,
    DICOM_TAG_PATIENT_BIRTH_DATE
  };

  static const DicomTag STUDY_IDENTIFIERS[] = 
  {
    DICOM_TAG_PATIENT_ID,
    DICOM_TAG_PATIENT_NAME,
    DICOM_TAG_PATIENT_BIRTH_DATE,
    DICOM_TAG_STUDY_INSTANCE_UID,
    DICOM_TAG_ACCESSION_NUMBER,
    DICOM_TAG_STUDY_DESCRIPTION,
    DICOM_TAG_STUDY_DATE
  };

  static const DicomTag SERIES_IDENTIFIERS[] = 
  {
    DICOM_TAG_SERIES_INSTANCE_UID
  };

  static const DicomTag INSTANCE_IDENTIFIERS[] = 
  {
    DICOM_TAG_SOP_INSTANCE_UID
  };


  namespace ServerToolbox
  {
    bool FindOneChildInstance(int64_t& result,
                              IDatabaseWrapper::ITransaction& transaction,
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
        transaction.GetChildrenInternalId(children, resource);
        if (children.empty())
        {
          return false;
        }

        resource = children.front();
        type = GetChildResourceType(type);    
      }
    }


    void ReconstructMainDicomTags(IDatabaseWrapper::ITransaction& transaction,
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

      const char* plural = Orthanc::GetResourceTypeText(level, true, true);

      LOG(WARNING) << "Upgrade: Reconstructing the main DICOM tags of all the " << plural << "...";

      std::list<std::string> resources;
      transaction.GetAllPublicIds(resources, level);

      for (std::list<std::string>::const_iterator
             it = resources.begin(); it != resources.end(); ++it)
      {
        // Locate the resource and one of its child instances
        int64_t resource, instance;
        ResourceType tmp;

        if (!transaction.LookupResource(resource, tmp, *it) ||
            tmp != level ||
            !FindOneChildInstance(instance, transaction, resource, level))
        {
          throw OrthancException(ErrorCode_InternalError,
                                 "Cannot find an instance for " +
                                 std::string(EnumerationToString(level)) +
                                 " with identifier " + *it);
        }

        // Get the DICOM file attached to some instances in the resource
        FileInfo attachment;
        int64_t revision;
        if (!transaction.LookupAttachment(attachment, revision, instance, FileContentType_Dicom))
        {
          throw OrthancException(ErrorCode_InternalError,
                                 "Cannot retrieve the DICOM file associated with instance " +
                                 transaction.GetPublicId(instance));
        }

        try
        {
          // Read and parse the content of the DICOM file
          StorageAccessor accessor(storageArea);  // no cache

          std::string content;
          accessor.Read(content, attachment);

          ParsedDicomFile dicom(content);

          // Update the tags of this resource
          DicomMap dicomSummary;
          OrthancConfiguration::DefaultExtractDicomSummary(dicomSummary, dicom);

          transaction.ClearMainDicomTags(resource);

          ResourcesContent tags(false /* prevent the setting of metadata */);
          tags.AddResource(resource, level, dicomSummary);
          transaction.SetResourcesContent(tags);

          transaction.DeleteMetadata(resource, MetadataType_MainDicomTagsSignature);
          transaction.SetMetadata(resource, MetadataType_MainDicomTagsSignature, DicomMap::GetMainDicomTagsSignature(level), 0);
        }
        catch (OrthancException&)
        {
          LOG(ERROR) << "Cannot decode the DICOM file with UUID " << attachment.GetUuid()
                     << " associated with instance " << transaction.GetPublicId(instance);
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
          tags = PATIENT_IDENTIFIERS;
          size = sizeof(PATIENT_IDENTIFIERS) / sizeof(DicomTag);
          break;

        case ResourceType_Study:
          tags = STUDY_IDENTIFIERS;
          size = sizeof(STUDY_IDENTIFIERS) / sizeof(DicomTag);
          break;

        case ResourceType_Series:
          tags = SERIES_IDENTIFIERS;
          size = sizeof(SERIES_IDENTIFIERS) / sizeof(DicomTag);
          break;

        case ResourceType_Instance:
          tags = INSTANCE_IDENTIFIERS;
          size = sizeof(INSTANCE_IDENTIFIERS) / sizeof(DicomTag);
          break;

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
    }


    std::string NormalizeIdentifier(const std::string& value)
    {
      std::string t;
      t.reserve(value.size());

#if 0
      // This version solves some indexing issue (https://discourse.orthanc-server.org/t/postgress-index-effectively-disabled-when-searching-for-greek-names/3371)
      // and seems functional: I could run the integration tests with both SQLite and PG + the DicomWeb tests with PG.
      // However, it can not go into production because NormalizeIdentifier is used both at ingest time and at search time;
      // therefore, if we change it while we have an already populated DB, the searches won't work anymore and, on very large
      // systems, running the Housekeeper to rebuild the indexes might take months ...
      // We keep it here because it might be handy once we refactor the DicomIdentifier searches in the future.
      for (size_t i = 0; i < value.size(); i++)
      {
        if (value[i] == '%' ||
            value[i] == '_')
        {
          t.push_back(' ');  // These characters might break wildcard queries in SQL
        }
        else if (//isascii(value[i]) &&
                 !iscntrl(value[i]) &&
                 (!isspace(value[i]) || value[i] == ' '))
        {
          t.push_back(value[i]);
        }
      }

      //Toolbox::ToUpperCase(t);
      t = Toolbox::ToUpperCaseWithAccents(t);
#else
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
#endif

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
                             const std::string& resource,
                             bool reconstructFiles,
                             bool limitToThisLevelDicomTags,
                             ResourceType limitToLevel)
    {
      LOG(WARNING) << "Reconstructing resource " << resource;
      
      std::list<std::string> instances;
      context.GetIndex().GetChildInstances(instances, resource);


      if (limitToThisLevelDicomTags && instances.size() > 0) // in this case, we only need to rebuild one instance !
      {
        ServerContext::DicomCacheLocker locker(context, instances.front());
        context.GetIndex().ReconstructInstance(locker.GetDicom(), true, limitToLevel);
      }
      else
      {
        for (std::list<std::string>::const_iterator 
              it = instances.begin(); it != instances.end(); ++it)
        {
          ServerContext::DicomCacheLocker locker(context, *it);

          // Delay the reconstruction of DICOM-as-JSON to its next access through "ServerContext"
          context.GetIndex().DeleteAttachment(*it, FileContentType_DicomAsJson, false /* no revision */,
                                              -1 /* dummy revision */, "" /* dummy MD5 */);
          
          context.GetIndex().ReconstructInstance(locker.GetDicom(), false, ResourceType_Instance /* dummy */);

          if (reconstructFiles)
          {
            std::string resultPublicId;  // ignored
            std::unique_ptr<DicomInstanceToStore> dicomInstancetoStore(DicomInstanceToStore::CreateFromParsedDicomFile(locker.GetDicom()));

            // TODO: TranscodeAndStore and specifically ServerIndex::Store have been "poluted" by the isReconstruct parameter
            // we should very likely refactor it
            context.TranscodeAndStore(resultPublicId, dicomInstancetoStore.get(), StoreInstanceMode_OverwriteDuplicate, true);
          }
        }
      }
    }

    
    bool IsValidLabel(const std::string& label)
    {
      if (label.empty())
      {
        return false;
      }

      if (label.size() > 64)
      {
        // This limitation is for MySQL, which cannot use a TEXT
        // column of undefined length as a primary key
        return false;
      }
      
      for (size_t i = 0; i < label.size(); i++)
      {
        if (!(label[i] == '_' ||
              label[i] == '-' ||
              (label[i] >= 'a' && label[i] <= 'z') ||
              (label[i] >= 'A' && label[i] <= 'Z') ||
              (label[i] >= '0' && label[i] <= '9')))
        {
          return false;
        }
      }

      return true;
    }


    void CheckValidLabel(const std::string& label)
    {
      if (!IsValidLabel(label))
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange,
                               "A label must be a non-empty, alphanumeric string, "
                               "possibly with '_' or '-' characters, "
                               "with maximum 64 characters, but got: " + label);
      }
    }
  }
}
