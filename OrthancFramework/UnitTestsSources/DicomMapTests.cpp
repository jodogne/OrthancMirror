/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 **/


#if ORTHANC_UNIT_TESTS_LINK_FRAMEWORK == 1
// Must be the first to be sure to use the Orthanc framework shared library
#  include <OrthancFramework.h>
#endif

#if !defined(DCMTK_VERSION_NUMBER)
#  error DCMTK_VERSION_NUMBER is not defined
#endif

#include <gtest/gtest.h>

#include "../Sources/Compatibility.h"
#include "../Sources/OrthancException.h"
#include "../Sources/DicomFormat/DicomMap.h"
#include "../Sources/DicomParsing/FromDcmtkBridge.h"
#include "../Sources/DicomParsing/ToDcmtkBridge.h"
#include "../Sources/DicomParsing/ParsedDicomFile.h"
#include "../Sources/DicomParsing/DicomWebJsonVisitor.h"


using namespace Orthanc;

TEST(DicomMap, MainTags)
{
  ASSERT_TRUE(DicomMap::IsMainDicomTag(DICOM_TAG_PATIENT_ID));
  ASSERT_TRUE(DicomMap::IsMainDicomTag(DICOM_TAG_PATIENT_ID, ResourceType_Patient));
  ASSERT_FALSE(DicomMap::IsMainDicomTag(DICOM_TAG_PATIENT_ID, ResourceType_Study));

  ASSERT_TRUE(DicomMap::IsMainDicomTag(DICOM_TAG_STUDY_INSTANCE_UID));
  ASSERT_TRUE(DicomMap::IsMainDicomTag(DICOM_TAG_ACCESSION_NUMBER));
  ASSERT_TRUE(DicomMap::IsMainDicomTag(DICOM_TAG_SERIES_INSTANCE_UID));
  ASSERT_TRUE(DicomMap::IsMainDicomTag(DICOM_TAG_SOP_INSTANCE_UID));

  std::set<DicomTag> s;
  DicomMap::GetMainDicomTags(s);
  ASSERT_TRUE(s.end() != s.find(DICOM_TAG_PATIENT_ID));
  ASSERT_TRUE(s.end() != s.find(DICOM_TAG_STUDY_INSTANCE_UID));
  ASSERT_TRUE(s.end() != s.find(DICOM_TAG_ACCESSION_NUMBER));
  ASSERT_TRUE(s.end() != s.find(DICOM_TAG_SERIES_INSTANCE_UID));
  ASSERT_TRUE(s.end() != s.find(DICOM_TAG_SOP_INSTANCE_UID));

  DicomMap::GetMainDicomTags(s, ResourceType_Patient);
  ASSERT_TRUE(s.end() != s.find(DICOM_TAG_PATIENT_ID));
  ASSERT_TRUE(s.end() == s.find(DICOM_TAG_STUDY_INSTANCE_UID));

  DicomMap::GetMainDicomTags(s, ResourceType_Study);
  ASSERT_TRUE(s.end() != s.find(DICOM_TAG_STUDY_INSTANCE_UID));
  ASSERT_TRUE(s.end() != s.find(DICOM_TAG_ACCESSION_NUMBER));
  ASSERT_TRUE(s.end() == s.find(DICOM_TAG_PATIENT_ID));

  DicomMap::GetMainDicomTags(s, ResourceType_Series);
  ASSERT_TRUE(s.end() != s.find(DICOM_TAG_SERIES_INSTANCE_UID));
  ASSERT_TRUE(s.end() == s.find(DICOM_TAG_PATIENT_ID));

  DicomMap::GetMainDicomTags(s, ResourceType_Instance);
  ASSERT_TRUE(s.end() != s.find(DICOM_TAG_SOP_INSTANCE_UID));
  ASSERT_TRUE(s.end() == s.find(DICOM_TAG_PATIENT_ID));
}


TEST(DicomMap, Tags)
{
  std::set<DicomTag> s;

  DicomMap m;
  m.GetTags(s);
  ASSERT_EQ(0u, s.size());

  ASSERT_FALSE(m.HasTag(DICOM_TAG_PATIENT_NAME));
  ASSERT_FALSE(m.HasTag(0x0010, 0x0010));
  m.SetValue(0x0010, 0x0010, "PatientName", false);
  ASSERT_TRUE(m.HasTag(DICOM_TAG_PATIENT_NAME));
  ASSERT_TRUE(m.HasTag(0x0010, 0x0010));

  m.GetTags(s);
  ASSERT_EQ(1u, s.size());
  ASSERT_EQ(DICOM_TAG_PATIENT_NAME, *s.begin());

  ASSERT_FALSE(m.HasTag(DICOM_TAG_PATIENT_ID));
  m.SetValue(DICOM_TAG_PATIENT_ID, "PatientID", false);
  ASSERT_TRUE(m.HasTag(0x0010, 0x0020));
  m.SetValue(DICOM_TAG_PATIENT_ID, "PatientID2", false);
  ASSERT_EQ("PatientID2", m.GetValue(0x0010, 0x0020).GetContent());

  m.GetTags(s);
  ASSERT_EQ(2u, s.size());

  m.Remove(DICOM_TAG_PATIENT_ID);
  ASSERT_THROW(m.GetValue(0x0010, 0x0020), OrthancException);

  m.GetTags(s);
  ASSERT_EQ(1u, s.size());
  ASSERT_EQ(DICOM_TAG_PATIENT_NAME, *s.begin());

  std::unique_ptr<DicomMap> mm(m.Clone());
  ASSERT_EQ("PatientName", mm->GetValue(DICOM_TAG_PATIENT_NAME).GetContent());  

  m.SetValue(DICOM_TAG_PATIENT_ID, "Hello", false);
  ASSERT_THROW(mm->GetValue(DICOM_TAG_PATIENT_ID), OrthancException);
  mm->CopyTagIfExists(m, DICOM_TAG_PATIENT_ID);
  ASSERT_EQ("Hello", mm->GetValue(DICOM_TAG_PATIENT_ID).GetContent());  

  DicomValue v;
  ASSERT_TRUE(v.IsNull());
}


TEST(DicomMap, FindTemplates)
{
  DicomMap m;

  DicomMap::SetupFindPatientTemplate(m);
  ASSERT_TRUE(m.HasTag(DICOM_TAG_PATIENT_ID));

  DicomMap::SetupFindStudyTemplate(m);
  ASSERT_TRUE(m.HasTag(DICOM_TAG_STUDY_INSTANCE_UID));
  ASSERT_TRUE(m.HasTag(DICOM_TAG_ACCESSION_NUMBER));

  DicomMap::SetupFindSeriesTemplate(m);
  ASSERT_TRUE(m.HasTag(DICOM_TAG_SERIES_INSTANCE_UID));

  DicomMap::SetupFindInstanceTemplate(m);
  ASSERT_TRUE(m.HasTag(DICOM_TAG_SOP_INSTANCE_UID));
}




static void TestModule(ResourceType level,
                       DicomModule module)
{
  // REFERENCE: DICOM PS3.3 2015c - Information Object Definitions
  // http://dicom.nema.org/medical/dicom/current/output/html/part03.html

  std::set<DicomTag> moduleTags, main;
  DicomTag::AddTagsForModule(moduleTags, module);
  DicomMap::GetMainDicomTags(main, level);
  
  // The main dicom tags are a subset of the module
  for (std::set<DicomTag>::const_iterator it = main.begin(); it != main.end(); ++it)
  {
    bool ok = moduleTags.find(*it) != moduleTags.end();

    // Exceptions for the Study level
    if (level == ResourceType_Study &&
        (*it == DicomTag(0x0008, 0x0080) ||  /* InstitutionName, from Visit identification module, related to Visit */
         *it == DicomTag(0x0032, 0x1032) ||  /* RequestingPhysician, from Imaging Service Request module, related to Study */
         *it == DicomTag(0x0032, 0x1060)))   /* RequestedProcedureDescription, from Requested Procedure module, related to Study */
    {
      ok = true;
    }

    // Exceptions for the Series level
    if (level == ResourceType_Series &&
        (*it == DicomTag(0x0008, 0x0070) ||  /* Manufacturer, from General Equipment Module */
         *it == DicomTag(0x0008, 0x1010) ||  /* StationName, from General Equipment Module */
         *it == DicomTag(0x0018, 0x0024) ||  /* SequenceName, from MR Image Module (SIMPLIFICATION => Series) */
         *it == DicomTag(0x0018, 0x1090) ||  /* CardiacNumberOfImages, from MR Image Module (SIMPLIFICATION => Series) */
         *it == DicomTag(0x0020, 0x0037) ||  /* ImageOrientationPatient, from Image Plane Module (SIMPLIFICATION => Series) */
         *it == DicomTag(0x0020, 0x0105) ||  /* NumberOfTemporalPositions, from MR Image Module (SIMPLIFICATION => Series) */
         *it == DicomTag(0x0020, 0x1002) ||  /* ImagesInAcquisition, from General Image Module (SIMPLIFICATION => Series) */
         *it == DicomTag(0x0054, 0x0081) ||  /* NumberOfSlices, from PET Series module */
         *it == DicomTag(0x0054, 0x0101) ||  /* NumberOfTimeSlices, from PET Series module */
         *it == DicomTag(0x0054, 0x1000) ||  /* SeriesType, from PET Series module */
         *it == DicomTag(0x0018, 0x1400) ||  /* AcquisitionDeviceProcessingDescription, from CR/X-Ray/DX/WholeSlideMicro Image (SIMPLIFICATION => Series) */
         *it == DicomTag(0x0018, 0x0010)))   /* ContrastBolusAgent, from Contrast/Bolus module (SIMPLIFICATION => Series) */
    {
      ok = true;
    }

    // Exceptions for the Instance level
    if (level == ResourceType_Instance &&
        (*it == DicomTag(0x0020, 0x0012) ||  /* AccessionNumber, from General Image module */
         *it == DicomTag(0x0054, 0x1330) ||  /* ImageIndex, from PET Image module */
         *it == DicomTag(0x0020, 0x0100) ||  /* TemporalPositionIdentifier, from MR Image module */
         *it == DicomTag(0x0028, 0x0008) ||  /* NumberOfFrames, from Multi-frame module attributes, related to Image */
         *it == DicomTag(0x0020, 0x0032) ||  /* ImagePositionPatient, from Image Plan module, related to Image */
         *it == DicomTag(0x0020, 0x0037) ||  /* ImageOrientationPatient, from Image Plane Module (Orthanc 1.4.2) */
         *it == DicomTag(0x0020, 0x4000)))   /* ImageComments, from General Image module */
    {
      ok = true;
    }

    if (!ok)
    {
      std::cout << it->Format() << ": " << FromDcmtkBridge::GetTagName(*it, "")
                << " not expected at level " << EnumerationToString(level) << std::endl;
    }

    EXPECT_TRUE(ok);
  }
}


TEST(DicomMap, Modules)
{
  TestModule(ResourceType_Patient, DicomModule_Patient);
  TestModule(ResourceType_Study, DicomModule_Study);
  TestModule(ResourceType_Series, DicomModule_Series);   // TODO
  TestModule(ResourceType_Instance, DicomModule_Instance);
}


TEST(DicomMap, Parse)
{
  DicomMap m;
  float f;
  double d;
  int32_t i;
  int64_t j;
  uint32_t k;
  uint64_t l;
  unsigned int ui;
  std::string s;
  
  m.SetValue(DICOM_TAG_PATIENT_NAME, "      ", false);  // Empty value
  ASSERT_FALSE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseFloat(f));
  ASSERT_FALSE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseDouble(d));
  ASSERT_FALSE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseInteger32(i));
  ASSERT_FALSE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseInteger64(j));
  ASSERT_FALSE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseUnsignedInteger32(k));
  ASSERT_FALSE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseUnsignedInteger64(l));
  
  m.SetValue(DICOM_TAG_PATIENT_NAME, "0", true);  // Binary value
  ASSERT_FALSE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseFloat(f));
  ASSERT_FALSE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseDouble(d));
  ASSERT_FALSE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseInteger32(i));
  ASSERT_FALSE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseInteger64(j));
  ASSERT_FALSE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseUnsignedInteger32(k));
  ASSERT_FALSE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseUnsignedInteger64(l));

  ASSERT_FALSE(m.LookupStringValue(s, DICOM_TAG_PATIENT_NAME, false));
  ASSERT_TRUE(m.LookupStringValue(s, DICOM_TAG_PATIENT_NAME, true));
  ASSERT_EQ("0", s);
               

  // 2**31-1
  m.SetValue(DICOM_TAG_PATIENT_NAME, "2147483647", false);
  ASSERT_TRUE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseFloat(f));
  ASSERT_TRUE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseDouble(d));
  ASSERT_TRUE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseInteger32(i));
  ASSERT_TRUE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseInteger64(j));
  ASSERT_TRUE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseUnsignedInteger32(k));
  ASSERT_TRUE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseUnsignedInteger64(l));
  ASSERT_FLOAT_EQ(2147483647.0f, f);
  ASSERT_DOUBLE_EQ(2147483647.0, d);
  ASSERT_EQ(2147483647, i);
  ASSERT_EQ(2147483647ll, j);
  ASSERT_EQ(2147483647u, k);
  ASSERT_EQ(2147483647ull, l);

  // Test shortcuts
  m.SetValue(DICOM_TAG_PATIENT_NAME, "42", false);
  ASSERT_TRUE(m.ParseFloat(f, DICOM_TAG_PATIENT_NAME));
  ASSERT_TRUE(m.ParseDouble(d, DICOM_TAG_PATIENT_NAME));
  ASSERT_TRUE(m.ParseInteger32(i, DICOM_TAG_PATIENT_NAME));
  ASSERT_TRUE(m.ParseInteger64(j, DICOM_TAG_PATIENT_NAME));
  ASSERT_TRUE(m.ParseUnsignedInteger32(k, DICOM_TAG_PATIENT_NAME));
  ASSERT_TRUE(m.ParseUnsignedInteger64(l, DICOM_TAG_PATIENT_NAME));
  ASSERT_FLOAT_EQ(42.0f, f);
  ASSERT_DOUBLE_EQ(42.0, d);
  ASSERT_EQ(42, i);
  ASSERT_EQ(42ll, j);
  ASSERT_EQ(42u, k);
  ASSERT_EQ(42ull, l);

  ASSERT_TRUE(m.LookupStringValue(s, DICOM_TAG_PATIENT_NAME, false));
  ASSERT_EQ("42", s);
  ASSERT_TRUE(m.LookupStringValue(s, DICOM_TAG_PATIENT_NAME, true));
  ASSERT_EQ("42", s);
               
  
  // 2**31
  m.SetValue(DICOM_TAG_PATIENT_NAME, "2147483648", false);
  ASSERT_TRUE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseFloat(f));
  ASSERT_TRUE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseDouble(d));
  ASSERT_FALSE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseInteger32(i));
  ASSERT_TRUE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseInteger64(j));
  ASSERT_TRUE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseUnsignedInteger32(k));
  ASSERT_TRUE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseUnsignedInteger64(l));
  ASSERT_FLOAT_EQ(2147483648.0f, f);
  ASSERT_DOUBLE_EQ(2147483648.0, d);
  ASSERT_EQ(2147483648ll, j);
  ASSERT_EQ(2147483648u, k);
  ASSERT_EQ(2147483648ull, l);

  // 2**32-1
  m.SetValue(DICOM_TAG_PATIENT_NAME, "4294967295", false);
  ASSERT_TRUE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseFloat(f));
  ASSERT_TRUE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseDouble(d));
  ASSERT_FALSE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseInteger32(i));
  ASSERT_TRUE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseInteger64(j));
  ASSERT_TRUE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseUnsignedInteger32(k));
  ASSERT_TRUE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseUnsignedInteger64(l));
  ASSERT_FLOAT_EQ(4294967295.0f, f);
  ASSERT_DOUBLE_EQ(4294967295.0, d);
  ASSERT_EQ(4294967295ll, j);
  ASSERT_EQ(4294967295u, k);
  ASSERT_EQ(4294967295ull, l);
  
  // 2**32
  m.SetValue(DICOM_TAG_PATIENT_NAME, "4294967296", false);
  ASSERT_TRUE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseFloat(f));
  ASSERT_TRUE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseDouble(d));
  ASSERT_FALSE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseInteger32(i));
  ASSERT_TRUE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseInteger64(j));
  ASSERT_FALSE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseUnsignedInteger32(k));
  ASSERT_TRUE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseUnsignedInteger64(l));
  ASSERT_FLOAT_EQ(4294967296.0f, f);
  ASSERT_DOUBLE_EQ(4294967296.0, d);
  ASSERT_EQ(4294967296ll, j);
  ASSERT_EQ(4294967296ull, l);
  
  m.SetValue(DICOM_TAG_PATIENT_NAME, "-1", false);
  ASSERT_TRUE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseFloat(f));
  ASSERT_TRUE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseDouble(d));
  ASSERT_TRUE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseInteger32(i));
  ASSERT_TRUE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseInteger64(j));
  ASSERT_FALSE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseUnsignedInteger32(k));
  ASSERT_FALSE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseUnsignedInteger64(l));
  ASSERT_FLOAT_EQ(-1.0f, f);
  ASSERT_DOUBLE_EQ(-1.0, d);
  ASSERT_EQ(-1, i);
  ASSERT_EQ(-1ll, j);

  // -2**31
  m.SetValue(DICOM_TAG_PATIENT_NAME, "-2147483648", false);
  ASSERT_TRUE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseFloat(f));
  ASSERT_TRUE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseDouble(d));
  ASSERT_TRUE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseInteger32(i));
  ASSERT_TRUE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseInteger64(j));
  ASSERT_FALSE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseUnsignedInteger32(k));
  ASSERT_FALSE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseUnsignedInteger64(l));
  ASSERT_FLOAT_EQ(-2147483648.0f, f);
  ASSERT_DOUBLE_EQ(-2147483648.0, d);
  ASSERT_EQ(static_cast<int32_t>(-2147483648ll), i);
  ASSERT_EQ(-2147483648ll, j);
  
  // -2**31 - 1
  m.SetValue(DICOM_TAG_PATIENT_NAME, "-2147483649", false);
  ASSERT_TRUE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseFloat(f));
  ASSERT_TRUE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseDouble(d));
  ASSERT_FALSE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseInteger32(i));
  ASSERT_TRUE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseInteger64(j));
  ASSERT_FALSE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseUnsignedInteger32(k));
  ASSERT_FALSE(m.GetValue(DICOM_TAG_PATIENT_NAME).ParseUnsignedInteger64(l));
  ASSERT_FLOAT_EQ(-2147483649.0f, f);
  ASSERT_DOUBLE_EQ(-2147483649.0, d); 
  ASSERT_EQ(-2147483649ll, j);


  // "800\0" in US COLMUNS tag
  m.SetValue(DICOM_TAG_COLUMNS, "800\0", false);
  ASSERT_TRUE(m.GetValue(DICOM_TAG_COLUMNS).ParseFirstUnsignedInteger(ui));
  ASSERT_EQ(800u, ui);
  m.SetValue(DICOM_TAG_COLUMNS, "800", false);
  ASSERT_TRUE(m.GetValue(DICOM_TAG_COLUMNS).ParseFirstUnsignedInteger(ui));
  ASSERT_EQ(800u, ui);
}


TEST(DicomMap, Serialize)
{
  Json::Value s;
  
  {
    DicomMap m;
    m.SetValue(DICOM_TAG_PATIENT_NAME, "Hello", false);
    m.SetValue(DICOM_TAG_STUDY_DESCRIPTION, "Binary", true);
    m.SetNullValue(DICOM_TAG_SERIES_DESCRIPTION);
    m.Serialize(s);
  }

  {
    DicomMap m;
    m.Unserialize(s);

    const DicomValue* v = m.TestAndGetValue(DICOM_TAG_ACCESSION_NUMBER);
    ASSERT_TRUE(v == NULL);

    v = m.TestAndGetValue(DICOM_TAG_PATIENT_NAME);
    ASSERT_TRUE(v != NULL);
    ASSERT_FALSE(v->IsNull());
    ASSERT_FALSE(v->IsBinary());
    ASSERT_EQ("Hello", v->GetContent());

    v = m.TestAndGetValue(DICOM_TAG_STUDY_DESCRIPTION);
    ASSERT_TRUE(v != NULL);
    ASSERT_FALSE(v->IsNull());
    ASSERT_TRUE(v->IsBinary());
    ASSERT_EQ("Binary", v->GetContent());

    v = m.TestAndGetValue(DICOM_TAG_SERIES_DESCRIPTION);
    ASSERT_TRUE(v != NULL);
    ASSERT_TRUE(v->IsNull());
    ASSERT_FALSE(v->IsBinary());
    ASSERT_THROW(v->GetContent(), OrthancException);
  }
}



TEST(DicomMap, ExtractMainDicomTags)
{
  DicomMap b;
  b.SetValue(DICOM_TAG_PATIENT_NAME, "E", false);
  ASSERT_TRUE(b.HasOnlyMainDicomTags());

  {
    DicomMap a;
    a.SetValue(DICOM_TAG_PATIENT_NAME, "A", false);
    a.SetValue(DICOM_TAG_STUDY_DESCRIPTION, "B", false);
    a.SetValue(DICOM_TAG_SERIES_DESCRIPTION, "C", false);
    a.SetValue(DICOM_TAG_NUMBER_OF_FRAMES, "D", false);
    a.SetValue(DICOM_TAG_SLICE_THICKNESS, "F", false);
    ASSERT_FALSE(a.HasOnlyMainDicomTags());
    b.ExtractMainDicomTags(a);
  }

  ASSERT_EQ(4u, b.GetSize());
  ASSERT_EQ("A", b.GetValue(DICOM_TAG_PATIENT_NAME).GetContent());
  ASSERT_EQ("B", b.GetValue(DICOM_TAG_STUDY_DESCRIPTION).GetContent());
  ASSERT_EQ("C", b.GetValue(DICOM_TAG_SERIES_DESCRIPTION).GetContent());
  ASSERT_EQ("D", b.GetValue(DICOM_TAG_NUMBER_OF_FRAMES).GetContent());
  ASSERT_FALSE(b.HasTag(DICOM_TAG_SLICE_THICKNESS));
  ASSERT_TRUE(b.HasOnlyMainDicomTags());

  b.SetValue(DICOM_TAG_PATIENT_NAME, "G", false);

  {
    DicomMap a;
    a.SetValue(DICOM_TAG_PATIENT_NAME, "A", false);
    a.SetValue(DICOM_TAG_SLICE_THICKNESS, "F", false);
    ASSERT_FALSE(a.HasOnlyMainDicomTags());
    b.Merge(a);
  }

  ASSERT_EQ(5u, b.GetSize());
  ASSERT_EQ("G", b.GetValue(DICOM_TAG_PATIENT_NAME).GetContent());
  ASSERT_EQ("B", b.GetValue(DICOM_TAG_STUDY_DESCRIPTION).GetContent());
  ASSERT_EQ("C", b.GetValue(DICOM_TAG_SERIES_DESCRIPTION).GetContent());
  ASSERT_EQ("D", b.GetValue(DICOM_TAG_NUMBER_OF_FRAMES).GetContent());
  ASSERT_EQ("F", b.GetValue(DICOM_TAG_SLICE_THICKNESS).GetContent());
  ASSERT_FALSE(b.HasOnlyMainDicomTags());
}


TEST(DicomMap, RemoveBinary)
{
  DicomMap b;
  b.SetValue(DICOM_TAG_PATIENT_NAME, "A", false);
  b.SetValue(DICOM_TAG_PATIENT_ID, "B", true);
  b.SetValue(DICOM_TAG_STUDY_INSTANCE_UID, DicomValue());  // NULL
  b.SetValue(DICOM_TAG_SERIES_INSTANCE_UID, DicomValue("C", false));
  b.SetValue(DICOM_TAG_SOP_INSTANCE_UID, DicomValue("D", true));

  b.RemoveBinaryTags();

  std::string s;
  ASSERT_EQ(2u, b.GetSize());
  ASSERT_TRUE(b.LookupStringValue(s, DICOM_TAG_PATIENT_NAME, false)); ASSERT_EQ("A", s);
  ASSERT_TRUE(b.LookupStringValue(s, DICOM_TAG_SERIES_INSTANCE_UID, false)); ASSERT_EQ("C", s);
}



TEST(DicomWebJson, Multiplicity)
{
  // http://dicom.nema.org/medical/dicom/current/output/chtml/part18/sect_F.2.4.html

  ParsedDicomFile dicom(false);
  dicom.ReplacePlainString(DICOM_TAG_PATIENT_NAME, "SB1^SB2^SB3^SB4^SB5");
  dicom.ReplacePlainString(DICOM_TAG_IMAGE_ORIENTATION_PATIENT, "1\\2.3\\4");
  dicom.ReplacePlainString(DICOM_TAG_IMAGE_POSITION_PATIENT, "");

  DicomWebJsonVisitor visitor;
  dicom.Apply(visitor);

  {
    const Json::Value& tag = visitor.GetResult() ["00200037"];  // ImageOrientationPatient
    const Json::Value& value = tag["Value"];
  
    ASSERT_EQ(EnumerationToString(ValueRepresentation_DecimalString), tag["vr"].asString());
    ASSERT_EQ(2u, tag.getMemberNames().size());
    ASSERT_EQ(3u, value.size());
    ASSERT_EQ(Json::realValue, value[1].type());
    ASSERT_FLOAT_EQ(1.0f, value[0].asFloat());
    ASSERT_FLOAT_EQ(2.3f, value[1].asFloat());
    ASSERT_FLOAT_EQ(4.0f, value[2].asFloat());
  }

  {
    const Json::Value& tag = visitor.GetResult() ["00200032"];  // ImagePositionPatient
    ASSERT_EQ(EnumerationToString(ValueRepresentation_DecimalString), tag["vr"].asString());
    ASSERT_EQ(1u, tag.getMemberNames().size());
  }

  std::string xml;
  visitor.FormatXml(xml);

  {
    DicomMap m;
    m.FromDicomWeb(visitor.GetResult());
    ASSERT_EQ(3u, m.GetSize());

    std::string s;
    ASSERT_TRUE(m.LookupStringValue(s, DICOM_TAG_PATIENT_NAME, false));
    ASSERT_EQ("SB1^SB2^SB3^SB4^SB5", s);
    ASSERT_TRUE(m.LookupStringValue(s, DICOM_TAG_IMAGE_POSITION_PATIENT, false));
    ASSERT_TRUE(s.empty());

    ASSERT_TRUE(m.LookupStringValue(s, DICOM_TAG_IMAGE_ORIENTATION_PATIENT, false));

    std::vector<std::string> v;
    Toolbox::TokenizeString(v, s, '\\');
    ASSERT_FLOAT_EQ(1.0f, boost::lexical_cast<float>(v[0]));
    ASSERT_FLOAT_EQ(2.3f, boost::lexical_cast<float>(v[1]));
    ASSERT_FLOAT_EQ(4.0f, boost::lexical_cast<float>(v[2]));
  }
}


TEST(DicomWebJson, NullValue)
{
  // http://dicom.nema.org/medical/dicom/current/output/chtml/part18/sect_F.2.5.html

  ParsedDicomFile dicom(false);
  dicom.ReplacePlainString(DICOM_TAG_IMAGE_ORIENTATION_PATIENT, "1.5\\\\\\2.5");

  DicomWebJsonVisitor visitor;
  dicom.Apply(visitor);

  {
    const Json::Value& tag = visitor.GetResult() ["00200037"];
    const Json::Value& value = tag["Value"];
  
    ASSERT_EQ(EnumerationToString(ValueRepresentation_DecimalString), tag["vr"].asString());
    ASSERT_EQ(2u, tag.getMemberNames().size());
    ASSERT_EQ(4u, value.size());
    ASSERT_EQ(Json::realValue, value[0].type());
    ASSERT_EQ(Json::nullValue, value[1].type());
    ASSERT_EQ(Json::nullValue, value[2].type());
    ASSERT_EQ(Json::realValue, value[3].type());
    ASSERT_FLOAT_EQ(1.5f, value[0].asFloat());
    ASSERT_FLOAT_EQ(2.5f, value[3].asFloat());
  }

  std::string xml;
  visitor.FormatXml(xml);
  
  {
    DicomMap m;
    m.FromDicomWeb(visitor.GetResult());
    ASSERT_EQ(1u, m.GetSize());

    std::string s;
    ASSERT_TRUE(m.LookupStringValue(s, DICOM_TAG_IMAGE_ORIENTATION_PATIENT, false));

    std::vector<std::string> v;
    Toolbox::TokenizeString(v, s, '\\');
    ASSERT_FLOAT_EQ(1.5f, boost::lexical_cast<float>(v[0]));
    ASSERT_TRUE(v[1].empty());
    ASSERT_TRUE(v[2].empty());
    ASSERT_FLOAT_EQ(2.5f, boost::lexical_cast<float>(v[3]));
  }
}


TEST(DicomWebJson, PixelSpacing)
{
  // Test related to locales: Make sure that decimal separator is
  // correctly handled (dot "." vs comma ",")
  ParsedDicomFile source(false);
  source.ReplacePlainString(DICOM_TAG_PIXEL_SPACING, "1.5\\1.3");

  DicomWebJsonVisitor visitor;
  source.Apply(visitor);

  DicomMap target;
  target.FromDicomWeb(visitor.GetResult());

  ASSERT_EQ("DS", visitor.GetResult() ["00280030"]["vr"].asString());
  ASSERT_FLOAT_EQ(1.5f, visitor.GetResult() ["00280030"]["Value"][0].asFloat());
  ASSERT_FLOAT_EQ(1.3f, visitor.GetResult() ["00280030"]["Value"][1].asFloat());

  std::string s;
  ASSERT_TRUE(target.LookupStringValue(s, DICOM_TAG_PIXEL_SPACING, false));
  ASSERT_EQ(s, "1.5\\1.3");
}


TEST(DicomMap, MainTagNames)
{
  ASSERT_EQ(3, ResourceType_Instance - ResourceType_Patient);
  
  for (int i = ResourceType_Patient; i <= ResourceType_Instance; i++)
  {
    ResourceType level = static_cast<ResourceType>(i);

    std::set<DicomTag> tags;
    DicomMap::GetMainDicomTags(tags, level);

    for (std::set<DicomTag>::const_iterator it = tags.begin(); it != tags.end(); ++it)
    {
      DicomMap a;
      a.SetValue(*it, "TEST", false);

      Json::Value json;
      a.DumpMainDicomTags(json, level);

      ASSERT_EQ(Json::objectValue, json.type());
      ASSERT_EQ(1u, json.getMemberNames().size());

      std::string name = json.getMemberNames() [0];
      EXPECT_EQ(name, FromDcmtkBridge::GetTagName(*it, ""));

      DicomMap b;
      b.ParseMainDicomTags(json, level);

      ASSERT_EQ(1u, b.GetSize());
      ASSERT_EQ("TEST", b.GetStringValue(*it, "", false));

      std::string main = it->GetMainTagsName();
      if (!main.empty())
      {
        ASSERT_EQ(main, name);
      }
    }
  }
}




#include "../Sources/SystemToolbox.h"

TEST(DicomMap, DISABLED_ParseDicomMetaInformation)
{
  static const std::string PATH = "/home/jodogne/Subversion/orthanc-tests/Database/TransferSyntaxes/";
  
  std::map<std::string, DicomTransferSyntax> f;
  f.insert(std::make_pair(PATH + "../ColorTestMalaterre.dcm", DicomTransferSyntax_LittleEndianImplicit));  // 1.2.840.10008.1.2
  f.insert(std::make_pair(PATH + "1.2.840.10008.1.2.1.dcm", DicomTransferSyntax_LittleEndianExplicit));
  f.insert(std::make_pair(PATH + "1.2.840.10008.1.2.2.dcm", DicomTransferSyntax_BigEndianExplicit));
  f.insert(std::make_pair(PATH + "1.2.840.10008.1.2.4.50.dcm", DicomTransferSyntax_JPEGProcess1));
  f.insert(std::make_pair(PATH + "1.2.840.10008.1.2.4.51.dcm", DicomTransferSyntax_JPEGProcess2_4));
  f.insert(std::make_pair(PATH + "1.2.840.10008.1.2.4.57.dcm", DicomTransferSyntax_JPEGProcess14));
  f.insert(std::make_pair(PATH + "1.2.840.10008.1.2.4.70.dcm", DicomTransferSyntax_JPEGProcess14SV1));
  f.insert(std::make_pair(PATH + "1.2.840.10008.1.2.4.80.dcm", DicomTransferSyntax_JPEGLSLossless));
  f.insert(std::make_pair(PATH + "1.2.840.10008.1.2.4.81.dcm", DicomTransferSyntax_JPEGLSLossy));
  f.insert(std::make_pair(PATH + "1.2.840.10008.1.2.4.90.dcm", DicomTransferSyntax_JPEG2000LosslessOnly));
  f.insert(std::make_pair(PATH + "1.2.840.10008.1.2.4.91.dcm", DicomTransferSyntax_JPEG2000));
  f.insert(std::make_pair(PATH + "1.2.840.10008.1.2.5.dcm", DicomTransferSyntax_RLELossless));

  for (std::map<std::string, DicomTransferSyntax>::const_iterator it = f.begin(); it != f.end(); ++it)
  {
    printf("\n== %s ==\n\n", it->first.c_str());
    
    std::string dicom;
    SystemToolbox::ReadFile(dicom, it->first, false);

    DicomMap d;
    ASSERT_TRUE(DicomMap::ParseDicomMetaInformation(d, dicom.c_str(), dicom.size()));
    d.Print(stdout);

    std::string s;
    ASSERT_TRUE(d.LookupStringValue(s, DICOM_TAG_TRANSFER_SYNTAX_UID, false));
    
    DicomTransferSyntax ts;
    ASSERT_TRUE(LookupTransferSyntax(ts, s));
    ASSERT_EQ(ts, it->second);
  }
}


namespace
{
  class StreamBlockReader : public boost::noncopyable
  {
  private:
    std::istream&  stream_;
    std::string    block_;
    size_t         blockPos_;
    uint64_t       processedBytes_;

  public:
    StreamBlockReader(std::istream& stream) :
      stream_(stream),
      blockPos_(0),
      processedBytes_(0)
    {
    }

    void Schedule(size_t blockSize)
    {
      if (!block_.empty())
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
      else
      {
        block_.resize(blockSize);
        blockPos_ = 0;
      }
    }

    bool Read(std::string& block)
    {
      if (block_.empty())
      {
        if (blockPos_ != 0)
        {
          throw OrthancException(ErrorCode_BadSequenceOfCalls);
        }
        
        block.clear();
        return true;
      }
      else
      {
        while (blockPos_ < block_.size())
        {
          char c;
          stream_.get(c);

          if (stream_.good())
          {
            block_[blockPos_] = c;
            blockPos_++;
          }
          else
          {
            return false;
          }
        }

        processedBytes_ += block_.size();

        block.swap(block_);
        block_.clear();
        return true;
      }
    }

    uint64_t GetProcessedBytes() const
    {
      return processedBytes_;
    }
  };




  /**
   * This class parses a stream containing a DICOM instance. It does
   * *not* support the visit of sequences (it only works at the first
   * level of the hierarchy), and it stops the processing once pixel
   * data is reached in compressed transfer syntaxes.
   **/
  class DicomStreamReader : public boost::noncopyable
  {
  public:
    class IVisitor : public boost::noncopyable
    {
    public:
      virtual ~IVisitor()
      {
      }

      // The data from this function will always be Little Endian (as
      // specified by the DICOM standard)
      virtual void VisitMetaHeaderTag(const DicomTag& tag,
                                      const ValueRepresentation& vr,
                                      const std::string& value) = 0;

      // Return "false" to stop processing
      virtual bool VisitDatasetTag(const DicomTag& tag,
                                   const ValueRepresentation& vr,
                                   DicomTransferSyntax transferSyntax,
                                   const std::string& value,
                                   bool isLittleEndian) = 0;
    };
    
  private:
    enum State
    {
      State_Preamble,
      State_MetaHeader,
      State_DatasetTag,
      State_SequenceExplicitLength,
      State_SequenceExplicitValue,
      State_DatasetExplicitLength,
      State_DatasetValue,
      State_Done
    };

    StreamBlockReader    reader_;
    State                state_;
    DicomTransferSyntax  transferSyntax_;
    DicomTag             danglingTag_;
    ValueRepresentation  danglingVR_;
    unsigned int         sequenceDepth_;
    
    static uint16_t ReadUnsignedInteger16(const char* dicom,
                                          bool littleEndian)
    {
      const uint8_t* p = reinterpret_cast<const uint8_t*>(dicom);

      if (littleEndian)
      {
        return (static_cast<uint16_t>(p[0]) |
                (static_cast<uint16_t>(p[1]) << 8));
      }
      else
      {
        return (static_cast<uint16_t>(p[1]) |
                (static_cast<uint16_t>(p[0]) << 8));
      }
    }


    static uint32_t ReadUnsignedInteger32(const char* dicom,
                                          bool littleEndian)
    {
      const uint8_t* p = reinterpret_cast<const uint8_t*>(dicom);

      if (littleEndian)
      {
        return (static_cast<uint32_t>(p[0]) |
                (static_cast<uint32_t>(p[1]) << 8) |
                (static_cast<uint32_t>(p[2]) << 16) |
                (static_cast<uint32_t>(p[3]) << 24));
      }
      else
      {
        return (static_cast<uint32_t>(p[3]) |
                (static_cast<uint32_t>(p[2]) << 8) |
                (static_cast<uint32_t>(p[1]) << 16) |
                (static_cast<uint32_t>(p[0]) << 24));
      }
    }


    static DicomTag ReadTag(const char* dicom,
                            bool littleEndian)
    {
      return DicomTag(ReadUnsignedInteger16(dicom, littleEndian),
                      ReadUnsignedInteger16(dicom + 2, littleEndian));
    }


    static bool IsShortExplicitTag(ValueRepresentation vr)
    {
      /**
       * Are we in the case of Table 7.1-2? "Data Element with
       * Explicit VR of AE, AS, AT, CS, DA, DS, DT, FL, FD, IS, LO,
       * LT, PN, SH, SL, SS, ST, TM, UI, UL and US"
       * http://dicom.nema.org/medical/dicom/current/output/chtml/part05/chapter_7.html#sect_7.1.2
       **/
      return (vr == ValueRepresentation_ApplicationEntity   /* AE */ ||
              vr == ValueRepresentation_AgeString           /* AS */ ||
              vr == ValueRepresentation_AttributeTag        /* AT */ ||
              vr == ValueRepresentation_CodeString          /* CS */ ||
              vr == ValueRepresentation_Date                /* DA */ ||
              vr == ValueRepresentation_DecimalString       /* DS */ ||
              vr == ValueRepresentation_DateTime            /* DT */ ||
              vr == ValueRepresentation_FloatingPointSingle /* FL */ ||
              vr == ValueRepresentation_FloatingPointDouble /* FD */ ||
              vr == ValueRepresentation_IntegerString       /* IS */ ||
              vr == ValueRepresentation_LongString          /* LO */ ||
              vr == ValueRepresentation_LongText            /* LT */ ||
              vr == ValueRepresentation_PersonName          /* PN */ ||
              vr == ValueRepresentation_ShortString         /* SH */ ||
              vr == ValueRepresentation_SignedLong          /* SL */ ||
              vr == ValueRepresentation_SignedShort         /* SS */ ||
              vr == ValueRepresentation_ShortText           /* ST */ ||
              vr == ValueRepresentation_Time                /* TM */ ||
              vr == ValueRepresentation_UniqueIdentifier    /* UI */ ||
              vr == ValueRepresentation_UnsignedLong        /* UL */ ||
              vr == ValueRepresentation_UnsignedShort       /* US */);
    }


    bool IsLittleEndian() const
    {
      return (transferSyntax_ != DicomTransferSyntax_BigEndianExplicit);
    }


    void PrintBlock(const std::string& block)
    {
      for (size_t i = 0; i < block.size(); i++)
      {
        printf("%02x ", static_cast<uint8_t>(block[i]));
        if (i % 16 == 15)
          printf("\n");
      }
      printf("\n");
    }
    
    void HandlePreamble(IVisitor& visitor,
                        const std::string& block)
    {
      //printf("PREAMBLE:\n");
      //PrintBlock(block);

      assert(block.size() == 144u);
      assert(reader_.GetProcessedBytes() == 144u);

      /**
       * The "DICOM file meta information" is always encoded using
       * "Explicit VR Little Endian Transfer Syntax"
       * http://dicom.nema.org/medical/dicom/current/output/chtml/part10/chapter_7.html
       **/
      if (block[128] != 'D' ||
          block[129] != 'I' ||
          block[130] != 'C' ||
          block[131] != 'M' ||
          ReadTag(block.c_str() + 132, true) != DicomTag(0x0002, 0x0000) ||
          block[136] != 'U' ||
          block[137] != 'L' ||
          ReadUnsignedInteger16(block.c_str() + 138, true) != 4)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }

      uint32_t length = ReadUnsignedInteger32(block.c_str() + 140, true);

      reader_.Schedule(length);
      state_ = State_MetaHeader;
    }

    
    void HandleMetaHeader(IVisitor& visitor,
                          const std::string& block)
    {
      //printf("META-HEADER:\n");
      //PrintBlock(block);

      size_t pos = 0;
      const char* p = block.c_str();

      bool hasTransferSyntax = false;

      while (pos + 8 <= block.size())
      {
        DicomTag tag = ReadTag(p + pos, true);
        
        ValueRepresentation vr = StringToValueRepresentation(std::string(p + pos + 4, 2), true);

        if (IsShortExplicitTag(vr))
        {
          uint16_t length = ReadUnsignedInteger16(p + pos + 6, true);

          std::string value;
          value.assign(p + pos + 8, length);

          if (tag.GetGroup() == 0x0002)
          {
            visitor.VisitMetaHeaderTag(tag, vr, value);
          }                  

          if (tag == DICOM_TAG_TRANSFER_SYNTAX_UID)
          {
            // Remove possible padding byte
            if (!value.empty() &&
                value[value.size() - 1] == '\0')
            {
              value.resize(value.size() - 1);
            }
            
            if (LookupTransferSyntax(transferSyntax_, value))
            {
              hasTransferSyntax = true;
            }
            else
            {
              throw OrthancException(ErrorCode_NotImplemented, "Unsupported transfer syntax: " + value);
            }
          }
          
          pos += length + 8;
        }
        else if (pos + 12 <= block.size())
        {
          uint16_t reserved = ReadUnsignedInteger16(p + pos + 6, true);
          if (reserved != 0)
          {
            break;
          }
          
          uint32_t length = ReadUnsignedInteger32(p + pos + 8, true);

          std::string value;
          value.assign(p + pos + 12, length);

          if (tag.GetGroup() == 0x0002)
          {
            visitor.VisitMetaHeaderTag(tag, vr, value);
          }                  
          
          pos += length + 12;
        }
      }

      if (pos != block.size())
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }

      if (!hasTransferSyntax)
      {
        throw OrthancException(ErrorCode_BadFileFormat, "DICOM file meta-header without transfer syntax UID");
      }

      reader_.Schedule(8);
      state_ = State_DatasetTag;
    }
    

    void HandleDatasetTag(const std::string& block)
    {
      static const DicomTag DICOM_TAG_SEQUENCE_ITEM(0xfffe, 0xe000);
      static const DicomTag DICOM_TAG_SEQUENCE_DELIMITATION_ITEM(0xfffe, 0xe00d);
      static const DicomTag DICOM_TAG_SEQUENCE_DELIMITATION_SEQUENCE(0xfffe, 0xe0dd);

      assert(block.size() == 8u);

      const bool littleEndian = IsLittleEndian();
      DicomTag tag = ReadTag(block.c_str(), littleEndian);

      if (tag == DICOM_TAG_SEQUENCE_ITEM ||
          tag == DICOM_TAG_SEQUENCE_DELIMITATION_ITEM ||
          tag == DICOM_TAG_SEQUENCE_DELIMITATION_SEQUENCE)
      {
        //printf("SEQUENCE TAG:\n");
        //PrintBlock(block);

        // The special sequence items are encoded like "Implicit VR"
        uint32_t length = ReadUnsignedInteger32(block.c_str() + 4, littleEndian);

        if (tag == DICOM_TAG_SEQUENCE_ITEM)
        {
          for (unsigned int i = 0; i <= sequenceDepth_; i++)
            printf("  ");
          if (length == 0xffffffffu)
          {
            // Undefined length: Need to loop over the tags of the nested dataset
            printf("...next dataset in sequence...\n");
            reader_.Schedule(8);
            state_ = State_DatasetTag;
          }
          else
          {
            // Explicit length: Can skip the full sequence at once
            printf("...next dataset in sequence... %u bytes\n", length);
            reader_.Schedule(length);
            state_ = State_DatasetValue;
          }
        }
        else if (tag == DICOM_TAG_SEQUENCE_DELIMITATION_ITEM ||
                 tag == DICOM_TAG_SEQUENCE_DELIMITATION_SEQUENCE)
        {
          if (length != 0 ||
              sequenceDepth_ == 0)
          {
            throw OrthancException(ErrorCode_BadFileFormat);
          }

          if (tag == DICOM_TAG_SEQUENCE_DELIMITATION_SEQUENCE)
          {
            for (unsigned int i = 0; i < sequenceDepth_; i++)
              printf("  ");
            printf("...leaving sequence...\n");

            sequenceDepth_ --;
          }
          else
          {
            if (sequenceDepth_ == 0)
            {
              throw OrthancException(ErrorCode_BadFileFormat);
            }
          }

          reader_.Schedule(8);
          state_ = State_DatasetTag;          
        }
        else
        {
          throw OrthancException(ErrorCode_InternalError);
        }
      }
      else
      {
        //printf("DATASET TAG:\n");
        //PrintBlock(block);

        ValueRepresentation vr = ValueRepresentation_Unknown;
        
        if (transferSyntax_ == DicomTransferSyntax_LittleEndianImplicit)
        {
          uint32_t length = ReadUnsignedInteger32(block.c_str() + 4, true /* little endian */);
        
          reader_.Schedule(length);
          state_ = State_DatasetValue;
        }
        else
        {
          // This in an explicit transfer syntax

          vr = StringToValueRepresentation(
            std::string(block.c_str() + 4, 2), false /* ignore unknown VR */);

          if (vr != ValueRepresentation_Sequence &&
              sequenceDepth_ > 0)
          {
            for (unsigned int i = 0; i <= sequenceDepth_; i++)
              printf("  ");
            printf("%s\n", tag.Format().c_str());
          }
          
          if (vr == ValueRepresentation_Sequence)
          {
            for (unsigned int i = 0; i <= sequenceDepth_; i++)
              printf("  ");
            printf("...entering sequence... %s\n", tag.Format().c_str());
            sequenceDepth_ ++;
            reader_.Schedule(4);
            state_ = State_SequenceExplicitLength;
          }
          else if (IsShortExplicitTag(vr))
          {
            uint16_t length = ReadUnsignedInteger16(block.c_str() + 6, littleEndian);

            reader_.Schedule(length);
            state_ = State_DatasetValue;
          }
          else
          {
            uint16_t reserved = ReadUnsignedInteger16(block.c_str() + 6, littleEndian);
            if (reserved != 0)
            {
              throw OrthancException(ErrorCode_BadFileFormat);
            }

            reader_.Schedule(4);
            state_ = State_DatasetExplicitLength;
          }
        }

        if (sequenceDepth_ == 0)
        {
          danglingTag_ = tag;
          danglingVR_ = vr;
        }
      }
    }


    void HandleDatasetExplicitLength(const std::string& block)
    {
      //printf("DATASET TAG LENGTH:\n");
      //PrintBlock(block);

      assert(block.size() == 4);

      uint32_t length = ReadUnsignedInteger32(block.c_str(), IsLittleEndian());
      
      if (length == 0xffffffffu)
      {
        /**
         * This is the case of pixel data with compressed transfer
         * syntaxes. Schedule the reading of the first tag of the
         * nested dataset.
         * http://dicom.nema.org/medical/dicom/current/output/chtml/part05/sect_7.5.html
         **/

        if (sequenceDepth_ != 0)
        {
          throw OrthancException(ErrorCode_BadFileFormat);
        }
        
        printf("  ...entering sequence... %s\n", danglingTag_.Format().c_str());
        state_ = State_DatasetTag;
        reader_.Schedule(8);
        sequenceDepth_ ++;
      }
      else
      {
        reader_.Schedule(length);
        state_ = State_DatasetValue;
      }
    }
    
    
    void HandleSequenceExplicitLength(const std::string& block)
    {
      //printf("DATASET TAG LENGTH:\n");
      //PrintBlock(block);

      assert(block.size() == 4);

      uint32_t length = ReadUnsignedInteger32(block.c_str(), IsLittleEndian());
      if (length == 0xffffffffu)
      {
        state_ = State_DatasetTag;
        reader_.Schedule(8);
      }
      else
      {
        for (unsigned int i = 0; i <= sequenceDepth_; i++)
          printf("  ");
        printf("...skipping sequence thanks to explicit length... %d\n", length);

        reader_.Schedule(length);
        state_ = State_SequenceExplicitValue;
      }
    }

    void HandleSequenceExplicitValue()
    {
      if (sequenceDepth_ == 0)
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      sequenceDepth_ --;

      state_ = State_DatasetTag;
      reader_.Schedule(8);
    }


    void HandleDatasetValue(IVisitor& visitor,
                            const std::string& block)
    {
      if (sequenceDepth_ == 0)
      {
        bool c;
        
        if (!block.empty() &&
            (block[block.size() - 1] == ' ' ||
             block[block.size() - 1] == '\0') &&
            (danglingVR_ == ValueRepresentation_ApplicationEntity ||
             danglingVR_ == ValueRepresentation_AgeString ||
             danglingVR_ == ValueRepresentation_CodeString ||
             danglingVR_ == ValueRepresentation_DecimalString ||
             danglingVR_ == ValueRepresentation_IntegerString ||
             danglingVR_ == ValueRepresentation_LongString ||
             danglingVR_ == ValueRepresentation_LongText ||
             danglingVR_ == ValueRepresentation_PersonName ||
             danglingVR_ == ValueRepresentation_ShortString ||
             danglingVR_ == ValueRepresentation_ShortText ||
             danglingVR_ == ValueRepresentation_UniqueIdentifier ||
             danglingVR_ == ValueRepresentation_UnlimitedText))
        {
          std::string s(block.begin(), block.end() - 1);
          c = visitor.VisitDatasetTag(danglingTag_, danglingVR_, transferSyntax_, s, IsLittleEndian());
        }
        else
        {
          c = visitor.VisitDatasetTag(danglingTag_, danglingVR_, transferSyntax_, block, IsLittleEndian());
        }
        
        if (!c)
        {
          state_ = State_Done;
          return;
        }
      }

      reader_.Schedule(8);
      state_ = State_DatasetTag;
    }
    
    
  public:
    DicomStreamReader(std::istream& stream) :
      reader_(stream),
      state_(State_Preamble),
      transferSyntax_(DicomTransferSyntax_LittleEndianImplicit),  // Dummy
      danglingTag_(0x0000, 0x0000),  // Dummy
      danglingVR_(ValueRepresentation_Unknown),  // Dummy
      sequenceDepth_(0)
    {
      reader_.Schedule(128 /* empty header */ +
                       4 /* "DICM" magic value */ +
                       4 /* (0x0002, 0x0000) tag */ +
                       2 /* value representation of (0x0002, 0x0000) == "UL" */ +
                       2 /* length of "UL" value == 4 */ +
                       4 /* actual length of the meta-header */);
    }

    void Consume(IVisitor& visitor)
    {
      while (state_ != State_Done)
      {
        std::string block;
        if (reader_.Read(block))
        {
          switch (state_)
          {
            case State_Preamble:
              HandlePreamble(visitor, block);
              break;

            case State_MetaHeader:
              HandleMetaHeader(visitor, block);
              break;

            case State_DatasetTag:
              HandleDatasetTag(block);
              break;

            case State_DatasetExplicitLength:
              HandleDatasetExplicitLength(block);
              break;

            case State_SequenceExplicitLength:
              HandleSequenceExplicitLength(block);
              break;

            case State_SequenceExplicitValue:
              HandleSequenceExplicitValue();
              break;

            case State_DatasetValue:
              HandleDatasetValue(visitor, block);
              break;

            default:
              throw OrthancException(ErrorCode_InternalError);
          }
        }
        else
        {
          return;  // No more data in the stream
        }
      }
    }

    bool IsDone() const
    {
      return (state_ == State_Done);
    }

    uint64_t GetProcessedBytes() const
    {
      return reader_.GetProcessedBytes();
    }
  };



  class V : public DicomStreamReader::IVisitor
  {
  public:
    virtual void VisitMetaHeaderTag(const DicomTag& tag,
                                    const ValueRepresentation& vr,
                                    const std::string& value) ORTHANC_OVERRIDE
    {
      std::cout << "Header: " << tag.Format() << " [" << Toolbox::ConvertToAscii(value).c_str() << "] (" << value.size() << ")" << std::endl;
    }

    virtual bool VisitDatasetTag(const DicomTag& tag,
                                 const ValueRepresentation& vr,
                                 DicomTransferSyntax transferSyntax,
                                 const std::string& value,
                                 bool isLittleEndian) ORTHANC_OVERRIDE
    {
      if (!isLittleEndian)
        printf("** ");
      if (tag.GetGroup() < 0x7f00)
        std::cout << "Dataset: " << tag.Format() << " " << EnumerationToString(vr)
                  << " [" << Toolbox::ConvertToAscii(value).c_str() << "] (" << value.size() << ")" << std::endl;
      else
        std::cout << "Dataset: " << tag.Format() << " " << EnumerationToString(vr)
                  << " [PIXEL] (" << value.size() << ")" << std::endl;

      return true;
    }
  };
}



TEST(DicomStreamReader, DISABLED_Tutu)
{
  static const std::string PATH = "/home/jodogne/Subversion/orthanc-tests/Database/TransferSyntaxes/";
  
  std::string dicom;
  SystemToolbox::ReadFile(dicom, PATH + "../ColorTestMalaterre.dcm", false);
  //SystemToolbox::ReadFile(dicom, PATH + "1.2.840.10008.1.2.1.dcm", false);
  //SystemToolbox::ReadFile(dicom, PATH + "1.2.840.10008.1.2.2.dcm", false);  // Big Endian
  //SystemToolbox::ReadFile(dicom, PATH + "1.2.840.10008.1.2.4.50.dcm", false);
  //SystemToolbox::ReadFile(dicom, PATH + "1.2.840.10008.1.2.4.51.dcm", false);
  //SystemToolbox::ReadFile(dicom, PATH + "1.2.840.10008.1.2.4.57.dcm", false);
  //SystemToolbox::ReadFile(dicom, PATH + "1.2.840.10008.1.2.4.70.dcm", false);
  //SystemToolbox::ReadFile(dicom, PATH + "1.2.840.10008.1.2.4.80.dcm", false);
  //SystemToolbox::ReadFile(dicom, PATH + "1.2.840.10008.1.2.4.81.dcm", false);
  //SystemToolbox::ReadFile(dicom, PATH + "1.2.840.10008.1.2.4.90.dcm", false);
  //SystemToolbox::ReadFile(dicom, PATH + "1.2.840.10008.1.2.4.91.dcm", false);
  //SystemToolbox::ReadFile(dicom, PATH + "1.2.840.10008.1.2.5.dcm", false);
  
  std::stringstream stream;
  size_t pos = 0;
  
  DicomStreamReader r(stream);
  V visitor;

  while (pos < dicom.size() &&
         !r.IsDone())
  {
    //printf("."); 
    //printf("%d\n", pos);
    r.Consume(visitor);
    stream.clear();
    stream.put(dicom[pos++]);
  }

  r.Consume(visitor);

  printf(">> %d\n", r.GetProcessedBytes());
}

TEST(DicomStreamReader, DISABLED_Tutu2)
{
  static const std::string PATH = "/home/jodogne/Subversion/orthanc-tests/Database/TransferSyntaxes/";

  //std::ifstream stream(PATH + "1.2.840.10008.1.2.4.50.dcm");
  std::ifstream stream(PATH + "1.2.840.10008.1.2.2.dcm");
  
  DicomStreamReader r(stream);
  V visitor;

  r.Consume(visitor);

  printf(">> %d\n", r.GetProcessedBytes());
}
