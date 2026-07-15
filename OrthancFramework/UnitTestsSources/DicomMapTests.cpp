/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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
#include <boost/tuple/tuple.hpp>

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
      std::set<DicomTag> s;
      DicomMap::GetAllMainDicomTags(s);
      ASSERT_TRUE(s.end() != s.find(DICOM_TAG_PATIENT_ID));
      ASSERT_TRUE(s.end() != s.find(DICOM_TAG_STUDY_INSTANCE_UID));
      ASSERT_TRUE(s.end() != s.find(DICOM_TAG_ACCESSION_NUMBER));
      ASSERT_TRUE(s.end() != s.find(DICOM_TAG_SERIES_INSTANCE_UID));
      ASSERT_TRUE(s.end() != s.find(DICOM_TAG_SOP_INSTANCE_UID));
    }

    {
      std::set<DicomTag> s;
      DicomMap::GetMainDicomTags(s, ResourceType_Patient);
      ASSERT_TRUE(s.end() != s.find(DICOM_TAG_PATIENT_ID));
      ASSERT_TRUE(s.end() == s.find(DICOM_TAG_STUDY_INSTANCE_UID));
    }

    {
      std::set<DicomTag> s;
      DicomMap::GetMainDicomTags(s, ResourceType_Study);
      ASSERT_TRUE(s.end() != s.find(DICOM_TAG_STUDY_INSTANCE_UID));
      ASSERT_TRUE(s.end() != s.find(DICOM_TAG_ACCESSION_NUMBER));
      ASSERT_TRUE(s.end() == s.find(DICOM_TAG_PATIENT_ID));
    }

    {
      std::set<DicomTag> s;
      DicomMap::GetMainDicomTags(s, ResourceType_Series);
      ASSERT_TRUE(s.end() != s.find(DICOM_TAG_SERIES_INSTANCE_UID));
      ASSERT_TRUE(s.end() == s.find(DICOM_TAG_PATIENT_ID));
    }

    {
      std::set<DicomTag> s;
      DicomMap::GetMainDicomTags(s, ResourceType_Instance);
      ASSERT_TRUE(s.end() != s.find(DICOM_TAG_SOP_INSTANCE_UID));
      ASSERT_TRUE(s.end() == s.find(DICOM_TAG_PATIENT_ID));
    }
  }

  TEST_F(DicomMapMainTagsTests, AddMainTags)
  {
    DicomMap::AddMainDicomTag(DICOM_TAG_BITS_ALLOCATED, ResourceType_Instance);

    {
      std::set<DicomTag> s;
      DicomMap::GetMainDicomTags(s, ResourceType_Instance);
      ASSERT_TRUE(s.end() != s.find(DICOM_TAG_BITS_ALLOCATED));
      ASSERT_TRUE(s.end() != s.find(DICOM_TAG_SOP_INSTANCE_UID));
    }
    {
      std::set<DicomTag> s;
      DicomMap::GetMainDicomTags(s, ResourceType_Series);
      ASSERT_TRUE(s.end() == s.find(DICOM_TAG_BITS_ALLOCATED));
    }

    ASSERT_TRUE(DicomMap::IsMainDicomTag(DICOM_TAG_BITS_ALLOCATED));
    ASSERT_TRUE(DicomMap::IsMainDicomTag(DICOM_TAG_BITS_ALLOCATED, ResourceType_Instance));

    // adding the same tag should throw
    ASSERT_THROW(DicomMap::AddMainDicomTag(DICOM_TAG_BITS_ALLOCATED, ResourceType_Instance), OrthancException);

  }

  TEST_F(DicomMapMainTagsTests, Signatures)
  {
    std::string defaultPatientSignature = DicomMap::GetDefaultMainDicomTagsSignatureFrom1_11(ResourceType_Patient);
    std::string defaultStudySignature = DicomMap::GetDefaultMainDicomTagsSignatureFrom1_11(ResourceType_Study);
    std::string defaultSeriesSignature = DicomMap::GetDefaultMainDicomTagsSignatureFrom1_11(ResourceType_Series);
    std::string defaultInstanceSignature = DicomMap::GetDefaultMainDicomTagsSignatureFrom1_11(ResourceType_Instance);

    ASSERT_NE(defaultInstanceSignature, defaultPatientSignature);
    ASSERT_NE(defaultSeriesSignature, defaultStudySignature);
    ASSERT_NE(defaultSeriesSignature, defaultPatientSignature);

    // std::string patientSignature = DicomMap::GetMainDicomTagsSignature(ResourceType_Patient);
    // std::string studySignature = DicomMap::GetMainDicomTagsSignature(ResourceType_Study);
    // std::string seriesSignature = DicomMap::GetMainDicomTagsSignature(ResourceType_Series);
    std::string instanceSignature = DicomMap::GetMainDicomTagsSignature(ResourceType_Instance);

    // // at start, default and current signature should be equal  !! This is not true anymore since we have added new MainDicomTags in 1.12.5
    // ASSERT_EQ(defaultPatientSignature, patientSignature);
    // ASSERT_EQ(defaultStudySignature, studySignature);
    // ASSERT_EQ(defaultSeriesSignature, seriesSignature);
    // ASSERT_EQ(defaultInstanceSignature, instanceSignature);

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

  std::set<DicomTag> main;
  DicomMap::GetMainDicomTags(main, level);

  std::set<DicomTag> moduleTags;
  DicomTag::AddTagsForModule(moduleTags, module);
  
  // The main dicom tags are a subset of the module
  for (std::set<DicomTag>::const_iterator it = main.begin(); it != main.end(); ++it)
  {
    bool ok = moduleTags.find(*it) != moduleTags.end();

    // Exceptions for the Study level
    if (level == ResourceType_Study &&
        (*it == DicomTag(0x0008, 0x0080) ||  /* InstitutionName, from Visit identification module, related to Visit */
         *it == DicomTag(0x0032, 0x1032) ||  /* RequestingPhysician, from Imaging Service Request module, related to Study */
         *it == DicomTag(0x0008, 0x0201) ||  /* TimezoneOffsetFromUTC */
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
         *it == DicomTag(0x0008, 0x0201) ||  /* TimezoneOffsetFromUTC */
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
  m.SetValue(DICOM_TAG_COLUMNS, "800\\0", false);
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

    ASSERT_EQ(1u, sequencesOnly.GetSize());
    ASSERT_TRUE(sequencesOnly.HasTag(0x0008, 0x1111));
    ASSERT_TRUE(sequencesOnly.GetValue(0x0008, 0x1111).GetSequenceContent()[0].isMember("0008,1150"));

    // copy sequence
    DicomMap sequencesCopy;
    sequencesCopy.SetValue(0x0008, 0x1111, sequencesOnly.GetValue(0x0008, 0x1111));

    ASSERT_EQ(1u, sequencesCopy.GetSize());
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
  dicom.ReplacePlainString(DICOM_TAG_PIXEL_SPACING, "0,143\\0,143");  // seen in https://discourse.orthanc-server.org/t/dicomwebplugin-does-not-return-series-metadata-properly/5195

  DicomWebJsonVisitor visitor;
  dicom.Apply(visitor);

  {
    const Json::Value& tag = visitor.GetResult() ["00200037"];  // ImageOrientationPatient
    const Json::Value& value = tag["Value"];
  
    ASSERT_EQ(EnumerationToString(ValueRepresentation_DecimalString), tag["vr"].asString());
    ASSERT_EQ(2u, tag.getMemberNames().size());
    ASSERT_EQ(3u, value.size());
    ASSERT_EQ(Json::stringValue, value[1].type());  // since Orthanc 1.12.5, this is now stored as a string
    ASSERT_EQ("1", value[0].asString());
    ASSERT_EQ("2.3", value[1].asString());
    ASSERT_EQ("4", value[2].asString());
  }

  {
    const Json::Value& tag = visitor.GetResult() ["00200032"];  // ImagePositionPatient
    ASSERT_EQ(EnumerationToString(ValueRepresentation_DecimalString), tag["vr"].asString());
    ASSERT_EQ(1u, tag.getMemberNames().size());
  }

  {
    const Json::Value& tag = visitor.GetResult() ["00280030"];  // PixelSpacing
    const Json::Value& value = tag["Value"];

    ASSERT_EQ(EnumerationToString(ValueRepresentation_DecimalString), tag["vr"].asString());
    ASSERT_EQ(2u, value.size());
    ASSERT_EQ("0.143", value[0].asString());
    ASSERT_EQ("0.143", value[1].asString());
  }

  std::string xml;
  visitor.FormatXml(xml);

  {
    DicomMap m;
    m.FromDicomWeb(visitor.GetResult());
    ASSERT_EQ(4u, m.GetSize());

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
    ASSERT_EQ(Json::stringValue, value[0].type());
    ASSERT_EQ(Json::nullValue, value[1].type());
    ASSERT_EQ(Json::nullValue, value[2].type());
    ASSERT_EQ(Json::stringValue, value[3].type());
    ASSERT_EQ("1.5", value[0].asString());
    ASSERT_EQ("2.5", value[3].asString());
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
  ASSERT_EQ("1.5", visitor.GetResult() ["00280030"]["Value"][0].asString());
  ASSERT_EQ("1.3", visitor.GetResult() ["00280030"]["Value"][1].asString());

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


TEST(DicomMap, SetupFindTemplates)
{
  /**
   * The templates for C-FIND must be common to all the Orthanc
   * servers, and must not be altered by the "ExtraMainDicomTags"
   * configuration option that was introduced in Orthanc 1.11.0.
   **/
  
  {
    DicomMap m;
    m.SetValue(DICOM_TAG_ENCAPSULATED_DOCUMENT, "nope", false);
    m.SetValue(DICOM_TAG_PATIENT_ID, "patient_id", false);
    
    DicomMap::SetupFindPatientTemplate(m);
    std::set<DicomTag> tags;
    m.GetTags(tags);

    // This corresponds to the values of DEFAULT_1_11_PATIENT_MAIN_DICOM_TAGS
    ASSERT_EQ(5u, tags.size());
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_PATIENT_ID, "nope", false));

    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_OTHER_PATIENT_IDS, "nope", false));
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_PATIENT_BIRTH_DATE, "nope", false));
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_PATIENT_NAME, "nope", false));
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_PATIENT_SEX, "nope", false));
  }
  
  {
    DicomMap m;
    m.SetValue(DICOM_TAG_ENCAPSULATED_DOCUMENT, "nope", false);
    m.SetValue(DICOM_TAG_PATIENT_ID, "patient_id", false);
    
    DicomMap::SetupFindStudyTemplate(m);
    std::set<DicomTag> tags;
    m.GetTags(tags);

    // This corresponds to the values of DEFAULT_STUDY_MAIN_DICOM_TAGS
    ASSERT_EQ(8u, tags.size());
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_PATIENT_ID, "nope", false));
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_ACCESSION_NUMBER, "nope", false));
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_STUDY_INSTANCE_UID, "nope", false));

    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_REFERRING_PHYSICIAN_NAME, "nope", false));
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_STUDY_DATE, "nope", false));
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_STUDY_DESCRIPTION, "nope", false));
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_STUDY_ID, "nope", false));
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_STUDY_TIME, "nope", false));
  }
  
  {
    DicomMap m;
    m.SetValue(DICOM_TAG_ENCAPSULATED_DOCUMENT, "nope", false);
    m.SetValue(DICOM_TAG_PATIENT_ID, "patient_id", false);
    
    DicomMap::SetupFindSeriesTemplate(m);
    std::set<DicomTag> tags;
    m.GetTags(tags);

    // This corresponds to the values of DEFAULT_SERIES_MAIN_DICOM_TAGS
    ASSERT_EQ(13u, tags.size());
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_PATIENT_ID, "nope", false));
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_ACCESSION_NUMBER, "nope", false));
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_STUDY_INSTANCE_UID, "nope", false));
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_SERIES_INSTANCE_UID, "nope", false));

    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_BODY_PART_EXAMINED, "nope", false));
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_MODALITY, "nope", false));
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_OPERATOR_NAME, "nope", false));
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_PERFORMED_PROCEDURE_STEP_DESCRIPTION, "nope", false));
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_PROTOCOL_NAME, "nope", false));
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_SERIES_DATE, "nope", false));
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_SERIES_DESCRIPTION, "nope", false));
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_SERIES_NUMBER, "nope", false));
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_SERIES_TIME, "nope", false));
  }
  
  {
    DicomMap m;
    m.SetValue(DICOM_TAG_ENCAPSULATED_DOCUMENT, "nope", false);
    m.SetValue(DICOM_TAG_PATIENT_ID, "patient_id", false);
    
    DicomMap::SetupFindInstanceTemplate(m);
    std::set<DicomTag> tags;
    m.GetTags(tags);

    // This corresponds to the values of DEFAULT_INSTANCE_MAIN_DICOM_TAGS
    ASSERT_EQ(15u, tags.size());
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_PATIENT_ID, "nope", false));
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_ACCESSION_NUMBER, "nope", false));
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_STUDY_INSTANCE_UID, "nope", false));
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_SERIES_INSTANCE_UID, "nope", false));
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_SOP_INSTANCE_UID, "nope", false));

    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_ACQUISITION_NUMBER, "nope", false));
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_IMAGE_COMMENTS, "nope", false));
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_IMAGE_INDEX, "nope", false));
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_IMAGE_ORIENTATION_PATIENT, "nope", false));
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_IMAGE_POSITION_PATIENT, "nope", false));
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_INSTANCE_CREATION_DATE, "nope", false));
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_INSTANCE_CREATION_TIME, "nope", false));
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_INSTANCE_NUMBER, "nope", false));
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_NUMBER_OF_FRAMES, "nope", false));
    ASSERT_EQ("", m.GetStringValue(DICOM_TAG_TEMPORAL_POSITION_IDENTIFIER, "nope", false));
  }
}


// Mock formatter for testing OW/OL/OF/OD binary encoding modes
namespace
{
  class MockBinaryFormatter : public DicomWebJsonVisitor::IBinaryFormatter
  {
  private:
    DicomWebJsonVisitor::BinaryMode  mode_;

  public:
    explicit MockBinaryFormatter(DicomWebJsonVisitor::BinaryMode mode) :
      mode_(mode)
    {
    }

    virtual DicomWebJsonVisitor::BinaryMode Format(std::string& bulkDataUri,
                                                   const std::vector<DicomTag>& parentTags,
                                                   const std::vector<size_t>& parentIndexes,
                                                   const DicomTag& tag,
                                                   ValueRepresentation vr) ORTHANC_OVERRIDE
    {
      if (mode_ == DicomWebJsonVisitor::BinaryMode_BulkDataUri)
      {
        bulkDataUri = "http://bulk/" + tag.Format();
      }
      else
      {
        bulkDataUri.clear();
      }

      return mode_;
    }
  };
}


TEST(DicomWebJson, VisitIntegers)
{
  std::vector<DicomTag> parentTags;
  std::vector<size_t> parentIndexes;
  DicomTag tag(0x0066, 0x0041);  // Long Triangle Point Index List (OL, but doesn't matter for this test)

  std::set<ValueRepresentation> vr;
  vr.insert(ValueRepresentation_OtherWord);
  vr.insert(ValueRepresentation_OtherLong);

#if DCMTK_VERSION_NUMBER >= 365
  vr.insert(ValueRepresentation_OtherVeryLong);
#endif

  // Firstly, test with an empty array of values

  std::vector<int64_t> values;

  for (std::set<ValueRepresentation>::const_iterator it = vr.begin(); it != vr.end(); ++it)
  {
    {
      DicomWebJsonVisitor visitor;
      MockBinaryFormatter formatter(DicomWebJsonVisitor::BinaryMode_Ignore);
      visitor.SetFormatter(formatter);
      visitor.VisitIntegers(parentTags, parentIndexes, tag, *it, values);

      const Json::Value& result = visitor.GetResult();
      ASSERT_FALSE(result.isMember("00660041"));
    }

    {
      DicomWebJsonVisitor visitor;
      MockBinaryFormatter formatter(DicomWebJsonVisitor::BinaryMode_BulkDataUri);
      visitor.SetFormatter(formatter);
      visitor.VisitIntegers(parentTags, parentIndexes, tag, *it, values);

      const Json::Value& result = visitor.GetResult();
      ASSERT_TRUE(result.isMember("00660041"));
      ASSERT_EQ(EnumerationToString(*it), result["00660041"]["vr"].asString());
      ASSERT_FALSE(result["00660041"].isMember("BulkDataURI"));
      ASSERT_FALSE(result["00660041"].isMember("Value"));
      ASSERT_FALSE(result["00660041"].isMember("InlineBinary"));
    }

    {
      DicomWebJsonVisitor visitor;
      MockBinaryFormatter formatter(DicomWebJsonVisitor::BinaryMode_ArrayOfValues);
      visitor.SetFormatter(formatter);
      visitor.VisitIntegers(parentTags, parentIndexes, tag, *it, values);

      const Json::Value& result = visitor.GetResult();
      ASSERT_TRUE(result.isMember("00660041"));
      ASSERT_EQ(EnumerationToString(*it), result["00660041"]["vr"].asString());
      ASSERT_FALSE(result["00660041"].isMember("BulkDataURI"));
      ASSERT_FALSE(result["00660041"].isMember("Value"));
      ASSERT_FALSE(result["00660041"].isMember("InlineBinary"));
    }

    {
      DicomWebJsonVisitor visitor;
      MockBinaryFormatter formatter(DicomWebJsonVisitor::BinaryMode_InlineBinary);
      visitor.SetFormatter(formatter);
      visitor.VisitIntegers(parentTags, parentIndexes, tag, *it, values);

      const Json::Value& result = visitor.GetResult();
      ASSERT_TRUE(result.isMember("00660041"));
      ASSERT_EQ(EnumerationToString(*it), result["00660041"]["vr"].asString());
      ASSERT_FALSE(result["00660041"].isMember("InlineBinary"));
      ASSERT_FALSE(result["00660041"].isMember("Value"));
      ASSERT_FALSE(result["00660041"].isMember("BulkDataURI"));
    }
  }

  // Secondly, test with non-empty array of values

  values.push_back(100);
  values.push_back(200);
  values.push_back(300);

  for (std::set<ValueRepresentation>::const_iterator it = vr.begin(); it != vr.end(); ++it)
  {
    {
      DicomWebJsonVisitor visitor;
      MockBinaryFormatter formatter(DicomWebJsonVisitor::BinaryMode_Ignore);
      visitor.SetFormatter(formatter);
      visitor.VisitIntegers(parentTags, parentIndexes, tag, *it, values);

      const Json::Value& result = visitor.GetResult();
      ASSERT_FALSE(result.isMember("00660041"));
    }

    {
      DicomWebJsonVisitor visitor;
      MockBinaryFormatter formatter(DicomWebJsonVisitor::BinaryMode_BulkDataUri);
      visitor.SetFormatter(formatter);
      visitor.VisitIntegers(parentTags, parentIndexes, tag, *it, values);

      const Json::Value& result = visitor.GetResult();
      ASSERT_TRUE(result.isMember("00660041"));
      ASSERT_EQ(EnumerationToString(*it), result["00660041"]["vr"].asString());
      ASSERT_TRUE(result["00660041"].isMember("BulkDataURI"));
      ASSERT_EQ("http://bulk/0066,0041", result["00660041"]["BulkDataURI"].asString());
      ASSERT_FALSE(result["00660041"].isMember("Value"));
      ASSERT_FALSE(result["00660041"].isMember("InlineBinary"));
    }

    {
      DicomWebJsonVisitor visitor;
      MockBinaryFormatter formatter(DicomWebJsonVisitor::BinaryMode_ArrayOfValues);
      visitor.SetFormatter(formatter);
      visitor.VisitIntegers(parentTags, parentIndexes, tag, *it, values);

      const Json::Value& result = visitor.GetResult();
      ASSERT_TRUE(result.isMember("00660041"));
      ASSERT_EQ(EnumerationToString(*it), result["00660041"]["vr"].asString());
      ASSERT_FALSE(result["00660041"].isMember("BulkDataURI"));
      ASSERT_TRUE(result["00660041"].isMember("Value"));
      ASSERT_FALSE(result["00660041"].isMember("InlineBinary"));
      ASSERT_EQ(3u, result["00660041"]["Value"].size());
      ASSERT_EQ(100, result["00660041"]["Value"][0].asInt());
      ASSERT_EQ(200, result["00660041"]["Value"][1].asInt());
      ASSERT_EQ(300, result["00660041"]["Value"][2].asInt());
    }
  }

  {
    DicomWebJsonVisitor visitor;
    MockBinaryFormatter formatter(DicomWebJsonVisitor::BinaryMode_InlineBinary);
    visitor.SetFormatter(formatter);
    visitor.VisitIntegers(parentTags, parentIndexes, tag, ValueRepresentation_OtherWord, values);

    const Json::Value& result = visitor.GetResult();
    ASSERT_TRUE(result.isMember("00660041"));
    ASSERT_EQ("OW", result["00660041"]["vr"].asString());
    ASSERT_TRUE(result["00660041"].isMember("InlineBinary"));
    ASSERT_FALSE(result["00660041"].isMember("Value"));
    ASSERT_FALSE(result["00660041"].isMember("BulkDataURI"));

    /**
     * "Other" transfer syntaxes must use Little Endian:
     * https://dicom.nema.org/medical/dicom/current/output/html/part18.html#sect_F.2.7
     * https://dicom.nema.org/medical/dicom/current/output/html/part05.html#sect_7.3
     **/
    ASSERT_EQ("ZADIACwB", result["00660041"]["InlineBinary"].asString());

    std::string decoded;
    Toolbox::DecodeBase64(decoded, result["00660041"]["InlineBinary"].asString());
    ASSERT_EQ(3 * sizeof(uint16_t), decoded.size());

    if (Toolbox::DetectEndianness() == Endianness_Big)
    {
      std::swap(decoded[0], decoded[1]);
      std::swap(decoded[2], decoded[3]);
      std::swap(decoded[4], decoded[5]);
    }

    uint16_t f0, f1, f2;
    memcpy(&f0, decoded.c_str(), sizeof(uint16_t));
    memcpy(&f1, decoded.c_str() + sizeof(uint16_t), sizeof(uint16_t));
    memcpy(&f2, decoded.c_str() + 2 * sizeof(uint16_t), sizeof(uint16_t));
    ASSERT_EQ(100, f0);
    ASSERT_EQ(200, f1);
    ASSERT_EQ(300, f2);
  }

  {
    DicomWebJsonVisitor visitor;
    MockBinaryFormatter formatter(DicomWebJsonVisitor::BinaryMode_InlineBinary);
    visitor.SetFormatter(formatter);
    visitor.VisitIntegers(parentTags, parentIndexes, tag, ValueRepresentation_OtherLong, values);

    const Json::Value& result = visitor.GetResult();
    ASSERT_TRUE(result.isMember("00660041"));
    ASSERT_EQ("OL", result["00660041"]["vr"].asString());
    ASSERT_TRUE(result["00660041"].isMember("InlineBinary"));
    ASSERT_FALSE(result["00660041"].isMember("Value"));
    ASSERT_FALSE(result["00660041"].isMember("BulkDataURI"));

    ASSERT_EQ("ZAAAAMgAAAAsAQAA", result["00660041"]["InlineBinary"].asString());

    std::string decoded;
    Toolbox::DecodeBase64(decoded, result["00660041"]["InlineBinary"].asString());
    ASSERT_EQ(3 * sizeof(uint32_t), decoded.size());

    if (Toolbox::DetectEndianness() == Endianness_Big)
    {
      Toolbox::SwapEndianness(decoded, sizeof(uint32_t));
    }

    uint32_t f0, f1, f2;
    memcpy(&f0, decoded.c_str(), sizeof(uint32_t));
    memcpy(&f1, decoded.c_str() + sizeof(uint32_t), sizeof(uint32_t));
    memcpy(&f2, decoded.c_str() + 2 * sizeof(uint32_t), sizeof(uint32_t));
    ASSERT_EQ(100, f0);
    ASSERT_EQ(200, f1);
    ASSERT_EQ(300, f2);
  }

#if DCMTK_VERSION_NUMBER >= 365
  {
    DicomWebJsonVisitor visitor;
    MockBinaryFormatter formatter(DicomWebJsonVisitor::BinaryMode_InlineBinary);
    visitor.SetFormatter(formatter);
    visitor.VisitIntegers(parentTags, parentIndexes, tag, ValueRepresentation_OtherVeryLong, values);

    const Json::Value& result = visitor.GetResult();
    ASSERT_TRUE(result.isMember("00660041"));
    ASSERT_EQ("OV", result["00660041"]["vr"].asString());
    ASSERT_TRUE(result["00660041"].isMember("InlineBinary"));
    ASSERT_FALSE(result["00660041"].isMember("Value"));
    ASSERT_FALSE(result["00660041"].isMember("BulkDataURI"));

    ASSERT_EQ("ZAAAAAAAAADIAAAAAAAAACwBAAAAAAAA", result["00660041"]["InlineBinary"].asString());

    std::string decoded;
    Toolbox::DecodeBase64(decoded, result["00660041"]["InlineBinary"].asString());
    ASSERT_EQ(3 * sizeof(uint64_t), decoded.size());

    if (Toolbox::DetectEndianness() == Endianness_Big)
    {
      Toolbox::SwapEndianness(decoded, sizeof(uint64_t));
    }

    uint64_t f0, f1, f2;
    memcpy(&f0, decoded.c_str(), sizeof(uint64_t));
    memcpy(&f1, decoded.c_str() + sizeof(uint64_t), sizeof(uint64_t));
    memcpy(&f2, decoded.c_str() + 2 * sizeof(uint64_t), sizeof(uint64_t));
    ASSERT_EQ(100, f0);
    ASSERT_EQ(200, f1);
    ASSERT_EQ(300, f2);
  }
#endif
}


TEST(DicomWebJson, VisitDoubles)
{
  std::vector<DicomTag> parentTags;
  std::vector<size_t> parentIndexes;
  DicomTag tag(0x0066, 0x0027);  // Triangle Point Index List (OF, but doesn't matter for this test)

  std::set<ValueRepresentation> vr;
  vr.insert(ValueRepresentation_OtherFloat);
  vr.insert(ValueRepresentation_OtherDouble);

  // Firstly, test with an empty array of values

  std::vector<double> values;

  for (std::set<ValueRepresentation>::const_iterator it = vr.begin(); it != vr.end(); ++it)
  {
    {
      DicomWebJsonVisitor visitor;
      MockBinaryFormatter formatter(DicomWebJsonVisitor::BinaryMode_Ignore);
      visitor.SetFormatter(formatter);
      visitor.VisitDoubles(parentTags, parentIndexes, tag, *it, values);

      const Json::Value& result = visitor.GetResult();
      ASSERT_FALSE(result.isMember("00660027"));
    }

    {
      DicomWebJsonVisitor visitor;
      MockBinaryFormatter formatter(DicomWebJsonVisitor::BinaryMode_BulkDataUri);
      visitor.SetFormatter(formatter);
      visitor.VisitDoubles(parentTags, parentIndexes, tag, *it, values);

      const Json::Value& result = visitor.GetResult();
      ASSERT_TRUE(result.isMember("00660027"));
      ASSERT_EQ(EnumerationToString(*it), result["00660027"]["vr"].asString());
      ASSERT_FALSE(result["00660027"].isMember("BulkDataURI"));
      ASSERT_FALSE(result["00660027"].isMember("Value"));
      ASSERT_FALSE(result["00660027"].isMember("InlineBinary"));
    }

    {
      DicomWebJsonVisitor visitor;
      MockBinaryFormatter formatter(DicomWebJsonVisitor::BinaryMode_ArrayOfValues);
      visitor.SetFormatter(formatter);
      visitor.VisitDoubles(parentTags, parentIndexes, tag, *it, values);

      const Json::Value& result = visitor.GetResult();
      ASSERT_TRUE(result.isMember("00660027"));
      ASSERT_EQ(EnumerationToString(*it), result["00660027"]["vr"].asString());
      ASSERT_FALSE(result["00660027"].isMember("BulkDataURI"));
      ASSERT_FALSE(result["00660027"].isMember("Value"));
      ASSERT_FALSE(result["00660027"].isMember("InlineBinary"));
    }

    {
      DicomWebJsonVisitor visitor;
      MockBinaryFormatter formatter(DicomWebJsonVisitor::BinaryMode_InlineBinary);
      visitor.SetFormatter(formatter);
      visitor.VisitDoubles(parentTags, parentIndexes, tag, *it, values);

      const Json::Value& result = visitor.GetResult();
      ASSERT_TRUE(result.isMember("00660027"));
      ASSERT_EQ(EnumerationToString(*it), result["00660027"]["vr"].asString());
      ASSERT_FALSE(result["00660027"].isMember("InlineBinary"));
      ASSERT_FALSE(result["00660027"].isMember("Value"));
      ASSERT_FALSE(result["00660027"].isMember("BulkDataURI"));
    }
  }

  // Secondly, test with non-empty array of values

  values.push_back(1.5);
  values.push_back(2.5);

  for (std::set<ValueRepresentation>::const_iterator it = vr.begin(); it != vr.end(); ++it)
  {
    {
      DicomWebJsonVisitor visitor;
      MockBinaryFormatter formatter(DicomWebJsonVisitor::BinaryMode_Ignore);
      visitor.SetFormatter(formatter);
      visitor.VisitDoubles(parentTags, parentIndexes, tag, *it, values);

      const Json::Value& result = visitor.GetResult();
      ASSERT_FALSE(result.isMember("00660027"));
    }

    {
      DicomWebJsonVisitor visitor;
      MockBinaryFormatter formatter(DicomWebJsonVisitor::BinaryMode_BulkDataUri);
      visitor.SetFormatter(formatter);
      visitor.VisitDoubles(parentTags, parentIndexes, tag, *it, values);

      const Json::Value& result = visitor.GetResult();
      ASSERT_TRUE(result.isMember("00660027"));
      ASSERT_EQ(EnumerationToString(*it), result["00660027"]["vr"].asString());
      ASSERT_TRUE(result["00660027"].isMember("BulkDataURI"));
      ASSERT_EQ("http://bulk/0066,0027", result["00660027"]["BulkDataURI"].asString());
      ASSERT_FALSE(result["00660027"].isMember("Value"));
      ASSERT_FALSE(result["00660027"].isMember("InlineBinary"));
    }

    {
      DicomWebJsonVisitor visitor;
      MockBinaryFormatter formatter(DicomWebJsonVisitor::BinaryMode_ArrayOfValues);
      visitor.SetFormatter(formatter);
      visitor.VisitDoubles(parentTags, parentIndexes, tag, *it, values);

      const Json::Value& result = visitor.GetResult();
      ASSERT_TRUE(result.isMember("00660027"));
      ASSERT_EQ(EnumerationToString(*it), result["00660027"]["vr"].asString());
      ASSERT_FALSE(result["00660027"].isMember("BulkDataURI"));
      ASSERT_TRUE(result["00660027"].isMember("Value"));
      ASSERT_FALSE(result["00660027"].isMember("InlineBinary"));
      ASSERT_EQ(2u, result["00660027"]["Value"].size());
      ASSERT_FLOAT_EQ(1.5f, result["00660027"]["Value"][0].asFloat());
      ASSERT_FLOAT_EQ(2.5f, result["00660027"]["Value"][1].asFloat());
    }
  }

  {
    DicomWebJsonVisitor visitor;
    MockBinaryFormatter formatter(DicomWebJsonVisitor::BinaryMode_InlineBinary);
    visitor.SetFormatter(formatter);
    visitor.VisitDoubles(parentTags, parentIndexes, tag, ValueRepresentation_OtherFloat, values);

    const Json::Value& result = visitor.GetResult();
    ASSERT_TRUE(result.isMember("00660027"));
    ASSERT_EQ("OF", result["00660027"]["vr"].asString());
    ASSERT_TRUE(result["00660027"].isMember("InlineBinary"));
    ASSERT_FALSE(result["00660027"].isMember("Value"));
    ASSERT_FALSE(result["00660027"].isMember("BulkDataURI"));

    /**
     * "Other" transfer syntaxes must use Little Endian:
     * https://dicom.nema.org/medical/dicom/current/output/html/part18.html#sect_F.2.7
     * https://dicom.nema.org/medical/dicom/current/output/html/part05.html#sect_7.3
     **/
    ASSERT_EQ("AADAPwAAIEA=", result["00660027"]["InlineBinary"].asString());

    std::string decoded;
    Toolbox::DecodeBase64(decoded, result["00660027"]["InlineBinary"].asString());
    ASSERT_EQ(2 * sizeof(float), decoded.size());

    if (Toolbox::DetectEndianness() == Endianness_Big)
    {
      Toolbox::SwapEndianness(decoded, sizeof(float));
    }

    float f0, f1;
    memcpy(&f0, decoded.c_str(), sizeof(float));
    memcpy(&f1, decoded.c_str() + sizeof(float), sizeof(float));
    ASSERT_FLOAT_EQ(1.5f, f0);
    ASSERT_FLOAT_EQ(2.5f, f1);
  }

  {
    DicomWebJsonVisitor visitor;
    MockBinaryFormatter formatter(DicomWebJsonVisitor::BinaryMode_InlineBinary);
    visitor.SetFormatter(formatter);
    visitor.VisitDoubles(parentTags, parentIndexes, tag, ValueRepresentation_OtherDouble, values);

    const Json::Value& result = visitor.GetResult();
    ASSERT_TRUE(result.isMember("00660027"));
    ASSERT_EQ("OD", result["00660027"]["vr"].asString());
    ASSERT_TRUE(result["00660027"].isMember("InlineBinary"));
    ASSERT_FALSE(result["00660027"].isMember("Value"));
    ASSERT_FALSE(result["00660027"].isMember("BulkDataURI"));

    ASSERT_EQ("AAAAAAAA+D8AAAAAAAAEQA==", result["00660027"]["InlineBinary"].asString());

    std::string decoded;
    Toolbox::DecodeBase64(decoded, result["00660027"]["InlineBinary"].asString());
    ASSERT_EQ(2 * sizeof(double), decoded.size());

    if (Toolbox::DetectEndianness() == Endianness_Big)
    {
      Toolbox::SwapEndianness(decoded, sizeof(double));
    }

    double f0, f1;
    memcpy(&f0, decoded.c_str(), sizeof(double));
    memcpy(&f1, decoded.c_str() + sizeof(double), sizeof(double));
    ASSERT_DOUBLE_EQ(1.5f, f0);
    ASSERT_DOUBLE_EQ(2.5f, f1);
  }
}


TEST(DicomWebJson, OnlyOtherAffectedByIntegersFormatter)
{
  std::vector<DicomTag> parentTags;
  std::vector<size_t> parentIndexes;
  DicomTag tag(0x0028, 0x0008);  // NumberOfFrames (IS, but doesn't matter for this test)

  std::vector<int64_t> values;
  values.push_back(10);

  std::set<DicomWebJsonVisitor::BinaryMode> modes;
  modes.insert(DicomWebJsonVisitor::BinaryMode_Ignore);
  modes.insert(DicomWebJsonVisitor::BinaryMode_BulkDataUri);
  modes.insert(DicomWebJsonVisitor::BinaryMode_ArrayOfValues);
  modes.insert(DicomWebJsonVisitor::BinaryMode_InlineBinary);

  std::set<ValueRepresentation> vrs;
  vrs.insert(ValueRepresentation_SignedLong);
  vrs.insert(ValueRepresentation_SignedShort);
  vrs.insert(ValueRepresentation_UnsignedLong);
  vrs.insert(ValueRepresentation_UnsignedShort);

#if DCMTK_VERSION_NUMBER >= 365
  vrs.insert(ValueRepresentation_SignedVeryLong);
  vrs.insert(ValueRepresentation_UnsignedVeryLong);
#endif

  for (std::set<ValueRepresentation>::const_iterator vr = vrs.begin(); vr != vrs.end(); ++vr)
  {
    for (std::set<DicomWebJsonVisitor::BinaryMode>::const_iterator mode = modes.begin(); mode != modes.end(); ++mode)
    {
      DicomWebJsonVisitor visitor;
      MockBinaryFormatter formatter(*mode);
      visitor.SetFormatter(formatter);

      visitor.VisitIntegers(parentTags, parentIndexes, tag, *vr, values);

      const Json::Value& result = visitor.GetResult();
      ASSERT_TRUE(result.isMember("00280008"));
      ASSERT_EQ(EnumerationToString(*vr), result["00280008"]["vr"].asString());
      ASSERT_TRUE(result["00280008"].isMember("Value"));
      ASSERT_FALSE(result["00280008"].isMember("BulkDataURI"));
      ASSERT_FALSE(result["00280008"].isMember("InlineBinary"));
      ASSERT_EQ(1u, result["00280008"]["Value"].size());
      ASSERT_EQ(10, result["00280008"]["Value"][0].asInt());
    }
  }
}


TEST(DicomWebJson, OnlyOtherAffectedByDoublesFormatter)
{
  std::vector<DicomTag> parentTags;
  std::vector<size_t> parentIndexes;
  DicomTag tag(0x0066, 0x0027);  // Triangle Point Index List (OF, but doesn't matter for this test)

  std::vector<double> values;
  values.push_back(21.5);

  std::set<DicomWebJsonVisitor::BinaryMode> modes;
  modes.insert(DicomWebJsonVisitor::BinaryMode_Ignore);
  modes.insert(DicomWebJsonVisitor::BinaryMode_BulkDataUri);
  modes.insert(DicomWebJsonVisitor::BinaryMode_ArrayOfValues);
  modes.insert(DicomWebJsonVisitor::BinaryMode_InlineBinary);

  std::set<ValueRepresentation> vrs;
  vrs.insert(ValueRepresentation_FloatingPointSingle);
  vrs.insert(ValueRepresentation_FloatingPointDouble);

  for (std::set<ValueRepresentation>::const_iterator vr = vrs.begin(); vr != vrs.end(); ++vr)
  {
    for (std::set<DicomWebJsonVisitor::BinaryMode>::const_iterator mode = modes.begin(); mode != modes.end(); ++mode)
    {
      DicomWebJsonVisitor visitor;
      MockBinaryFormatter formatter(*mode);
      visitor.SetFormatter(formatter);

      visitor.VisitDoubles(parentTags, parentIndexes, tag, *vr, values);

      const Json::Value& result = visitor.GetResult();
      ASSERT_TRUE(result.isMember("00660027"));
      ASSERT_EQ(EnumerationToString(*vr), result["00660027"]["vr"].asString());
      ASSERT_TRUE(result["00660027"].isMember("Value"));
      ASSERT_FALSE(result["00660027"].isMember("BulkDataURI"));
      ASSERT_FALSE(result["00660027"].isMember("InlineBinary"));
      ASSERT_EQ(1u, result["00660027"]["Value"].size());
      ASSERT_FLOAT_EQ(21.5, result["00660027"]["Value"][0].asFloat());
    }
  }
}


TEST(DicomWebJson, BinaryModes)
{
  ParsedDicomFile dicom(false);
  dicom.ReplacePlainString(DicomTag(0x0010, 0x9431), "43.5");  // FL
  dicom.ReplacePlainString(DicomTag(0x0008, 0x1163), "44.5");  // FD
  dicom.ReplacePlainString(DicomTag(0x0008, 0x1160), "45");    // IS
  dicom.ReplacePlainString(DicomTag(0x0028, 0x1201), "57");    // OW
  dicom.ReplacePlainString(DicomTag(0x7fe0, 0x0009), "3.5");   // OD (other double)
  dicom.ReplacePlainString(DicomTag(0x0064, 0x0009), "2.5");   // OF (other float)
  dicom.ReplacePlainString(DicomTag(0x0066, 0x0040), "46");    // OL (other long)
  dicom.ReplacePlainString(DicomTag(0x0008, 0x1161), "128");   // UL
  dicom.ReplacePlainString(DicomTag(0x0008, 0x0301), "17");    // US
  dicom.ReplacePlainString(DicomTag(0x0002, 0x0102), "MyPrivateInformation");   // OB

  {
    DicomWebJsonVisitor visitor;
    dicom.Apply(visitor);

    const Json::Value& result = visitor.GetResult();

    ASSERT_EQ("FL", result["00109431"]["vr"].asString());
    ASSERT_FLOAT_EQ(43.5f, result["00109431"]["Value"][0].asFloat());
    ASSERT_EQ("FD", result["00081163"]["vr"].asString());
    ASSERT_FLOAT_EQ(44.5f, result["00081163"]["Value"][0].asFloat());
    ASSERT_EQ("IS", result["00081160"]["vr"].asString());
    ASSERT_FLOAT_EQ(45.0f, result["00081160"]["Value"][0].asFloat());
    ASSERT_EQ("UL", result["00081161"]["vr"].asString());
    ASSERT_EQ(128u, result["00081161"]["Value"][0].asUInt());

#if DCMTK_VERSION_NUMBER >= 361
    ASSERT_EQ("US", result["00080301"]["vr"].asString());
    ASSERT_EQ(17u, result["00080301"]["Value"][0].asUInt());

    ASSERT_EQ("OD", result["7FE00009"]["vr"].asString());
    ASSERT_FLOAT_EQ(3.5f, result["7FE00009"]["Value"][0].asFloat());

#if DCMTK_VERSION_NUMBER > 361
    ASSERT_EQ("OL", result["00660040"]["vr"].asString());
#else
    // DCMTK 3.6.1 confused "OL" with "UL", so it was handled as an array of values
    ASSERT_EQ("UL", result["00660040"]["vr"].asString());
#endif

    ASSERT_EQ(46, result["00660040"]["Value"][0].asInt());
#endif

    ASSERT_EQ("OW", result["00281201"]["vr"].asString());
    ASSERT_EQ(57, result["00281201"]["Value"][0].asInt());

    ASSERT_EQ("OF", result["00640009"]["vr"].asString());
    ASSERT_FLOAT_EQ(2.5f, result["00640009"]["Value"][0].asFloat());

    ASSERT_EQ("OB", result["00020102"]["vr"].asString());

    std::string decoded;
    Toolbox::DecodeBase64(decoded, result["00020102"]["InlineBinary"].asString());
    ASSERT_EQ("MyPrivateInformation", decoded);
  }

  std::set<DicomWebJsonVisitor::BinaryMode> modes;
  modes.insert(DicomWebJsonVisitor::BinaryMode_Ignore);
  modes.insert(DicomWebJsonVisitor::BinaryMode_BulkDataUri);
  modes.insert(DicomWebJsonVisitor::BinaryMode_ArrayOfValues);
  modes.insert(DicomWebJsonVisitor::BinaryMode_InlineBinary);

  for (std::set<DicomWebJsonVisitor::BinaryMode>::const_iterator mode = modes.begin(); mode != modes.end(); ++mode)
  {
    DicomWebJsonVisitor visitor;
    MockBinaryFormatter formatter(*mode);
    visitor.SetFormatter(formatter);
    dicom.Apply(visitor);

    const Json::Value& result = visitor.GetResult();

    ASSERT_EQ("FL", result["00109431"]["vr"].asString());
    ASSERT_FLOAT_EQ(43.5f, result["00109431"]["Value"][0].asFloat());
    ASSERT_EQ("FD", result["00081163"]["vr"].asString());
    ASSERT_FLOAT_EQ(44.5f, result["00081163"]["Value"][0].asFloat());
    ASSERT_EQ("IS", result["00081160"]["vr"].asString());
    ASSERT_FLOAT_EQ(45.0f, result["00081160"]["Value"][0].asFloat());
    ASSERT_EQ("UL", result["00081161"]["vr"].asString());
    ASSERT_EQ(128u, result["00081161"]["Value"][0].asUInt());

#if DCMTK_VERSION_NUMBER >= 361
    ASSERT_EQ("US", result["00080301"]["vr"].asString());
    ASSERT_EQ(17u, result["00080301"]["Value"][0].asUInt());
#endif

    switch (*mode)
    {
      case DicomWebJsonVisitor::BinaryMode_Ignore:
#if DCMTK_VERSION_NUMBER >= 361
        ASSERT_FALSE(result.isMember("7FE00009"));
#endif

#if DCMTK_VERSION_NUMBER > 361
        ASSERT_FALSE(result.isMember("00660040"));  // DCMTK 3.6.1 considered OL as a non-other VR
#endif

        ASSERT_FALSE(result.isMember("00281201"));
        ASSERT_FALSE(result.isMember("00640009"));
        ASSERT_FALSE(result.isMember("00020102"));
        break;

      case DicomWebJsonVisitor::BinaryMode_BulkDataUri:
#if DCMTK_VERSION_NUMBER >= 361
        ASSERT_EQ("OD", result["7FE00009"]["vr"].asString());
#endif

#if DCMTK_VERSION_NUMBER > 361
        ASSERT_EQ("OL", result["00660040"]["vr"].asString());
        ASSERT_EQ("http://bulk/0066,0040", result["00660040"]["BulkDataURI"].asString());
#endif

        ASSERT_EQ("OW", result["00281201"]["vr"].asString());
        ASSERT_EQ("OF", result["00640009"]["vr"].asString());
        ASSERT_EQ("OB", result["00020102"]["vr"].asString());

        ASSERT_FALSE(result["00281201"].isMember("Value"));
        ASSERT_TRUE(result["00281201"].isMember("BulkDataURI"));
        ASSERT_FALSE(result["00281201"].isMember("InlineBinary"));

        ASSERT_EQ("http://bulk/0028,1201", result["00281201"]["BulkDataURI"].asString());
        ASSERT_EQ("http://bulk/0064,0009", result["00640009"]["BulkDataURI"].asString());
        ASSERT_EQ("http://bulk/7fe0,0009", result["7FE00009"]["BulkDataURI"].asString());
        ASSERT_EQ("http://bulk/0002,0102", result["00020102"]["BulkDataURI"].asString());
        break;

      case DicomWebJsonVisitor::BinaryMode_ArrayOfValues:
      {
#if DCMTK_VERSION_NUMBER >= 361
        ASSERT_EQ("OD", result["7FE00009"]["vr"].asString());
#endif

#if DCMTK_VERSION_NUMBER > 361
        ASSERT_EQ("OL", result["00660040"]["vr"].asString());
#endif

        ASSERT_EQ("OW", result["00281201"]["vr"].asString());
        ASSERT_EQ("OF", result["00640009"]["vr"].asString());
        ASSERT_EQ("OB", result["00020102"]["vr"].asString());

        ASSERT_TRUE(result["00281201"].isMember("Value"));
        ASSERT_FALSE(result["00281201"].isMember("BulkDataURI"));
        ASSERT_FALSE(result["00281201"].isMember("InlineBinary"));

        ASSERT_EQ(57, result["00281201"]["Value"][0].asInt());
        ASSERT_FLOAT_EQ(2.5f, result["00640009"]["Value"][0].asFloat());

#if DCMTK_VERSION_NUMBER >= 361
        ASSERT_FLOAT_EQ(3.5f, result["7FE00009"]["Value"][0].asFloat());
        ASSERT_EQ(46, result["00660040"]["Value"][0].asInt());
#endif

        // Special case: OB, UN, and OW for PixelData are always inlined
        std::string decoded;
        Toolbox::DecodeBase64(decoded, result["00020102"]["InlineBinary"].asString());
        ASSERT_EQ("MyPrivateInformation", decoded);

        break;
      }

      case DicomWebJsonVisitor::BinaryMode_InlineBinary:
      {
#if DCMTK_VERSION_NUMBER >= 361
        ASSERT_EQ("OD", result["7FE00009"]["vr"].asString());
#endif

#if DCMTK_VERSION_NUMBER > 361
        ASSERT_EQ("OL", result["00660040"]["vr"].asString());
#endif

        ASSERT_EQ("OW", result["00281201"]["vr"].asString());
        ASSERT_EQ("OF", result["00640009"]["vr"].asString());
        ASSERT_EQ("OB", result["00020102"]["vr"].asString());

        ASSERT_FALSE(result["00281201"].isMember("Value"));
        ASSERT_FALSE(result["00281201"].isMember("BulkDataURI"));
        ASSERT_TRUE(result["00281201"].isMember("InlineBinary"));

        ASSERT_EQ("OQA=", result["00281201"]["InlineBinary"].asString());
        ASSERT_EQ("AAAgQA==", result["00640009"]["InlineBinary"].asString());

#if DCMTK_VERSION_NUMBER >= 361
        ASSERT_EQ("AAAAAAAADEA=", result["7FE00009"]["InlineBinary"].asString());
#endif

#if DCMTK_VERSION_NUMBER > 361
        ASSERT_EQ("LgAAAA==", result["00660040"]["InlineBinary"].asString());
#endif

        std::string decoded;
        Toolbox::DecodeBase64(decoded, result["00020102"]["InlineBinary"].asString());
        ASSERT_EQ("MyPrivateInformation", decoded);

        break;
      }

      default:
        break;
    }
  }
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

  typedef boost::tuple<std::string, uint64_t, ValueRepresentation> Source;
  typedef std::list<Source>  Sources;

  // $ ~/Subversion/orthanc-tests/Tests/GetPixelDataVR.py ~/Subversion/orthanc-tests/Database/ColorTestMalaterre.dcm ~/Subversion/orthanc-tests/Database/ColorTestImageJ.dcm ~/Subversion/orthanc-tests/Database/Knee/T1/IM-0001-0001.dcm ~/Subversion/orthanc-tests/Database/TransferSyntaxes/*.dcm

  Sources sources;
  sources.push_back(Source(PATH + "../ColorTestMalaterre.dcm",   0x03a0u, ValueRepresentation_Unknown));  // This file has strange VR
  sources.push_back(Source(PATH + "../ColorTestImageJ.dcm",      0x00924, ValueRepresentation_OtherByte));
  sources.push_back(Source(PATH + "../Knee/T1/IM-0001-0001.dcm", 0x00c78, ValueRepresentation_OtherWord));
  sources.push_back(Source(PATH + "1.2.840.10008.1.2.1.dcm",     0x037cu, ValueRepresentation_OtherByte));
  sources.push_back(Source(PATH + "1.2.840.10008.1.2.2.dcm",     0x03e8u, ValueRepresentation_OtherByte));  // Big Endian
  sources.push_back(Source(PATH + "1.2.840.10008.1.2.4.50.dcm",  0x04acu, ValueRepresentation_OtherByte));
  sources.push_back(Source(PATH + "1.2.840.10008.1.2.4.51.dcm",  0x072cu, ValueRepresentation_OtherByte));
  sources.push_back(Source(PATH + "1.2.840.10008.1.2.4.57.dcm",  0x0620u, ValueRepresentation_OtherByte));
  sources.push_back(Source(PATH + "1.2.840.10008.1.2.4.70.dcm",  0x065au, ValueRepresentation_OtherByte));
  sources.push_back(Source(PATH + "1.2.840.10008.1.2.4.80.dcm",  0x0b46u, ValueRepresentation_OtherByte));
  sources.push_back(Source(PATH + "1.2.840.10008.1.2.4.81.dcm",  0x073eu, ValueRepresentation_OtherByte));
  sources.push_back(Source(PATH + "1.2.840.10008.1.2.4.90.dcm",  0x0b66u, ValueRepresentation_OtherByte));
  sources.push_back(Source(PATH + "1.2.840.10008.1.2.4.91.dcm",  0x19b8u, ValueRepresentation_OtherByte));
  sources.push_back(Source(PATH + "1.2.840.10008.1.2.5.dcm",     0x0b0au, ValueRepresentation_OtherByte));

  {
    std::string dicom;

    uint64_t offset;
    ValueRepresentation vr;

    // Not a DICOM image
    SystemToolbox::ReadFile(dicom, PATH + "1.2.840.10008.1.2.4.50.png", false);
    ASSERT_FALSE(DicomStreamReader::LookupPixelDataOffset(offset, vr, dicom));

    // Image without valid DICOM preamble
    SystemToolbox::ReadFile(dicom, PATH + "1.2.840.10008.1.2.dcm", false);
    ASSERT_FALSE(DicomStreamReader::LookupPixelDataOffset(offset, vr, dicom));
  }
  
  for (Sources::const_iterator it = sources.begin(); it != sources.end(); ++it)
  {
    std::string dicom;
    SystemToolbox::ReadFile(dicom, it->get<0>(), false);

    {
      uint64_t offset;
      ValueRepresentation vr;
      ASSERT_TRUE(DicomStreamReader::LookupPixelDataOffset(offset, vr, dicom));
      ASSERT_EQ(it->get<1>(), offset);
      ASSERT_EQ(it->get<2>(), vr);
    }
    
    {
      uint64_t offset;
      ValueRepresentation vr;
      ASSERT_TRUE(DicomStreamReader::LookupPixelDataOffset(offset, vr, dicom.c_str(), dicom.size()));
      ASSERT_EQ(it->get<1>(), offset);
      ASSERT_EQ(it->get<2>(), vr);
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

    ASSERT_EQ(it->get<1>(), visitor.GetPixelDataOffset());

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
            SystemToolbox::ReadFile(dicom, current->path());
            
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
      catch (boost::filesystem::filesystem_error&) // NOLINT(bugprone-empty-catch)
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
