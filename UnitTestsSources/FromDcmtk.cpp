#include "gtest/gtest.h"

#include "../OrthancServer/FromDcmtkBridge.h"
#include "../OrthancServer/OrthancInitialization.h"
#include "../OrthancServer/DicomModification.h"
#include "../Core/OrthancException.h"

using namespace Orthanc;

TEST(DicomFormat, Tag)
{
  ASSERT_EQ("PatientName", FromDcmtkBridge::GetName(DicomTag(0x0010, 0x0010)));

  DicomTag t = FromDcmtkBridge::ParseTag("SeriesDescription");
  ASSERT_EQ(0x0008, t.GetGroup());
  ASSERT_EQ(0x103E, t.GetElement());

  t = FromDcmtkBridge::ParseTag("0020-e040");
  ASSERT_EQ(0x0020, t.GetGroup());
  ASSERT_EQ(0xe040, t.GetElement());

  // Test ==() and !=() operators
  ASSERT_TRUE(DICOM_TAG_PATIENT_ID == DicomTag(0x0010, 0x0020));
  ASSERT_FALSE(DICOM_TAG_PATIENT_ID != DicomTag(0x0010, 0x0020));
}


TEST(DicomModification, Basic)
{
  DicomModification m;
  m.SetupAnonymization();
  //m.SetLevel(DicomRootLevel_Study);
  //m.Replace(DICOM_TAG_PATIENT_ID, "coucou");
  //m.Replace(DICOM_TAG_PATIENT_NAME, "coucou");

  ParsedDicomFile o;
  o.SaveToFile("anon.dcm");

  for (int i = 0; i < 10; i++)
  {
    char b[1024];
    sprintf(b, "anon%06d.dcm", i);
    std::auto_ptr<ParsedDicomFile> f(o.Clone());
    if (i > 4)
      o.Replace(DICOM_TAG_SERIES_INSTANCE_UID, "coucou");
    m.Apply(*f);
    f->SaveToFile(b);
  }
}
