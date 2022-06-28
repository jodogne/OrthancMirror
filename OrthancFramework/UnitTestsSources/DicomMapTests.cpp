/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
#include "../Sources/DicomFormat/DicomStreamReader.h"
#include "../Sources/DicomParsing/FromDcmtkBridge.h"
#include "../Sources/DicomParsing/ToDcmtkBridge.h"
#include "../Sources/DicomParsing/ParsedDicomFile.h"
#include "../Sources/DicomParsing/DicomWebJsonVisitor.h"

#include <boost/lexical_cast.hpp>

using namespace Orthanc;


namespace Orthanc
{
  // The namespace is necessary because of FRIEND_TEST
  // http://code.google.com/p/googletest/wiki/AdvancedGuide#Private_Class_Members

  class DicomMapMainTagsTests : public ::testing::Test
  {
  public:
    DicomMapMainTagsTests()
    {
    }

    virtual void SetUp() ORTHANC_OVERRIDE
    {
      DicomMap::ResetDefaultMainDicomTags();
    }

    virtual void TearDown() ORTHANC_OVERRIDE
    {
      DicomMap::ResetDefaultMainDicomTags();
    }
  };

  TEST_F(DicomMapMainTagsTests, MainTags)
  {
    ASSERT_TRUE(DicomMap::IsMainDicomTag(DICOM_TAG_PATIENT_ID));
    ASSERT_TRUE(DicomMap::IsMainDicomTag(DICOM_TAG_PATIENT_ID, ResourceType_Patient));
    ASSERT_FALSE(DicomMap::IsMainDicomTag(DICOM_TAG_PATIENT_ID, ResourceType_Study));

    ASSERT_TRUE(DicomMap::IsMainDicomTag(DICOM_TAG_STUDY_INSTANCE_UID));
    ASSERT_TRUE(DicomMap::IsMainDicomTag(DICOM_TAG_ACCESSION_NUMBER));
    ASSERT_TRUE(DicomMap::IsMainDicomTag(DICOM_TAG_SERIES_INSTANCE_UID));
    ASSERT_TRUE(DicomMap::IsMainDicomTag(DICOM_TAG_SOP_INSTANCE_UID));

    {
      const std::set<DicomTag>& s = DicomMap::GetAllMainDicomTags();
      ASSERT_TRUE(s.end() != s.find(DICOM_TAG_PATIENT_ID));
      ASSERT_TRUE(s.end() != s.find(DICOM_TAG_STUDY_INSTANCE_UID));
      ASSERT_TRUE(s.end() != s.find(DICOM_TAG_ACCESSION_NUMBER));
      ASSERT_TRUE(s.end() != s.find(DICOM_TAG_SERIES_INSTANCE_UID));
      ASSERT_TRUE(s.end() != s.find(DICOM_TAG_SOP_INSTANCE_UID));
    }

    {
      const std::set<DicomTag>& s = DicomMap::GetMainDicomTags(ResourceType_Patient);
      ASSERT_TRUE(s.end() != s.find(DICOM_TAG_PATIENT_ID));
      ASSERT_TRUE(s.end() == s.find(DICOM_TAG_STUDY_INSTANCE_UID));
    }

    {
      const std::set<DicomTag>& s = DicomMap::GetMainDicomTags(ResourceType_Study);
      ASSERT_TRUE(s.end() != s.find(DICOM_TAG_STUDY_INSTANCE_UID));
      ASSERT_TRUE(s.end() != s.find(DICOM_TAG_ACCESSION_NUMBER));
      ASSERT_TRUE(s.end() == s.find(DICOM_TAG_PATIENT_ID));
    }

    {
      const std::set<DicomTag>& s = DicomMap::GetMainDicomTags(ResourceType_Series);
      ASSERT_TRUE(s.end() != s.find(DICOM_TAG_SERIES_INSTANCE_UID));
      ASSERT_TRUE(s.end() == s.find(DICOM_TAG_PATIENT_ID));
    }

    {
      const std::set<DicomTag>& s = DicomMap::GetMainDicomTags(ResourceType_Instance);
      ASSERT_TRUE(s.end() != s.find(DICOM_TAG_SOP_INSTANCE_UID));
      ASSERT_TRUE(s.end() == s.find(DICOM_TAG_PATIENT_ID));
    }
  }

  TEST_F(DicomMapMainTagsTests, AddMainTags)
  {
    DicomMap::AddMainDicomTag(DICOM_TAG_BITS_ALLOCATED, ResourceType_Instance);

    {
      const std::set<DicomTag>& s = DicomMap::GetMainDicomTags(ResourceType_Instance);
      ASSERT_TRUE(s.end() != s.find(DICOM_TAG_BITS_ALLOCATED));
      ASSERT_TRUE(s.end() != s.find(DICOM_TAG_SOP_INSTANCE_UID));
    }
    {
      const std::set<DicomTag>& s = DicomMap::GetMainDicomTags(ResourceType_Series);
      ASSERT_TRUE(s.end() == s.find(DICOM_TAG_BITS_ALLOCATED));
    }

    ASSERT_TRUE(DicomMap::IsMainDicomTag(DICOM_TAG_BITS_ALLOCATED));
    ASSERT_TRUE(DicomMap::IsMainDicomTag(DICOM_TAG_BITS_ALLOCATED, ResourceType_Instance));

    // adding the same tag should throw
    ASSERT_THROW(DicomMap::AddMainDicomTag(DICOM_TAG_BITS_ALLOCATED, ResourceType_Instance), OrthancException);

  }

  TEST_F(DicomMapMainTagsTests, Signatures)
  {
    std::string defaultPatientSignature = DicomMap::GetDefaultMainDicomTagsSignature(ResourceType_Patient);
    std::string defaultStudySignature = DicomMap::GetDefaultMainDicomTagsSignature(ResourceType_Study);
    std::string defaultSeriesSignature = DicomMap::GetDefaultMainDicomTagsSignature(ResourceType_Series);
    std::string defaultInstanceSignature = DicomMap::GetDefaultMainDicomTagsSignature(ResourceType_Instance);

    ASSERT_NE(defaultInstanceSignature, defaultPatientSignature);
    ASSERT_NE(defaultSeriesSignature, defaultStudySignature);
    ASSERT_NE(defaultSeriesSignature, defaultPatientSignature);

    std::string patientSignature = DicomMap::GetMainDicomTagsSignature(ResourceType_Patient);
    std::string studySignature = DicomMap::GetMainDicomTagsSignature(ResourceType_Study);
    std::string seriesSignature = DicomMap::GetMainDicomTagsSignature(ResourceType_Series);
    std::string instanceSignature = DicomMap::GetMainDicomTagsSignature(ResourceType_Instance);

    // at start, default and current signature should be equal
    ASSERT_EQ(defaultPatientSignature, patientSignature);
    ASSERT_EQ(defaultStudySignature, studySignature);
    ASSERT_EQ(defaultSeriesSignature, seriesSignature);
    ASSERT_EQ(defaultInstanceSignature, instanceSignature);

    DicomMap::AddMainDicomTag(DICOM_TAG_BITS_ALLOCATED, ResourceType_Instance);
    instanceSignature = DicomMap::GetMainDicomTagsSignature(ResourceType_Instance);
    
    ASSERT_NE(defaultInstanceSignature, instanceSignature);
  }

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

  std::set<DicomTag> moduleTags;
  const std::set<DicomTag>& main = DicomMap::GetMainDicomTags(level);
  DicomTag::AddTagsForModule(moduleTags, module);
  
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


TEST(DicomMap, ComputedTags)
{
  {
    std::set<DicomTag> tags;

    ASSERT_FALSE(DicomMap::HasOnlyComputedTags(tags));
    ASSERT_FALSE(DicomMap::HasComputedTags(tags, ResourceType_Instance));
    ASSERT_FALSE(DicomMap::HasComputedTags(tags, ResourceType_Series));
    ASSERT_FALSE(DicomMap::HasComputedTags(tags, ResourceType_Study));
    ASSERT_FALSE(DicomMap::HasComputedTags(tags, ResourceType_Patient));
  }

  {
    std::set<DicomTag> tags;
    tags.insert(DICOM_TAG_ACCESSION_NUMBER);

    ASSERT_FALSE(DicomMap::HasOnlyComputedTags(tags));
    ASSERT_FALSE(DicomMap::HasComputedTags(tags, ResourceType_Instance));
    ASSERT_FALSE(DicomMap::HasComputedTags(tags, ResourceType_Series));
    ASSERT_FALSE(DicomMap::HasComputedTags(tags, ResourceType_Study));
    ASSERT_FALSE(DicomMap::HasComputedTags(tags, ResourceType_Patient));
  }

  {
    std::set<DicomTag> tags;
    tags.insert(DICOM_TAG_MODALITIES_IN_STUDY);
    tags.insert(DICOM_TAG_RETRIEVE_URL);

    ASSERT_TRUE(DicomMap::HasOnlyComputedTags(tags));
    ASSERT_TRUE(DicomMap::HasComputedTags(tags, ResourceType_Study));
    ASSERT_FALSE(DicomMap::HasComputedTags(tags, ResourceType_Patient));
    ASSERT_FALSE(DicomMap::HasComputedTags(tags, ResourceType_Series));
    ASSERT_FALSE(DicomMap::HasComputedTags(tags, ResourceType_Instance));
  }

  {
    std::set<DicomTag> tags;
    tags.insert(DICOM_TAG_ACCESSION_NUMBER);
    tags.insert(DICOM_TAG_MODALITIES_IN_STUDY);

    ASSERT_FALSE(DicomMap::HasOnlyComputedTags(tags));
    ASSERT_TRUE(DicomMap::HasComputedTags(tags, ResourceType_Study));
    ASSERT_FALSE(DicomMap::HasComputedTags(tags, ResourceType_Patient));
    ASSERT_FALSE(DicomMap::HasComputedTags(tags, ResourceType_Series));
    ASSERT_FALSE(DicomMap::HasComputedTags(tags, ResourceType_Instance));
  }

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


TEST(DicomMap, FromDicomAsJsonAndSequences)
{
  DicomMap m;
  std::string jsonFullString = "{"
   "\"0008,1090\" : "
   "{"
      "\"Name\" : \"ManufacturerModelName\","
      "\"Type\" : \"String\","
      "\"Value\" : \"MyModel\""
   "},"
   "\"0008,1111\" : "
   "{"
      "\"Name\" : \"ReferencedPerformedProcedureStepSequence\","
      "\"Type\" : \"Sequence\","
      "\"Value\" : "
      "["
         "{"
            "\"0008,1150\" : "
            "{"
               "\"Name\" : \"ReferencedSOPClassUID\","
               "\"Type\" : \"String\","
               "\"Value\" : \"1.2.4\""
            "},"
            "\"0008,1155\" : "
            "{"
               "\"Name\" : \"ReferencedSOPInstanceUID\","
               "\"Type\" : \"String\","
               "\"Value\" : \"1.2.3\""
            "}"
         "}"
      "]"
   "}}";

  Json::Value parsedJson;
  bool ret = Toolbox::ReadJson(parsedJson, jsonFullString);
  
  m.FromDicomAsJson(parsedJson, false /* append */, true /* parseSequences*/);
  ASSERT_TRUE(ret);

  ASSERT_TRUE(m.HasTag(DicomTag(0x0008, 0x1090)));
  ASSERT_EQ("MyModel", m.GetValue(0x0008,0x1090).GetContent());

  ASSERT_TRUE(m.HasTag(DicomTag(0x0008, 0x1111)));
  const Json::Value& jsonSequence = m.GetValue(0x0008, 0x1111).GetSequenceContent();
  ASSERT_EQ("ReferencedSOPClassUID", jsonSequence[0]["0008,1150"]["Name"].asString());

  {// serialize to human dicomAsJson
    Json::Value dicomAsJson = Json::objectValue;
    FromDcmtkBridge::ToJson(dicomAsJson, m, DicomToJsonFormat_Human);
    // printf("%s", dicomAsJson.toStyledString().c_str());

    ASSERT_TRUE(dicomAsJson.isMember("ManufacturerModelName"));
    ASSERT_TRUE(dicomAsJson.isMember("ReferencedPerformedProcedureStepSequence"));
    ASSERT_TRUE(dicomAsJson["ReferencedPerformedProcedureStepSequence"][0].isMember("ReferencedSOPClassUID"));
    ASSERT_EQ("1.2.4", dicomAsJson["ReferencedPerformedProcedureStepSequence"][0]["ReferencedSOPClassUID"].asString());
  }

  {// serialize to full dicomAsJson
    Json::Value dicomAsJson = Json::objectValue;
    FromDcmtkBridge::ToJson(dicomAsJson, m, DicomToJsonFormat_Full);
    // printf("%s", dicomAsJson.toStyledString().c_str());

    ASSERT_TRUE(dicomAsJson.isMember("0008,1090"));
    ASSERT_TRUE(dicomAsJson.isMember("0008,1111"));
    ASSERT_TRUE(dicomAsJson["0008,1111"]["Value"][0].isMember("0008,1150"));
    ASSERT_EQ("1.2.4", dicomAsJson["0008,1111"]["Value"][0]["0008,1150"]["Value"].asString());
    ASSERT_EQ("MyModel", dicomAsJson["0008,1090"]["Value"].asString());
  }

  {// serialize to short dicomAsJson
    Json::Value dicomAsJson = Json::objectValue;
    FromDcmtkBridge::ToJson(dicomAsJson, m, DicomToJsonFormat_Short);
    // printf("%s", dicomAsJson.toStyledString().c_str());

    ASSERT_TRUE(dicomAsJson.isMember("0008,1090"));
    ASSERT_TRUE(dicomAsJson.isMember("0008,1111"));
    ASSERT_TRUE(dicomAsJson["0008,1111"][0].isMember("0008,1150"));
    ASSERT_EQ("1.2.4", dicomAsJson["0008,1111"][0]["0008,1150"].asString());
    ASSERT_EQ("MyModel", dicomAsJson["0008,1090"].asString());
  }

  {// extract sequence
    DicomMap sequencesOnly;
    m.ExtractSequences(sequencesOnly);

    ASSERT_EQ(1, sequencesOnly.GetSize());
    ASSERT_TRUE(sequencesOnly.HasTag(0x0008, 0x1111));
    ASSERT_TRUE(sequencesOnly.GetValue(0x0008, 0x1111).GetSequenceContent()[0].isMember("0008,1150"));

    // copy sequence
    DicomMap sequencesCopy;
    sequencesCopy.SetValue(0x0008, 0x1111, sequencesOnly.GetValue(0x0008, 0x1111));

    ASSERT_EQ(1, sequencesCopy.GetSize());
    ASSERT_TRUE(sequencesCopy.HasTag(0x0008, 0x1111));
    ASSERT_TRUE(sequencesCopy.GetValue(0x0008, 0x1111).GetSequenceContent()[0].isMember("0008,1150"));
  }
}

TEST(DicomMap, ExtractSummary)
{
  Json::Value v = Json::objectValue;
  v["PatientName"] = "Hello";
  v["ReferencedSOPClassUID"] = "1.2.840.10008.5.1.4.1.1.4";

  {
    Json::Value a = Json::arrayValue;

    {
      Json::Value item = Json::objectValue;
      item["ReferencedSOPClassUID"] = "1.2.840.10008.5.1.4.1.1.4";
      item["ReferencedSOPInstanceUID"] = "1.2.840.113619.2.176.2025.1499492.7040.1171286241.719";
      a.append(item);
    }
      
    {
      Json::Value item = Json::objectValue;
      item["ReferencedSOPClassUID"] = "1.2.840.10008.5.1.4.1.1.4";  // ReferencedSOPClassUID
      item["ReferencedSOPInstanceUID"] = "1.2.840.113619.2.176.2025.1499492.7040.1171286241.726";
      a.append(item);
    }
      
    v["ReferencedImageSequence"] = a;
  }
    
  {
    Json::Value a = Json::arrayValue;

    {
      Json::Value item = Json::objectValue;
      item["StudyInstanceUID"] = "1.2.840.113704.1.111.7016.1342451220.40";

      {
        Json::Value b = Json::arrayValue;

        {
          Json::Value c = Json::objectValue;
          c["CodeValue"] = "122403";
          c["0008,103e"] = "WORLD";  // Series description
          b.append(c);
        }

        item["PurposeOfReferenceCodeSequence"] = b;
      }
        
      a.append(item);
    }
      
    v["RelatedSeriesSequence"] = a;
  }

  std::unique_ptr<ParsedDicomFile> dicom(ParsedDicomFile::CreateFromJson(v, DicomFromJsonFlags_None, ""));

  DicomMap summary;
  std::set<DicomTag> ignoreTagLength;
  dicom->ExtractDicomSummary(summary, 256, ignoreTagLength);

  ASSERT_TRUE(summary.HasTag(0x0008, 0x1140));
  ASSERT_EQ("1.2.840.10008.5.1.4.1.1.4", summary.GetValue(0x0008, 0x1140).GetSequenceContent()[0]["0008,1150"]["Value"].asString());
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

    const std::set<DicomTag>& tags = DicomMap::GetMainDicomTags(level);

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

      std::string main = FromDcmtkBridge::GetTagName(*it, "");
      if (!main.empty())
      {
        ASSERT_EQ(main, name);
      }
    }
  }
}



TEST(DicomTag, Comparisons)
{
  DicomTag a(0x0000, 0x0000);
  DicomTag b(0x0010, 0x0010);
  DicomTag c(0x0010, 0x0020);
  DicomTag d(0x0020, 0x0000);

  // operator==()
  ASSERT_TRUE(a == a);
  ASSERT_FALSE(a == b);

  // operator!=()
  ASSERT_FALSE(a != a);
  ASSERT_TRUE(a != b);

  // operator<=()
  ASSERT_TRUE(a <= a);
  ASSERT_TRUE(a <= b);
  ASSERT_TRUE(a <= c);
  ASSERT_TRUE(a <= d);

  ASSERT_FALSE(b <= a);
  ASSERT_TRUE(b <= b);
  ASSERT_TRUE(b <= c);
  ASSERT_TRUE(b <= d);

  ASSERT_FALSE(c <= a);
  ASSERT_FALSE(c <= b);
  ASSERT_TRUE(c <= c);
  ASSERT_TRUE(c <= d);

  ASSERT_FALSE(d <= a);
  ASSERT_FALSE(d <= b);
  ASSERT_FALSE(d <= c);
  ASSERT_TRUE(d <= d);

  // operator<()
  ASSERT_FALSE(a < a);
  ASSERT_TRUE(a < b);
  ASSERT_TRUE(a < c);
  ASSERT_TRUE(a < d);

  ASSERT_FALSE(b < a);
  ASSERT_FALSE(b < b);
  ASSERT_TRUE(b < c);
  ASSERT_TRUE(b < d);

  ASSERT_FALSE(c < a);
  ASSERT_FALSE(c < b);
  ASSERT_FALSE(c < c);
  ASSERT_TRUE(c < d);

  ASSERT_FALSE(d < a);
  ASSERT_FALSE(d < b);
  ASSERT_FALSE(d < c);
  ASSERT_FALSE(d < d);

  // operator>=()
  ASSERT_TRUE(a >= a);
  ASSERT_FALSE(a >= b);
  ASSERT_FALSE(a >= c);
  ASSERT_FALSE(a >= d);

  ASSERT_TRUE(b >= a);
  ASSERT_TRUE(b >= b);
  ASSERT_FALSE(b >= c);
  ASSERT_FALSE(b >= d);

  ASSERT_TRUE(c >= a);
  ASSERT_TRUE(c >= b);
  ASSERT_TRUE(c >= c);
  ASSERT_FALSE(c >= d);

  ASSERT_TRUE(d >= a);
  ASSERT_TRUE(d >= b);
  ASSERT_TRUE(d >= c);
  ASSERT_TRUE(d >= d);

  // operator>()
  ASSERT_FALSE(a > a);
  ASSERT_FALSE(a > b);
  ASSERT_FALSE(a > c);
  ASSERT_FALSE(a > d);

  ASSERT_TRUE(b > a);
  ASSERT_FALSE(b > b);
  ASSERT_FALSE(b > c);
  ASSERT_FALSE(b > d);

  ASSERT_TRUE(c > a);
  ASSERT_TRUE(c > b);
  ASSERT_FALSE(c > c);
  ASSERT_FALSE(c > d);

  ASSERT_TRUE(d > a);
  ASSERT_TRUE(d > b);
  ASSERT_TRUE(d > c);
  ASSERT_FALSE(d > d);
}

TEST(ParsedDicomFile, canIncludeXsVrTags)
{
  Json::Value tags;
  tags["0028,0034"] = "1\\1";         // PixelAspectRatio
  tags["0028,1101"] = "256\\0\\16";   // RedPaletteColorLookupTableDescriptor which is declared as xs VR in dicom.dic

  std::unique_ptr<ParsedDicomFile> dicom(ParsedDicomFile::CreateFromJson(tags, DicomFromJsonFlags_DecodeDataUriScheme, ""));
  // simply make sure it does not throw !
}


#if ORTHANC_SANDBOXED != 1

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
  class V : public DicomStreamReader::IVisitor
  {
  private:
    DicomMap  map_;
    uint64_t  pixelDataOffset_;
    
  public:
    V() :
      pixelDataOffset_(0)
    {
    }
    
    const DicomMap& GetDicomMap() const
    {
      return map_;
    }
    
    virtual void VisitMetaHeaderTag(const DicomTag& tag,
                                    const ValueRepresentation& vr,
                                    const std::string& value) ORTHANC_OVERRIDE
    {
      std::cout << "Header: " << tag.Format() << " [" << Toolbox::ConvertToAscii(value).c_str() << "] (" << value.size() << ")" << std::endl;
    }

    virtual void VisitTransferSyntax(DicomTransferSyntax transferSyntax) ORTHANC_OVERRIDE
    {
      printf("TRANSFER SYNTAX: %s\n", GetTransferSyntaxUid(transferSyntax));
    }
    
    virtual bool VisitDatasetTag(const DicomTag& tag,
                                 const ValueRepresentation& vr,
                                 const std::string& value,
                                 bool isLittleEndian,
                                 uint64_t fileOffset) ORTHANC_OVERRIDE
    {
      if (!isLittleEndian)
        printf("** ");

      if (tag == DICOM_TAG_PIXEL_DATA)
      {
        std::cout << "Dataset: " << tag.Format() << " " << EnumerationToString(vr)
                  << " [PIXEL] (" << value.size() << "), offset: " << std::hex << fileOffset << std::dec << std::endl;
        pixelDataOffset_ = fileOffset;
        return false;
      }
      else
      {
        std::cout << "Dataset: " << tag.Format() << " " << EnumerationToString(vr)
                  << " [" << Toolbox::ConvertToAscii(value).c_str() << "] (" << value.size()
                  << "), offset: " << std::hex << fileOffset << std::dec << std::endl;
      }

      map_.SetValue(tag, value, Toolbox::IsAsciiString(value));
                                                            
      return true;
    }

    uint64_t GetPixelDataOffset() const
    {
      return pixelDataOffset_;
    }
  };
}



TEST(DicomStreamReader, DISABLED_Tutu)
{
  static const std::string PATH = "/home/jodogne/Subversion/orthanc-tests/Database/TransferSyntaxes/";

  typedef std::list< std::pair<std::string, uint64_t> >  Sources;

  Sources sources;
  sources.push_back(std::make_pair(PATH + "../ColorTestMalaterre.dcm", 0x03a0u));
  sources.push_back(std::make_pair(PATH + "1.2.840.10008.1.2.1.dcm", 0x037cu));
  sources.push_back(std::make_pair(PATH + "1.2.840.10008.1.2.2.dcm", 0x03e8u));  // Big Endian
  sources.push_back(std::make_pair(PATH + "1.2.840.10008.1.2.4.50.dcm", 0x04acu));
  sources.push_back(std::make_pair(PATH + "1.2.840.10008.1.2.4.51.dcm", 0x072cu));
  sources.push_back(std::make_pair(PATH + "1.2.840.10008.1.2.4.57.dcm", 0x0620u));
  sources.push_back(std::make_pair(PATH + "1.2.840.10008.1.2.4.70.dcm", 0x065au));
  sources.push_back(std::make_pair(PATH + "1.2.840.10008.1.2.4.80.dcm", 0x0b46u));
  sources.push_back(std::make_pair(PATH + "1.2.840.10008.1.2.4.81.dcm", 0x073eu));
  sources.push_back(std::make_pair(PATH + "1.2.840.10008.1.2.4.90.dcm", 0x0b66u));
  sources.push_back(std::make_pair(PATH + "1.2.840.10008.1.2.4.91.dcm", 0x19b8u));
  sources.push_back(std::make_pair(PATH + "1.2.840.10008.1.2.5.dcm", 0x0b0au));

  {
    std::string dicom;

    uint64_t offset;
    // Not a DICOM image
    SystemToolbox::ReadFile(dicom, PATH + "1.2.840.10008.1.2.4.50.png", false);
    ASSERT_FALSE(DicomStreamReader::LookupPixelDataOffset(offset, dicom));

    // Image without valid DICOM preamble
    SystemToolbox::ReadFile(dicom, PATH + "1.2.840.10008.1.2.dcm", false);
    ASSERT_FALSE(DicomStreamReader::LookupPixelDataOffset(offset, dicom));
  }
  
  for (Sources::const_iterator it = sources.begin(); it != sources.end(); ++it)
  {
    std::string dicom;
    SystemToolbox::ReadFile(dicom, it->first, false);

    {
      uint64_t offset;
      ASSERT_TRUE(DicomStreamReader::LookupPixelDataOffset(offset, dicom));
      ASSERT_EQ(it->second, offset);
    }
    
    {
      uint64_t offset;
      ASSERT_TRUE(DicomStreamReader::LookupPixelDataOffset(offset, dicom.c_str(), dicom.size()));
      ASSERT_EQ(it->second, offset);
    }
    
    ParsedDicomFile a(dicom);
    Json::Value aa;
    a.DatasetToJson(aa, DicomToJsonFormat_Short, DicomToJsonFlags_Default, 0);

    std::stringstream stream;
    size_t pos = 0;
  
    DicomStreamReader r(stream);
    V visitor;

    // Test reading byte per byte
    while (pos < dicom.size() &&
           !r.IsDone())
    {
      r.Consume(visitor);
      stream.clear();
      stream.put(dicom[pos++]);
    }

    r.Consume(visitor);

    ASSERT_EQ(it->second, visitor.GetPixelDataOffset());

    // Truncate the original DICOM up to pixel data
    dicom.resize(visitor.GetPixelDataOffset());

    ParsedDicomFile b(dicom);
    Json::Value bb;
    b.DatasetToJson(bb, DicomToJsonFormat_Short, DicomToJsonFlags_Default, 0);

    aa.removeMember("7fe0,0010");
    aa.removeMember("fffc,fffc");  // For "1.2.840.10008.1.2.5.dcm"
    ASSERT_EQ(aa.toStyledString(), bb.toStyledString());
  }
}

TEST(DicomStreamReader, DISABLED_Tutu2)
{
  //static const std::string PATH = "/home/jodogne/Subversion/orthanc-tests/Database/TransferSyntaxes/";

  //const std::string path = PATH + "1.2.840.10008.1.2.4.50.dcm";
  //const std::string path = PATH + "1.2.840.10008.1.2.2.dcm";
  const std::string path = "/home/jodogne/Subversion/orthanc-tests/Database/HierarchicalAnonymization/RTH/RT.dcm";
  
  std::ifstream stream(path.c_str());
  
  DicomStreamReader r(stream);
  V visitor;

  r.Consume(visitor);

  printf(">> %d\n", static_cast<int>(r.GetProcessedBytes()));
}


#include <boost/filesystem.hpp>

TEST(DicomStreamReader, DISABLED_Tutu3)
{
  static const std::string PATH = "/home/jodogne/Subversion/orthanc-tests/Database/";

  std::set<std::string> errors;
  unsigned int success = 0;
  
  for (boost::filesystem::recursive_directory_iterator current(PATH), end;
       current != end ; ++current)
  {
    if (SystemToolbox::IsRegularFile(current->path().string()))
    {
      try
      {
        if (current->path().extension() == ".dcm")
        {
          const std::string path = current->path().string();
          printf("[%s]\n", path.c_str());

          DicomMap m1;

          {
            std::ifstream stream(path.c_str());
            
            DicomStreamReader r(stream);
            V visitor;
            
            try
            {
              r.Consume(visitor, DICOM_TAG_PIXEL_DATA);
              //r.Consume(visitor);
              success++;
            }
            catch (OrthancException& e)
            {
              errors.insert(path);
              continue;
            }

            m1.Assign(visitor.GetDicomMap());
          }

          m1.SetValue(DICOM_TAG_PIXEL_DATA, "", true);


          DicomMap m2;
          
          {
            std::string dicom;
            SystemToolbox::ReadFile(dicom, path);
            
            ParsedDicomFile f(dicom);
            f.ExtractDicomSummary(m2, 256);
          }

          std::set<DicomTag> tags;
          m2.GetTags(tags);

          bool first = true;
          for (std::set<DicomTag>::const_iterator it = tags.begin(); it != tags.end(); ++it)
          {
            if (!m1.HasTag(*it))
            {
              if (first)
              {
                fprintf(stderr, "[%s]\n", path.c_str());
                first = false;
              }
              
              std::cerr << "ERROR: " << it->Format() << std::endl;
            }
            else if (!m2.GetValue(*it).IsNull() &&
                     !m2.GetValue(*it).IsBinary() &&
                     Toolbox::IsAsciiString(m1.GetValue(*it).GetContent()))
            {
              const std::string& v1 = m1.GetValue(*it).GetContent();
              const std::string& v2 = m2.GetValue(*it).GetContent();
              
              if (v1 != v2 &&
                  (v1.size() != v2.size() + 1 ||
                   v1.substr(0, v2.size()) != v2))
              {
                std::cerr << "ERROR: [" << v1 << "] [" << v2 << "]" << std::endl;
              }
            }
          }
        }
      }
      catch (boost::filesystem::filesystem_error&)
      {
      }
    }
  }


  printf("\n== ERRORS ==\n");
  for (std::set<std::string>::const_iterator
         it = errors.begin(); it != errors.end(); ++it)
  {
    printf("[%s]\n", it->c_str());
  }

  printf("\n== SUCCESSES: %u ==\n\n", success);
}

#endif
