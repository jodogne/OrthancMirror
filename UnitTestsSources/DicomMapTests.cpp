/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2019 Osimis S.A., Belgium
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
#include "../Core/DicomParsing/ParsedDicomFile.h"

#include "../OrthancServer/DicomInstanceToStore.h"

#include <memory>
#include <dcmtk/dcmdata/dcdeftag.h>

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

  std::string utf8 = Toolbox::ConvertToUtf8(latin1, Encoding_Latin1);

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
    std::auto_ptr<DcmSequenceOfItems> sequence(new DcmSequenceOfItems(DCM_ReferencedSeriesSequence));

    {
      std::auto_ptr<DcmItem> item(new DcmItem);
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





#include <boost/math/special_functions/round.hpp>
#include <pugixml.hpp>


static const char* const KEY_ALPHABETIC = "Alphabetic";
static const char* const KEY_BULK_DATA_URI = "BulkDataURI";
static const char* const KEY_INLINE_BINARY = "InlineBinary";
static const char* const KEY_SQ = "SQ";
static const char* const KEY_VALUE = "Value";
static const char* const KEY_VR = "vr";

namespace Orthanc
{  
  static void ExploreDataset(pugi::xml_node& target,
                             const Json::Value& source)
  {
    assert(source.type() == Json::objectValue);

    Json::Value::Members members = source.getMemberNames();
    for (size_t i = 0; i < members.size(); i++)
    {
      const DicomTag tag = FromDcmtkBridge::ParseTag(members[i]);
      const Json::Value& content = source[members[i]];

      assert(content.type() == Json::objectValue &&
             content.isMember("vr") &&
             content["vr"].type() == Json::stringValue);
      const std::string vr = content["vr"].asString();

      const std::string keyword = FromDcmtkBridge::GetTagName(tag, "");
    
      pugi::xml_node node = target.append_child("DicomAttribute");
      node.append_attribute("tag").set_value(members[i].c_str());
      node.append_attribute("vr").set_value(vr.c_str());

      if (keyword != std::string(DcmTag_ERROR_TagName))
      {
        node.append_attribute("keyword").set_value(keyword.c_str());
      }   

      if (content.isMember(KEY_VALUE))
      {
        assert(content[KEY_VALUE].type() == Json::arrayValue);
        
        for (Json::Value::ArrayIndex j = 0; j < content[KEY_VALUE].size(); j++)
        {
          std::string number = boost::lexical_cast<std::string>(j + 1);

          if (vr == "SQ")
          {
            if (content[KEY_VALUE][j].type() == Json::objectValue)
            {
              pugi::xml_node child = node.append_child("Item");
              child.append_attribute("number").set_value(number.c_str());
              ExploreDataset(child, content[KEY_VALUE][j]);
            }
          }
          if (vr == "PN")
          {
            if (content[KEY_VALUE][j].isMember(KEY_ALPHABETIC) &&
                content[KEY_VALUE][j][KEY_ALPHABETIC].type() == Json::stringValue)
            {
              std::vector<std::string> tokens;
              Toolbox::TokenizeString(tokens, content[KEY_VALUE][j][KEY_ALPHABETIC].asString(), '^');

              pugi::xml_node child = node.append_child("PersonName");
              child.append_attribute("number").set_value(number.c_str());
            
              pugi::xml_node name = child.append_child(KEY_ALPHABETIC);
            
              if (tokens.size() >= 1)
              {
                name.append_child("FamilyName").text() = tokens[0].c_str();
              }
            
              if (tokens.size() >= 2)
              {
                name.append_child("GivenName").text() = tokens[1].c_str();
              }
            
              if (tokens.size() >= 3)
              {
                name.append_child("MiddleName").text() = tokens[2].c_str();
              }
            
              if (tokens.size() >= 4)
              {
                name.append_child("NamePrefix").text() = tokens[3].c_str();
              }
            
              if (tokens.size() >= 5)
              {
                name.append_child("NameSuffix").text() = tokens[4].c_str();
              }
            }
          }
          else
          {
            pugi::xml_node child = node.append_child("Value");
            child.append_attribute("number").set_value(number.c_str());

            switch (content[KEY_VALUE][j].type())
            {
              case Json::stringValue:
                child.text() = content[KEY_VALUE][j].asCString();
                break;

              case Json::realValue:
                child.text() = content[KEY_VALUE][j].asFloat();
                break;

              case Json::intValue:
                child.text() = content[KEY_VALUE][j].asInt();
                break;

              case Json::uintValue:
                child.text() = content[KEY_VALUE][j].asUInt();
                break;

              default:
                break;
            }
          }
        }
      }
      else if (content.isMember(KEY_BULK_DATA_URI) &&
               content[KEY_BULK_DATA_URI].type() == Json::stringValue)
      {
        pugi::xml_node child = node.append_child("BulkData");
        child.append_attribute("URI").set_value(content[KEY_BULK_DATA_URI].asCString());
      }
      else if (content.isMember(KEY_INLINE_BINARY) &&
               content[KEY_INLINE_BINARY].type() == Json::stringValue)
      {
        pugi::xml_node child = node.append_child("InlineBinary");
        child.text() = content[KEY_INLINE_BINARY].asCString();
      }
    }
  }


  static void DicomWebJsonToXml(pugi::xml_document& target,
                                const Json::Value& source)
  {
    pugi::xml_node root = target.append_child("NativeDicomModel");
    root.append_attribute("xmlns").set_value("http://dicom.nema.org/PS3.19/models/NativeDICOM");
    root.append_attribute("xsi:schemaLocation").set_value("http://dicom.nema.org/PS3.19/models/NativeDICOM");
    root.append_attribute("xmlns:xsi").set_value("http://www.w3.org/2001/XMLSchema-instance");

    ExploreDataset(root, source);

    pugi::xml_node decl = target.prepend_child(pugi::node_declaration);
    decl.append_attribute("version").set_value("1.0");
    decl.append_attribute("encoding").set_value("utf-8");
  }


  enum DicomWebBinaryMode
  {
    DicomWebBinaryMode_Ignore,
    DicomWebBinaryMode_BulkDataUri,
    DicomWebBinaryMode_InlineBinary
  };
    
  class IDicomWebBinaryFormatter : public boost::noncopyable
  {
  public:
    virtual ~IDicomWebBinaryFormatter()
    {
    }

    virtual DicomWebBinaryMode Format(std::string& bulkDataUri,
                                      const std::vector<DicomTag>& parentTags,
                                      const std::vector<size_t>& parentIndexes,
                                      const DicomTag& tag,
                                      ValueRepresentation vr) = 0;
  };

  class DicomWebJsonVisitor : public ITagVisitor
  {
  private:
    Json::Value                result_;
    IDicomWebBinaryFormatter  *formatter_;

    static std::string FormatTag(const DicomTag& tag)
    {
      char buf[16];
      sprintf(buf, "%04X%04X", tag.GetGroup(), tag.GetElement());
      return std::string(buf);
    }
    
    Json::Value& CreateNode(const std::vector<DicomTag>& parentTags,
                            const std::vector<size_t>& parentIndexes,
                            const DicomTag& tag)
    {
      assert(parentTags.size() == parentIndexes.size());      

      Json::Value* node = &result_;

      for (size_t i = 0; i < parentTags.size(); i++)
      {
        std::string t = FormatTag(parentTags[i]);

        if (!node->isMember(t))
        {
          Json::Value item = Json::objectValue;
          item[KEY_VR] = KEY_SQ;
          item[KEY_VALUE] = Json::arrayValue;
          item[KEY_VALUE].append(Json::objectValue);
          (*node) [t] = item;

          node = &(*node)[t][KEY_VALUE][0];
        }
        else if ((*node)  [t].type() != Json::objectValue ||
                 !(*node) [t].isMember(KEY_VR) ||
                 (*node)  [t][KEY_VR].type() != Json::stringValue ||
                 (*node)  [t][KEY_VR].asString() != KEY_SQ ||
                 !(*node) [t].isMember(KEY_VALUE) ||
                 (*node)  [t][KEY_VALUE].type() != Json::arrayValue)
        {
          throw OrthancException(ErrorCode_InternalError);
        }
        else
        {
          size_t currentSize = (*node) [t][KEY_VALUE].size();

          if (parentIndexes[i] < currentSize)
          {
            // The node already exists
          }
          else if (parentIndexes[i] == currentSize)
          {
            (*node) [t][KEY_VALUE].append(Json::objectValue);
          }
          else
          {
            throw OrthancException(ErrorCode_InternalError);
          }
          
          node = &(*node) [t][KEY_VALUE][Json::ArrayIndex(parentIndexes[i])];
        }
      }

      assert(node->type() == Json::objectValue);

      std::string t = FormatTag(tag);
      if (node->isMember(t))
      {
        throw OrthancException(ErrorCode_InternalError);
      }
      else
      {
        (*node) [t] = Json::objectValue;
        return (*node) [t];
      }
    }

    static Json::Value FormatInteger(int64_t value)
    {
      if (value < 0)
      {
        return Json::Value(static_cast<int32_t>(value));
      }
      else
      {
        return Json::Value(static_cast<uint32_t>(value));
      }
    }

    static Json::Value FormatDouble(double value)
    {
      long long a = boost::math::llround<double>(value);

      double d = fabs(value - static_cast<double>(a));

      if (d <= std::numeric_limits<double>::epsilon() * 100.0)
      {
        return FormatInteger(a);
      }
      else
      {
        return Json::Value(value);
      }
    }

  public:
    DicomWebJsonVisitor() :
      formatter_(NULL)
    {
      Clear();
    }

    void SetFormatter(IDicomWebBinaryFormatter& formatter)
    {
      formatter_ = &formatter;
    }
    
    void Clear()
    {
      result_ = Json::objectValue;
    }

    const Json::Value& GetResult() const
    {
      return result_;
    }

    void FormatXml(pugi::xml_document& target) const
    {
      DicomWebJsonToXml(target, result_);
    }

    virtual void VisitNotSupported(const std::vector<DicomTag>& parentTags,
                                   const std::vector<size_t>& parentIndexes,
                                   const DicomTag& tag,
                                   ValueRepresentation vr) ORTHANC_OVERRIDE
    {
    }

    virtual void VisitEmptySequence(const std::vector<DicomTag>& parentTags,
                                    const std::vector<size_t>& parentIndexes,
                                    const DicomTag& tag) ORTHANC_OVERRIDE
    {
      if (tag.GetElement() != 0x0000)
      {
        Json::Value& node = CreateNode(parentTags, parentIndexes, tag);
        node[KEY_VR] = EnumerationToString(ValueRepresentation_Sequence);
      }
    }

    virtual void VisitBinary(const std::vector<DicomTag>& parentTags,
                             const std::vector<size_t>& parentIndexes,
                             const DicomTag& tag,
                             ValueRepresentation vr,
                             const void* data,
                             size_t size) ORTHANC_OVERRIDE
    {
      assert(vr == ValueRepresentation_OtherByte ||
             vr == ValueRepresentation_OtherDouble ||
             vr == ValueRepresentation_OtherFloat ||
             vr == ValueRepresentation_OtherLong ||
             vr == ValueRepresentation_OtherWord ||
             vr == ValueRepresentation_Unknown);

      if (tag.GetElement() != 0x0000)
      {
        DicomWebBinaryMode mode;
        std::string bulkDataUri;
        
        if (formatter_ == NULL)
        {
          mode = DicomWebBinaryMode_InlineBinary;
        }
        else
        {
          mode = formatter_->Format(bulkDataUri, parentTags, parentIndexes, tag, vr);
        }

        /*mode = DicomWebBinaryMode_BulkDataUri;
          bulkDataUri = "http://localhost/" + tag.Format();*/

        if (mode != DicomWebBinaryMode_Ignore)
        {
          Json::Value& node = CreateNode(parentTags, parentIndexes, tag);
          node[KEY_VR] = EnumerationToString(vr);

          switch (mode)
          {
            case DicomWebBinaryMode_BulkDataUri:
              node[KEY_BULK_DATA_URI] = bulkDataUri;
              break;

            case DicomWebBinaryMode_InlineBinary:
            {
              std::string tmp(static_cast<const char*>(data), size);
          
              std::string base64;
              Toolbox::EncodeBase64(base64, tmp);

              node[KEY_INLINE_BINARY] = base64;
              break;
            }

            default:
              throw OrthancException(ErrorCode_ParameterOutOfRange);
          }
        }
      }
    }

    virtual void VisitIntegers(const std::vector<DicomTag>& parentTags,
                               const std::vector<size_t>& parentIndexes,
                               const DicomTag& tag,
                               ValueRepresentation vr,
                               const std::vector<int64_t>& values) ORTHANC_OVERRIDE
    {
      if (tag.GetElement() != 0x0000 &&
          vr != ValueRepresentation_NotSupported)
      {
        Json::Value& node = CreateNode(parentTags, parentIndexes, tag);
        node[KEY_VR] = EnumerationToString(vr);

        if (!values.empty())
        {
          Json::Value content = Json::arrayValue;
          for (size_t i = 0; i < values.size(); i++)
          {
            content.append(FormatInteger(values[i]));
          }

          node[KEY_VALUE] = content;
        }
      }
    }

    virtual void VisitDoubles(const std::vector<DicomTag>& parentTags,
                              const std::vector<size_t>& parentIndexes,
                              const DicomTag& tag,
                              ValueRepresentation vr,
                              const std::vector<double>& values) ORTHANC_OVERRIDE
    {
      if (tag.GetElement() != 0x0000 &&
          vr != ValueRepresentation_NotSupported)
      {
        Json::Value& node = CreateNode(parentTags, parentIndexes, tag);
        node[KEY_VR] = EnumerationToString(vr);

        if (!values.empty())
        {
          Json::Value content = Json::arrayValue;
          for (size_t i = 0; i < values.size(); i++)
          {
            content.append(FormatDouble(values[i]));
          }
          
          node[KEY_VALUE] = content;
        }
      }
    }

    virtual void VisitAttributes(const std::vector<DicomTag>& parentTags,
                                 const std::vector<size_t>& parentIndexes,
                                 const DicomTag& tag,
                                 const std::vector<DicomTag>& values) ORTHANC_OVERRIDE
    {
      if (tag.GetElement() != 0x0000)
      {
        Json::Value& node = CreateNode(parentTags, parentIndexes, tag);
        node[KEY_VR] = EnumerationToString(ValueRepresentation_AttributeTag);

        if (!values.empty())
        {
          Json::Value content = Json::arrayValue;
          for (size_t i = 0; i < values.size(); i++)
          {
            content.append(FormatTag(values[i]));
          }
          
          node[KEY_VALUE] = content;
        }
      }
    }

    virtual Action VisitString(std::string& newValue,
                               const std::vector<DicomTag>& parentTags,
                               const std::vector<size_t>& parentIndexes,
                               const DicomTag& tag,
                               ValueRepresentation vr,
                               const std::string& value) ORTHANC_OVERRIDE
    {
      if (tag.GetElement() == 0x0000 ||
          vr == ValueRepresentation_NotSupported)
      {
        return Action_None;
      }
      else
      {
        Json::Value& node = CreateNode(parentTags, parentIndexes, tag);
        node[KEY_VR] = EnumerationToString(vr);

        if (tag == DICOM_TAG_SPECIFIC_CHARACTER_SET)
        {
          // TODO - The JSON file has an UTF-8 encoding, thus DCMTK
          // replaces the specific character set with "ISO_IR 192"
          // (UNICODE UTF-8). It is unclear whether the source
          // character set should be kept: We thus mimic DCMTK.
          node[KEY_VALUE].append("ISO_IR 192");
        }
        else
        {
          std::string truncated;
        
          if (!value.empty() &&
              value[value.size() - 1] == '\0')
          {
            truncated = value.substr(0, value.size() - 1);
          }
          else
          {
            truncated = value;
          }
        
          if (!truncated.empty())
          {
            std::vector<std::string> tokens;
            Toolbox::TokenizeString(tokens, truncated, '\\');

            node[KEY_VALUE] = Json::arrayValue;
            for (size_t i = 0; i < tokens.size(); i++)
            {
              try
              {
                switch (vr)
                {
                  case ValueRepresentation_PersonName:
                  {
                    Json::Value value = Json::objectValue;
                    if (!tokens[i].empty())
                    {
                      value[KEY_ALPHABETIC] = tokens[i];
                    }
                    node[KEY_VALUE].append(value);
                    break;
                  }
                  
                  case ValueRepresentation_IntegerString:
                    if (tokens[i].empty())
                    {
                      node[KEY_VALUE].append(Json::nullValue);
                    }
                    else
                    {
                      int64_t value = boost::lexical_cast<int64_t>(tokens[i]);
                      node[KEY_VALUE].append(FormatInteger(value));
                    }
                  
                    break;
              
                  case ValueRepresentation_DecimalString:
                    if (tokens[i].empty())
                    {
                      node[KEY_VALUE].append(Json::nullValue);
                    }
                    else
                    {
                      double value = boost::lexical_cast<double>(tokens[i]);
                      node[KEY_VALUE].append(FormatDouble(value));
                    }
                    break;
              
                  default:
                    if (tokens[i].empty())
                    {
                      node[KEY_VALUE].append(Json::nullValue);
                    }
                    else
                    {
                      node[KEY_VALUE].append(tokens[i]);
                    }
                  
                    break;
                }
              }
              catch (boost::bad_lexical_cast&)
              {
                throw OrthancException(ErrorCode_BadFileFormat);
              }
            }
          }
        }
      }
      
      return Action_None;
    }
  };
}






#include "../Core/SystemToolbox.h"


/* 

MarekLatin2.dcm 
HierarchicalAnonymization/StructuredReports/IM0
DummyCT.dcm
Brainix/Epi/IM-0001-0018.dcm
Issue22.dcm


cat << EOF > /tmp/tutu.py
import json
import sys
j = json.loads(sys.stdin.read().decode("utf-8-sig"))
print(json.dumps(j, indent=4, sort_keys=True, ensure_ascii=False).encode('utf-8'))
EOF

DCMDICTPATH=/home/jodogne/Downloads/dcmtk-3.6.4/dcmdata/data/dicom.dic /home/jodogne/Downloads/dcmtk-3.6.4/i/bin/dcm2json ~/Subversion/orthanc-tests/Database/DummyCT.dcm | tr -d '\0' | sed 's/\\u0000//g' | sed 's/\.0$//' | python /tmp/tutu.py > /tmp/a.json

make -j4 && ./UnitTests --gtest_filter=DicomWeb* && python /tmp/tutu.py < tutu.json > /tmp/b.json && diff -i /tmp/a.json /tmp/b.json

*/

TEST(DicomWebJson, Basic)
{
  std::string content;
  Orthanc::SystemToolbox::ReadFile(content, "/home/jodogne/Subversion/orthanc-tests/Database/DummyCT.dcm");

  Orthanc::ParsedDicomFile dicom(content);

  Orthanc::DicomWebJsonVisitor visitor;
  dicom.Apply(visitor);

  Orthanc::SystemToolbox::WriteFile(visitor.GetResult().toStyledString(), "tutu.json");

  pugi::xml_document xml;
  visitor.FormatXml(xml);
  xml.print(std::cout);
}


TEST(DicomWebJson, Multiplicity)
{
  // http://dicom.nema.org/medical/dicom/current/output/chtml/part18/sect_F.2.4.html

  ParsedDicomFile dicom(false);
  dicom.ReplacePlainString(DICOM_TAG_PATIENT_NAME, "SB1^SB2^SB3^SB4^SB5");
  dicom.ReplacePlainString(DICOM_TAG_IMAGE_ORIENTATION_PATIENT, "1\\2.3\\4");
  dicom.ReplacePlainString(DICOM_TAG_IMAGE_POSITION_PATIENT, "");

  Orthanc::DicomWebJsonVisitor visitor;
  dicom.Apply(visitor);

  {
    const Json::Value& tag = visitor.GetResult() ["00200037"];
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
    const Json::Value& tag = visitor.GetResult() ["00200032"];
    ASSERT_EQ(EnumerationToString(ValueRepresentation_DecimalString), tag["vr"].asString());
    ASSERT_EQ(1u, tag.getMemberNames().size());
  }

  pugi::xml_document xml;
  visitor.FormatXml(xml);
  xml.print(std::cout);
}


TEST(DicomWebJson, NullValue)
{
  // http://dicom.nema.org/medical/dicom/current/output/chtml/part18/sect_F.2.5.html

  ParsedDicomFile dicom(false);
  dicom.ReplacePlainString(DICOM_TAG_IMAGE_ORIENTATION_PATIENT, "1.5\\\\\\2.5");

  Orthanc::DicomWebJsonVisitor visitor;
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

  pugi::xml_document xml;
  visitor.FormatXml(xml);
  xml.print(std::cout);
}


TEST(DicomWebJson, ValueRepresentation)
{
  // http://dicom.nema.org/medical/dicom/current/output/chtml/part18/sect_F.2.3.html

  ParsedDicomFile dicom(false);
  dicom.ReplacePlainString(DicomTag(0x0040, 0x0241), "AE");
  dicom.ReplacePlainString(DicomTag(0x0010, 0x1010), "AS");
  ASSERT_TRUE(dicom.GetDcmtkObject().getDataset()->
              putAndInsertTagKey(DcmTag(0x0020, 0x9165),
                                 DcmTagKey(0x0010, 0x0020)).good());
  dicom.ReplacePlainString(DicomTag(0x0008, 0x0052), "CS");
  dicom.ReplacePlainString(DicomTag(0x0008, 0x0012), "DA");
  dicom.ReplacePlainString(DicomTag(0x0010, 0x1020), "42");  // DS
  dicom.ReplacePlainString(DicomTag(0x0008, 0x002a), "DT");
  dicom.ReplacePlainString(DicomTag(0x0010, 0x9431), "43");  // FL
  dicom.ReplacePlainString(DicomTag(0x0008, 0x1163), "44");  // FD
  dicom.ReplacePlainString(DicomTag(0x0008, 0x1160), "45");  // IS
  dicom.ReplacePlainString(DicomTag(0x0008, 0x0070), "LO");
  dicom.ReplacePlainString(DicomTag(0x0008, 0x0108), "LT");
  dicom.ReplacePlainString(DicomTag(0x0028, 0x2000), "OB");
  dicom.ReplacePlainString(DicomTag(0x7fe0, 0x0009), "OD");
  dicom.ReplacePlainString(DicomTag(0x0064, 0x0009), "OF");

#if DCMTK_VERSION_NUMBER >= 362 
  dicom.ReplacePlainString(DicomTag(0x0066, 0x0040), "OLOL");
#endif

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
  
  
  Orthanc::DicomWebJsonVisitor visitor;
  dicom.Apply(visitor);

  std::string s;
  
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
  ASSERT_EQ("LT", visitor.GetResult() ["00080108"]["vr"].asString());
  ASSERT_EQ("LT", visitor.GetResult() ["00080108"]["Value"][0].asString());

  ASSERT_EQ("OB", visitor.GetResult() ["00282000"]["vr"].asString());
  Toolbox::DecodeBase64(s, visitor.GetResult() ["00282000"]["InlineBinary"].asString());
  ASSERT_EQ("OB", s);

  ASSERT_EQ("OD", visitor.GetResult() ["7FE00009"]["vr"].asString());
  Toolbox::DecodeBase64(s, visitor.GetResult() ["7FE00009"]["InlineBinary"].asString());
  ASSERT_EQ("OD", s);

  ASSERT_EQ("OF", visitor.GetResult() ["00640009"]["vr"].asString());
  Toolbox::DecodeBase64(s, visitor.GetResult() ["00640009"]["InlineBinary"].asString());
  ASSERT_EQ("OF", s);

#if DCMTK_VERSION_NUMBER >= 362
  ASSERT_EQ("OL", visitor.GetResult() ["00660040"]["vr"].asString());
  Toolbox::DecodeBase64(s, visitor.GetResult() ["00660040"]["InlineBinary"].asString());
  ASSERT_EQ("OLOL", s);
#endif

  ASSERT_EQ("OW", visitor.GetResult() ["00281201"]["vr"].asString());
  Toolbox::DecodeBase64(s, visitor.GetResult() ["00281201"]["InlineBinary"].asString());
  ASSERT_EQ("OWOW", s);

  ASSERT_EQ("PN", visitor.GetResult() ["00100010"]["vr"].asString());
  ASSERT_EQ("PN", visitor.GetResult() ["00100010"]["Value"][0]["Alphabetic"].asString());

  ASSERT_EQ("SH", visitor.GetResult() ["00080050"]["vr"].asString());
  ASSERT_EQ("SH", visitor.GetResult() ["00080050"]["Value"][0].asString());

  ASSERT_EQ("SL", visitor.GetResult() ["00186020"]["vr"].asString());
  ASSERT_FLOAT_EQ(-15, visitor.GetResult() ["00186020"]["Value"][0].asInt());

  ASSERT_EQ("SS", visitor.GetResult() ["00189219"]["vr"].asString());
  ASSERT_FLOAT_EQ(-16, visitor.GetResult() ["00189219"]["Value"][0].asInt());

  ASSERT_EQ("ST", visitor.GetResult() ["00080081"]["vr"].asString());
  ASSERT_EQ("ST", visitor.GetResult() ["00080081"]["Value"][0].asString());

  ASSERT_EQ("TM", visitor.GetResult() ["00080013"]["vr"].asString());
  ASSERT_EQ("TM", visitor.GetResult() ["00080013"]["Value"][0].asString());

  ASSERT_EQ("UC", visitor.GetResult() ["00080119"]["vr"].asString());
  ASSERT_EQ("UC", visitor.GetResult() ["00080119"]["Value"][0].asString());

  ASSERT_EQ("UI", visitor.GetResult() ["00080016"]["vr"].asString());
  ASSERT_EQ("UI", visitor.GetResult() ["00080016"]["Value"][0].asString());

  ASSERT_EQ("UL", visitor.GetResult() ["00081161"]["vr"].asString());
  ASSERT_EQ(128, visitor.GetResult() ["00081161"]["Value"][0].asUInt());

  ASSERT_EQ("UN", visitor.GetResult() ["43421234"]["vr"].asString());
  Toolbox::DecodeBase64(s, visitor.GetResult() ["43421234"]["InlineBinary"].asString());
  ASSERT_EQ("UN", s);

  ASSERT_EQ("UR", visitor.GetResult() ["00080120"]["vr"].asString());
  ASSERT_EQ("UR", visitor.GetResult() ["00080120"]["Value"][0].asString());

  ASSERT_EQ("US", visitor.GetResult() ["00080301"]["vr"].asString());
  ASSERT_EQ(17, visitor.GetResult() ["00080301"]["Value"][0].asUInt());

  ASSERT_EQ("UT", visitor.GetResult() ["00400031"]["vr"].asString());
  ASSERT_EQ("UT", visitor.GetResult() ["00400031"]["Value"][0].asString());


  pugi::xml_document xml;
  visitor.FormatXml(xml);
  xml.print(std::cout);  
}


TEST(DicomWebJson, Sequence)
{
  ParsedDicomFile dicom(false);
  
  {
    std::auto_ptr<DcmSequenceOfItems> sequence(new DcmSequenceOfItems(DCM_ReferencedSeriesSequence));

    for (unsigned int i = 0; i < 3; i++)
    {
      std::auto_ptr<DcmItem> item(new DcmItem);
      std::string s = "item" + boost::lexical_cast<std::string>(i);
      item->putAndInsertString(DCM_ReferencedSOPInstanceUID, s.c_str(), OFFalse);
      ASSERT_TRUE(sequence->insert(item.release(), false, false).good());
    }

    ASSERT_TRUE(dicom.GetDcmtkObject().getDataset()->insert(sequence.release(), false, false).good());
  }

  Orthanc::DicomWebJsonVisitor visitor;
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

  pugi::xml_document xml;
  visitor.FormatXml(xml);
  xml.print(std::cout);
}
