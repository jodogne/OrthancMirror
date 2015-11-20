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


#include "PrecompiledHeadersUnitTests.h"
#include "gtest/gtest.h"

#include "../OrthancServer/FromDcmtkBridge.h"
#include "../OrthancServer/OrthancInitialization.h"
#include "../OrthancServer/DicomModification.h"
#include "../OrthancServer/ServerToolbox.h"
#include "../Core/OrthancException.h"
#include "../Core/Images/ImageBuffer.h"
#include "../Core/Images/PngReader.h"
#include "../Core/Images/PngWriter.h"
#include "../Core/Uuid.h"
#include "../Resources/EncodingTests.h"
#include "../OrthancServer/DicomProtocol/DicomFindAnswers.h"

#include <dcmtk/dcmdata/dcelem.h>

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
  o.SaveToFile("UnitTestsResults/anon.dcm");

  for (int i = 0; i < 10; i++)
  {
    char b[1024];
    sprintf(b, "UnitTestsResults/anon%06d.dcm", i);
    std::auto_ptr<ParsedDicomFile> f(o.Clone());
    if (i > 4)
      o.Replace(DICOM_TAG_SERIES_INSTANCE_UID, "coucou");
    m.Apply(*f);
    f->SaveToFile(b);
  }
}


TEST(DicomModification, Anonymization)
{
  ASSERT_EQ(DICOM_TAG_PATIENT_NAME, FromDcmtkBridge::ParseTag("PatientName"));

  const DicomTag privateTag(0x0045, 0x0010);
  const DicomTag privateTag2(FromDcmtkBridge::ParseTag("0031-1020"));
  ASSERT_TRUE(FromDcmtkBridge::IsPrivateTag(privateTag));
  ASSERT_TRUE(FromDcmtkBridge::IsPrivateTag(privateTag2));
  ASSERT_EQ(0x0031, privateTag2.GetGroup());
  ASSERT_EQ(0x1020, privateTag2.GetElement());

  std::string s;
  ParsedDicomFile o;
  o.Replace(DICOM_TAG_PATIENT_NAME, "coucou");
  ASSERT_FALSE(o.GetTagValue(s, privateTag));
  o.Insert(privateTag, "private tag", false);
  ASSERT_TRUE(o.GetTagValue(s, privateTag));
  ASSERT_STREQ("private tag", s.c_str());

  ASSERT_FALSE(o.GetTagValue(s, privateTag2));
  ASSERT_THROW(o.Replace(privateTag2, "hello", DicomReplaceMode_ThrowIfAbsent), OrthancException);
  ASSERT_FALSE(o.GetTagValue(s, privateTag2));
  o.Replace(privateTag2, "hello", DicomReplaceMode_IgnoreIfAbsent);
  ASSERT_FALSE(o.GetTagValue(s, privateTag2));
  o.Replace(privateTag2, "hello", DicomReplaceMode_InsertIfAbsent);
  ASSERT_TRUE(o.GetTagValue(s, privateTag2));
  ASSERT_STREQ("hello", s.c_str());
  o.Replace(privateTag2, "hello world");
  ASSERT_TRUE(o.GetTagValue(s, privateTag2));
  ASSERT_STREQ("hello world", s.c_str());

  ASSERT_TRUE(o.GetTagValue(s, DICOM_TAG_PATIENT_NAME));
  ASSERT_FALSE(Toolbox::IsUuid(s));

  DicomModification m;
  m.SetupAnonymization();
  m.Keep(privateTag);

  m.Apply(o);

  ASSERT_TRUE(o.GetTagValue(s, DICOM_TAG_PATIENT_NAME));
  ASSERT_TRUE(Toolbox::IsUuid(s));
  ASSERT_TRUE(o.GetTagValue(s, privateTag));
  ASSERT_STREQ("private tag", s.c_str());
  
  m.SetupAnonymization();
  m.Apply(o);
  ASSERT_FALSE(o.GetTagValue(s, privateTag));
}


#include <dcmtk/dcmdata/dcuid.h>

TEST(DicomModification, Png)
{
  // Red dot in http://en.wikipedia.org/wiki/Data_URI_scheme (RGBA image)
  std::string s = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==";

  std::string m, cc;
  Toolbox::DecodeDataUriScheme(m, cc, s);

  ASSERT_EQ("image/png", m);

  PngReader reader;
  reader.ReadFromMemory(cc);

  ASSERT_EQ(5u, reader.GetHeight());
  ASSERT_EQ(5u, reader.GetWidth());
  ASSERT_EQ(PixelFormat_RGBA32, reader.GetFormat());

  ParsedDicomFile o;
  o.EmbedContent(s);
  o.SaveToFile("UnitTestsResults/png1.dcm");

  // Red dot, without alpha channel
  s = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAIAAAACDbGyAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH3gUGDTcIn2+8BgAAACJJREFUCNdj/P//PwMjIwME/P/P+J8BBTAxEOL/R9Lx/z8AynoKAXOeiV8AAAAASUVORK5CYII=";
  o.EmbedContent(s);
  o.SaveToFile("UnitTestsResults/png2.dcm");

  // Check box in Graylevel8
  s = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAAAAAA6mKC9AAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH3gUGDDcB53FulQAAAElJREFUGNNtj0sSAEEEQ1+U+185s1CtmRkblQ9CZldsKHJDk6DLGLJa6chjh0ooQmpjXMM86zPwydGEj6Ed/UGykkEM8X+p3u8/8LcOJIWLGeMAAAAASUVORK5CYII=";
  o.EmbedContent(s);
  //o.Replace(DICOM_TAG_SOP_CLASS_UID, UID_DigitalXRayImageStorageForProcessing);
  o.SaveToFile("UnitTestsResults/png3.dcm");


  {
    // Gradient in Graylevel16

    ImageBuffer img;
    img.SetWidth(256);
    img.SetHeight(256);
    img.SetFormat(PixelFormat_Grayscale16);

    uint16_t v = 0;
    for (unsigned int y = 0; y < img.GetHeight(); y++)
    {
      uint16_t *p = reinterpret_cast<uint16_t*>(img.GetAccessor().GetRow(y));
      for (unsigned int x = 0; x < img.GetWidth(); x++, p++, v++)
      {
        *p = v;
      }
    }

    o.EmbedImage(img.GetAccessor());
    o.SaveToFile("UnitTestsResults/png4.dcm");
  }
}


TEST(FromDcmtkBridge, Encodings1)
{
  for (unsigned int i = 0; i < testEncodingsCount; i++)
  {
    std::string source(testEncodingsEncoded[i]);
    std::string expected(testEncodingsExpected[i]);
    std::string s = Toolbox::ConvertToUtf8(source, testEncodings[i]);
    //std::cout << EnumerationToString(testEncodings[i]) << std::endl;
    EXPECT_EQ(expected, s);
  }
}


TEST(FromDcmtkBridge, Enumerations)
{
  Encoding e;

  ASSERT_FALSE(GetDicomEncoding(e, ""));
  ASSERT_TRUE(GetDicomEncoding(e, "ISO_IR 6"));  ASSERT_EQ(Encoding_Utf8, e);

  // http://www.dabsoft.ch/dicom/3/C.12.1.1.2/ - Table C.12-2
  ASSERT_TRUE(GetDicomEncoding(e, "ISO_IR 100"));  ASSERT_EQ(Encoding_Latin1, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO_IR 101"));  ASSERT_EQ(Encoding_Latin2, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO_IR 109"));  ASSERT_EQ(Encoding_Latin3, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO_IR 110"));  ASSERT_EQ(Encoding_Latin4, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO_IR 144"));  ASSERT_EQ(Encoding_Cyrillic, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO_IR 127"));  ASSERT_EQ(Encoding_Arabic, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO_IR 126"));  ASSERT_EQ(Encoding_Greek, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO_IR 138"));  ASSERT_EQ(Encoding_Hebrew, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO_IR 148"));  ASSERT_EQ(Encoding_Latin5, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO_IR 13"));  ASSERT_EQ(Encoding_Japanese, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO_IR 166"));  ASSERT_EQ(Encoding_Thai, e);

  // http://www.dabsoft.ch/dicom/3/C.12.1.1.2/ - Table C.12-3
  ASSERT_TRUE(GetDicomEncoding(e, "ISO 2022 IR 6"));  ASSERT_EQ(Encoding_Utf8, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO 2022 IR 100"));  ASSERT_EQ(Encoding_Latin1, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO 2022 IR 101"));  ASSERT_EQ(Encoding_Latin2, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO 2022 IR 109"));  ASSERT_EQ(Encoding_Latin3, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO 2022 IR 110"));  ASSERT_EQ(Encoding_Latin4, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO 2022 IR 144"));  ASSERT_EQ(Encoding_Cyrillic, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO 2022 IR 127"));  ASSERT_EQ(Encoding_Arabic, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO 2022 IR 126"));  ASSERT_EQ(Encoding_Greek, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO 2022 IR 138"));  ASSERT_EQ(Encoding_Hebrew, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO 2022 IR 148"));  ASSERT_EQ(Encoding_Latin5, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO 2022 IR 13"));  ASSERT_EQ(Encoding_Japanese, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO 2022 IR 166"));  ASSERT_EQ(Encoding_Thai, e);

  // http://www.dabsoft.ch/dicom/3/C.12.1.1.2/ - Table C.12-4
  ASSERT_FALSE(GetDicomEncoding(e, "ISO 2022 IR 87"));  //ASSERT_EQ(Encoding_JapaneseKanji, e);
  ASSERT_FALSE(GetDicomEncoding(e, "ISO 2022 IR 159"));  //ASSERT_EQ(Encoding_JapaneseKanjiSupplementary, e);
  ASSERT_FALSE(GetDicomEncoding(e, "ISO 2022 IR 149"));  //ASSERT_EQ(Encoding_Korean, e);

  // http://www.dabsoft.ch/dicom/3/C.12.1.1.2/ - Table C.12-5
  ASSERT_TRUE(GetDicomEncoding(e, "ISO_IR 192"));  ASSERT_EQ(Encoding_Utf8, e);
  ASSERT_TRUE(GetDicomEncoding(e, "GB18030")); ASSERT_EQ(Encoding_Chinese, e);
}


TEST(FromDcmtkBridge, Encodings3)
{
  for (unsigned int i = 0; i < testEncodingsCount; i++)
  {
    //std::cout << EnumerationToString(testEncodings[i]) << std::endl;
    std::string dicom;

    {
      ParsedDicomFile f;
      f.SetEncoding(testEncodings[i]);

      std::string s = Toolbox::ConvertToUtf8(testEncodingsEncoded[i], testEncodings[i]);
      f.Insert(DICOM_TAG_PATIENT_NAME, s, false);
      f.SaveToMemoryBuffer(dicom);
    }

    if (testEncodings[i] != Encoding_Windows1251)
    {
      ParsedDicomFile g(dicom);

      if (testEncodings[i] != Encoding_Ascii)
      {
        ASSERT_EQ(testEncodings[i], g.GetEncoding());
      }

      std::string tag;
      ASSERT_TRUE(g.GetTagValue(tag, DICOM_TAG_PATIENT_NAME));
      ASSERT_EQ(std::string(testEncodingsExpected[i]), tag);
    }
  }
}


TEST(FromDcmtkBridge, ValueRepresentation)
{
  ASSERT_EQ(ValueRepresentation_PatientName, 
            FromDcmtkBridge::GetValueRepresentation(DICOM_TAG_PATIENT_NAME));
  ASSERT_EQ(ValueRepresentation_Date, 
            FromDcmtkBridge::GetValueRepresentation(DicomTag(0x0008, 0x0020) /* StudyDate */));
  ASSERT_EQ(ValueRepresentation_Time, 
            FromDcmtkBridge::GetValueRepresentation(DicomTag(0x0008, 0x0030) /* StudyTime */));
  ASSERT_EQ(ValueRepresentation_DateTime, 
            FromDcmtkBridge::GetValueRepresentation(DicomTag(0x0008, 0x002a) /* AcquisitionDateTime */));
  ASSERT_EQ(ValueRepresentation_Other, 
            FromDcmtkBridge::GetValueRepresentation(DICOM_TAG_PATIENT_ID));
}



static const DicomTag REFERENCED_STUDY_SEQUENCE(0x0008, 0x1110);
static const DicomTag REFERENCED_PATIENT_SEQUENCE(0x0008, 0x1120);

static void CreateSampleJson(Json::Value& a)
{
  {
    Json::Value b = Json::objectValue;
    b["PatientName"] = "Hello";
    b["PatientID"] = "World";
    b["StudyDescription"] = "Toto";
    a.append(b);
  }

  {
    Json::Value b = Json::objectValue;
    b["PatientName"] = "data:application/octet-stream;base64,SGVsbG8y";  // echo -n "Hello2" | base64
    b["PatientID"] = "World2";
    a.append(b);
  }
}


TEST(FromDcmtkBridge, FromJson)
{
  std::auto_ptr<DcmElement> element;

  {
    Json::Value a;
    a = "Hello";
    element.reset(FromDcmtkBridge::FromJson(DICOM_TAG_PATIENT_NAME, a, false, Encoding_Utf8));

    Json::Value b;
    FromDcmtkBridge::ToJson(b, *element, DicomToJsonFormat_Short, DicomToJsonFlags_Default, 0, Encoding_Ascii);
    ASSERT_EQ("Hello", b["0010,0010"].asString());
  }

  {
    Json::Value a;
    a = "Hello";
    // Cannot assign a string to a sequence
    ASSERT_THROW(element.reset(FromDcmtkBridge::FromJson(REFERENCED_STUDY_SEQUENCE, a, false, Encoding_Utf8)), OrthancException);
  }

  {
    Json::Value a = Json::arrayValue;
    a.append("Hello");
    // Cannot assign an array to a string
    ASSERT_THROW(element.reset(FromDcmtkBridge::FromJson(DICOM_TAG_PATIENT_NAME, a, false, Encoding_Utf8)), OrthancException);
  }

  {
    Json::Value a;
    a = "data:application/octet-stream;base64,SGVsbG8=";  // echo -n "Hello" | base64
    element.reset(FromDcmtkBridge::FromJson(DICOM_TAG_PATIENT_NAME, a, true, Encoding_Utf8));

    Json::Value b;
    FromDcmtkBridge::ToJson(b, *element, DicomToJsonFormat_Short, DicomToJsonFlags_Default, 0, Encoding_Ascii);
    ASSERT_EQ("Hello", b["0010,0010"].asString());
  }

  {
    Json::Value a = Json::arrayValue;
    CreateSampleJson(a);
    element.reset(FromDcmtkBridge::FromJson(REFERENCED_STUDY_SEQUENCE, a, true, Encoding_Utf8));

    {
      Json::Value b;
      FromDcmtkBridge::ToJson(b, *element, DicomToJsonFormat_Short, DicomToJsonFlags_Default, 0, Encoding_Ascii);
      ASSERT_EQ(Json::arrayValue, b["0008,1110"].type());
      ASSERT_EQ(2, b["0008,1110"].size());
      
      Json::Value::ArrayIndex i = (b["0008,1110"][0]["0010,0010"].asString() == "Hello") ? 0 : 1;

      ASSERT_EQ(3, b["0008,1110"][i].size());
      ASSERT_EQ(2, b["0008,1110"][1 - i].size());
      ASSERT_EQ(b["0008,1110"][i]["0010,0010"].asString(), "Hello");
      ASSERT_EQ(b["0008,1110"][i]["0010,0020"].asString(), "World");
      ASSERT_EQ(b["0008,1110"][i]["0008,1030"].asString(), "Toto");
      ASSERT_EQ(b["0008,1110"][1 - i]["0010,0010"].asString(), "Hello2");
      ASSERT_EQ(b["0008,1110"][1 - i]["0010,0020"].asString(), "World2");
    }

    {
      Json::Value b;
      FromDcmtkBridge::ToJson(b, *element, DicomToJsonFormat_Full, DicomToJsonFlags_Default, 0, Encoding_Ascii);

      Json::Value c;
      Toolbox::SimplifyTags(c, b);

      a[1]["PatientName"] = "Hello2";  // To remove the Data URI Scheme encoding
      ASSERT_EQ(0, c["ReferencedStudySequence"].compare(a));
    }
  }
}



TEST(ParsedDicomFile, InsertReplaceStrings)
{
  ParsedDicomFile f;

  f.Insert(DICOM_TAG_PATIENT_NAME, "World", false);
  ASSERT_THROW(f.Insert(DICOM_TAG_PATIENT_ID, "Hello", false), OrthancException);  // Already existing tag
  f.Replace(DICOM_TAG_SOP_INSTANCE_UID, "Toto");  // (*)
  f.Replace(DICOM_TAG_SOP_CLASS_UID, "Tata");  // (**)

  std::string s;

  ASSERT_THROW(f.Replace(DICOM_TAG_ACCESSION_NUMBER, "Accession", DicomReplaceMode_ThrowIfAbsent), OrthancException);
  f.Replace(DICOM_TAG_ACCESSION_NUMBER, "Accession", DicomReplaceMode_IgnoreIfAbsent);
  ASSERT_FALSE(f.GetTagValue(s, DICOM_TAG_ACCESSION_NUMBER));
  f.Replace(DICOM_TAG_ACCESSION_NUMBER, "Accession", DicomReplaceMode_InsertIfAbsent);
  ASSERT_TRUE(f.GetTagValue(s, DICOM_TAG_ACCESSION_NUMBER));
  ASSERT_EQ(s, "Accession");
  f.Replace(DICOM_TAG_ACCESSION_NUMBER, "Accession2", DicomReplaceMode_IgnoreIfAbsent);
  ASSERT_TRUE(f.GetTagValue(s, DICOM_TAG_ACCESSION_NUMBER));
  ASSERT_EQ(s, "Accession2");
  f.Replace(DICOM_TAG_ACCESSION_NUMBER, "Accession3", DicomReplaceMode_ThrowIfAbsent);
  ASSERT_TRUE(f.GetTagValue(s, DICOM_TAG_ACCESSION_NUMBER));
  ASSERT_EQ(s, "Accession3");

  ASSERT_TRUE(f.GetTagValue(s, DICOM_TAG_PATIENT_NAME));
  ASSERT_EQ(s, "World");
  ASSERT_TRUE(f.GetTagValue(s, DICOM_TAG_SOP_INSTANCE_UID));
  ASSERT_EQ(s, "Toto");
  ASSERT_TRUE(f.GetTagValue(s, DICOM_TAG_MEDIA_STORAGE_SOP_INSTANCE_UID));  // Implicitly modified by (*)
  ASSERT_EQ(s, "Toto");
  ASSERT_TRUE(f.GetTagValue(s, DICOM_TAG_SOP_CLASS_UID));
  ASSERT_EQ(s, "Tata");
  ASSERT_TRUE(f.GetTagValue(s, DICOM_TAG_MEDIA_STORAGE_SOP_CLASS_UID));  // Implicitly modified by (**)
  ASSERT_EQ(s, "Tata");
}




TEST(ParsedDicomFile, InsertReplaceJson)
{
  ParsedDicomFile f;

  Json::Value a;
  CreateSampleJson(a);

  ASSERT_FALSE(f.HasTag(REFERENCED_STUDY_SEQUENCE));
  f.Remove(REFERENCED_STUDY_SEQUENCE);  // No effect
  f.Insert(REFERENCED_STUDY_SEQUENCE, a, true);
  ASSERT_TRUE(f.HasTag(REFERENCED_STUDY_SEQUENCE));
  ASSERT_THROW(f.Insert(REFERENCED_STUDY_SEQUENCE, a, true), OrthancException);
  f.Remove(REFERENCED_STUDY_SEQUENCE);
  ASSERT_FALSE(f.HasTag(REFERENCED_STUDY_SEQUENCE));
  f.Insert(REFERENCED_STUDY_SEQUENCE, a, true);
  ASSERT_TRUE(f.HasTag(REFERENCED_STUDY_SEQUENCE));

  ASSERT_FALSE(f.HasTag(REFERENCED_PATIENT_SEQUENCE));
  ASSERT_THROW(f.Replace(REFERENCED_PATIENT_SEQUENCE, a, false, DicomReplaceMode_ThrowIfAbsent), OrthancException);
  ASSERT_FALSE(f.HasTag(REFERENCED_PATIENT_SEQUENCE));
  f.Replace(REFERENCED_PATIENT_SEQUENCE, a, false, DicomReplaceMode_IgnoreIfAbsent);
  ASSERT_FALSE(f.HasTag(REFERENCED_PATIENT_SEQUENCE));
  f.Replace(REFERENCED_PATIENT_SEQUENCE, a, false, DicomReplaceMode_InsertIfAbsent);
  ASSERT_TRUE(f.HasTag(REFERENCED_PATIENT_SEQUENCE));

  {
    Json::Value b;
    f.ToJson(b, DicomToJsonFormat_Full, DicomToJsonFlags_Default, 0);

    Json::Value c;
    Toolbox::SimplifyTags(c, b);

    ASSERT_EQ(0, c["ReferencedPatientSequence"].compare(a));
    ASSERT_NE(0, c["ReferencedStudySequence"].compare(a));  // Because Data URI Scheme decoding was enabled
  }

  a = "data:application/octet-stream;base64,VGF0YQ==";   // echo -n "Tata" | base64 
  f.Replace(DICOM_TAG_SOP_INSTANCE_UID, a, false);  // (*)
  f.Replace(DICOM_TAG_SOP_CLASS_UID, a, true);  // (**)

  std::string s;
  ASSERT_TRUE(f.GetTagValue(s, DICOM_TAG_SOP_INSTANCE_UID));
  ASSERT_EQ(s, a.asString());
  ASSERT_TRUE(f.GetTagValue(s, DICOM_TAG_MEDIA_STORAGE_SOP_INSTANCE_UID));  // Implicitly modified by (*)
  ASSERT_EQ(s, a.asString());
  ASSERT_TRUE(f.GetTagValue(s, DICOM_TAG_SOP_CLASS_UID));
  ASSERT_EQ(s, "Tata");
  ASSERT_TRUE(f.GetTagValue(s, DICOM_TAG_MEDIA_STORAGE_SOP_CLASS_UID));  // Implicitly modified by (**)
  ASSERT_EQ(s, "Tata");
}


TEST(ParsedDicomFile, JsonEncoding)
{
  ParsedDicomFile f;

  for (unsigned int i = 0; i < testEncodingsCount; i++)
  {
    if (testEncodings[i] != Encoding_Windows1251)
    {
      //std::cout << EnumerationToString(testEncodings[i]) << std::endl;
      f.SetEncoding(testEncodings[i]);

      if (testEncodings[i] != Encoding_Ascii)
      {
        ASSERT_EQ(testEncodings[i], f.GetEncoding());
      }

      Json::Value s = Toolbox::ConvertToUtf8(testEncodingsEncoded[i], testEncodings[i]);
      f.Replace(DICOM_TAG_PATIENT_NAME, s, false);

      Json::Value v;
      f.ToJson(v, DicomToJsonFormat_Simple, DicomToJsonFlags_Default, 0);
      ASSERT_EQ(v["PatientName"].asString(), std::string(testEncodingsExpected[i]));
    }
  }
}


TEST(ParsedDicomFile, ToJsonFlags1)
{
  FromDcmtkBridge::RegisterDictionaryTag(DicomTag(0x7053, 0x1000), EVR_PN, "MyPrivateTag", 1, 1);
  FromDcmtkBridge::RegisterDictionaryTag(DicomTag(0x7050, 0x1000), EVR_PN, "Declared public tag", 1, 1);

  ParsedDicomFile f;
  f.Insert(DicomTag(0x7050, 0x1000), "Some public tag", false);  // Even group => public tag
  f.Insert(DicomTag(0x7052, 0x1000), "Some unknown tag", false);  // Even group => public, unknown tag
  f.Insert(DicomTag(0x7053, 0x1000), "Some private tag", false);  // Odd group => private tag

  Json::Value v;
  f.ToJson(v, DicomToJsonFormat_Short, DicomToJsonFlags_None, 0);
  ASSERT_EQ(Json::objectValue, v.type());
  ASSERT_EQ(6, v.getMemberNames().size());
  ASSERT_FALSE(v.isMember("7052,1000"));
  ASSERT_FALSE(v.isMember("7053,1000"));
  ASSERT_TRUE(v.isMember("7050,1000"));
  ASSERT_EQ(Json::stringValue, v["7050,1000"].type());
  ASSERT_EQ("Some public tag", v["7050,1000"].asString());

  f.ToJson(v, DicomToJsonFormat_Short, DicomToJsonFlags_IncludePrivateTags, 0);
  ASSERT_EQ(Json::objectValue, v.type());
  ASSERT_EQ(7, v.getMemberNames().size());
  ASSERT_FALSE(v.isMember("7052,1000"));
  ASSERT_TRUE(v.isMember("7050,1000"));
  ASSERT_TRUE(v.isMember("7053,1000"));
  ASSERT_EQ("Some public tag", v["7050,1000"].asString());
  ASSERT_EQ(Json::nullValue, v["7053,1000"].type());  // TODO SHOULD BE STRING

  f.ToJson(v, DicomToJsonFormat_Short, DicomToJsonFlags_IncludeUnknownTags, 0);
  ASSERT_EQ(Json::objectValue, v.type());
  ASSERT_EQ(7, v.getMemberNames().size());
  ASSERT_TRUE(v.isMember("7050,1000"));
  ASSERT_TRUE(v.isMember("7052,1000"));
  ASSERT_FALSE(v.isMember("7053,1000"));
  ASSERT_EQ("Some public tag", v["7050,1000"].asString());
  ASSERT_EQ(Json::nullValue, v["7052,1000"].type());  // TODO SHOULD BE STRING

  f.ToJson(v, DicomToJsonFormat_Short, static_cast<DicomToJsonFlags>(DicomToJsonFlags_IncludeUnknownTags | DicomToJsonFlags_IncludePrivateTags), 0);
  ASSERT_EQ(Json::objectValue, v.type());
  ASSERT_EQ(8, v.getMemberNames().size());
  ASSERT_TRUE(v.isMember("7050,1000"));
  ASSERT_TRUE(v.isMember("7052,1000"));
  ASSERT_TRUE(v.isMember("7053,1000"));
  ASSERT_EQ("Some public tag", v["7050,1000"].asString());
  ASSERT_EQ(Json::nullValue, v["7052,1000"].type());  // TODO SHOULD BE STRING
  ASSERT_EQ(Json::nullValue, v["7053,1000"].type());  // TODO SHOULD BE STRING
}


TEST(ParsedDicomFile, ToJsonFlags2)
{
  ParsedDicomFile f;
  f.Insert(DICOM_TAG_PIXEL_DATA, "Pixels", false);

  Json::Value v;
  f.ToJson(v, DicomToJsonFormat_Short, DicomToJsonFlags_None, 0);
  ASSERT_EQ(Json::objectValue, v.type());
  ASSERT_EQ(5, v.getMemberNames().size());
  ASSERT_FALSE(v.isMember("7fe0,0010"));  

  f.ToJson(v, DicomToJsonFormat_Short, static_cast<DicomToJsonFlags>(DicomToJsonFlags_IncludePixelData | DicomToJsonFlags_ConvertBinaryToNull), 0);
  ASSERT_EQ(Json::objectValue, v.type());
  ASSERT_EQ(6, v.getMemberNames().size());
  ASSERT_TRUE(v.isMember("7fe0,0010"));  
  ASSERT_EQ(Json::nullValue, v["7fe0,0010"].type());  

  f.ToJson(v, DicomToJsonFormat_Short, static_cast<DicomToJsonFlags>(DicomToJsonFlags_IncludePixelData | DicomToJsonFlags_ConvertBinaryToAscii), 0);
  ASSERT_EQ(Json::objectValue, v.type());
  ASSERT_EQ(6, v.getMemberNames().size());
  ASSERT_TRUE(v.isMember("7fe0,0010"));  
  ASSERT_EQ(Json::stringValue, v["7fe0,0010"].type());  
  ASSERT_EQ("Pixels", v["7fe0,0010"].asString());  

  f.ToJson(v, DicomToJsonFormat_Short, DicomToJsonFlags_IncludePixelData, 0);
  ASSERT_EQ(Json::objectValue, v.type());
  ASSERT_EQ(6, v.getMemberNames().size());
  ASSERT_TRUE(v.isMember("7fe0,0010"));  
  ASSERT_EQ(Json::stringValue, v["7fe0,0010"].type());
  std::string mime, content;
  Toolbox::DecodeDataUriScheme(mime, content, v["7fe0,0010"].asString());
  ASSERT_EQ("application/octet-stream", mime);
  ASSERT_EQ("Pixels", content);
}


TEST(DicomFindAnswers, Basic)
{
  DicomFindAnswers a;

  {
    DicomMap m;
    m.SetValue(DICOM_TAG_PATIENT_ID, "hello");
    a.Add(m);
  }

  {
    ParsedDicomFile d;
    d.Replace(DICOM_TAG_PATIENT_ID, "my");
    a.Add(d);
  }

  {
    DicomMap m;
    m.SetValue(DICOM_TAG_PATIENT_ID, "world");
    a.Add(m);
  }

  Json::Value j;
  a.ToJson(j, true);
  ASSERT_EQ(3u, j.size());

  //std::cout << j;
}
