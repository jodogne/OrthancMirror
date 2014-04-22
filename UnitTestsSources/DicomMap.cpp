#include "gtest/gtest.h"

#include "../Core/Uuid.h"
#include "../Core/OrthancException.h"
#include "../Core/DicomFormat/DicomMap.h"
#include "../Core/DicomFormat/DicomNullValue.h"

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
  DicomMap m;
  ASSERT_FALSE(m.HasTag(DICOM_TAG_PATIENT_NAME));
  ASSERT_FALSE(m.HasTag(0x0010, 0x0010));
  m.SetValue(0x0010, 0x0010, "PatientName");
  ASSERT_TRUE(m.HasTag(DICOM_TAG_PATIENT_NAME));
  ASSERT_TRUE(m.HasTag(0x0010, 0x0010));

  ASSERT_FALSE(m.HasTag(DICOM_TAG_PATIENT_ID));
  m.SetValue(DICOM_TAG_PATIENT_ID, "PatientID");
  ASSERT_TRUE(m.HasTag(0x0010, 0x0020));
  m.SetValue(DICOM_TAG_PATIENT_ID, "PatientID2");
  ASSERT_EQ("PatientID2", m.GetValue(0x0010, 0x0020).AsString());

  m.Remove(DICOM_TAG_PATIENT_ID);
  ASSERT_THROW(m.GetValue(0x0010, 0x0020), OrthancException);

  std::auto_ptr<DicomMap> mm(m.Clone());
  ASSERT_EQ("PatientName", mm->GetValue(DICOM_TAG_PATIENT_NAME).AsString());  

  m.SetValue(DICOM_TAG_PATIENT_ID, "Hello");
  ASSERT_THROW(mm->GetValue(DICOM_TAG_PATIENT_ID), OrthancException);
  mm->CopyTagIfExists(m, DICOM_TAG_PATIENT_ID);
  ASSERT_EQ("Hello", mm->GetValue(DICOM_TAG_PATIENT_ID).AsString());  

  DicomNullValue v;
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
