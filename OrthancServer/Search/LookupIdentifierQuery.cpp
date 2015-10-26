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
#include "LookupIdentifierQuery.h"

#include "../../Core/OrthancException.h"
#include "SetOfResources.h"

#include <cassert>



namespace Orthanc
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

  static void LoadIdentifiers(const DicomTag*& tags,
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


  LookupIdentifierQuery::Union::~Union()
  {
    for (size_t i = 0; i < union_.size(); i++)
    {
      delete union_[i];
    }
  }


  void LookupIdentifierQuery::Union::Add(const Constraint& constraint)
  {
    union_.push_back(new Constraint(constraint));
  }


  LookupIdentifierQuery::~LookupIdentifierQuery()
  {
    for (Constraints::iterator it = constraints_.begin();
         it != constraints_.end(); ++it)
    {
      delete *it;
    }
  }



  bool LookupIdentifierQuery::IsIdentifier(const DicomTag& tag,
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


  void LookupIdentifierQuery::AddConstraint(DicomTag tag,
                                            IdentifierConstraintType type,
                                            const std::string& value)
  {
    assert(IsIdentifier(tag));

    Constraint constraint(tag, type, NormalizeIdentifier(value));
    constraints_.push_back(new Union);
    constraints_.back()->Add(constraint);
  }


  void LookupIdentifierQuery::AddDisjunction(const std::list<Constraint>& constraints)
  {
    constraints_.push_back(new Union);

    for (std::list<Constraint>::const_iterator
           it = constraints.begin(); it != constraints.end(); ++it)
    {
      assert(IsIdentifier(it->GetTag()));
      constraints_.back()->Add(*it);
    }
  }


  std::string LookupIdentifierQuery::NormalizeIdentifier(const std::string& value)
  {
    std::string s = Toolbox::ConvertToAscii(Toolbox::StripSpaces(value));
    Toolbox::ToUpperCase(s);
    return s;
  }


  void LookupIdentifierQuery::StoreIdentifiers(IDatabaseWrapper& database,
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


  void LookupIdentifierQuery::Apply(std::list<std::string>& result,
                                    IDatabaseWrapper& database)
  {
    SetOfResources resources(database, level_);
    
    for (size_t i = 0; i < GetSize(); i++)
    {
      std::list<int64_t> a;

      for (size_t j = 0; j < constraints_[i]->GetSize(); j++)
      {
        const Constraint& constraint = constraints_[i]->GetConstraint(j);
        std::list<int64_t> b;
        database.LookupIdentifier(b, level_, constraint.GetTag(), constraint.GetType(), constraint.GetValue());

        a.splice(a.end(), b);
      }

      resources.Intersect(a);
    }

    resources.Flatten(result);
  }
}
