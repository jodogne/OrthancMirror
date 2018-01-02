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


#include "PrecompiledHeadersUnitTests.h"
#include "gtest/gtest.h"

#include "../Core/OrthancException.h"
#include "../Core/DicomFormat/DicomMap.h"
#include "../Core/DicomParsing/FromDcmtkBridge.h"

#include <memory>

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

  std::auto_ptr<DicomMap> mm(m.Clone());
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

  ASSERT_FALSE(m.CopyToString(s, DICOM_TAG_PATIENT_NAME, false));
  ASSERT_TRUE(m.CopyToString(s, DICOM_TAG_PATIENT_NAME, true));
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

  ASSERT_TRUE(m.CopyToString(s, DICOM_TAG_PATIENT_NAME, false));
  ASSERT_EQ("42", s);
  ASSERT_TRUE(m.CopyToString(s, DICOM_TAG_PATIENT_NAME, true));
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
  ASSERT_EQ(-2147483648, i);
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
}
