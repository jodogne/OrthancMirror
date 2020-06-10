/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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


#include "PrecompiledHeadersUnitTests.h"
#include "gtest/gtest.h"

#include "../OrthancServer/Search/DatabaseLookup.h"
#include "../Core/OrthancException.h"

using namespace Orthanc;


TEST(DatabaseLookup, SingleConstraint)
{
  {
    ASSERT_THROW(DicomTagConstraint tag(DICOM_TAG_PATIENT_NAME, ConstraintType_Equal, 
                                        "HEL*LO", true, true), OrthancException);
    ASSERT_THROW(DicomTagConstraint tag(DICOM_TAG_PATIENT_NAME, ConstraintType_Equal,
                                        "HEL?LO", true, true), OrthancException);
    ASSERT_THROW(DicomTagConstraint tag(DICOM_TAG_PATIENT_NAME, ConstraintType_Equal,
                                        true, true), OrthancException);

    DicomTagConstraint tag(DICOM_TAG_PATIENT_NAME, ConstraintType_Equal, "HELLO", true, true);
    ASSERT_TRUE(tag.IsMatch("HELLO"));
    ASSERT_FALSE(tag.IsMatch("hello"));

    ASSERT_TRUE(tag.IsCaseSensitive());
    ASSERT_EQ(ConstraintType_Equal, tag.GetConstraintType());

    DicomMap m;
    ASSERT_FALSE(tag.IsMatch(m));
    m.SetNullValue(DICOM_TAG_PATIENT_NAME);
    ASSERT_FALSE(tag.IsMatch(m));
    m.SetValue(DICOM_TAG_PATIENT_NAME, "HELLO", true /* binary */);
    ASSERT_FALSE(tag.IsMatch(m));
    m.SetValue(DICOM_TAG_PATIENT_NAME, "HELLO", false /* string */);
    ASSERT_TRUE(tag.IsMatch(m));
  }

  {
    DicomTagConstraint tag(DICOM_TAG_PATIENT_NAME, ConstraintType_Equal, "HELlo", false, true);
    ASSERT_TRUE(tag.IsMatch("HELLO"));
    ASSERT_TRUE(tag.IsMatch("hello"));

    ASSERT_EQ("HELlo", tag.GetValue());
  }

  {
    DicomTagConstraint tag(DICOM_TAG_PATIENT_NAME, ConstraintType_Wildcard, "HE*L?O", true, true);
    ASSERT_TRUE(tag.IsMatch("HELLO"));
    ASSERT_TRUE(tag.IsMatch("HELLLLLO"));
    ASSERT_TRUE(tag.IsMatch("HELxO"));
    ASSERT_FALSE(tag.IsMatch("hello"));
  }

  {
    DicomTagConstraint tag(DICOM_TAG_PATIENT_NAME, ConstraintType_Wildcard, "HE*l?o", false, true);
    ASSERT_TRUE(tag.IsMatch("HELLO"));
    ASSERT_TRUE(tag.IsMatch("HELLLLLO"));
    ASSERT_TRUE(tag.IsMatch("HELxO"));
    ASSERT_TRUE(tag.IsMatch("hello"));

    ASSERT_FALSE(tag.IsCaseSensitive());
    ASSERT_EQ(ConstraintType_Wildcard, tag.GetConstraintType());
  }

  {
    DicomTagConstraint tag(DICOM_TAG_PATIENT_NAME, ConstraintType_SmallerOrEqual, "123", true, true);
    ASSERT_TRUE(tag.IsMatch("120"));
    ASSERT_TRUE(tag.IsMatch("123"));
    ASSERT_FALSE(tag.IsMatch("124"));
    ASSERT_TRUE(tag.IsMandatory());
  }

  {
    DicomTagConstraint tag(DICOM_TAG_PATIENT_NAME, ConstraintType_GreaterOrEqual, "123", true, false);
    ASSERT_FALSE(tag.IsMatch("122"));
    ASSERT_TRUE(tag.IsMatch("123"));
    ASSERT_TRUE(tag.IsMatch("124"));
    ASSERT_FALSE(tag.IsMandatory());
  }

  {
    DicomTagConstraint tag(DICOM_TAG_PATIENT_NAME, ConstraintType_List, true, true);
    ASSERT_FALSE(tag.IsMatch("CT"));
    ASSERT_FALSE(tag.IsMatch("MR"));

    tag.AddValue("CT");
    ASSERT_TRUE(tag.IsMatch("CT"));
    ASSERT_FALSE(tag.IsMatch("MR"));

    tag.AddValue("MR");
    ASSERT_TRUE(tag.IsMatch("CT"));
    ASSERT_TRUE(tag.IsMatch("MR"));
    ASSERT_FALSE(tag.IsMatch("ct"));
    ASSERT_FALSE(tag.IsMatch("mr"));

    ASSERT_THROW(tag.GetValue(), OrthancException);
    ASSERT_EQ(2u, tag.GetValues().size());
  }

  {
    DicomTagConstraint tag(DICOM_TAG_PATIENT_NAME, ConstraintType_List, false, true);

    tag.AddValue("ct");
    tag.AddValue("mr");

    ASSERT_TRUE(tag.IsMatch("CT"));
    ASSERT_TRUE(tag.IsMatch("MR"));
    ASSERT_TRUE(tag.IsMatch("ct"));
    ASSERT_TRUE(tag.IsMatch("mr"));
  }
}



TEST(DatabaseLookup, FromDicom)
{
  {
    DatabaseLookup lookup;
    lookup.AddDicomConstraint(DICOM_TAG_PATIENT_ID, "HELLO", true, true);
    ASSERT_EQ(1u, lookup.GetConstraintsCount());
    ASSERT_EQ(ConstraintType_Equal, lookup.GetConstraint(0).GetConstraintType());
    ASSERT_EQ("HELLO", lookup.GetConstraint(0).GetValue());
    ASSERT_TRUE(lookup.GetConstraint(0).IsCaseSensitive());
  }

  {
    DatabaseLookup lookup;
    lookup.AddDicomConstraint(DICOM_TAG_PATIENT_ID, "HELLO", false, true);
    ASSERT_EQ(1u, lookup.GetConstraintsCount());

    // This is *not* a PN VR => "false" above is *not* used
    ASSERT_TRUE(lookup.GetConstraint(0).IsCaseSensitive());
  }

  {
    DatabaseLookup lookup;
    lookup.AddDicomConstraint(DICOM_TAG_PATIENT_NAME, "HELLO", true, true);
    ASSERT_EQ(1u, lookup.GetConstraintsCount());
    ASSERT_TRUE(lookup.GetConstraint(0).IsCaseSensitive());
  }

  {
    DatabaseLookup lookup;
    lookup.AddDicomConstraint(DICOM_TAG_PATIENT_NAME, "HELLO", false, true);
    ASSERT_EQ(1u, lookup.GetConstraintsCount());

    // This is a PN VR => "false" above is used
    ASSERT_FALSE(lookup.GetConstraint(0).IsCaseSensitive());
  }

  {
    DatabaseLookup lookup;
    lookup.AddDicomConstraint(DICOM_TAG_SERIES_DESCRIPTION, "2012-2016", false, true);

    // This is not a data VR
    ASSERT_EQ(ConstraintType_Equal, lookup.GetConstraint(0).GetConstraintType());
  }

  {
    DatabaseLookup lookup;
    lookup.AddDicomConstraint(DICOM_TAG_PATIENT_BIRTH_DATE, "2012-2016", false, true);

    // This is a data VR => range is effective
    ASSERT_EQ(2u, lookup.GetConstraintsCount());

    ASSERT_TRUE(lookup.GetConstraint(0).GetConstraintType() != lookup.GetConstraint(1).GetConstraintType());

    for (size_t i = 0; i < 2; i++)
    {
      ASSERT_TRUE(lookup.GetConstraint(i).GetConstraintType() == ConstraintType_SmallerOrEqual ||
                  lookup.GetConstraint(i).GetConstraintType() == ConstraintType_GreaterOrEqual);
    }
  }

  {
    DatabaseLookup lookup;
    lookup.AddDicomConstraint(DICOM_TAG_PATIENT_BIRTH_DATE, "2012-", false, true);

    ASSERT_EQ(1u, lookup.GetConstraintsCount());
    ASSERT_EQ(ConstraintType_GreaterOrEqual, lookup.GetConstraint(0).GetConstraintType());
    ASSERT_EQ("2012", lookup.GetConstraint(0).GetValue());
  }

  {
    DatabaseLookup lookup;
    lookup.AddDicomConstraint(DICOM_TAG_PATIENT_BIRTH_DATE, "-2016", false, true);

    ASSERT_EQ(1u, lookup.GetConstraintsCount());
    ASSERT_EQ(DICOM_TAG_PATIENT_BIRTH_DATE,  lookup.GetConstraint(0).GetTag());
    ASSERT_EQ(ConstraintType_SmallerOrEqual, lookup.GetConstraint(0).GetConstraintType());
    ASSERT_EQ("2016", lookup.GetConstraint(0).GetValue());
  }

  {
    DatabaseLookup lookup;
    lookup.AddDicomConstraint(DICOM_TAG_MODALITIES_IN_STUDY, "CT\\MR", false, true);

    ASSERT_EQ(1u, lookup.GetConstraintsCount());
    ASSERT_EQ(DICOM_TAG_MODALITY,  lookup.GetConstraint(0).GetTag());
    ASSERT_EQ(ConstraintType_List, lookup.GetConstraint(0).GetConstraintType());

    const std::set<std::string>& values = lookup.GetConstraint(0).GetValues();
    ASSERT_EQ(2u, values.size());
    ASSERT_TRUE(values.find("CT") != values.end());
    ASSERT_TRUE(values.find("MR") != values.end());
    ASSERT_TRUE(values.find("nope") == values.end());
  }

  {
    DatabaseLookup lookup;
    lookup.AddDicomConstraint(DICOM_TAG_STUDY_DESCRIPTION, "CT\\MR", false, true);

    ASSERT_EQ(1u, lookup.GetConstraintsCount());
    ASSERT_EQ(DICOM_TAG_STUDY_DESCRIPTION, lookup.GetConstraint(0).GetTag());
    ASSERT_EQ(ConstraintType_List, lookup.GetConstraint(0).GetConstraintType());

    const std::set<std::string>& values = lookup.GetConstraint(0).GetValues();
    ASSERT_EQ(2u, values.size());
    ASSERT_TRUE(values.find("CT") != values.end());
    ASSERT_TRUE(values.find("MR") != values.end());
    ASSERT_TRUE(values.find("nope") == values.end());
  }

  {
    DatabaseLookup lookup;
    lookup.AddDicomConstraint(DICOM_TAG_STUDY_DESCRIPTION, "HE*O", false, true);

    ASSERT_EQ(1u, lookup.GetConstraintsCount());
    ASSERT_EQ(ConstraintType_Wildcard, lookup.GetConstraint(0).GetConstraintType());
  }

  {
    DatabaseLookup lookup;
    lookup.AddDicomConstraint(DICOM_TAG_STUDY_DESCRIPTION, "HE?O", false, true);

    ASSERT_EQ(1u, lookup.GetConstraintsCount());
    ASSERT_EQ(ConstraintType_Wildcard, lookup.GetConstraint(0).GetConstraintType());
  }

  {
    DatabaseLookup lookup;
    lookup.AddDicomConstraint(DICOM_TAG_RELATED_FRAME_OF_REFERENCE_UID, "TEST", false, true);
    lookup.AddDicomConstraint(DICOM_TAG_PATIENT_NAME, "TEST2", false, false);
    ASSERT_TRUE(lookup.GetConstraint(0).IsMandatory());
    ASSERT_FALSE(lookup.GetConstraint(1).IsMandatory());
  }
}
