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


#include "../PrecompiledHeadersServer.h"
#include "MainDicomTagsRegistry.h"

#include "../ServerToolbox.h"

namespace Orthanc
{
  void MainDicomTagsRegistry::LoadTags(ResourceType level)
  {
    {
      const DicomTag* tags = NULL;
      size_t size;

      ServerToolbox::LoadIdentifiers(tags, size, level);

      for (size_t i = 0; i < size; i++)
      {
        if (registry_.find(tags[i]) == registry_.end())
        {
          registry_[tags[i]] = TagInfo(level, DicomTagType_Identifier);
        }
        else
        {
          // These patient-level tags are copied in the study level
          assert(level == ResourceType_Study &&
                 (tags[i] == DICOM_TAG_PATIENT_ID ||
                  tags[i] == DICOM_TAG_PATIENT_NAME ||
                  tags[i] == DICOM_TAG_PATIENT_BIRTH_DATE));
        }
      }
    }

    {
      std::set<DicomTag> tags;
      DicomMap::GetMainDicomTags(tags, level);

      for (std::set<DicomTag>::const_iterator
             tag = tags.begin(); tag != tags.end(); ++tag)
      {
        if (registry_.find(*tag) == registry_.end())
        {
          registry_[*tag] = TagInfo(level, DicomTagType_Main);
        }
      }
    }
  }


  MainDicomTagsRegistry::MainDicomTagsRegistry()
  {
    LoadTags(ResourceType_Patient);
    LoadTags(ResourceType_Study);
    LoadTags(ResourceType_Series);
    LoadTags(ResourceType_Instance);
  }


  void MainDicomTagsRegistry::LookupTag(ResourceType& level,
                                        DicomTagType& type,
                                        const DicomTag& tag) const
  {
    Registry::const_iterator it = registry_.find(tag);

    if (it == registry_.end())
    {
      // Default values
      level = ResourceType_Instance;
      type = DicomTagType_Generic;
    }
    else
    {
      level = it->second.GetLevel();
      type = it->second.GetType();
    }
  }
}
