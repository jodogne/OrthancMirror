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

#include "../Core/Compatibility.h"
#include "../Core/OrthancException.h"
#include "../Core/DicomFormat/DicomMap.h"
#include "../Core/DicomParsing/FromDcmtkBridge.h"
#include "../Core/DicomParsing/ToDcmtkBridge.h"
#include "../Core/DicomParsing/ParsedDicomFile.h"
#include "../Core/DicomParsing/DicomWebJsonVisitor.h"

#include "../OrthancServer/DicomInstanceToStore.h"

#include <memory>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcvrat.h>

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



TEST(DicomMap, DicomAsJson)
{
  // This is a Latin-1 test string: "crane" with a circumflex accent
  const unsigned char raw[] = { 0x63, 0x72, 0xe2, 0x6e, 0x65 };
  std::string latin1((char*) &raw[0], sizeof(raw) / sizeof(char));

  std::string utf8 = Toolbox::ConvertToUtf8(latin1, Encoding_Latin1, false);

  ParsedDicomFile dicom(false);
  dicom.SetEncoding(Encoding_Latin1);
  dicom.ReplacePlainString(DICOM_TAG_PATIENT_NAME, "Hello");
  dicom.ReplacePlainString(DICOM_TAG_STUDY_DESCRIPTION, utf8);
  dicom.ReplacePlainString(DICOM_TAG_SERIES_DESCRIPTION, std::string(ORTHANC_MAXIMUM_TAG_LENGTH, 'a'));
  dicom.ReplacePlainString(DICOM_TAG_MANUFACTURER, std::string(ORTHANC_MAXIMUM_TAG_LENGTH + 1, 'a'));
  dicom.ReplacePlainString(DICOM_TAG_PIXEL_DATA, "binary");
  dicom.ReplacePlainString(DICOM_TAG_ROWS, "512");

  DcmDataset& dataset = *dicom.GetDcmtkObject().getDataset();
  dataset.insertEmptyElement(DCM_StudyID, OFFalse);

  {
    std::unique_ptr<DcmSequenceOfItems> sequence(new DcmSequenceOfItems(DCM_ReferencedSeriesSequence));

    {
      std::unique_ptr<DcmItem> item(new DcmItem);
      item->putAndInsertString(DCM_ReferencedSOPInstanceUID, "nope", OFFalse);
      ASSERT_TRUE(sequence->insert(item.release(), false, false).good());
    }

    ASSERT_TRUE(dataset.insert(sequence.release(), false, false).good());
  }
  
                          
  // Check re-encoding
  DcmElement* element = NULL;
  ASSERT_TRUE(dataset.findAndGetElement(DCM_StudyDescription, element).good() &&
              element != NULL);

  char* c = NULL;
  ASSERT_TRUE(element != NULL &&
              element->isLeaf() &&
              element->isaString() &&
              element->getString(c).good());
  ASSERT_EQ(0, memcmp(c, raw, latin1.length()));

  ASSERT_TRUE(dataset.findAndGetElement(DCM_Rows, element).good() &&
              element != NULL &&
              element->getTag().getEVR() == EVR_US);

  DicomInstanceToStore toStore;
  toStore.SetParsedDicomFile(dicom);

  DicomMap m;
  m.FromDicomAsJson(toStore.GetJson());

  ASSERT_EQ("ISO_IR 100", m.GetValue(DICOM_TAG_SPECIFIC_CHARACTER_SET).GetContent());
  
  ASSERT_FALSE(m.GetValue(DICOM_TAG_PATIENT_NAME).IsBinary());
  ASSERT_EQ("Hello", m.GetValue(DICOM_TAG_PATIENT_NAME).GetContent());
  
  ASSERT_FALSE(m.GetValue(DICOM_TAG_STUDY_DESCRIPTION).IsBinary());
  ASSERT_EQ(utf8, m.GetValue(DICOM_TAG_STUDY_DESCRIPTION).GetContent());

  ASSERT_FALSE(m.HasTag(DICOM_TAG_MANUFACTURER));                // Too long
  ASSERT_FALSE(m.HasTag(DICOM_TAG_PIXEL_DATA));                  // Pixel data
  ASSERT_FALSE(m.HasTag(DICOM_TAG_REFERENCED_SERIES_SEQUENCE));  // Sequence
  ASSERT_EQ(DICOM_TAG_REFERENCED_SERIES_SEQUENCE.GetGroup(), DCM_ReferencedSeriesSequence.getGroup());
  ASSERT_EQ(DICOM_TAG_REFERENCED_SERIES_SEQUENCE.GetElement(), DCM_ReferencedSeriesSequence.getElement());

  ASSERT_TRUE(m.HasTag(DICOM_TAG_SERIES_DESCRIPTION));  // Maximum length
  ASSERT_FALSE(m.GetValue(DICOM_TAG_SERIES_DESCRIPTION).IsBinary());
  ASSERT_EQ(ORTHANC_MAXIMUM_TAG_LENGTH,
            static_cast<int>(m.GetValue(DICOM_TAG_SERIES_DESCRIPTION).GetContent().length()));

  ASSERT_FALSE(m.GetValue(DICOM_TAG_ROWS).IsBinary());
  ASSERT_EQ("512", m.GetValue(DICOM_TAG_ROWS).GetContent());

  ASSERT_FALSE(m.GetValue(DICOM_TAG_STUDY_ID).IsNull());
  ASSERT_FALSE(m.GetValue(DICOM_TAG_STUDY_ID).IsBinary());
  ASSERT_EQ("", m.GetValue(DICOM_TAG_STUDY_ID).GetContent());

  DicomArray a(m);
  ASSERT_EQ(6u, a.GetSize());

  
  //dicom.SaveToFile("/tmp/test.dcm"); 
  //std::cout << toStore.GetJson() << std::endl;
  //a.Print(stdout);
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


static void SetTagKey(ParsedDicomFile& dicom,
                      const DicomTag& tag,
                      const DicomTag& value)
{
  // This function emulates a call to function
  // "dicom.GetDcmtkObject().getDataset()->putAndInsertTagKey(tag,
  // value)" that was not available in DCMTK 3.6.0

  std::unique_ptr<DcmAttributeTag> element(new DcmAttributeTag(ToDcmtkBridge::Convert(tag)));

  DcmTagKey v = ToDcmtkBridge::Convert(value);
  if (!element->putTagVal(v).good())
  {
    throw OrthancException(ErrorCode_InternalError);
  }

  dicom.GetDcmtkObject().getDataset()->insert(element.release());
}


TEST(DicomWebJson, ValueRepresentation)
{
  // http://dicom.nema.org/medical/dicom/current/output/chtml/part18/sect_F.2.3.html

  ParsedDicomFile dicom(false);
  dicom.ReplacePlainString(DicomTag(0x0040, 0x0241), "AE");
  dicom.ReplacePlainString(DicomTag(0x0010, 0x1010), "AS");
  SetTagKey(dicom, DicomTag(0x0020, 0x9165), DicomTag(0x0010, 0x0020));
  dicom.ReplacePlainString(DicomTag(0x0008, 0x0052), "CS");
  dicom.ReplacePlainString(DicomTag(0x0008, 0x0012), "DA");
  dicom.ReplacePlainString(DicomTag(0x0010, 0x1020), "42");  // DS
  dicom.ReplacePlainString(DicomTag(0x0008, 0x002a), "DT");
  dicom.ReplacePlainString(DicomTag(0x0010, 0x9431), "43");  // FL
  dicom.ReplacePlainString(DicomTag(0x0008, 0x1163), "44");  // FD
  dicom.ReplacePlainString(DicomTag(0x0008, 0x1160), "45");  // IS
  dicom.ReplacePlainString(DicomTag(0x0008, 0x0070), "LO");
  dicom.ReplacePlainString(DicomTag(0x0010, 0x4000), "LT");
  dicom.ReplacePlainString(DicomTag(0x0028, 0x2000), "OB");
  dicom.ReplacePlainString(DicomTag(0x7fe0, 0x0009), "3.14159");  // OD (other double)
  dicom.ReplacePlainString(DicomTag(0x0064, 0x0009), "2.71828");  // OF (other float)
  dicom.ReplacePlainString(DicomTag(0x0066, 0x0040), "46");  // OL (other long)
  ASSERT_THROW(dicom.ReplacePlainString(DicomTag(0x0028, 0x1201), "O"), OrthancException);
  dicom.ReplacePlainString(DicomTag(0x0028, 0x1201), "OWOW");
  dicom.ReplacePlainString(DicomTag(0x0010, 0x0010), "PN");
  dicom.ReplacePlainString(DicomTag(0x0008, 0x0050), "SH");
  dicom.ReplacePlainString(DicomTag(0x0018, 0x6020), "-15");  // SL
  dicom.ReplacePlainString(DicomTag(0x0018, 0x9219), "-16");  // SS
  dicom.ReplacePlainString(DicomTag(0x0008, 0x0081), "ST");
  dicom.ReplacePlainString(DicomTag(0x0008, 0x0013), "TM");
  dicom.ReplacePlainString(DicomTag(0x0008, 0x0119), "UC");
  dicom.ReplacePlainString(DicomTag(0x0008, 0x0016), "UI");
  dicom.ReplacePlainString(DicomTag(0x0008, 0x1161), "128");  // UL
  dicom.ReplacePlainString(DicomTag(0x4342, 0x1234), "UN");   // Inexistent tag
  dicom.ReplacePlainString(DicomTag(0x0008, 0x0120), "UR");
  dicom.ReplacePlainString(DicomTag(0x0008, 0x0301), "17");   // US
  dicom.ReplacePlainString(DicomTag(0x0040, 0x0031), "UT");  

  DicomWebJsonVisitor visitor;
  dicom.Apply(visitor);

  std::string s;

  // The tag (0002,0002) is "Media Storage SOP Class UID" and is
  // automatically copied by DCMTK from tag (0008,0016)
  ASSERT_EQ("UI", visitor.GetResult() ["00020002"]["vr"].asString());
  ASSERT_EQ("UI", visitor.GetResult() ["00020002"]["Value"][0].asString());
  ASSERT_EQ("AE", visitor.GetResult() ["00400241"]["vr"].asString());
  ASSERT_EQ("AE", visitor.GetResult() ["00400241"]["Value"][0].asString());
  ASSERT_EQ("AS", visitor.GetResult() ["00101010"]["vr"].asString());
  ASSERT_EQ("AS", visitor.GetResult() ["00101010"]["Value"][0].asString());
  ASSERT_EQ("AT", visitor.GetResult() ["00209165"]["vr"].asString());
  ASSERT_EQ("00100020", visitor.GetResult() ["00209165"]["Value"][0].asString());
  ASSERT_EQ("CS", visitor.GetResult() ["00080052"]["vr"].asString());
  ASSERT_EQ("CS", visitor.GetResult() ["00080052"]["Value"][0].asString());
  ASSERT_EQ("DA", visitor.GetResult() ["00080012"]["vr"].asString());
  ASSERT_EQ("DA", visitor.GetResult() ["00080012"]["Value"][0].asString());
  ASSERT_EQ("DS", visitor.GetResult() ["00101020"]["vr"].asString());
  ASSERT_FLOAT_EQ(42.0f, visitor.GetResult() ["00101020"]["Value"][0].asFloat());
  ASSERT_EQ("DT", visitor.GetResult() ["0008002A"]["vr"].asString());
  ASSERT_EQ("DT", visitor.GetResult() ["0008002A"]["Value"][0].asString());
  ASSERT_EQ("FL", visitor.GetResult() ["00109431"]["vr"].asString());
  ASSERT_FLOAT_EQ(43.0f, visitor.GetResult() ["00109431"]["Value"][0].asFloat());
  ASSERT_EQ("FD", visitor.GetResult() ["00081163"]["vr"].asString());
  ASSERT_FLOAT_EQ(44.0f, visitor.GetResult() ["00081163"]["Value"][0].asFloat());
  ASSERT_EQ("IS", visitor.GetResult() ["00081160"]["vr"].asString());
  ASSERT_FLOAT_EQ(45.0f, visitor.GetResult() ["00081160"]["Value"][0].asFloat());
  ASSERT_EQ("LO", visitor.GetResult() ["00080070"]["vr"].asString());
  ASSERT_EQ("LO", visitor.GetResult() ["00080070"]["Value"][0].asString());
  ASSERT_EQ("LT", visitor.GetResult() ["00104000"]["vr"].asString());
  ASSERT_EQ("LT", visitor.GetResult() ["00104000"]["Value"][0].asString());

  ASSERT_EQ("OB", visitor.GetResult() ["00282000"]["vr"].asString());
  Toolbox::DecodeBase64(s, visitor.GetResult() ["00282000"]["InlineBinary"].asString());
  ASSERT_EQ("OB", s);

#if DCMTK_VERSION_NUMBER >= 361
  ASSERT_EQ("OD", visitor.GetResult() ["7FE00009"]["vr"].asString());
  ASSERT_FLOAT_EQ(3.14159f, boost::lexical_cast<float>(visitor.GetResult() ["7FE00009"]["Value"][0].asString()));
#else
  ASSERT_EQ("UN", visitor.GetResult() ["7FE00009"]["vr"].asString());
  Toolbox::DecodeBase64(s, visitor.GetResult() ["7FE00009"]["InlineBinary"].asString());
  ASSERT_EQ(8u, s.size()); // Because of padding
  ASSERT_EQ(0, s[7]);
  ASSERT_EQ("3.14159", s.substr(0, 7));
#endif

  ASSERT_EQ("OF", visitor.GetResult() ["00640009"]["vr"].asString());
  ASSERT_FLOAT_EQ(2.71828f, boost::lexical_cast<float>(visitor.GetResult() ["00640009"]["Value"][0].asString()));

#if DCMTK_VERSION_NUMBER < 361
  ASSERT_EQ("UN", visitor.GetResult() ["00660040"]["vr"].asString());
  Toolbox::DecodeBase64(s, visitor.GetResult() ["00660040"]["InlineBinary"].asString());
  ASSERT_EQ("46", s);
#elif DCMTK_VERSION_NUMBER == 361
  ASSERT_EQ("UL", visitor.GetResult() ["00660040"]["vr"].asString());
  ASSERT_EQ(46, visitor.GetResult() ["00660040"]["Value"][0].asInt());
#else
  ASSERT_EQ("OL", visitor.GetResult() ["00660040"]["vr"].asString());
  ASSERT_EQ(46, visitor.GetResult() ["00660040"]["Value"][0].asInt());
#endif

  ASSERT_EQ("OW", visitor.GetResult() ["00281201"]["vr"].asString());
  Toolbox::DecodeBase64(s, visitor.GetResult() ["00281201"]["InlineBinary"].asString());
  ASSERT_EQ("OWOW", s);

  ASSERT_EQ("PN", visitor.GetResult() ["00100010"]["vr"].asString());
  ASSERT_EQ("PN", visitor.GetResult() ["00100010"]["Value"][0]["Alphabetic"].asString());

  ASSERT_EQ("SH", visitor.GetResult() ["00080050"]["vr"].asString());
  ASSERT_EQ("SH", visitor.GetResult() ["00080050"]["Value"][0].asString());

  ASSERT_EQ("SL", visitor.GetResult() ["00186020"]["vr"].asString());
  ASSERT_EQ(-15, visitor.GetResult() ["00186020"]["Value"][0].asInt());

  ASSERT_EQ("SS", visitor.GetResult() ["00189219"]["vr"].asString());
  ASSERT_EQ(-16, visitor.GetResult() ["00189219"]["Value"][0].asInt());

  ASSERT_EQ("ST", visitor.GetResult() ["00080081"]["vr"].asString());
  ASSERT_EQ("ST", visitor.GetResult() ["00080081"]["Value"][0].asString());

  ASSERT_EQ("TM", visitor.GetResult() ["00080013"]["vr"].asString());
  ASSERT_EQ("TM", visitor.GetResult() ["00080013"]["Value"][0].asString());

#if DCMTK_VERSION_NUMBER >= 361
  ASSERT_EQ("UC", visitor.GetResult() ["00080119"]["vr"].asString());
  ASSERT_EQ("UC", visitor.GetResult() ["00080119"]["Value"][0].asString());
#else
  ASSERT_EQ("UN", visitor.GetResult() ["00080119"]["vr"].asString());
  Toolbox::DecodeBase64(s, visitor.GetResult() ["00080119"]["InlineBinary"].asString());
  ASSERT_EQ("UC", s);
#endif

  ASSERT_EQ("UI", visitor.GetResult() ["00080016"]["vr"].asString());
  ASSERT_EQ("UI", visitor.GetResult() ["00080016"]["Value"][0].asString());

  ASSERT_EQ("UL", visitor.GetResult() ["00081161"]["vr"].asString());
  ASSERT_EQ(128u, visitor.GetResult() ["00081161"]["Value"][0].asUInt());

  ASSERT_EQ("UN", visitor.GetResult() ["43421234"]["vr"].asString());
  Toolbox::DecodeBase64(s, visitor.GetResult() ["43421234"]["InlineBinary"].asString());
  ASSERT_EQ("UN", s);

#if DCMTK_VERSION_NUMBER >= 361
  ASSERT_EQ("UR", visitor.GetResult() ["00080120"]["vr"].asString());
  ASSERT_EQ("UR", visitor.GetResult() ["00080120"]["Value"][0].asString());
#else
  ASSERT_EQ("UN", visitor.GetResult() ["00080120"]["vr"].asString());
  Toolbox::DecodeBase64(s, visitor.GetResult() ["00080120"]["InlineBinary"].asString());
  ASSERT_EQ("UR", s);
#endif

#if DCMTK_VERSION_NUMBER >= 361
  ASSERT_EQ("US", visitor.GetResult() ["00080301"]["vr"].asString());
  ASSERT_EQ(17u, visitor.GetResult() ["00080301"]["Value"][0].asUInt());
#else
  ASSERT_EQ("UN", visitor.GetResult() ["00080301"]["vr"].asString());
  Toolbox::DecodeBase64(s, visitor.GetResult() ["00080301"]["InlineBinary"].asString());
  ASSERT_EQ("17", s);
#endif

  ASSERT_EQ("UT", visitor.GetResult() ["00400031"]["vr"].asString());
  ASSERT_EQ("UT", visitor.GetResult() ["00400031"]["Value"][0].asString());

  std::string xml;
  visitor.FormatXml(xml);
  
  {
    DicomMap m;
    m.FromDicomWeb(visitor.GetResult());
    ASSERT_EQ(31u, m.GetSize());

    std::string s;
    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x0002, 0x0002), false));  ASSERT_EQ("UI", s);
    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x0040, 0x0241), false));  ASSERT_EQ("AE", s);
    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x0010, 0x1010), false));  ASSERT_EQ("AS", s);
    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x0020, 0x9165), false));  ASSERT_EQ("00100020", s);
    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x0008, 0x0052), false));  ASSERT_EQ("CS", s);
    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x0008, 0x0012), false));  ASSERT_EQ("DA", s);
    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x0010, 0x1020), false));  ASSERT_EQ("42", s);
    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x0008, 0x002a), false));  ASSERT_EQ("DT", s);
    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x0010, 0x9431), false));  ASSERT_EQ("43", s);
    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x0008, 0x1163), false));  ASSERT_EQ("44", s);
    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x0008, 0x1160), false));  ASSERT_EQ("45", s);
    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x0008, 0x0070), false));  ASSERT_EQ("LO", s);
    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x0010, 0x4000), false));  ASSERT_EQ("LT", s);
    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x0028, 0x2000), true));   ASSERT_EQ("OB", s);
    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x7fe0, 0x0009), true));

#if DCMTK_VERSION_NUMBER >= 361
    ASSERT_FLOAT_EQ(3.14159f, boost::lexical_cast<float>(s));
#else
    ASSERT_EQ(8u, s.size()); // Because of padding
    ASSERT_EQ(0, s[7]);
    ASSERT_EQ("3.14159", s.substr(0, 7));
#endif

    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x0064, 0x0009), true));
    ASSERT_FLOAT_EQ(2.71828f, boost::lexical_cast<float>(s));
    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x0028, 0x1201), true));   ASSERT_EQ("OWOW", s);
    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x0010, 0x0010), false));  ASSERT_EQ("PN", s);
    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x0008, 0x0050), false));  ASSERT_EQ("SH", s);
    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x0018, 0x6020), false));  ASSERT_EQ("-15", s);
    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x0018, 0x9219), false));  ASSERT_EQ("-16", s);
    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x0008, 0x0081), false));  ASSERT_EQ("ST", s);
    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x0008, 0x0013), false));  ASSERT_EQ("TM", s);
    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x0008, 0x0016), false));  ASSERT_EQ("UI", s);
    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x0008, 0x1161), false));  ASSERT_EQ("128", s);
    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x4342, 0x1234), true));   ASSERT_EQ("UN", s);
    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x0040, 0x0031), false));  ASSERT_EQ("UT", s);

#if DCMTK_VERSION_NUMBER >= 361
    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x0066, 0x0040), false));  ASSERT_EQ("46", s);
    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x0008, 0x0119), false));  ASSERT_EQ("UC", s);
    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x0008, 0x0120), false));  ASSERT_EQ("UR", s);
    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x0008, 0x0301), false));  ASSERT_EQ("17", s);
#else
    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x0066, 0x0040), true));  ASSERT_EQ("46", s);  // OL
    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x0008, 0x0119), true));  ASSERT_EQ("UC", s);
    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x0008, 0x0120), true));  ASSERT_EQ("UR", s);
    ASSERT_TRUE(m.LookupStringValue(s, DicomTag(0x0008, 0x0301), true));  ASSERT_EQ("17", s);  // US (but tag unknown to DCMTK 3.6.0)
#endif
    
  }
}


TEST(DicomWebJson, Sequence)
{
  ParsedDicomFile dicom(false);
  
  {
    std::unique_ptr<DcmSequenceOfItems> sequence(new DcmSequenceOfItems(DCM_ReferencedSeriesSequence));

    for (unsigned int i = 0; i < 3; i++)
    {
      std::unique_ptr<DcmItem> item(new DcmItem);
      std::string s = "item" + boost::lexical_cast<std::string>(i);
      item->putAndInsertString(DCM_ReferencedSOPInstanceUID, s.c_str(), OFFalse);
      ASSERT_TRUE(sequence->insert(item.release(), false, false).good());
    }

    ASSERT_TRUE(dicom.GetDcmtkObject().getDataset()->insert(sequence.release(), false, false).good());
  }

  DicomWebJsonVisitor visitor;
  dicom.Apply(visitor);

  ASSERT_EQ("SQ", visitor.GetResult() ["00081115"]["vr"].asString());
  ASSERT_EQ(3u, visitor.GetResult() ["00081115"]["Value"].size());

  std::set<std::string> items;
  
  for (Json::Value::ArrayIndex i = 0; i < 3; i++)
  {
    ASSERT_EQ(1u, visitor.GetResult() ["00081115"]["Value"][i].size());
    ASSERT_EQ(1u, visitor.GetResult() ["00081115"]["Value"][i]["00081155"]["Value"].size());
    ASSERT_EQ("UI", visitor.GetResult() ["00081115"]["Value"][i]["00081155"]["vr"].asString());
    items.insert(visitor.GetResult() ["00081115"]["Value"][i]["00081155"]["Value"][0].asString());
  }

  ASSERT_EQ(3u, items.size());
  ASSERT_TRUE(items.find("item0") != items.end());
  ASSERT_TRUE(items.find("item1") != items.end());
  ASSERT_TRUE(items.find("item2") != items.end());

  std::string xml;
  visitor.FormatXml(xml);

  {
    DicomMap m;
    m.FromDicomWeb(visitor.GetResult());
    ASSERT_EQ(0u, m.GetSize());  // Sequences are not handled by DicomMap
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
