/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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


  bool MainDicomTagsRegistry::NormalizeLookup(bool& canBeFullyPerformedInDb,
                                              DatabaseDicomTagConstraints& target,
                                              const DatabaseLookup& source,
                                              ResourceType queryLevel,
                                              bool allowChildrenExistsQueries) const
  {
    bool isEquivalentLookup = true;
    canBeFullyPerformedInDb = true;

    target.Clear();

    for (size_t i = 0; i < source.GetConstraintsCount(); i++)
    {
      ResourceType level;
      DicomTagType type;
      const DicomTag& tag = source.GetConstraint(i).GetTag();

      LookupTag(level, type, tag);


      if (type == DicomTagType_Identifier ||
          type == DicomTagType_Main)
      {
        // Use the fact that patient-level tags are copied at the study level
        if (level == ResourceType_Patient &&
            queryLevel != ResourceType_Patient)
        {
          level = ResourceType_Study;
        }

        bool isEquivalentConstraint;
        
        // DicomIdentifiers are stored UPPERCASE -> as soon as a case senstive search happens, it is currently not possible to perform it in DB only
        if (type == DicomTagType_Identifier && source.GetConstraint(i).IsCaseSensitive())
        {
          canBeFullyPerformedInDb = false;
        }

        target.AddConstraint(source.GetConstraint(i).ConvertToDatabaseConstraint(isEquivalentConstraint, level, type));

        if (!isEquivalentConstraint)
        {
          isEquivalentLookup = false;
        }
      }
      else
      {
        if (allowChildrenExistsQueries &&
            tag == DICOM_TAG_MODALITIES_IN_STUDY && queryLevel == ResourceType_Study)
        {
          // transform the query into a children query
          std::vector<std::string> values(source.GetConstraint(i).GetValues().begin(), source.GetConstraint(i).GetValues().end());

          std::unique_ptr<DatabaseDicomTagConstraint> constraint(new DatabaseDicomTagConstraint(ResourceType_Series,
                                                                 DICOM_TAG_MODALITY,
                                                                 false,
                                                                 ConstraintType_List,
                                                                 values,
                                                                 false,
                                                                 true));
          target.AddConstraint(constraint.release());
        }
        else
        {
          isEquivalentLookup = false;
          canBeFullyPerformedInDb = false;
        }
      }
    }

    return isEquivalentLookup;
  }
}
