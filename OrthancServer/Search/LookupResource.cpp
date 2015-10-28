/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
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

#include "ListConstraint.h"
#include "RangeConstraint.h"
#include "ValueConstraint.h"
#include "WildcardConstraint.h"

#include "../../Core/OrthancException.h"
#include "../../Core/FileStorage/StorageAccessor.h"
#include "../ServerToolbox.h"
#include "../FromDcmtkBridge.h"


namespace Orthanc
{
  LookupResource::Level::Level(ResourceType level) : level_(level)
  {
    const DicomTag* tags = NULL;
    size_t size;
    
    LookupIdentifierQuery::LoadIdentifiers(tags, size, level);
    
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
      delete *it;
    }

    for (Constraints::iterator it = identifiersConstraints_.begin();
         it != identifiersConstraints_.end(); ++it)
    {
      delete *it;
    }
  }

  bool LookupResource::Level::Add(std::auto_ptr<IFindConstraint>& constraint)
  {
    if (identifiers_.find(constraint->GetTag()) != identifiers_.end())
    {
      if (level_ == ResourceType_Patient)
      {
        // The filters on the patient level must be cloned to the study level
        identifiersConstraints_.push_back(constraint->Clone());
      }
      else
      {
        identifiersConstraints_.push_back(constraint.release());
      }

      return true;
    }
    else if (mainTags_.find(constraint->GetTag()) != mainTags_.end())
    {
      if (level_ == ResourceType_Patient)
      {
        // The filters on the patient level must be cloned to the study level
        mainTagsConstraints_.push_back(constraint->Clone());
      }
      else
      {
        mainTagsConstraints_.push_back(constraint.release());
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

      case ResourceType_Study:
        levels_[ResourceType_Study] = new Level(ResourceType_Study);
        // Do not add "break" here

      case ResourceType_Series:
        levels_[ResourceType_Series] = new Level(ResourceType_Series);
        // Do not add "break" here

      case ResourceType_Instance:
        levels_[ResourceType_Instance] = new Level(ResourceType_Instance);
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
      delete *it;
    }    
  }



  bool LookupResource::AddInternal(ResourceType level,
                                   std::auto_ptr<IFindConstraint>& constraint)
  {
    Levels::iterator it = levels_.find(level);
    if (it != levels_.end())
    {
      if (it->second->Add(constraint))
      {
        return true;
      }
    }

    return false;
  }


  void LookupResource::Add(IFindConstraint* constraint)
  {
    std::auto_ptr<IFindConstraint> c(constraint);

    if (!AddInternal(ResourceType_Patient, c) &&
        !AddInternal(ResourceType_Study, c) &&
        !AddInternal(ResourceType_Series, c) &&
        !AddInternal(ResourceType_Instance, c))
    {
      unoptimizedConstraints_.push_back(c.release());
    }
  }


  static bool Match(const DicomMap& tags,
                    const IFindConstraint& constraint)
  {
    const DicomValue* value = tags.TestAndGetValue(constraint.GetTag());

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
      (*it)->Setup(query);
    }

    query.Apply(candidates, database);

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
          if (!Match(tags, **it))
          {
            match = false;
          }
        }

        for (Constraints::const_iterator it = mainTagsConstraints_.begin(); 
             match && it != mainTagsConstraints_.end(); ++it)
        {
          if (!Match(tags, **it))
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
      std::string tag = (*it)->GetTag().Format();
      if (dicomAsJson.isMember(tag) &&
          dicomAsJson[tag]["Type"] == "String")
      {
        std::string value = dicomAsJson[tag]["Value"].asString();
        if (!(*it)->Match(value))
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
    modalitiesInStudy_.reset(new ListConstraint(DICOM_TAG_MODALITIES_IN_STUDY, 
                                                true /* case sensitive */));
    
    std::vector<std::string> items;
    Toolbox::TokenizeString(items, modalities, '\\');
    
    for (size_t i = 0; i < items.size(); i++)
    {
      modalitiesInStudy_->AddAllowedValue(items[i]);
    }
  }


  void LookupResource::AddDicomConstraint(const DicomTag& tag,
                                          const std::string& dicomQuery,
                                          bool caseSensitivePN)
  {
    ValueRepresentation vr = FromDcmtkBridge::GetValueRepresentation(tag);

    bool sensitive = true;
    if (vr == ValueRepresentation_PatientName)
    {
      sensitive = caseSensitivePN;
    }

    // http://www.itk.org/Wiki/DICOM_QueryRetrieve_Explained
    // http://dicomiseasy.blogspot.be/2012/01/dicom-queryretrieve-part-i.html  

    if (tag == DICOM_TAG_MODALITIES_IN_STUDY)
    {
      SetModalitiesInStudy(dicomQuery);
    }
    else if ((vr == ValueRepresentation_Date ||
              vr == ValueRepresentation_DateTime ||
              vr == ValueRepresentation_Time) &&
             dicomQuery.find('-') != std::string::npos)
    {
      /**
       * Range matching is only defined for TM, DA and DT value
       * representations. This code fixes issues 35 and 37.
       *
       * Reference: "Range matching is not defined for types of
       * Attributes other than dates and times", DICOM PS 3.4,
       * C.2.2.2.5 ("Range Matching").
       **/
      size_t separator = dicomQuery.find('-');
      std::string lower = dicomQuery.substr(0, separator);
      std::string upper = dicomQuery.substr(separator + 1);
      Add(new RangeConstraint(tag, lower, upper, sensitive));
    }
    else if (dicomQuery.find('\\') != std::string::npos)
    {
      std::auto_ptr<ListConstraint> constraint(new ListConstraint(tag, sensitive));

      std::vector<std::string> items;
      Toolbox::TokenizeString(items, dicomQuery, '\\');

      for (size_t i = 0; i < items.size(); i++)
      {
        constraint->AddAllowedValue(items[i]);
      }

      Add(constraint.release());
    }
    else if (dicomQuery.find('*') != std::string::npos ||
             dicomQuery.find('?') != std::string::npos)
    {
      Add(new WildcardConstraint(tag, dicomQuery, sensitive));
    }
    else
    {
      /**
       * Case-insensitive match for PN value representation (Patient
       * Name). Case-senstive match for all the other value
       * representations.
       *
       * Reference: DICOM PS 3.4
       *   - C.2.2.2.1 ("Single Value Matching") 
       *   - C.2.2.2.4 ("Wild Card Matching")
       * http://medical.nema.org/Dicom/2011/11_04pu.pdf
       *
       * "Except for Attributes with a PN Value Representation, only
       * entities with values which match exactly the value specified in the
       * request shall match. This matching is case-sensitive, i.e.,
       * sensitive to the exact encoding of the key attribute value in
       * character sets where a letter may have multiple encodings (e.g.,
       * based on its case, its position in a word, or whether it is
       * accented)
       * 
       * For Attributes with a PN Value Representation (e.g., Patient Name
       * (0010,0010)), an application may perform literal matching that is
       * either case-sensitive, or that is insensitive to some or all
       * aspects of case, position, accent, or other character encoding
       * variants."
       *
       * (0008,0018) UI SOPInstanceUID     => Case-sensitive
       * (0008,0050) SH AccessionNumber    => Case-sensitive
       * (0010,0020) LO PatientID          => Case-sensitive
       * (0020,000D) UI StudyInstanceUID   => Case-sensitive
       * (0020,000E) UI SeriesInstanceUID  => Case-sensitive
      **/

      Add(new ValueConstraint(tag, dicomQuery, sensitive));
    }
  }
}
