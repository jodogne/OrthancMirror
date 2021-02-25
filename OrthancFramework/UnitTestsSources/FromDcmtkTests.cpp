/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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

#if !defined(ORTHANC_ENABLE_DCMTK_TRANSCODING)
#  error ORTHANC_ENABLE_DCMTK_TRANSCODING is not defined
#endif

#if !defined(ORTHANC_ENABLE_PUGIXML)
#  error ORTHANC_ENABLE_PUGIXML is not defined
#endif

#include <gtest/gtest.h>

#include "../Sources/Compatibility.h"
#include "../Sources/DicomNetworking/DicomFindAnswers.h"
#include "../Sources/DicomParsing/DicomModification.h"
#include "../Sources/DicomParsing/DicomWebJsonVisitor.h"
#include "../Sources/DicomParsing/FromDcmtkBridge.h"
#include "../Sources/DicomParsing/ToDcmtkBridge.h"
#include "../Sources/DicomParsing/ParsedDicomCache.h"
#include "../Sources/Endianness.h"
#include "../Sources/Images/Image.h"
#include "../Sources/Images/ImageBuffer.h"
#include "../Sources/Images/ImageProcessing.h"
#include "../Sources/Images/PngReader.h"
#include "../Sources/Images/PngWriter.h"
#include "../Sources/Logging.h"
#include "../Sources/OrthancException.h"
#include "../Resources/CodeGeneration/EncodingTests.h"

#if ORTHANC_SANDBOXED != 1
#  include "../Sources/SystemToolbox.h"
#endif

#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcelem.h>
#include <dcmtk/dcmdata/dcvrat.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/lexical_cast.hpp>

#if ORTHANC_ENABLE_PUGIXML == 1
#  include <pugixml.hpp>
#  if !defined(PUGIXML_VERSION)
#    error PUGIXML_VERSION is not available
#  endif
#endif

using namespace Orthanc;

TEST(DicomFormat, Tag)
{
  ASSERT_EQ("PatientName", FromDcmtkBridge::GetTagName(DicomTag(0x0010, 0x0010), ""));

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


#if ORTHANC_SANDBOXED != 1
TEST(DicomModification, Basic)
{
  DicomModification m;
  m.SetupAnonymization(DicomVersion_2008);
  //m.SetLevel(DicomRootLevel_Study);
  //m.ReplacePlainString(DICOM_TAG_PATIENT_ID, "coucou");
  //m.ReplacePlainString(DICOM_TAG_PATIENT_NAME, "coucou");

  ParsedDicomFile o(true);
  o.SaveToFile("UnitTestsResults/anon.dcm");

  for (int i = 0; i < 10; i++)
  {
    char b[1024];
    sprintf(b, "UnitTestsResults/anon%06d.dcm", i);
    std::unique_ptr<ParsedDicomFile> f(o.Clone(false));
    if (i > 4)
      o.ReplacePlainString(DICOM_TAG_SERIES_INSTANCE_UID, "coucou");
    m.Apply(*f);
    f->SaveToFile(b);
  }
}
#endif


TEST(DicomModification, Anonymization)
{
  ASSERT_EQ(DICOM_TAG_PATIENT_NAME, FromDcmtkBridge::ParseTag("PatientName"));

  const DicomTag privateTag(0x0045, 0x1010);
  const DicomTag privateTag2(FromDcmtkBridge::ParseTag("0031-1020"));
  ASSERT_TRUE(privateTag.IsPrivate());
  ASSERT_TRUE(privateTag2.IsPrivate());
  ASSERT_EQ(0x0031, privateTag2.GetGroup());
  ASSERT_EQ(0x1020, privateTag2.GetElement());

  std::string s;
  ParsedDicomFile o(true);
  o.ReplacePlainString(DICOM_TAG_PATIENT_NAME, "coucou");
  ASSERT_FALSE(o.GetTagValue(s, privateTag));
  o.Insert(privateTag, "private tag", false, "OrthancCreator");
  ASSERT_TRUE(o.GetTagValue(s, privateTag));
  ASSERT_STREQ("private tag", s.c_str());

  ASSERT_FALSE(o.GetTagValue(s, privateTag2));
  ASSERT_THROW(o.Replace(privateTag2, std::string("hello"), false, DicomReplaceMode_ThrowIfAbsent, "OrthancCreator"), OrthancException);
  ASSERT_FALSE(o.GetTagValue(s, privateTag2));
  o.Replace(privateTag2, std::string("hello"), false, DicomReplaceMode_IgnoreIfAbsent, "OrthancCreator");
  ASSERT_FALSE(o.GetTagValue(s, privateTag2));
  o.Replace(privateTag2, std::string("hello"), false, DicomReplaceMode_InsertIfAbsent, "OrthancCreator");
  ASSERT_TRUE(o.GetTagValue(s, privateTag2));
  ASSERT_STREQ("hello", s.c_str());
  o.Replace(privateTag2, std::string("hello world"), false, DicomReplaceMode_InsertIfAbsent, "OrthancCreator");
  ASSERT_TRUE(o.GetTagValue(s, privateTag2));
  ASSERT_STREQ("hello world", s.c_str());

  ASSERT_TRUE(o.GetTagValue(s, DICOM_TAG_PATIENT_NAME));
  ASSERT_FALSE(Toolbox::IsUuid(s));

  DicomModification m;
  m.SetupAnonymization(DicomVersion_2008);
  m.Keep(privateTag);

  m.Apply(o);

  ASSERT_TRUE(o.GetTagValue(s, DICOM_TAG_PATIENT_NAME));
  ASSERT_TRUE(Toolbox::IsUuid(s));
  ASSERT_TRUE(o.GetTagValue(s, privateTag));
  ASSERT_STREQ("private tag", s.c_str());
  
  m.SetupAnonymization(DicomVersion_2008);
  m.Apply(o);
  ASSERT_FALSE(o.GetTagValue(s, privateTag));
}


#include <dcmtk/dcmdata/dcuid.h>

TEST(DicomModification, Png)
{
  // Red dot in http://en.wikipedia.org/wiki/Data_URI_scheme (RGBA image)
  std::string s = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==";

  std::string m, cc;
  ASSERT_TRUE(Toolbox::DecodeDataUriScheme(m, cc, s));

  ASSERT_EQ("image/png", m);

  PngReader reader;
  reader.ReadFromMemory(cc);

  ASSERT_EQ(5u, reader.GetHeight());
  ASSERT_EQ(5u, reader.GetWidth());
  ASSERT_EQ(PixelFormat_RGBA32, reader.GetFormat());

  ParsedDicomFile o(true);
  o.EmbedContent(s);

#if ORTHANC_SANDBOXED != 1
  o.SaveToFile("UnitTestsResults/png1.dcm");
#endif

  // Red dot, without alpha channel
  s = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAIAAAACDbGyAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH3gUGDTcIn2+8BgAAACJJREFUCNdj/P//PwMjIwME/P/P+J8BBTAxEOL/R9Lx/z8AynoKAXOeiV8AAAAASUVORK5CYII=";
  o.EmbedContent(s);

#if ORTHANC_SANDBOXED != 1
  o.SaveToFile("UnitTestsResults/png2.dcm");
#endif

  // Check box in Graylevel8
  s = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAAAAAA6mKC9AAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH3gUGDDcB53FulQAAAElJREFUGNNtj0sSAEEEQ1+U+185s1CtmRkblQ9CZldsKHJDk6DLGLJa6chjh0ooQmpjXMM86zPwydGEj6Ed/UGykkEM8X+p3u8/8LcOJIWLGeMAAAAASUVORK5CYII=";
  o.EmbedContent(s);
  //o.ReplacePlainString(DICOM_TAG_SOP_CLASS_UID, UID_DigitalXRayImageStorageForProcessing);

#if ORTHANC_SANDBOXED != 1
  o.SaveToFile("UnitTestsResults/png3.dcm");
#endif


  {
    // Gradient in Graylevel16

    ImageBuffer img;
    img.SetWidth(256);
    img.SetHeight(256);
    img.SetFormat(PixelFormat_Grayscale16);

    ImageAccessor accessor;
    img.GetWriteableAccessor(accessor);
    
    uint16_t v = 0;
    for (unsigned int y = 0; y < img.GetHeight(); y++)
    {
      uint16_t *p = reinterpret_cast<uint16_t*>(accessor.GetRow(y));
      for (unsigned int x = 0; x < img.GetWidth(); x++, p++, v++)
      {
        *p = v;
      }
    }

    o.EmbedImage(accessor);

#if ORTHANC_SANDBOXED != 1
    o.SaveToFile("UnitTestsResults/png4.dcm");
#endif
  }
}


TEST(FromDcmtkBridge, Encodings1)
{
  for (unsigned int i = 0; i < testEncodingsCount; i++)
  {
    std::string source(testEncodingsEncoded[i]);
    std::string expected(testEncodingsExpected[i]);
    std::string s = Toolbox::ConvertToUtf8(source, testEncodings[i], false);
    //std::cout << EnumerationToString(testEncodings[i]) << std::endl;
    EXPECT_EQ(expected, s);
  }
}


TEST(FromDcmtkBridge, Enumerations)
{
  // http://dicom.nema.org/medical/dicom/current/output/html/part03.html#sect_C.12.1.1.2
  Encoding e;

  ASSERT_FALSE(GetDicomEncoding(e, ""));
  ASSERT_TRUE(GetDicomEncoding(e, "ISO_IR 6"));  ASSERT_EQ(Encoding_Ascii, e);

  // http://dicom.nema.org/medical/dicom/current/output/html/part03.html#table_C.12-2
  ASSERT_TRUE(GetDicomEncoding(e, "ISO_IR 100"));  ASSERT_EQ(Encoding_Latin1, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO_IR 101"));  ASSERT_EQ(Encoding_Latin2, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO_IR 109"));  ASSERT_EQ(Encoding_Latin3, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO_IR 110"));  ASSERT_EQ(Encoding_Latin4, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO_IR 144"));  ASSERT_EQ(Encoding_Cyrillic, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO_IR 127"));  ASSERT_EQ(Encoding_Arabic, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO_IR 126"));  ASSERT_EQ(Encoding_Greek, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO_IR 138"));  ASSERT_EQ(Encoding_Hebrew, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO_IR 148"));  ASSERT_EQ(Encoding_Latin5, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO_IR 13"));   ASSERT_EQ(Encoding_Japanese, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO_IR 166"));  ASSERT_EQ(Encoding_Thai, e);

  // http://dicom.nema.org/medical/dicom/current/output/html/part03.html#table_C.12-3
  ASSERT_TRUE(GetDicomEncoding(e, "ISO 2022 IR 6"));    ASSERT_EQ(Encoding_Ascii, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO 2022 IR 100"));  ASSERT_EQ(Encoding_Latin1, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO 2022 IR 101"));  ASSERT_EQ(Encoding_Latin2, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO 2022 IR 109"));  ASSERT_EQ(Encoding_Latin3, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO 2022 IR 110"));  ASSERT_EQ(Encoding_Latin4, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO 2022 IR 144"));  ASSERT_EQ(Encoding_Cyrillic, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO 2022 IR 127"));  ASSERT_EQ(Encoding_Arabic, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO 2022 IR 126"));  ASSERT_EQ(Encoding_Greek, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO 2022 IR 138"));  ASSERT_EQ(Encoding_Hebrew, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO 2022 IR 148"));  ASSERT_EQ(Encoding_Latin5, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO 2022 IR 13"));   ASSERT_EQ(Encoding_Japanese, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO 2022 IR 166"));  ASSERT_EQ(Encoding_Thai, e);

  // http://dicom.nema.org/medical/dicom/current/output/html/part03.html#table_C.12-4
  ASSERT_TRUE(GetDicomEncoding(e, "ISO 2022 IR 87"));    ASSERT_EQ(Encoding_JapaneseKanji, e);
  ASSERT_FALSE(GetDicomEncoding(e, "ISO 2022 IR 159"));  //ASSERT_EQ(Encoding_JapaneseKanjiSupplementary, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO 2022 IR 149"));   ASSERT_EQ(Encoding_Korean, e);
  ASSERT_TRUE(GetDicomEncoding(e, "ISO 2022 IR 58"));    ASSERT_EQ(Encoding_SimplifiedChinese, e);

  // http://dicom.nema.org/medical/dicom/current/output/html/part03.html#table_C.12-5
  ASSERT_TRUE(GetDicomEncoding(e, "ISO_IR 192"));  ASSERT_EQ(Encoding_Utf8, e);
  ASSERT_TRUE(GetDicomEncoding(e, "GB18030"));     ASSERT_EQ(Encoding_Chinese, e);
  ASSERT_TRUE(GetDicomEncoding(e, "GBK"));         ASSERT_EQ(Encoding_Chinese, e);
}


TEST(FromDcmtkBridge, Encodings3)
{
  for (unsigned int i = 0; i < testEncodingsCount; i++)
  {
    //std::cout << EnumerationToString(testEncodings[i]) << std::endl;
    std::string dicom;

    {
      ParsedDicomFile f(true);
      f.SetEncoding(testEncodings[i]);

      std::string s = Toolbox::ConvertToUtf8(testEncodingsEncoded[i], testEncodings[i], false);
      f.Insert(DICOM_TAG_PATIENT_NAME, s, false, "");
      f.SaveToMemoryBuffer(dicom);
    }

    if (testEncodings[i] != Encoding_Windows1251)
    {
      ParsedDicomFile g(dicom);

      if (testEncodings[i] != Encoding_Ascii)
      {
        bool hasCodeExtensions;
        ASSERT_EQ(testEncodings[i], g.DetectEncoding(hasCodeExtensions));
        ASSERT_FALSE(hasCodeExtensions);
      }

      std::string tag;
      ASSERT_TRUE(g.GetTagValue(tag, DICOM_TAG_PATIENT_NAME));
      ASSERT_EQ(std::string(testEncodingsExpected[i]), tag);
    }
  }
}


TEST(FromDcmtkBridge, ValueRepresentation)
{
  ASSERT_EQ(ValueRepresentation_PersonName, 
            FromDcmtkBridge::LookupValueRepresentation(DICOM_TAG_PATIENT_NAME));
  ASSERT_EQ(ValueRepresentation_Date, 
            FromDcmtkBridge::LookupValueRepresentation(DicomTag(0x0008, 0x0020) /* StudyDate */));
  ASSERT_EQ(ValueRepresentation_Time, 
            FromDcmtkBridge::LookupValueRepresentation(DicomTag(0x0008, 0x0030) /* StudyTime */));
  ASSERT_EQ(ValueRepresentation_DateTime, 
            FromDcmtkBridge::LookupValueRepresentation(DicomTag(0x0008, 0x002a) /* AcquisitionDateTime */));
  ASSERT_EQ(ValueRepresentation_NotSupported, 
            FromDcmtkBridge::LookupValueRepresentation(DicomTag(0x0001, 0x0001) /* some private tag */));
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



TEST(ParsedDicomFile, InsertReplaceStrings)
{
  ParsedDicomFile f(true);

  f.Insert(DICOM_TAG_PATIENT_NAME, "World", false, "");
  ASSERT_THROW(f.Insert(DICOM_TAG_PATIENT_ID, "Hello", false, ""), OrthancException);  // Already existing tag
  f.ReplacePlainString(DICOM_TAG_SOP_INSTANCE_UID, "Toto");  // (*)
  f.ReplacePlainString(DICOM_TAG_SOP_CLASS_UID, "Tata");  // (**)

  DicomTransferSyntax syntax;
  ASSERT_TRUE(f.LookupTransferSyntax(syntax));
  // The default transfer syntax depends on the OS endianness
  ASSERT_TRUE(syntax == DicomTransferSyntax_LittleEndianExplicit ||
              syntax == DicomTransferSyntax_BigEndianExplicit);

  ASSERT_THROW(f.Replace(DICOM_TAG_ACCESSION_NUMBER, std::string("Accession"),
                         false, DicomReplaceMode_ThrowIfAbsent, ""), OrthancException);
  f.Replace(DICOM_TAG_ACCESSION_NUMBER, std::string("Accession"), false, DicomReplaceMode_IgnoreIfAbsent, "");

  std::string s;
  ASSERT_FALSE(f.GetTagValue(s, DICOM_TAG_ACCESSION_NUMBER));
  f.Replace(DICOM_TAG_ACCESSION_NUMBER, std::string("Accession"), false, DicomReplaceMode_InsertIfAbsent, "");
  ASSERT_TRUE(f.GetTagValue(s, DICOM_TAG_ACCESSION_NUMBER));
  ASSERT_EQ(s, "Accession");
  f.Replace(DICOM_TAG_ACCESSION_NUMBER, std::string("Accession2"), false, DicomReplaceMode_IgnoreIfAbsent, "");
  ASSERT_TRUE(f.GetTagValue(s, DICOM_TAG_ACCESSION_NUMBER));
  ASSERT_EQ(s, "Accession2");
  f.Replace(DICOM_TAG_ACCESSION_NUMBER, std::string("Accession3"), false, DicomReplaceMode_ThrowIfAbsent, "");
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
  ParsedDicomFile f(true);

  Json::Value a;
  CreateSampleJson(a);

  ASSERT_FALSE(f.HasTag(REFERENCED_STUDY_SEQUENCE));
  f.Remove(REFERENCED_STUDY_SEQUENCE);  // No effect
  f.Insert(REFERENCED_STUDY_SEQUENCE, a, true, "");
  ASSERT_TRUE(f.HasTag(REFERENCED_STUDY_SEQUENCE));
  ASSERT_THROW(f.Insert(REFERENCED_STUDY_SEQUENCE, a, true, ""), OrthancException);
  f.Remove(REFERENCED_STUDY_SEQUENCE);
  ASSERT_FALSE(f.HasTag(REFERENCED_STUDY_SEQUENCE));
  f.Insert(REFERENCED_STUDY_SEQUENCE, a, true, "");
  ASSERT_TRUE(f.HasTag(REFERENCED_STUDY_SEQUENCE));

  ASSERT_FALSE(f.HasTag(REFERENCED_PATIENT_SEQUENCE));
  ASSERT_THROW(f.Replace(REFERENCED_PATIENT_SEQUENCE, a, false, DicomReplaceMode_ThrowIfAbsent, ""), OrthancException);
  ASSERT_FALSE(f.HasTag(REFERENCED_PATIENT_SEQUENCE));
  f.Replace(REFERENCED_PATIENT_SEQUENCE, a, false, DicomReplaceMode_IgnoreIfAbsent, "");
  ASSERT_FALSE(f.HasTag(REFERENCED_PATIENT_SEQUENCE));
  f.Replace(REFERENCED_PATIENT_SEQUENCE, a, false, DicomReplaceMode_InsertIfAbsent, "");
  ASSERT_TRUE(f.HasTag(REFERENCED_PATIENT_SEQUENCE));

  {
    Json::Value b;
    f.DatasetToJson(b, DicomToJsonFormat_Full, DicomToJsonFlags_Default, 0);

    Json::Value c;
    Toolbox::SimplifyDicomAsJson(c, b, DicomToJsonFormat_Human);

    ASSERT_EQ(0, c["ReferencedPatientSequence"].compare(a));
    ASSERT_NE(0, c["ReferencedStudySequence"].compare(a));  // Because Data URI Scheme decoding was enabled
  }

  a = "data:application/octet-stream;base64,VGF0YQ==";   // echo -n "Tata" | base64 
  f.Replace(DICOM_TAG_SOP_INSTANCE_UID, a, false, DicomReplaceMode_InsertIfAbsent, "");  // (*)
  f.Replace(DICOM_TAG_SOP_CLASS_UID, a, true, DicomReplaceMode_InsertIfAbsent, "");  // (**)

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
  ParsedDicomFile f(true);

  for (unsigned int i = 0; i < testEncodingsCount; i++)
  {
    if (testEncodings[i] != Encoding_Windows1251)
    {
      //std::cout << EnumerationToString(testEncodings[i]) << std::endl;
      f.SetEncoding(testEncodings[i]);

      if (testEncodings[i] != Encoding_Ascii)
      {
        bool hasCodeExtensions;
        ASSERT_EQ(testEncodings[i], f.DetectEncoding(hasCodeExtensions));
        ASSERT_FALSE(hasCodeExtensions);
      }

      Json::Value s = Toolbox::ConvertToUtf8(testEncodingsEncoded[i], testEncodings[i], false);
      f.Replace(DICOM_TAG_PATIENT_NAME, s, false, DicomReplaceMode_InsertIfAbsent, "");

      Json::Value v;
      f.DatasetToJson(v, DicomToJsonFormat_Human, DicomToJsonFlags_Default, 0);
      ASSERT_EQ(v["PatientName"].asString(), std::string(testEncodingsExpected[i]));
    }
  }
}


TEST(ParsedDicomFile, ToJsonFlags1)
{
  FromDcmtkBridge::RegisterDictionaryTag(DicomTag(0x7053, 0x1000), ValueRepresentation_OtherByte, "MyPrivateTag", 1, 1, "OrthancCreator");
  FromDcmtkBridge::RegisterDictionaryTag(DicomTag(0x7050, 0x1000), ValueRepresentation_PersonName, "Declared public tag", 1, 1, "");

  ParsedDicomFile f(true);
  f.Insert(DicomTag(0x7050, 0x1000), "Some public tag", false, "");  // Even group => public tag
  f.Insert(DicomTag(0x7052, 0x1000), "Some unknown tag", false, "");  // Even group => public, unknown tag
  f.Insert(DicomTag(0x7053, 0x1000), "Some private tag", false, "OrthancCreator");  // Odd group => private tag

  Json::Value v;
  f.DatasetToJson(v, DicomToJsonFormat_Short, DicomToJsonFlags_None, 0);
  ASSERT_EQ(Json::objectValue, v.type());
  ASSERT_EQ(6u, v.getMemberNames().size());
  ASSERT_FALSE(v.isMember("7052,1000"));
  ASSERT_FALSE(v.isMember("7053,1000"));
  ASSERT_TRUE(v.isMember("7050,1000"));
  ASSERT_EQ(Json::stringValue, v["7050,1000"].type());
  ASSERT_EQ("Some public tag", v["7050,1000"].asString());

  f.DatasetToJson(v, DicomToJsonFormat_Short, static_cast<DicomToJsonFlags>(DicomToJsonFlags_IncludePrivateTags | DicomToJsonFlags_IncludeBinary | DicomToJsonFlags_ConvertBinaryToNull), 0);
  ASSERT_EQ(Json::objectValue, v.type());
  ASSERT_EQ(7u, v.getMemberNames().size());
  ASSERT_FALSE(v.isMember("7052,1000"));
  ASSERT_TRUE(v.isMember("7050,1000"));
  ASSERT_TRUE(v.isMember("7053,1000"));
  ASSERT_EQ("Some public tag", v["7050,1000"].asString());
  ASSERT_EQ(Json::nullValue, v["7053,1000"].type());

  f.DatasetToJson(v, DicomToJsonFormat_Short, static_cast<DicomToJsonFlags>(DicomToJsonFlags_IncludePrivateTags), 0);
  ASSERT_EQ(Json::objectValue, v.type());
  ASSERT_EQ(6u, v.getMemberNames().size());
  ASSERT_FALSE(v.isMember("7052,1000"));
  ASSERT_TRUE(v.isMember("7050,1000"));
  ASSERT_FALSE(v.isMember("7053,1000"));

  f.DatasetToJson(v, DicomToJsonFormat_Short, static_cast<DicomToJsonFlags>(DicomToJsonFlags_IncludePrivateTags | DicomToJsonFlags_IncludeBinary), 0);
  ASSERT_EQ(Json::objectValue, v.type());
  ASSERT_EQ(7u, v.getMemberNames().size());
  ASSERT_FALSE(v.isMember("7052,1000"));
  ASSERT_TRUE(v.isMember("7050,1000"));
  ASSERT_TRUE(v.isMember("7053,1000"));
  ASSERT_EQ("Some public tag", v["7050,1000"].asString());
  std::string mime, content;
  ASSERT_EQ(Json::stringValue, v["7053,1000"].type());
  ASSERT_TRUE(Toolbox::DecodeDataUriScheme(mime, content, v["7053,1000"].asString()));
  ASSERT_EQ("application/octet-stream", mime);
  ASSERT_EQ("Some private tag", content);

  f.DatasetToJson(v, DicomToJsonFormat_Short, static_cast<DicomToJsonFlags>(DicomToJsonFlags_IncludeUnknownTags | DicomToJsonFlags_IncludeBinary | DicomToJsonFlags_ConvertBinaryToNull), 0);
  ASSERT_EQ(Json::objectValue, v.type());
  ASSERT_EQ(7u, v.getMemberNames().size());
  ASSERT_TRUE(v.isMember("7050,1000"));
  ASSERT_TRUE(v.isMember("7052,1000"));
  ASSERT_FALSE(v.isMember("7053,1000"));
  ASSERT_EQ("Some public tag", v["7050,1000"].asString());
  ASSERT_EQ(Json::nullValue, v["7052,1000"].type());

  f.DatasetToJson(v, DicomToJsonFormat_Short, static_cast<DicomToJsonFlags>(DicomToJsonFlags_IncludeUnknownTags | DicomToJsonFlags_IncludeBinary), 0);
  ASSERT_EQ(Json::objectValue, v.type());
  ASSERT_EQ(7u, v.getMemberNames().size());
  ASSERT_TRUE(v.isMember("7050,1000"));
  ASSERT_TRUE(v.isMember("7052,1000"));
  ASSERT_FALSE(v.isMember("7053,1000"));
  ASSERT_EQ("Some public tag", v["7050,1000"].asString());
  ASSERT_EQ(Json::stringValue, v["7052,1000"].type());
  ASSERT_TRUE(Toolbox::DecodeDataUriScheme(mime, content, v["7052,1000"].asString()));
  ASSERT_EQ("application/octet-stream", mime);
  ASSERT_EQ("Some unknown tag", content);

  f.DatasetToJson(v, DicomToJsonFormat_Short, static_cast<DicomToJsonFlags>(DicomToJsonFlags_IncludeUnknownTags | DicomToJsonFlags_IncludePrivateTags | DicomToJsonFlags_IncludeBinary | DicomToJsonFlags_ConvertBinaryToNull), 0);
  ASSERT_EQ(Json::objectValue, v.type());
  ASSERT_EQ(8u, v.getMemberNames().size());
  ASSERT_TRUE(v.isMember("7050,1000"));
  ASSERT_TRUE(v.isMember("7052,1000"));
  ASSERT_TRUE(v.isMember("7053,1000"));
  ASSERT_EQ("Some public tag", v["7050,1000"].asString());
  ASSERT_EQ(Json::nullValue, v["7052,1000"].type());
  ASSERT_EQ(Json::nullValue, v["7053,1000"].type());
}


TEST(ParsedDicomFile, ToJsonFlags2)
{
  ParsedDicomFile f(true);

  {
    // "ParsedDicomFile" uses Little Endian => 'B' (least significant
    // byte) will be stored first in the memory buffer and in the
    // file, then 'A'. Hence the expected "BA" value below.
    Uint16 v[] = { 'A' * 256 + 'B', 0 };
    ASSERT_TRUE(f.GetDcmtkObject().getDataset()->putAndInsertUint16Array(DCM_PixelData, v, 2).good());
  }

  Json::Value v;
  f.DatasetToJson(v, DicomToJsonFormat_Short, DicomToJsonFlags_None, 0);
  ASSERT_EQ(Json::objectValue, v.type());
  ASSERT_EQ(5u, v.getMemberNames().size());
  ASSERT_FALSE(v.isMember("7fe0,0010"));  

  f.DatasetToJson(v, DicomToJsonFormat_Short, static_cast<DicomToJsonFlags>(DicomToJsonFlags_IncludePixelData | DicomToJsonFlags_ConvertBinaryToNull), 0);
  ASSERT_EQ(Json::objectValue, v.type());
  ASSERT_EQ(6u, v.getMemberNames().size());
  ASSERT_TRUE(v.isMember("7fe0,0010"));  
  ASSERT_EQ(Json::nullValue, v["7fe0,0010"].type());  

  f.DatasetToJson(v, DicomToJsonFormat_Short, static_cast<DicomToJsonFlags>(DicomToJsonFlags_IncludePixelData | DicomToJsonFlags_ConvertBinaryToAscii), 0);
  ASSERT_EQ(Json::objectValue, v.type());
  ASSERT_EQ(6u, v.getMemberNames().size());
  ASSERT_TRUE(v.isMember("7fe0,0010"));  
  ASSERT_EQ(Json::stringValue, v["7fe0,0010"].type());  
  ASSERT_EQ("BA", v["7fe0,0010"].asString().substr(0, 2));

  f.DatasetToJson(v, DicomToJsonFormat_Short, DicomToJsonFlags_IncludePixelData, 0);
  ASSERT_EQ(Json::objectValue, v.type());
  ASSERT_EQ(6u, v.getMemberNames().size());
  ASSERT_TRUE(v.isMember("7fe0,0010"));  
  ASSERT_EQ(Json::stringValue, v["7fe0,0010"].type());
  std::string mime, content;
  ASSERT_TRUE(Toolbox::DecodeDataUriScheme(mime, content, v["7fe0,0010"].asString()));
  ASSERT_EQ("application/octet-stream", mime);
  ASSERT_EQ("BA", content.substr(0, 2));
}


TEST(ParsedDicomFile, ToJsonFlags3)
{
  ParsedDicomFile f(false);

  {
    Uint8 v[2] = { 0, 0 };
    ASSERT_TRUE(f.GetDcmtkObject().getDataset()->putAndInsertString(DCM_PatientName, "HELLO^").good());
    ASSERT_TRUE(f.GetDcmtkObject().getDataset()->putAndInsertUint32(DcmTag(0x4000, 0x0000), 42).good());
    ASSERT_TRUE(f.GetDcmtkObject().getDataset()->putAndInsertUint8Array(DCM_PixelData, v, 2).good());
    ASSERT_TRUE(f.GetDcmtkObject().getDataset()->putAndInsertString(DcmTag(0x07fe1, 0x0010), "WORLD^").good());
  }

  std::string s;
  Toolbox::EncodeDataUriScheme(s, "application/octet-stream", std::string(2, '\0'));

  {
    Json::Value v;
    f.DatasetToJson(v, DicomToJsonFormat_Short, static_cast<DicomToJsonFlags>(DicomToJsonFlags_IncludePrivateTags | DicomToJsonFlags_IncludePixelData | DicomToJsonFlags_StopAfterPixelData), 0);
    ASSERT_EQ(Json::objectValue, v.type());
    ASSERT_EQ(3u, v.size());
    ASSERT_EQ("HELLO^", v["0010,0010"].asString());
    ASSERT_EQ("42", v["4000,0000"].asString());
    ASSERT_EQ(s, v["7fe0,0010"].asString());
  }

  {
    Json::Value v;
    f.DatasetToJson(v, DicomToJsonFormat_Short, static_cast<DicomToJsonFlags>(DicomToJsonFlags_IncludePrivateTags | DicomToJsonFlags_SkipGroupLengths), 0);
    ASSERT_EQ(Json::objectValue, v.type());
    ASSERT_EQ(2u, v.size());
    ASSERT_EQ("HELLO^", v["0010,0010"].asString());
    ASSERT_EQ("WORLD^", v["7fe1,0010"].asString());
  }  
}


TEST(DicomFindAnswers, Basic)
{
  DicomFindAnswers a(false);

  {
    DicomMap m;
    m.SetValue(DICOM_TAG_PATIENT_ID, "hello", false);
    a.Add(m);
  }

  {
    ParsedDicomFile d(true);
    d.ReplacePlainString(DICOM_TAG_PATIENT_ID, "my");
    a.Add(d);
  }

  {
    DicomMap m;
    m.SetValue(DICOM_TAG_PATIENT_ID, "world", false);
    a.Add(m);
  }

  Json::Value j;
  a.ToJson(j, true);
  ASSERT_EQ(3u, j.size());

  //std::cout << j;
}


TEST(ParsedDicomFile, FromJson)
{
  FromDcmtkBridge::RegisterDictionaryTag(DicomTag(0x7057, 0x1000), ValueRepresentation_OtherByte, "MyPrivateTag2", 1, 1, "ORTHANC");
  FromDcmtkBridge::RegisterDictionaryTag(DicomTag(0x7059, 0x1000), ValueRepresentation_OtherByte, "MyPrivateTag3", 1, 1, "");
  FromDcmtkBridge::RegisterDictionaryTag(DicomTag(0x7050, 0x1000), ValueRepresentation_PersonName, "Declared public tag2", 1, 1, "");

  Json::Value v;
  const std::string sopClassUid = "1.2.840.10008.5.1.4.1.1.1";  // CR Image Storage:

  // Test the private creator
  ASSERT_EQ(DcmTag_ERROR_TagName, FromDcmtkBridge::GetTagName(DicomTag(0x7057, 0x1000), "NOPE"));
  ASSERT_EQ("MyPrivateTag2", FromDcmtkBridge::GetTagName(DicomTag(0x7057, 0x1000), "ORTHANC"));

  {
    v["SOPClassUID"] = sopClassUid;
    v["SpecificCharacterSet"] = "ISO_IR 148";    // This is latin-5
    v["PatientName"] = "Sébastien";
    v["7050-1000"] = "Some public tag";  // Even group => public tag
    v["7052-1000"] = "Some unknown tag";  // Even group => public, unknown tag
    v["7057-1000"] = "Some private tag";  // Odd group => private tag
    v["7059-1000"] = "Some private tag2";  // Odd group => private tag, with an odd length to test padding
  
    std::string s;
    Toolbox::EncodeDataUriScheme(s, "application/octet-stream", "Sebastien");
    v["StudyDescription"] = s;

    v["PixelData"] = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==";  // A red dot of 5x5 pixels
    v["0040,0100"] = Json::arrayValue;  // ScheduledProcedureStepSequence

    Json::Value vv;
    vv["Modality"] = "MR";
    v["0040,0100"].append(vv);

    vv["Modality"] = "CT";
    v["0040,0100"].append(vv);
  }

  const DicomToJsonFlags toJsonFlags = static_cast<DicomToJsonFlags>(DicomToJsonFlags_IncludeBinary |
                                                                     DicomToJsonFlags_IncludePixelData | 
                                                                     DicomToJsonFlags_IncludePrivateTags | 
                                                                     DicomToJsonFlags_IncludeUnknownTags | 
                                                                     DicomToJsonFlags_ConvertBinaryToAscii);


  {
    std::unique_ptr<ParsedDicomFile> dicom
      (ParsedDicomFile::CreateFromJson(v, static_cast<DicomFromJsonFlags>(DicomFromJsonFlags_GenerateIdentifiers), ""));

    Json::Value vv;
    dicom->DatasetToJson(vv, DicomToJsonFormat_Human, toJsonFlags, 0);

    ASSERT_EQ(vv["SOPClassUID"].asString(), sopClassUid);
    ASSERT_EQ(vv["MediaStorageSOPClassUID"].asString(), sopClassUid);
    ASSERT_TRUE(vv.isMember("SOPInstanceUID"));
    ASSERT_TRUE(vv.isMember("SeriesInstanceUID"));
    ASSERT_TRUE(vv.isMember("StudyInstanceUID"));
    ASSERT_TRUE(vv.isMember("PatientID"));
  }


  {
    std::unique_ptr<ParsedDicomFile> dicom
      (ParsedDicomFile::CreateFromJson(v, static_cast<DicomFromJsonFlags>(DicomFromJsonFlags_GenerateIdentifiers), ""));

    Json::Value vv;
    dicom->DatasetToJson(vv, DicomToJsonFormat_Human, static_cast<DicomToJsonFlags>(DicomToJsonFlags_IncludePixelData), 0);

    std::string mime, content;
    ASSERT_TRUE(Toolbox::DecodeDataUriScheme(mime, content, vv["PixelData"].asString()));
    ASSERT_EQ("application/octet-stream", mime);
    ASSERT_EQ(5u * 5u * 3u /* the red dot is 5x5 pixels in RGB24 */ + 1 /* for padding */, content.size());
  }


  {
    std::unique_ptr<ParsedDicomFile> dicom
      (ParsedDicomFile::CreateFromJson(v, static_cast<DicomFromJsonFlags>(DicomFromJsonFlags_DecodeDataUriScheme), ""));

    Json::Value vv;
    dicom->DatasetToJson(vv, DicomToJsonFormat_Short, toJsonFlags, 0);

    ASSERT_FALSE(vv.isMember("SOPInstanceUID"));
    ASSERT_FALSE(vv.isMember("SeriesInstanceUID"));
    ASSERT_FALSE(vv.isMember("StudyInstanceUID"));
    ASSERT_FALSE(vv.isMember("PatientID"));
    ASSERT_EQ(2u, vv["0040,0100"].size());
    ASSERT_EQ("MR", vv["0040,0100"][0]["0008,0060"].asString());
    ASSERT_EQ("CT", vv["0040,0100"][1]["0008,0060"].asString());
    ASSERT_EQ("Some public tag", vv["7050,1000"].asString());
    ASSERT_EQ("Some unknown tag", vv["7052,1000"].asString());
    ASSERT_EQ("Some private tag", vv["7057,1000"].asString());
    ASSERT_EQ("Some private tag2", vv["7059,1000"].asString());
    ASSERT_EQ("Sébastien", vv["0010,0010"].asString());
    ASSERT_EQ("Sebastien", vv["0008,1030"].asString());
    ASSERT_EQ("ISO_IR 148", vv["0008,0005"].asString());
    ASSERT_EQ("5", vv[DICOM_TAG_ROWS.Format()].asString());
    ASSERT_EQ("5", vv[DICOM_TAG_COLUMNS.Format()].asString());
    ASSERT_TRUE(vv[DICOM_TAG_PIXEL_DATA.Format()].asString().empty());
  }
}



TEST(TestImages, PatternGrayscale8)
{
  Orthanc::Image image(Orthanc::PixelFormat_Grayscale8, 256, 256, false);

  for (int y = 0; y < 256; y++)
  {
    uint8_t *p = reinterpret_cast<uint8_t*>(image.GetRow(y));
    for (int x = 0; x < 256; x++, p++)
    {
      *p = y;
    }
  }

  Orthanc::ImageAccessor r;

  image.GetRegion(r, 32, 32, 64, 192);
  Orthanc::ImageProcessing::Set(r, 0);
  
  image.GetRegion(r, 160, 32, 64, 192);
  Orthanc::ImageProcessing::Set(r, 255); 

  std::string saved;
  
  {
    ParsedDicomFile f(true);
    f.ReplacePlainString(DICOM_TAG_SOP_CLASS_UID, "1.2.840.10008.5.1.4.1.1.7");
    f.ReplacePlainString(DICOM_TAG_STUDY_INSTANCE_UID, "1.2.276.0.7230010.3.1.2.2831176407.321.1458901422.884998");
    f.ReplacePlainString(DICOM_TAG_PATIENT_ID, "ORTHANC");
    f.ReplacePlainString(DICOM_TAG_PATIENT_NAME, "Orthanc");
    f.ReplacePlainString(DICOM_TAG_STUDY_DESCRIPTION, "Patterns");
    f.ReplacePlainString(DICOM_TAG_SERIES_DESCRIPTION, "Grayscale8");
    f.EmbedImage(image);

    f.SaveToMemoryBuffer(saved);
  }

  {
    Orthanc::ParsedDicomFile f(saved);
    
    std::unique_ptr<Orthanc::ImageAccessor> decoded(f.DecodeFrame(0));
    ASSERT_EQ(256u, decoded->GetWidth());
    ASSERT_EQ(256u, decoded->GetHeight());
    ASSERT_EQ(Orthanc::PixelFormat_Grayscale8, decoded->GetFormat());

    for (int y = 0; y < 256; y++)
    {
      const void* a = image.GetConstRow(y);
      const void* b = decoded->GetConstRow(y);
      ASSERT_EQ(0, memcmp(a, b, 256));
    }
  }
}


TEST(TestImages, PatternRGB)
{
  Orthanc::Image image(Orthanc::PixelFormat_RGB24, 384, 256, false);

  for (int y = 0; y < 256; y++)
  {
    uint8_t *p = reinterpret_cast<uint8_t*>(image.GetRow(y));
    for (int x = 0; x < 128; x++, p += 3)
    {
      p[0] = y;
      p[1] = 0;
      p[2] = 0;
    }
    for (int x = 128; x < 128 * 2; x++, p += 3)
    {
      p[0] = 0;
      p[1] = 255 - y;
      p[2] = 0;
    }
    for (int x = 128 * 2; x < 128 * 3; x++, p += 3)
    {
      p[0] = 0;
      p[1] = 0;
      p[2] = y;
    }
  }

  std::string saved;

  {
    ParsedDicomFile f(true);
    f.ReplacePlainString(DICOM_TAG_SOP_CLASS_UID, "1.2.840.10008.5.1.4.1.1.7");
    f.ReplacePlainString(DICOM_TAG_STUDY_INSTANCE_UID, "1.2.276.0.7230010.3.1.2.2831176407.321.1458901422.884998");
    f.ReplacePlainString(DICOM_TAG_PATIENT_ID, "ORTHANC");
    f.ReplacePlainString(DICOM_TAG_PATIENT_NAME, "Orthanc");
    f.ReplacePlainString(DICOM_TAG_STUDY_DESCRIPTION, "Patterns");
    f.ReplacePlainString(DICOM_TAG_SERIES_DESCRIPTION, "RGB24");
    f.EmbedImage(image);

    f.SaveToMemoryBuffer(saved);
  }

  {
    Orthanc::ParsedDicomFile f(saved);
    
    std::unique_ptr<Orthanc::ImageAccessor> decoded(f.DecodeFrame(0));
    ASSERT_EQ(384u, decoded->GetWidth());
    ASSERT_EQ(256u, decoded->GetHeight());
    ASSERT_EQ(Orthanc::PixelFormat_RGB24, decoded->GetFormat());

    for (int y = 0; y < 256; y++)
    {
      const void* a = image.GetConstRow(y);
      const void* b = decoded->GetConstRow(y);
      ASSERT_EQ(0, memcmp(a, b, 3 * 384));
    }
  }
}


TEST(TestImages, PatternUint16)
{
  Orthanc::Image image(Orthanc::PixelFormat_Grayscale16, 256, 256, false);

  uint16_t v = 0;
  for (int y = 0; y < 256; y++)
  {
    uint16_t *p = reinterpret_cast<uint16_t*>(image.GetRow(y));
    for (int x = 0; x < 256; x++, v++, p++)
    {
      *p = v;
    }
  }

  Orthanc::ImageAccessor r;
  
  image.GetRegion(r, 32, 32, 64, 192);
  Orthanc::ImageProcessing::Set(r, 0);
  
  image.GetRegion(r, 160, 32, 64, 192);
  Orthanc::ImageProcessing::Set(r, 65535); 

  std::string saved;
  
  {
    ParsedDicomFile f(true);
    f.ReplacePlainString(DICOM_TAG_SOP_CLASS_UID, "1.2.840.10008.5.1.4.1.1.7");
    f.ReplacePlainString(DICOM_TAG_STUDY_INSTANCE_UID, "1.2.276.0.7230010.3.1.2.2831176407.321.1458901422.884998");
    f.ReplacePlainString(DICOM_TAG_PATIENT_ID, "ORTHANC");
    f.ReplacePlainString(DICOM_TAG_PATIENT_NAME, "Orthanc");
    f.ReplacePlainString(DICOM_TAG_STUDY_DESCRIPTION, "Patterns");
    f.ReplacePlainString(DICOM_TAG_SERIES_DESCRIPTION, "Grayscale16");
    f.EmbedImage(image);

    f.SaveToMemoryBuffer(saved);
  }

  {
    Orthanc::ParsedDicomFile f(saved);
    
    std::unique_ptr<Orthanc::ImageAccessor> decoded(f.DecodeFrame(0));
    ASSERT_EQ(256u, decoded->GetWidth());
    ASSERT_EQ(256u, decoded->GetHeight());
    ASSERT_EQ(Orthanc::PixelFormat_Grayscale16, decoded->GetFormat());

    for (int y = 0; y < 256; y++)
    {
      const void* a = image.GetConstRow(y);
      const void* b = decoded->GetConstRow(y);
      ASSERT_EQ(0, memcmp(a, b, 512));
    }
  }
}


TEST(TestImages, PatternInt16)
{
  Orthanc::Image image(Orthanc::PixelFormat_SignedGrayscale16, 256, 256, false);

  int16_t v = -32768;
  for (int y = 0; y < 256; y++)
  {
    int16_t *p = reinterpret_cast<int16_t*>(image.GetRow(y));
    for (int x = 0; x < 256; x++, v++, p++)
    {
      *p = v;
    }
  }

  Orthanc::ImageAccessor r;
  image.GetRegion(r, 32, 32, 64, 192);
  Orthanc::ImageProcessing::Set(r, -32768);
  
  image.GetRegion(r, 160, 32, 64, 192);
  Orthanc::ImageProcessing::Set(r, 32767); 

  std::string saved;
  
  {
    ParsedDicomFile f(true);
    f.ReplacePlainString(DICOM_TAG_SOP_CLASS_UID, "1.2.840.10008.5.1.4.1.1.7");
    f.ReplacePlainString(DICOM_TAG_STUDY_INSTANCE_UID, "1.2.276.0.7230010.3.1.2.2831176407.321.1458901422.884998");
    f.ReplacePlainString(DICOM_TAG_PATIENT_ID, "ORTHANC");
    f.ReplacePlainString(DICOM_TAG_PATIENT_NAME, "Orthanc");
    f.ReplacePlainString(DICOM_TAG_STUDY_DESCRIPTION, "Patterns");
    f.ReplacePlainString(DICOM_TAG_SERIES_DESCRIPTION, "SignedGrayscale16");
    f.EmbedImage(image);

    f.SaveToMemoryBuffer(saved);
  }

  {
    Orthanc::ParsedDicomFile f(saved);
    
    std::unique_ptr<Orthanc::ImageAccessor> decoded(f.DecodeFrame(0));
    ASSERT_EQ(256u, decoded->GetWidth());
    ASSERT_EQ(256u, decoded->GetHeight());
    ASSERT_EQ(Orthanc::PixelFormat_SignedGrayscale16, decoded->GetFormat());

    for (int y = 0; y < 256; y++)
    {
      const void* a = image.GetConstRow(y);
      const void* b = decoded->GetConstRow(y);
      ASSERT_EQ(0, memcmp(a, b, 512));
    }
  }
}



static void CheckEncoding(ParsedDicomFile& dicom,
                          Encoding expected)
{
  const char* value = NULL;
  ASSERT_TRUE(dicom.GetDcmtkObject().getDataset()->findAndGetString(DCM_SpecificCharacterSet, value).good());

  Encoding encoding;
  ASSERT_TRUE(GetDicomEncoding(encoding, value));
  ASSERT_EQ(expected, encoding);
}


TEST(ParsedDicomFile, DicomMapEncodings1)
{
  SetDefaultDicomEncoding(Encoding_Ascii);
  ASSERT_EQ(Encoding_Ascii, GetDefaultDicomEncoding());

  {
    DicomMap m;
    ParsedDicomFile dicom(m, GetDefaultDicomEncoding(), false);
    ASSERT_EQ(1u, dicom.GetDcmtkObject().getDataset()->card());
    CheckEncoding(dicom, Encoding_Ascii);
  }

  {
    DicomMap m;
    ParsedDicomFile dicom(m, Encoding_Latin4, false);
    ASSERT_EQ(1u, dicom.GetDcmtkObject().getDataset()->card());
    CheckEncoding(dicom, Encoding_Latin4);
  }

  {
    DicomMap m;
    m.SetValue(DICOM_TAG_SPECIFIC_CHARACTER_SET, "ISO_IR 148", false);
    ParsedDicomFile dicom(m, GetDefaultDicomEncoding(), false);
    ASSERT_EQ(1u, dicom.GetDcmtkObject().getDataset()->card());
    CheckEncoding(dicom, Encoding_Latin5);
  }

  {
    DicomMap m;
    m.SetValue(DICOM_TAG_SPECIFIC_CHARACTER_SET, "ISO_IR 148", false);
    ParsedDicomFile dicom(m, Encoding_Latin1, false);
    ASSERT_EQ(1u, dicom.GetDcmtkObject().getDataset()->card());
    CheckEncoding(dicom, Encoding_Latin5);
  }
}


TEST(ParsedDicomFile, DicomMapEncodings2)
{
  const char* utf8 = NULL;
  for (unsigned int i = 0; i < testEncodingsCount; i++)
  {
    if (testEncodings[i] == Encoding_Utf8)
    {
      utf8 = testEncodingsEncoded[i];
      break;
    }
  }  

  ASSERT_TRUE(utf8 != NULL);

  for (unsigned int i = 0; i < testEncodingsCount; i++)
  {
    // 1251 codepage is not supported by the core DICOM standard, ignore it
    if (testEncodings[i] != Encoding_Windows1251) 
    {
      {
        // Sanity check to test the proper behavior of "EncodingTests.py"
        std::string encoded = Toolbox::ConvertFromUtf8(testEncodingsExpected[i], testEncodings[i]);
        ASSERT_STREQ(testEncodingsEncoded[i], encoded.c_str());
        std::string decoded = Toolbox::ConvertToUtf8(encoded, testEncodings[i], false);
        ASSERT_STREQ(testEncodingsExpected[i], decoded.c_str());

        if (testEncodings[i] != Encoding_Chinese)
        {
          // A specific source string is used in "EncodingTests.py" to
          // test against Chinese, it is normal that it does not correspond to UTF8

          const std::string tmp = Toolbox::ConvertToUtf8(
            Toolbox::ConvertFromUtf8(utf8, testEncodings[i]), testEncodings[i], false);
          ASSERT_STREQ(testEncodingsExpected[i], tmp.c_str());
        }
      }


      Json::Value v;

      {
        DicomMap m;
        m.SetValue(DICOM_TAG_PATIENT_NAME, testEncodingsExpected[i], false);

        ParsedDicomFile dicom(m, testEncodings[i], false);
    
        const char* encoded = NULL;
        ASSERT_TRUE(dicom.GetDcmtkObject().getDataset()->findAndGetString(DCM_PatientName, encoded).good());
        ASSERT_STREQ(testEncodingsEncoded[i], encoded);

        dicom.DatasetToJson(v, DicomToJsonFormat_Human, DicomToJsonFlags_Default, 0);

        Encoding encoding;
        ASSERT_TRUE(GetDicomEncoding(encoding, v["SpecificCharacterSet"].asCString()));
        ASSERT_EQ(encoding, testEncodings[i]);
        ASSERT_STREQ(testEncodingsExpected[i], v["PatientName"].asCString());
      }


      {
        DicomMap m;
        m.SetValue(DICOM_TAG_SPECIFIC_CHARACTER_SET, GetDicomSpecificCharacterSet(testEncodings[i]), false);
        m.SetValue(DICOM_TAG_PATIENT_NAME, testEncodingsExpected[i], false);

        ParsedDicomFile dicom(m, testEncodings[i], false);

        Json::Value v2;
        dicom.DatasetToJson(v2, DicomToJsonFormat_Human, DicomToJsonFlags_Default, 0);
        
        ASSERT_EQ(v2["PatientName"].asString(), v["PatientName"].asString());
        ASSERT_EQ(v2["SpecificCharacterSet"].asString(), v["SpecificCharacterSet"].asString());
      }
    }
  }
}


TEST(ParsedDicomFile, ChangeEncoding)
{
  for (unsigned int i = 0; i < testEncodingsCount; i++)
  {
    // 1251 codepage is not supported by the core DICOM standard, ignore it
    if (testEncodings[i] != Encoding_Windows1251) 
    {
      DicomMap m;
      m.SetValue(DICOM_TAG_PATIENT_NAME, testEncodingsExpected[i], false);

      std::string tag;

      ParsedDicomFile dicom(m, Encoding_Utf8, false);
      bool hasCodeExtensions;
      ASSERT_EQ(Encoding_Utf8, dicom.DetectEncoding(hasCodeExtensions));
      ASSERT_FALSE(hasCodeExtensions);
      ASSERT_TRUE(dicom.GetTagValue(tag, DICOM_TAG_PATIENT_NAME));
      ASSERT_EQ(tag, testEncodingsExpected[i]);

      {
        Json::Value v;
        dicom.DatasetToJson(v, DicomToJsonFormat_Human, DicomToJsonFlags_Default, 0);
        ASSERT_STREQ(v["SpecificCharacterSet"].asCString(), "ISO_IR 192");
        ASSERT_STREQ(v["PatientName"].asCString(), testEncodingsExpected[i]);
      }

      dicom.ChangeEncoding(testEncodings[i]);

      ASSERT_EQ(testEncodings[i], dicom.DetectEncoding(hasCodeExtensions));
      ASSERT_FALSE(hasCodeExtensions);
      
      const char* c = NULL;
      ASSERT_TRUE(dicom.GetDcmtkObject().getDataset()->findAndGetString(DCM_PatientName, c).good());
      EXPECT_STREQ(c, testEncodingsEncoded[i]);
      
      ASSERT_TRUE(dicom.GetTagValue(tag, DICOM_TAG_PATIENT_NAME));  // Decodes to UTF-8
      EXPECT_EQ(tag, testEncodingsExpected[i]);

      {
        Json::Value v;
        dicom.DatasetToJson(v, DicomToJsonFormat_Human, DicomToJsonFlags_Default, 0);
        ASSERT_STREQ(v["SpecificCharacterSet"].asCString(), GetDicomSpecificCharacterSet(testEncodings[i]));
        ASSERT_STREQ(v["PatientName"].asCString(), testEncodingsExpected[i]);
      }
    }
  }
}


TEST(Toolbox, CaseWithAccents)
{
  ASSERT_EQ(toUpperResult, Toolbox::ToUpperCaseWithAccents(toUpperSource));
}



TEST(ParsedDicomFile, InvalidCharacterSets)
{
  {
    // No encoding provided, fallback to default encoding
    DicomMap m;
    m.SetValue(DICOM_TAG_PATIENT_NAME, "HELLO", false);

    ParsedDicomFile d(m, Encoding_Latin3 /* default encoding */, false);

    bool hasCodeExtensions;
    ASSERT_EQ(Encoding_Latin3, d.DetectEncoding(hasCodeExtensions));
    ASSERT_FALSE(hasCodeExtensions);
  }
  
  {
    // Valid encoding, "ISO_IR 13" is Japanese
    DicomMap m;
    m.SetValue(DICOM_TAG_SPECIFIC_CHARACTER_SET, "ISO_IR 13", false);
    m.SetValue(DICOM_TAG_PATIENT_NAME, "HELLO", false);

    ParsedDicomFile d(m, Encoding_Latin3 /* default encoding */, false);

    bool hasCodeExtensions;
    ASSERT_EQ(Encoding_Japanese, d.DetectEncoding(hasCodeExtensions));
    ASSERT_FALSE(hasCodeExtensions);
  }
  
  {
    // Invalid value for an encoding ("nope" is not in the DICOM standard)
    DicomMap m;
    m.SetValue(DICOM_TAG_SPECIFIC_CHARACTER_SET, "nope", false);
    m.SetValue(DICOM_TAG_PATIENT_NAME, "HELLO", false);

    ASSERT_THROW(ParsedDicomFile d(m, Encoding_Latin3, false),
                 OrthancException);
  }
  
  {
    // Invalid encoding, as provided as a binary string
    DicomMap m;
    m.SetValue(DICOM_TAG_SPECIFIC_CHARACTER_SET, "ISO_IR 13", true);
    m.SetValue(DICOM_TAG_PATIENT_NAME, "HELLO", false);

    ASSERT_THROW(ParsedDicomFile d(m, Encoding_Latin3, false),
                 OrthancException);
  }
  
  {
    // Encoding provided as an empty string, fallback to default encoding
    // In Orthanc <= 1.3.1, this test was throwing an exception
    DicomMap m;
    m.SetValue(DICOM_TAG_SPECIFIC_CHARACTER_SET, "", false);
    m.SetValue(DICOM_TAG_PATIENT_NAME, "HELLO", false);

    ParsedDicomFile d(m, Encoding_Latin3 /* default encoding */, false);

    bool hasCodeExtensions;
    ASSERT_EQ(Encoding_Latin3, d.DetectEncoding(hasCodeExtensions));
    ASSERT_FALSE(hasCodeExtensions);
  }
}



TEST(ParsedDicomFile, FloatPrecision)
{
  Float32 v;

  switch (Toolbox::DetectEndianness())
  {
    case Endianness_Little:
      reinterpret_cast<uint8_t*>(&v)[3] = 0x4E;
      reinterpret_cast<uint8_t*>(&v)[2] = 0x9C;
      reinterpret_cast<uint8_t*>(&v)[1] = 0xAD;
      reinterpret_cast<uint8_t*>(&v)[0] = 0x8F;
      break;

    case Endianness_Big:
      reinterpret_cast<uint8_t*>(&v)[0] = 0x4E;
      reinterpret_cast<uint8_t*>(&v)[1] = 0x9C;
      reinterpret_cast<uint8_t*>(&v)[2] = 0xAD;
      reinterpret_cast<uint8_t*>(&v)[3] = 0x8F;
      break;

    default:
      throw OrthancException(ErrorCode_InternalError);
  }

  ParsedDicomFile f(false);
  ASSERT_TRUE(f.GetDcmtkObject().getDataset()->putAndInsertFloat32(DCM_ExaminedBodyThickness /* VR: FL */, v).good());

  {
    Float32 u;
    ASSERT_TRUE(f.GetDcmtkObject().getDataset()->findAndGetFloat32(DCM_ExaminedBodyThickness, u).good());
    ASSERT_FLOAT_EQ(u, v);
    ASSERT_TRUE(memcmp(&u, &v, 4) == 0);
  }

  {
    Json::Value json;
    f.DatasetToJson(json, DicomToJsonFormat_Short, DicomToJsonFlags_None, 256);
    ASSERT_EQ("1314310016", json["0010,9431"].asString());
  }

  {
    DicomMap summary;
    f.ExtractDicomSummary(summary, 256);
    ASSERT_EQ("1314310016", summary.GetStringValue(DicomTag(0x0010, 0x9431), "nope", false));
  }

  {
    // This flavor uses "Json::Value" serialization
    DicomWebJsonVisitor visitor;
    f.Apply(visitor);
    Float32 u = visitor.GetResult() ["00109431"]["Value"][0].asFloat();
    ASSERT_FLOAT_EQ(u, v);
    ASSERT_TRUE(memcmp(&u, &v, 4) == 0);
  }
}



TEST(Toolbox, RemoveIso2022EscapeSequences)
{
  // +----------------------------------+
  // | one-byte control messages        |
  // +----------------------------------+

  static const uint8_t iso2022_cstr_oneByteControl[] = {
    0x0f, 0x41, 
    0x0e, 0x42, 
    0x8e, 0x1b, 0x4e, 0x43, 
    0x8f, 0x1b, 0x4f, 0x44,
    0x8e, 0x1b, 0x4a, 0x45, 
    0x8f, 0x1b, 0x4a, 0x46,
    0x50, 0x51, 0x52, 0x00
  };
  
  static const uint8_t iso2022_cstr_oneByteControl_ref[] = {
    0x41,
    0x42,
    0x43,
    0x44,
    0x8e, 0x1b, 0x4a, 0x45, 
    0x8f, 0x1b, 0x4a, 0x46,
    0x50, 0x51, 0x52, 0x00
  };

  // +----------------------------------+
  // | two-byte control messages        |
  // +----------------------------------+

  static const uint8_t iso2022_cstr_twoByteControl[] = {
    0x1b, 0x6e, 0x41,
    0x1b, 0x6f, 0x42,
    0x1b, 0x4e, 0x43,
    0x1b, 0x4f, 0x44,
    0x1b, 0x7e, 0x45,
    0x1b, 0x7d, 0x46,
    0x1b, 0x7c, 0x47, 0x00
  };
  
  static const uint8_t iso2022_cstr_twoByteControl_ref[] = {
    0x41,
    0x42,
    0x43,
    0x44,
    0x45,
    0x46,
    0x47, 0x00
  };

  // +----------------------------------+
  // | various-length escape sequences  |
  // +----------------------------------+

  static const uint8_t iso2022_cstr_escapeSequence[] = {
    0x1b, 0x40, 0x41, // 1b and 40 should not be removed (invalid esc seq)
    0x1b, 0x50, 0x42, // ditto 
    0x1b, 0x7f, 0x43, // ditto
    0x1b, 0x21, 0x4a, 0x44, // this will match
    0x1b, 0x20, 0x21, 0x2f, 0x40, 0x45, // this will match
    0x1b, 0x20, 0x21, 0x2f, 0x2f, 0x40, 0x46, // this will match too
    0x1b, 0x20, 0x21, 0x2f, 0x1f, 0x47, 0x48, 0x00 // this will NOT match!
  };
  
  static const uint8_t iso2022_cstr_escapeSequence_ref[] = {
    0x1b, 0x40, 0x41, // 1b and 40 should not be removed (invalid esc seq)
    0x1b, 0x50, 0x42, // ditto 
    0x1b, 0x7f, 0x43, // ditto
    0x44, // this will match
    0x45, // this will match
    0x46, // this will match too
    0x1b, 0x20, 0x21, 0x2f, 0x1f, 0x47, 0x48, 0x00 // this will NOT match!
  };

  
  // +----------------------------------+
  // | a real-world japanese sample     |
  // +----------------------------------+

  static const uint8_t iso2022_cstr_real_ir13[] = {
    0xd4, 0xcf, 0xc0, 0xde, 0x5e, 0xc0, 0xdb, 0xb3,
    0x3d, 0x1b, 0x24, 0x42, 0x3b, 0x33, 0x45, 0x44,
    0x1b, 0x28, 0x4a, 0x5e, 0x1b, 0x24, 0x42, 0x42,
    0x40, 0x4f, 0x3a, 0x1b, 0x28, 0x4a, 0x3d, 0x1b,
    0x24, 0x42, 0x24, 0x64, 0x24, 0x5e, 0x24, 0x40,
    0x1b, 0x28, 0x4a, 0x5e, 0x1b, 0x24, 0x42, 0x24,
    0x3f, 0x24, 0x6d, 0x24, 0x26, 0x1b, 0x28, 0x4a, 0x00
  };

  static const uint8_t iso2022_cstr_real_ir13_ref[] = {
    0xd4, 0xcf, 0xc0, 0xde, 0x5e, 0xc0, 0xdb, 0xb3,
    0x3d,
    0x3b, 0x33, 0x45, 0x44,
    0x5e,
    0x42,
    0x40, 0x4f, 0x3a,
    0x3d,
    0x24, 0x64, 0x24, 0x5e, 0x24, 0x40,
    0x5e,
    0x24,
    0x3f, 0x24, 0x6d, 0x24, 0x26, 0x00
  };



  // +----------------------------------+
  // | the actual test                  |
  // +----------------------------------+

  std::string iso2022_str_oneByteControl(
    reinterpret_cast<const char*>(iso2022_cstr_oneByteControl));
  std::string iso2022_str_oneByteControl_ref(
    reinterpret_cast<const char*>(iso2022_cstr_oneByteControl_ref));
  std::string iso2022_str_twoByteControl(
    reinterpret_cast<const char*>(iso2022_cstr_twoByteControl));
  std::string iso2022_str_twoByteControl_ref(
    reinterpret_cast<const char*>(iso2022_cstr_twoByteControl_ref));
  std::string iso2022_str_escapeSequence(
    reinterpret_cast<const char*>(iso2022_cstr_escapeSequence));
  std::string iso2022_str_escapeSequence_ref(
    reinterpret_cast<const char*>(iso2022_cstr_escapeSequence_ref));
  std::string iso2022_str_real_ir13(
    reinterpret_cast<const char*>(iso2022_cstr_real_ir13));
  std::string iso2022_str_real_ir13_ref(
    reinterpret_cast<const char*>(iso2022_cstr_real_ir13_ref));

  std::string dest;

  Toolbox::RemoveIso2022EscapeSequences(dest, iso2022_str_oneByteControl);
  ASSERT_EQ(dest, iso2022_str_oneByteControl_ref);

  Toolbox::RemoveIso2022EscapeSequences(dest, iso2022_str_twoByteControl);
  ASSERT_EQ(dest, iso2022_str_twoByteControl_ref);

  Toolbox::RemoveIso2022EscapeSequences(dest, iso2022_str_escapeSequence);
  ASSERT_EQ(dest, iso2022_str_escapeSequence_ref);

  Toolbox::RemoveIso2022EscapeSequences(dest, iso2022_str_real_ir13);
  ASSERT_EQ(dest, iso2022_str_real_ir13_ref);
}



static std::string DecodeFromSpecification(const std::string& s)
{
  std::vector<std::string> tokens;
  Toolbox::TokenizeString(tokens, s, ' ');

  std::string result;
  result.resize(tokens.size());
  
  for (size_t i = 0; i < tokens.size(); i++)
  {
    std::vector<std::string> components;
    Toolbox::TokenizeString(components, tokens[i], '/');

    if (components.size() != 2)
    {
      throw;
    }

    int a = boost::lexical_cast<int>(components[0]);
    int b = boost::lexical_cast<int>(components[1]);
    if (a < 0 || a > 15 ||
        b < 0 || b > 15 ||
        (a == 0 && b == 0))
    {
      throw;
    }

    result[i] = static_cast<uint8_t>(a * 16 + b);
  }

  return result;
}



// Compatibility wrapper
static pugi::xpath_node SelectNode(const pugi::xml_document& doc,
                                   const char* xpath)
{
#if PUGIXML_VERSION <= 140
  return doc.select_single_node(xpath);  // Deprecated in pugixml 1.5
#else
  return doc.select_node(xpath);
#endif
}


TEST(Toolbox, EncodingsKorean)
{
  // http://dicom.nema.org/MEDICAL/dicom/current/output/chtml/part05/sect_I.2.html

  std::string korean = DecodeFromSpecification(
    "04/08 06/15 06/14 06/07 05/14 04/07 06/09 06/12 06/04 06/15 06/14 06/07 03/13 "
    "01/11 02/04 02/09 04/03 15/11 15/03 05/14 01/11 02/04 02/09 04/03 13/01 12/14 "
    "13/04 13/07 03/13 01/11 02/04 02/09 04/03 12/08 10/11 05/14 01/11 02/04 02/09 "
    "04/03 11/01 14/06 11/05 11/15");

  // This array can be re-generated using command-line:
  // echo -n "Hong^Gildong=..." | hexdump -v -e '14/1 "0x%02x, "' -e '"\n"'
  static const uint8_t utf8raw[] = {
    0x48, 0x6f, 0x6e, 0x67, 0x5e, 0x47, 0x69, 0x6c, 0x64, 0x6f, 0x6e, 0x67, 0x3d, 0xe6,
    0xb4, 0xaa, 0x5e, 0xe5, 0x90, 0x89, 0xe6, 0xb4, 0x9e, 0x3d, 0xed, 0x99, 0x8d, 0x5e,
    0xea, 0xb8, 0xb8, 0xeb, 0x8f, 0x99
  };

  std::string utf8(reinterpret_cast<const char*>(utf8raw), sizeof(utf8raw));

  ParsedDicomFile dicom(false);
  dicom.ReplacePlainString(DICOM_TAG_SPECIFIC_CHARACTER_SET, "\\ISO 2022 IR 149");
  ASSERT_TRUE(dicom.GetDcmtkObject().getDataset()->putAndInsertString
              (DCM_PatientName, korean.c_str(), OFBool(true)).good());

  bool hasCodeExtensions;
  Encoding encoding = dicom.DetectEncoding(hasCodeExtensions);
  ASSERT_EQ(Encoding_Korean, encoding);
  ASSERT_TRUE(hasCodeExtensions);
  
  std::string value;
  ASSERT_TRUE(dicom.GetTagValue(value, DICOM_TAG_PATIENT_NAME));
  ASSERT_EQ(utf8, value);
  
  DicomWebJsonVisitor visitor;
  dicom.Apply(visitor);
  ASSERT_EQ(utf8.substr(0, 12), visitor.GetResult()["00100010"]["Value"][0]["Alphabetic"].asString());
  ASSERT_EQ(utf8.substr(13, 10), visitor.GetResult()["00100010"]["Value"][0]["Ideographic"].asString());
  ASSERT_EQ(utf8.substr(24), visitor.GetResult()["00100010"]["Value"][0]["Phonetic"].asString());

#if ORTHANC_ENABLE_PUGIXML == 1
  // http://dicom.nema.org/medical/dicom/current/output/chtml/part18/sect_F.3.html#table_F.3.1-1
  std::string xml;
  visitor.FormatXml(xml);

  pugi::xml_document doc;
  doc.load_buffer(xml.c_str(), xml.size());

  pugi::xpath_node node = SelectNode(doc, "//NativeDicomModel/DicomAttribute[@tag=\"00080005\"]/Value");
  ASSERT_STREQ("ISO 2022 IR 149", node.node().text().as_string());

  node = SelectNode(doc, "//NativeDicomModel/DicomAttribute[@tag=\"00080005\"]");
  ASSERT_STREQ("CS", node.node().attribute("vr").value());

  node = SelectNode(doc, "//NativeDicomModel/DicomAttribute[@tag=\"00100010\"]");
  ASSERT_STREQ("PN", node.node().attribute("vr").value());

  node = SelectNode(doc, "//NativeDicomModel/DicomAttribute[@tag=\"00100010\"]/PersonName/Alphabetic/FamilyName");
  ASSERT_STREQ("Hong", node.node().text().as_string());

  node = SelectNode(doc, "//NativeDicomModel/DicomAttribute[@tag=\"00100010\"]/PersonName/Alphabetic/GivenName");
  ASSERT_STREQ("Gildong", node.node().text().as_string());

  node = SelectNode(doc, "//NativeDicomModel/DicomAttribute[@tag=\"00100010\"]/PersonName/Ideographic/FamilyName");
  ASSERT_EQ(utf8.substr(13, 3), node.node().text().as_string());

  node = SelectNode(doc, "//NativeDicomModel/DicomAttribute[@tag=\"00100010\"]/PersonName/Ideographic/GivenName");
  ASSERT_EQ(utf8.substr(17, 6), node.node().text().as_string());

  node = SelectNode(doc, "//NativeDicomModel/DicomAttribute[@tag=\"00100010\"]/PersonName/Phonetic/FamilyName");
  ASSERT_EQ(utf8.substr(24, 3), node.node().text().as_string());

  node = SelectNode(doc, "//NativeDicomModel/DicomAttribute[@tag=\"00100010\"]/PersonName/Phonetic/GivenName");
  ASSERT_EQ(utf8.substr(28), node.node().text().as_string());
#endif
  
  {
    DicomMap m;
    m.FromDicomWeb(visitor.GetResult());
    ASSERT_EQ(2u, m.GetSize());

    std::string s;
    ASSERT_TRUE(m.LookupStringValue(s, DICOM_TAG_SPECIFIC_CHARACTER_SET, false));
    ASSERT_EQ("ISO 2022 IR 149", s);

    ASSERT_TRUE(m.LookupStringValue(s, DICOM_TAG_PATIENT_NAME, false));
    std::vector<std::string> v;
    Toolbox::TokenizeString(v, s, '=');
    ASSERT_EQ(3u, v.size());
    ASSERT_EQ("Hong^Gildong", v[0]);
    ASSERT_EQ(utf8, s);
  }
}


TEST(Toolbox, EncodingsJapaneseKanji)
{
  // http://dicom.nema.org/MEDICAL/dicom/current/output/chtml/part05/sect_H.3.html

  std::string japanese = DecodeFromSpecification(
    "05/09 06/01 06/13 06/01 06/04 06/01 05/14 05/04 06/01 07/02 06/15 07/05 03/13 "
    "01/11 02/04 04/02 03/11 03/03 04/05 04/04 01/11 02/08 04/02 05/14 01/11 02/04 "
    "04/02 04/02 04/00 04/15 03/10 01/11 02/08 04/02 03/13 01/11 02/04 04/02 02/04 "
    "06/04 02/04 05/14 02/04 04/00 01/11 02/08 04/02 05/14 01/11 02/04 04/02 02/04 "
    "03/15 02/04 06/13 02/04 02/06 01/11 02/08 04/02");

  // This array can be re-generated using command-line:
  // echo -n "Yamada^Tarou=..." | hexdump -v -e '14/1 "0x%02x, "' -e '"\n"'
  static const uint8_t utf8raw[] = {
    0x59, 0x61, 0x6d, 0x61, 0x64, 0x61, 0x5e, 0x54, 0x61, 0x72, 0x6f, 0x75, 0x3d, 0xe5,
    0xb1, 0xb1, 0xe7, 0x94, 0xb0, 0x5e, 0xe5, 0xa4, 0xaa, 0xe9, 0x83, 0x8e, 0x3d, 0xe3,
    0x82, 0x84, 0xe3, 0x81, 0xbe, 0xe3, 0x81, 0xa0, 0x5e, 0xe3, 0x81, 0x9f, 0xe3, 0x82,
    0x8d, 0xe3, 0x81, 0x86
  };

  std::string utf8(reinterpret_cast<const char*>(utf8raw), sizeof(utf8raw));

  ParsedDicomFile dicom(false);
  dicom.ReplacePlainString(DICOM_TAG_SPECIFIC_CHARACTER_SET, "\\ISO 2022 IR 87");
  ASSERT_TRUE(dicom.GetDcmtkObject().getDataset()->putAndInsertString
              (DCM_PatientName, japanese.c_str(), OFBool(true)).good());

  bool hasCodeExtensions;
  Encoding encoding = dicom.DetectEncoding(hasCodeExtensions);
  ASSERT_EQ(Encoding_JapaneseKanji, encoding);
  ASSERT_TRUE(hasCodeExtensions);

  std::string value;
  ASSERT_TRUE(dicom.GetTagValue(value, DICOM_TAG_PATIENT_NAME));
  ASSERT_EQ(utf8, value);
  
  DicomWebJsonVisitor visitor;
  dicom.Apply(visitor);
  ASSERT_EQ(utf8.substr(0, 12), visitor.GetResult()["00100010"]["Value"][0]["Alphabetic"].asString());
  ASSERT_EQ(utf8.substr(13, 13), visitor.GetResult()["00100010"]["Value"][0]["Ideographic"].asString());
  ASSERT_EQ(utf8.substr(27), visitor.GetResult()["00100010"]["Value"][0]["Phonetic"].asString());

#if ORTHANC_ENABLE_PUGIXML == 1
  // http://dicom.nema.org/medical/dicom/current/output/chtml/part18/sect_F.3.html#table_F.3.1-1
  std::string xml;
  visitor.FormatXml(xml);

  pugi::xml_document doc;
  doc.load_buffer(xml.c_str(), xml.size());

  pugi::xpath_node node = SelectNode(doc, "//NativeDicomModel/DicomAttribute[@tag=\"00080005\"]/Value");
  ASSERT_STREQ("ISO 2022 IR 87", node.node().text().as_string());

  node = SelectNode(doc, "//NativeDicomModel/DicomAttribute[@tag=\"00080005\"]");
  ASSERT_STREQ("CS", node.node().attribute("vr").value());

  node = SelectNode(doc, "//NativeDicomModel/DicomAttribute[@tag=\"00100010\"]");
  ASSERT_STREQ("PN", node.node().attribute("vr").value());

  node = SelectNode(doc, "//NativeDicomModel/DicomAttribute[@tag=\"00100010\"]/PersonName/Alphabetic/FamilyName");
  ASSERT_STREQ("Yamada", node.node().text().as_string());

  node = SelectNode(doc, "//NativeDicomModel/DicomAttribute[@tag=\"00100010\"]/PersonName/Alphabetic/GivenName");
  ASSERT_STREQ("Tarou", node.node().text().as_string());

  node = SelectNode(doc, "//NativeDicomModel/DicomAttribute[@tag=\"00100010\"]/PersonName/Ideographic/FamilyName");
  ASSERT_EQ(utf8.substr(13, 6), node.node().text().as_string());

  node = SelectNode(doc, "//NativeDicomModel/DicomAttribute[@tag=\"00100010\"]/PersonName/Ideographic/GivenName");
  ASSERT_EQ(utf8.substr(20, 6), node.node().text().as_string());

  node = SelectNode(doc, "//NativeDicomModel/DicomAttribute[@tag=\"00100010\"]/PersonName/Phonetic/FamilyName");
  ASSERT_EQ(utf8.substr(27, 9), node.node().text().as_string());

  node = SelectNode(doc, "//NativeDicomModel/DicomAttribute[@tag=\"00100010\"]/PersonName/Phonetic/GivenName");
  ASSERT_EQ(utf8.substr(37), node.node().text().as_string());
#endif  
  
  {
    DicomMap m;
    m.FromDicomWeb(visitor.GetResult());
    ASSERT_EQ(2u, m.GetSize());

    std::string s;
    ASSERT_TRUE(m.LookupStringValue(s, DICOM_TAG_SPECIFIC_CHARACTER_SET, false));
    ASSERT_EQ("ISO 2022 IR 87", s);

    ASSERT_TRUE(m.LookupStringValue(s, DICOM_TAG_PATIENT_NAME, false));
    std::vector<std::string> v;
    Toolbox::TokenizeString(v, s, '=');
    ASSERT_EQ(3u, v.size());
    ASSERT_EQ("Yamada^Tarou", v[0]);
    ASSERT_EQ(utf8, s);
  }
}



TEST(Toolbox, EncodingsChinese3)
{
  // http://dicom.nema.org/MEDICAL/dicom/current/output/chtml/part05/sect_J.3.html

  static const uint8_t chinese[] = {
    0x57, 0x61, 0x6e, 0x67, 0x5e, 0x58, 0x69, 0x61, 0x6f, 0x44, 0x6f,
    0x6e, 0x67, 0x3d, 0xcd, 0xf5, 0x5e, 0xd0, 0xa1, 0xb6, 0xab, 0x3d, 0x00
  };

  ParsedDicomFile dicom(false);
  dicom.ReplacePlainString(DICOM_TAG_SPECIFIC_CHARACTER_SET, "GB18030");
  ASSERT_TRUE(dicom.GetDcmtkObject().getDataset()->putAndInsertString
              (DCM_PatientName, reinterpret_cast<const char*>(chinese), OFBool(true)).good());

  bool hasCodeExtensions;
  Encoding encoding = dicom.DetectEncoding(hasCodeExtensions);
  ASSERT_EQ(Encoding_Chinese, encoding);
  ASSERT_FALSE(hasCodeExtensions);

  std::string value;
  ASSERT_TRUE(dicom.GetTagValue(value, DICOM_TAG_PATIENT_NAME));

  std::vector<std::string> tokens;
  Orthanc::Toolbox::TokenizeString(tokens, value, '=');
  ASSERT_EQ(3u, tokens.size());
  ASSERT_EQ("Wang^XiaoDong", tokens[0]);
  ASSERT_TRUE(tokens[2].empty());

  std::vector<std::string> middle;
  Orthanc::Toolbox::TokenizeString(middle, tokens[1], '^');
  ASSERT_EQ(2u, middle.size());
  ASSERT_EQ(3u, middle[0].size());
  ASSERT_EQ(6u, middle[1].size());

  // CDF5 in GB18030
  ASSERT_EQ(static_cast<char>(0xe7), middle[0][0]);
  ASSERT_EQ(static_cast<char>(0x8e), middle[0][1]);
  ASSERT_EQ(static_cast<char>(0x8b), middle[0][2]);

  // D0A1 in GB18030
  ASSERT_EQ(static_cast<char>(0xe5), middle[1][0]);
  ASSERT_EQ(static_cast<char>(0xb0), middle[1][1]);
  ASSERT_EQ(static_cast<char>(0x8f), middle[1][2]);

  // B6AB in GB18030
  ASSERT_EQ(static_cast<char>(0xe4), middle[1][3]);
  ASSERT_EQ(static_cast<char>(0xb8), middle[1][4]);
  ASSERT_EQ(static_cast<char>(0x9c), middle[1][5]);
}


TEST(Toolbox, EncodingsChinese4)
{
  // http://dicom.nema.org/MEDICAL/dicom/current/output/chtml/part05/sect_J.4.html

  static const uint8_t chinese[] = {
    0x54, 0x68, 0x65, 0x20, 0x66, 0x69, 0x72, 0x73, 0x74, 0x20, 0x6c, 0x69, 0x6e,
    0x65, 0x20, 0x69, 0x6e, 0x63, 0x6c, 0x75, 0x64, 0x65, 0x73, 0xd6, 0xd0, 0xce,
    0xc4, 0x2e, 0x0d, 0x0a, 0x54, 0x68, 0x65, 0x20, 0x73, 0x65, 0x63, 0x6f, 0x6e,
    0x64, 0x20, 0x6c, 0x69, 0x6e, 0x65, 0x20, 0x69, 0x6e, 0x63, 0x6c, 0x75, 0x64,
    0x65, 0x73, 0xd6, 0xd0, 0xce, 0xc4, 0x2c, 0x20, 0x74, 0x6f, 0x6f, 0x2e, 0x0d,
    0x0a, 0x54, 0x68, 0x65, 0x20, 0x74, 0x68, 0x69, 0x72, 0x64, 0x20, 0x6c, 0x69,
    0x6e, 0x65, 0x2e, 0x0d, 0x0a, 0x00
  };

  static const uint8_t patternRaw[] = {
    0xe4, 0xb8, 0xad, 0xe6, 0x96, 0x87
  };

  const std::string pattern(reinterpret_cast<const char*>(patternRaw), sizeof(patternRaw));

  ParsedDicomFile dicom(false);
  dicom.ReplacePlainString(DICOM_TAG_SPECIFIC_CHARACTER_SET, "GB18030");
  ASSERT_TRUE(dicom.GetDcmtkObject().getDataset()->putAndInsertString
              (DCM_PatientComments, reinterpret_cast<const char*>(chinese), OFBool(true)).good());

  bool hasCodeExtensions;
  Encoding encoding = dicom.DetectEncoding(hasCodeExtensions);
  ASSERT_EQ(Encoding_Chinese, encoding);
  ASSERT_FALSE(hasCodeExtensions);

  std::string value;
  ASSERT_TRUE(dicom.GetTagValue(value, DICOM_TAG_PATIENT_COMMENTS));

  std::vector<std::string> lines;
  Orthanc::Toolbox::TokenizeString(lines, value, '\n');
  ASSERT_EQ(4u, lines.size());
  ASSERT_TRUE(boost::starts_with(lines[0], "The first line includes"));
  ASSERT_TRUE(boost::ends_with(lines[0], ".\r"));
  ASSERT_TRUE(lines[0].find(pattern) != std::string::npos);
  ASSERT_TRUE(boost::starts_with(lines[1], "The second line includes"));
  ASSERT_TRUE(boost::ends_with(lines[1], ", too.\r"));
  ASSERT_TRUE(lines[1].find(pattern) != std::string::npos);
  ASSERT_EQ("The third line.\r", lines[2]);
  ASSERT_FALSE(lines[1].find(pattern) == std::string::npos);
  ASSERT_TRUE(lines[3].empty());
}


TEST(Toolbox, EncodingsSimplifiedChinese2)
{
  // http://dicom.nema.org/MEDICAL/dicom/current/output/chtml/part05/sect_K.2.html

  static const uint8_t chinese[] = {
    0x5a, 0x68, 0x61, 0x6e, 0x67, 0x5e, 0x58, 0x69, 0x61, 0x6f, 0x44, 0x6f,
    0x6e, 0x67, 0x3d, 0x1b, 0x24, 0x29, 0x41, 0xd5, 0xc5, 0x5e, 0x1b, 0x24,
    0x29, 0x41, 0xd0, 0xa1, 0xb6, 0xab, 0x3d, 0x20, 0x00
  };

  // echo -n "Zhang^XiaoDong=..." | hexdump -v -e '14/1 "0x%02x, "' -e '"\n"'
  static const uint8_t utf8[] = {
    0x5a, 0x68, 0x61, 0x6e, 0x67, 0x5e, 0x58, 0x69, 0x61, 0x6f, 0x44, 0x6f, 0x6e, 0x67,
    0x3d, 0xe5, 0xbc, 0xa0, 0x5e, 0xe5, 0xb0, 0x8f, 0xe4, 0xb8, 0x9c, 0x3d
  };
  
  ParsedDicomFile dicom(false);
  dicom.ReplacePlainString(DICOM_TAG_SPECIFIC_CHARACTER_SET, "\\ISO 2022 IR 58");
  ASSERT_TRUE(dicom.GetDcmtkObject().getDataset()->putAndInsertString
              (DCM_PatientName, reinterpret_cast<const char*>(chinese), OFBool(true)).good());

  bool hasCodeExtensions;
  Encoding encoding = dicom.DetectEncoding(hasCodeExtensions);
  ASSERT_EQ(Encoding_SimplifiedChinese, encoding);
  ASSERT_TRUE(hasCodeExtensions);

  std::string value;
  ASSERT_TRUE(dicom.GetTagValue(value, DICOM_TAG_PATIENT_NAME));
  ASSERT_EQ(value, std::string(reinterpret_cast<const char*>(utf8), sizeof(utf8)));
}


TEST(Toolbox, EncodingsSimplifiedChinese3)
{
  // http://dicom.nema.org/MEDICAL/dicom/current/output/chtml/part05/sect_K.2.html

  static const uint8_t chinese[] = {
    0x31, 0x2e, 0x1b, 0x24, 0x29, 0x41, 0xb5, 0xda, 0xd2, 0xbb, 0xd0, 0xd0, 0xce, 0xc4, 0xd7, 0xd6, 0xa1, 0xa3, 0x0d, 0x0a,
    0x32, 0x2e, 0x1b, 0x24, 0x29, 0x41, 0xb5, 0xda, 0xb6, 0xfe, 0xd0, 0xd0, 0xce, 0xc4, 0xd7, 0xd6, 0xa1, 0xa3, 0x0d, 0x0a,
    0x33, 0x2e, 0x1b, 0x24, 0x29, 0x41, 0xb5, 0xda, 0xc8, 0xfd, 0xd0, 0xd0, 0xce, 0xc4, 0xd7, 0xd6, 0xa1, 0xa3, 0x0d, 0x0a, 0x00
  };

  static const uint8_t line1[] = {
    0x31, 0x2e, 0xe7, 0xac, 0xac, 0xe4, 0xb8, 0x80, 0xe8, 0xa1, 0x8c, 0xe6, 0x96, 0x87,
    0xe5, 0xad, 0x97, 0xe3, 0x80, 0x82, '\r'
  };

  static const uint8_t line2[] = {
    0x32, 0x2e, 0xe7, 0xac, 0xac, 0xe4, 0xba, 0x8c, 0xe8, 0xa1, 0x8c, 0xe6, 0x96, 0x87,
    0xe5, 0xad, 0x97, 0xe3, 0x80, 0x82, '\r'
  };

  static const uint8_t line3[] = {
    0x33, 0x2e, 0xe7, 0xac, 0xac, 0xe4, 0xb8, 0x89, 0xe8, 0xa1, 0x8c, 0xe6, 0x96, 0x87,
    0xe5, 0xad, 0x97, 0xe3, 0x80, 0x82, '\r'
  };

  ParsedDicomFile dicom(false);
  dicom.ReplacePlainString(DICOM_TAG_SPECIFIC_CHARACTER_SET, "\\ISO 2022 IR 58");
  ASSERT_TRUE(dicom.GetDcmtkObject().getDataset()->putAndInsertString
              (DCM_PatientName, reinterpret_cast<const char*>(chinese), OFBool(true)).good());

  bool hasCodeExtensions;
  Encoding encoding = dicom.DetectEncoding(hasCodeExtensions);
  ASSERT_EQ(Encoding_SimplifiedChinese, encoding);
  ASSERT_TRUE(hasCodeExtensions);

  std::string value;
  ASSERT_TRUE(dicom.GetTagValue(value, DICOM_TAG_PATIENT_NAME));

  std::vector<std::string> lines;
  Toolbox::TokenizeString(lines, value, '\n');
  ASSERT_EQ(4u, lines.size());
  ASSERT_EQ(std::string(reinterpret_cast<const char*>(line1), sizeof(line1)), lines[0]);
  ASSERT_EQ(std::string(reinterpret_cast<const char*>(line2), sizeof(line2)), lines[1]);
  ASSERT_EQ(std::string(reinterpret_cast<const char*>(line3), sizeof(line3)), lines[2]);
  ASSERT_TRUE(lines[3].empty());
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


TEST(ParsedDicomCache, Basic)
{
  ParsedDicomCache cache(10);
  ASSERT_EQ(0u, cache.GetCurrentSize());
  ASSERT_EQ(0u, cache.GetNumberOfItems());

  DicomMap tags;
  tags.SetValue(DICOM_TAG_PATIENT_ID, "patient1", false);
  cache.Acquire("a", new ParsedDicomFile(tags, Encoding_Latin1, true), 20);
  ASSERT_EQ(20u, cache.GetCurrentSize());
  ASSERT_EQ(1u, cache.GetNumberOfItems());

  {
    ParsedDicomCache::Accessor accessor(cache, "b");
    ASSERT_FALSE(accessor.IsValid());
    ASSERT_THROW(accessor.GetDicom(), OrthancException);
    ASSERT_THROW(accessor.GetFileSize(), OrthancException);
  }
  
  {
    ParsedDicomCache::Accessor accessor(cache, "a");
    ASSERT_TRUE(accessor.IsValid());
    std::string s;
    ASSERT_TRUE(accessor.GetDicom().GetTagValue(s, DICOM_TAG_PATIENT_ID));
    ASSERT_EQ("patient1", s);
    ASSERT_EQ(20u, accessor.GetFileSize());
  }
  
  tags.SetValue(DICOM_TAG_PATIENT_ID, "patient2", false);
  cache.Acquire("b", new ParsedDicomFile(tags, Encoding_Latin1, true), 5);  
  ASSERT_EQ(5u, cache.GetCurrentSize());
  ASSERT_EQ(1u, cache.GetNumberOfItems());

  cache.Acquire("c", new ParsedDicomFile(true), 5);
  ASSERT_EQ(10u, cache.GetCurrentSize());
  ASSERT_EQ(2u, cache.GetNumberOfItems());

  {
    ParsedDicomCache::Accessor accessor(cache, "b");
    ASSERT_TRUE(accessor.IsValid());
    std::string s;
    ASSERT_TRUE(accessor.GetDicom().GetTagValue(s, DICOM_TAG_PATIENT_ID));
    ASSERT_EQ("patient2", s);
    ASSERT_EQ(5u, accessor.GetFileSize());
  }
  
  cache.Acquire("d", new ParsedDicomFile(true), 5);
  ASSERT_EQ(10u, cache.GetCurrentSize());
  ASSERT_EQ(2u, cache.GetNumberOfItems());

  ASSERT_TRUE(ParsedDicomCache::Accessor(cache, "b").IsValid());
  ASSERT_FALSE(ParsedDicomCache::Accessor(cache, "c").IsValid());  // recycled by LRU
  ASSERT_TRUE(ParsedDicomCache::Accessor(cache, "d").IsValid());

  cache.Invalidate("d");
  ASSERT_EQ(5u, cache.GetCurrentSize());
  ASSERT_EQ(1u, cache.GetNumberOfItems());
  ASSERT_TRUE(ParsedDicomCache::Accessor(cache, "b").IsValid());
  ASSERT_FALSE(ParsedDicomCache::Accessor(cache, "d").IsValid());

  cache.Acquire("e", new ParsedDicomFile(true), 15);
  ASSERT_EQ(15u, cache.GetCurrentSize());
  ASSERT_EQ(1u, cache.GetNumberOfItems());

  ASSERT_FALSE(ParsedDicomCache::Accessor(cache, "c").IsValid());
  ASSERT_FALSE(ParsedDicomCache::Accessor(cache, "d").IsValid());
  ASSERT_TRUE(ParsedDicomCache::Accessor(cache, "e").IsValid());

  cache.Invalidate("e");
  ASSERT_EQ(0u, cache.GetCurrentSize());
  ASSERT_EQ(0u, cache.GetNumberOfItems());
  ASSERT_FALSE(ParsedDicomCache::Accessor(cache, "e").IsValid());
}




#if ORTHANC_ENABLE_DCMTK_TRANSCODING == 1

#include "../Sources/DicomNetworking/DicomStoreUserConnection.h"
#include "../Sources/DicomParsing/DcmtkTranscoder.h"

TEST(Toto, DISABLED_Transcode3)
{
  DicomAssociationParameters p;
  p.SetRemotePort(2000);

  DicomStoreUserConnection scu(p);
  scu.SetCommonClassesProposed(false);
  scu.SetRetiredBigEndianProposed(true);

  DcmtkTranscoder transcoder;

  for (int j = 0; j < 2; j++)
    for (int i = 0; i <= DicomTransferSyntax_XML; i++)
    {
      DicomTransferSyntax a = (DicomTransferSyntax) i;

      std::string path = ("/home/jodogne/Subversion/orthanc-tests/Database/TransferSyntaxes/" +
                          std::string(GetTransferSyntaxUid(a)) + ".dcm");
      if (Orthanc::SystemToolbox::IsRegularFile(path))
      {
        printf("\n======= %s\n", GetTransferSyntaxUid(a));

        std::string source;
        Orthanc::SystemToolbox::ReadFile(source, path);

        std::string c, k;
        try
        {
          scu.Transcode(c, k, transcoder, source.c_str(), source.size(),
                        DicomTransferSyntax_LittleEndianExplicit, false, "", 0);
        }
        catch (OrthancException& e)
        {
          if (e.GetErrorCode() == ErrorCode_NotImplemented)
          {
            LOG(ERROR) << "cannot transcode " << GetTransferSyntaxUid(a);
          }
          else
          {
            throw;
          }
        }
      }
    }
}


TEST(Toto, DISABLED_Transcode4)
{
  std::unique_ptr<DcmFileFormat> toto;

  {
    std::string source;
    Orthanc::SystemToolbox::ReadFile(source, "/home/jodogne/Subversion/orthanc-tests/Database/KarstenHilbertRF.dcm");
    toto.reset(FromDcmtkBridge::LoadFromMemoryBuffer(source.c_str(), source.size()));
  }
  
  const std::string sourceUid = IDicomTranscoder::GetSopInstanceUid(*toto);
  
  DicomTransferSyntax sourceSyntax;
  ASSERT_TRUE(FromDcmtkBridge::LookupOrthancTransferSyntax(sourceSyntax, *toto));

  DcmtkTranscoder transcoder;

  for (int i = 0; i <= DicomTransferSyntax_XML; i++)
  {
    DicomTransferSyntax a = (DicomTransferSyntax) i;
    
    std::set<DicomTransferSyntax> s;
    s.insert(a);

    std::string t;

    IDicomTranscoder::DicomImage source, target;
    source.AcquireParsed(dynamic_cast<DcmFileFormat*>(toto->clone()));

    if (!transcoder.Transcode(target, source, s, true))
    {
      printf("**************** CANNOT: [%s] => [%s]\n",
             GetTransferSyntaxUid(sourceSyntax), GetTransferSyntaxUid(a));
    }
    else
    {
      DicomTransferSyntax targetSyntax;
      ASSERT_TRUE(FromDcmtkBridge::LookupOrthancTransferSyntax(targetSyntax, target.GetParsed()));
      
      ASSERT_EQ(targetSyntax, a);
      bool lossy = (a == DicomTransferSyntax_JPEGProcess1 ||
                    a == DicomTransferSyntax_JPEGProcess2_4 ||
                    a == DicomTransferSyntax_JPEGLSLossy);
      
      printf("SIZE: %d\n", static_cast<int>(t.size()));
      if (sourceUid == IDicomTranscoder::GetSopInstanceUid(target.GetParsed()))
      {
        ASSERT_FALSE(lossy);
      }
      else
      {
        ASSERT_TRUE(lossy);
      }
    }
  }
}

#endif
