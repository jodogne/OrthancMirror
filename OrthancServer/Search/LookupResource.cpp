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


#include "../PrecompiledHeadersServer.h"
#include "LookupResource.h"

#include "../../Core/OrthancException.h"
#include "../../Core/FileStorage/StorageAccessor.h"
#include "../ServerToolbox.h"
#include "../../Core/DicomParsing/FromDcmtkBridge.h"


namespace Orthanc
{
  LookupResource::Level::Level(ResourceType level) : level_(level)
  {
    const DicomTag* tags = NULL;
    size_t size;
    
    ServerToolbox::LoadIdentifiers(tags, size, level);
    
    for (size_t i = 0; i < size; i++)
    {
      identifiers_.insert(tags[i]);
    }
    
    DicomMap::LoadMainDicomTags(tags, size, level);
    
    for (size_t i = 0; i < size; i++)
    {
      if (identifiers_.find(tags[i]) == identifiers_.end())
      {
        mainTags_.insert(tags[i]);
      }
    }    
  }

  LookupResource::Level::~Level()
  {
    for (Constraints::iterator it = mainTagsConstraints_.begin();
         it != mainTagsConstraints_.end(); ++it)
    {
      delete it->second;
    }

    for (Constraints::iterator it = identifiersConstraints_.begin();
         it != identifiersConstraints_.end(); ++it)
    {
      delete it->second;
    }
  }

  bool LookupResource::Level::Add(const DicomTag& tag,
                                  std::auto_ptr<IFindConstraint>& constraint)
  {
    if (identifiers_.find(tag) != identifiers_.end())
    {
      if (level_ == ResourceType_Patient)
      {
        // The filters on the patient level must be cloned to the study level
        identifiersConstraints_[tag] = constraint->Clone();
      }
      else
      {
        identifiersConstraints_[tag] = constraint.release();
      }

      return true;
    }
    else if (mainTags_.find(tag) != mainTags_.end())
    {
      if (level_ == ResourceType_Patient)
      {
        // The filters on the patient level must be cloned to the study level
        mainTagsConstraints_[tag] = constraint->Clone();
      }
      else
      {
        mainTagsConstraints_[tag] = constraint.release();
      }

      return true;
    }
    else
    {
      return false;
    }
  }


  LookupResource::LookupResource(ResourceType level) : level_(level)
  {
    switch (level)
    {
      case ResourceType_Patient:
        levels_[ResourceType_Patient] = new Level(ResourceType_Patient);
        break;

      case ResourceType_Instance:
        levels_[ResourceType_Instance] = new Level(ResourceType_Instance);
        // Do not add "break" here

      case ResourceType_Series:
        levels_[ResourceType_Series] = new Level(ResourceType_Series);
        // Do not add "break" here

      case ResourceType_Study:
        levels_[ResourceType_Study] = new Level(ResourceType_Study);
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }
  }


  LookupResource::~LookupResource()
  {
    for (Levels::iterator it = levels_.begin();
         it != levels_.end(); ++it)
    {
      delete it->second;
    }

    for (Constraints::iterator it = unoptimizedConstraints_.begin();
         it != unoptimizedConstraints_.end(); ++it)
    {
      delete it->second;
    }    
  }



  bool LookupResource::AddInternal(ResourceType level,
                                   const DicomTag& tag,
                                   std::auto_ptr<IFindConstraint>& constraint)
  {
    Levels::iterator it = levels_.find(level);
    if (it != levels_.end())
    {
      if (it->second->Add(tag, constraint))
      {
        return true;
      }
    }

    return false;
  }


  void LookupResource::Add(const DicomTag& tag,
                           IFindConstraint* constraint)
  {
    std::auto_ptr<IFindConstraint> c(constraint);

    if (!AddInternal(ResourceType_Patient, tag, c) &&
        !AddInternal(ResourceType_Study, tag, c) &&
        !AddInternal(ResourceType_Series, tag, c) &&
        !AddInternal(ResourceType_Instance, tag, c))
    {
      unoptimizedConstraints_[tag] = c.release();
    }
  }


  static bool Match(const DicomMap& tags,
                    const DicomTag& tag,
                    const IFindConstraint& constraint)
  {
    const DicomValue* value = tags.TestAndGetValue(tag);

    if (value == NULL ||
        value->IsNull() ||
        value->IsBinary())
    {
      return false;
    }
    else
    {
      return constraint.Match(value->GetContent());
    }
  }


  void LookupResource::Level::Apply(SetOfResources& candidates,
                                    IDatabaseWrapper& database) const
  {
    // First, use the indexed identifiers
    LookupIdentifierQuery query(level_);

    for (Constraints::const_iterator it = identifiersConstraints_.begin(); 
         it != identifiersConstraints_.end(); ++it)
    {
      it->second->Setup(query, it->first);
    }

    query.Apply(candidates, database);

    /*{
      query.Print(std::cout);
      std::list<int64_t>  source;
      candidates.Flatten(source);
      printf("=> %d\n", source.size());
      }*/

    // Secondly, filter using the main DICOM tags
    if (!identifiersConstraints_.empty() ||
        !mainTagsConstraints_.empty())
    {
      std::list<int64_t>  source;
      candidates.Flatten(source);
      candidates.Clear();

      std::list<int64_t>  filtered;
      for (std::list<int64_t>::const_iterator candidate = source.begin(); 
           candidate != source.end(); ++candidate)
      {
        DicomMap tags;
        database.GetMainDicomTags(tags, *candidate);

        bool match = true;

        // Re-apply the identifier constraints, as their "Setup"
        // method is less restrictive than their "Match" method
        for (Constraints::const_iterator it = identifiersConstraints_.begin(); 
             match && it != identifiersConstraints_.end(); ++it)
        {
          if (!Match(tags, it->first, *it->second))
          {
            match = false;
          }
        }

        for (Constraints::const_iterator it = mainTagsConstraints_.begin(); 
             match && it != mainTagsConstraints_.end(); ++it)
        {
          if (!Match(tags, it->first, *it->second))
          {
            match = false;
          }
        }

        if (match)
        {
          filtered.push_back(*candidate);
        }
      }
      
      candidates.Intersect(filtered);
    }
  }



  bool LookupResource::IsMatch(const Json::Value& dicomAsJson) const
  {
    for (Constraints::const_iterator it = unoptimizedConstraints_.begin(); 
         it != unoptimizedConstraints_.end(); ++it)
    {
      std::string tag = it->first.Format();
      if (dicomAsJson.isMember(tag) &&
          dicomAsJson[tag]["Type"] == "String")
      {
        std::string value = dicomAsJson[tag]["Value"].asString();
        if (!it->second->Match(value))
        {
          return false;
        }
      }
      else
      {
        return false;
      }
    }

    return true;
  }


  void LookupResource::ApplyLevel(SetOfResources& candidates,
                                  ResourceType level,
                                  IDatabaseWrapper& database) const
  {
    Levels::const_iterator it = levels_.find(level);
    if (it != levels_.end())
    {
      it->second->Apply(candidates, database);
    }

    if (level == ResourceType_Study &&
        modalitiesInStudy_.get() != NULL)
    {
      // There is a constraint on the "ModalitiesInStudy" DICOM
      // extension. Check out whether one child series has one of the
      // allowed modalities
      std::list<int64_t> allStudies, matchingStudies;
      candidates.Flatten(allStudies);
 
      for (std::list<int64_t>::const_iterator
             study = allStudies.begin(); study != allStudies.end(); ++study)
      {
        std::list<int64_t> childrenSeries;
        database.GetChildrenInternalId(childrenSeries, *study);

        for (std::list<int64_t>::const_iterator
               series = childrenSeries.begin(); series != childrenSeries.end(); ++series)
        {
          DicomMap tags;
          database.GetMainDicomTags(tags, *series);

          const DicomValue* value = tags.TestAndGetValue(DICOM_TAG_MODALITY);
          if (value != NULL &&
              !value->IsNull() &&
              !value->IsBinary())
          {
            if (modalitiesInStudy_->Match(value->GetContent()))
            {
              matchingStudies.push_back(*study);
              break;
            }
          }
        }
      }

      candidates.Intersect(matchingStudies);
    }
  }


  void LookupResource::FindCandidates(std::list<int64_t>& result,
                                      IDatabaseWrapper& database) const
  {
    ResourceType startingLevel;
    if (level_ == ResourceType_Patient)
    {
      startingLevel = ResourceType_Patient;
    }
    else
    {
      startingLevel = ResourceType_Study;
    }

    SetOfResources candidates(database, startingLevel);

    switch (level_)
    {
      case ResourceType_Patient:
        ApplyLevel(candidates, ResourceType_Patient, database);
        break;

      case ResourceType_Study:
        ApplyLevel(candidates, ResourceType_Study, database);
        break;

      case ResourceType_Series:
        ApplyLevel(candidates, ResourceType_Study, database);
        candidates.GoDown();
        ApplyLevel(candidates, ResourceType_Series, database);
        break;

      case ResourceType_Instance:
        ApplyLevel(candidates, ResourceType_Study, database);
        candidates.GoDown();
        ApplyLevel(candidates, ResourceType_Series, database);
        candidates.GoDown();
        ApplyLevel(candidates, ResourceType_Instance, database);
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    candidates.Flatten(result);
  }


  void LookupResource::SetModalitiesInStudy(const std::string& modalities)
  {
    modalitiesInStudy_.reset(new ListConstraint(true /* case sensitive */));
    
    std::vector<std::string> items;
    Toolbox::TokenizeString(items, modalities, '\\');
    
    for (size_t i = 0; i < items.size(); i++)
    {
      modalitiesInStudy_->AddAllowedValue(items[i]);
    }
  }


  void LookupResource::AddDicomConstraint(const DicomTag& tag,
                                          const std::string& dicomQuery,
                                          bool caseSensitive)
  {
    // http://www.itk.org/Wiki/DICOM_QueryRetrieve_Explained
    // http://dicomiseasy.blogspot.be/2012/01/dicom-queryretrieve-part-i.html  
    if (tag == DICOM_TAG_MODALITIES_IN_STUDY)
    {
      SetModalitiesInStudy(dicomQuery);
    }
    else 
    {
      Add(tag, IFindConstraint::ParseDicomConstraint(tag, dicomQuery, caseSensitive));
    }
  }

}
