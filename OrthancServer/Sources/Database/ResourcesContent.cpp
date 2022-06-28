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


#include "../PrecompiledHeadersServer.h"
#include "ResourcesContent.h"

#include "Compatibility/ISetResourcesContent.h"
#include "../ServerToolbox.h"

#include "../../../OrthancFramework/Sources/DicomFormat/DicomArray.h"
#include "../../../OrthancFramework/Sources/OrthancException.h"
#include "../../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"

#include <cassert>


namespace Orthanc
{
  static void StoreMainDicomTagsInternal(ResourcesContent& target,
                                         int64_t resource,
                                         const DicomMap& tags)
  {
    DicomArray flattened(tags);

    for (size_t i = 0; i < flattened.GetSize(); i++)
    {
      const DicomElement& element = flattened.GetElement(i);
      const DicomTag& tag = element.GetTag();
      const DicomValue& value = element.GetValue();
      if (value.IsString())
      {
        target.AddMainDicomTag(resource, tag, element.GetValue().GetContent());
      }
    }
  }


  static void StoreIdentifiers(ResourcesContent& target,
                               int64_t resource,
                               ResourceType level,
                               const DicomMap& map)
  {
    const DicomTag* tags;
    size_t size;

    ServerToolbox::LoadIdentifiers(tags, size, level);

    for (size_t i = 0; i < size; i++)
    {
      // The identifiers tags are a subset of the main DICOM tags
      assert(DicomMap::IsMainDicomTag(tags[i]));
        
      const DicomValue* value = map.TestAndGetValue(tags[i]);
      if (value != NULL && value->IsString())
      {
        std::string s = ServerToolbox::NormalizeIdentifier(value->GetContent());
        target.AddIdentifierTag(resource, tags[i], s);
      }
    }
  }


  void ResourcesContent::AddMetadata(int64_t resourceId,
                                     MetadataType metadata,
                                     const std::string& value)
  {
    if (isNewResource_)
    {
      metadata_.push_back(Metadata(resourceId, metadata, value));
    }
    else
    {
      // This would require to handle the incrementation of revision
      // numbers in the database backend => only allow setting
      // metadata on new resources
      throw OrthancException(ErrorCode_NotImplemented);
    }
  }


  void ResourcesContent::AddResource(int64_t resource,
                                     ResourceType level,
                                     const DicomMap& dicomSummary)
  {
    StoreIdentifiers(*this, resource, level, dicomSummary);

    DicomMap tags;

    switch (level)
    {
      case ResourceType_Patient:
        dicomSummary.ExtractPatientInformation(tags);
        break;

      case ResourceType_Study:
        // Duplicate the patient tags at the study level (new in Orthanc 0.9.5 - db v6)
        dicomSummary.ExtractPatientInformation(tags);
        StoreMainDicomTagsInternal(*this, resource, tags);

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

    StoreMainDicomTagsInternal(*this, resource, tags);  // saves only leaf tags, not sequences
  }


  void ResourcesContent::Store(Compatibility::ISetResourcesContent& compatibility) const
  {
    for (std::list<TagValue>::const_iterator
           it = tags_.begin(); it != tags_.end(); ++it)
    {
      if (it->isIdentifier_)
      {
        compatibility.SetIdentifierTag(it->resourceId_, it->tag_,  it->value_);
      }
      else
      {
        compatibility.SetMainDicomTag(it->resourceId_, it->tag_,  it->value_);
      }
    }

    for (std::list<Metadata>::const_iterator
           it = metadata_.begin(); it != metadata_.end(); ++it)
    {
      assert(isNewResource_);
      compatibility.SetMetadata(it->resourceId_, it->metadata_,  it->value_, 0 /* initial revision number */);
    }
  }
}
