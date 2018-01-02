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

#include "../Core/DicomParsing/FromDcmtkBridge.h"
#include "../Core/DicomParsing/ToDcmtkBridge.h"
#include "../Core/DicomParsing/DicomModification.h"
#include "../OrthancServer/ServerToolbox.h"
#include "../Core/OrthancException.h"
#include "../Core/Images/ImageBuffer.h"
#include "../Core/Images/PngReader.h"
#include "../Core/Images/PngWriter.h"
#include "../Core/Images/Image.h"
#include "../Core/Images/ImageProcessing.h"
#include "../Core/Endianness.h"
#include "../Resources/EncodingTests.h"
#include "../Core/DicomNetworking/DicomFindAnswers.h"
#include "../Core/DicomParsing/Internals/DicomImageDecoder.h"
#include "../Plugins/Engine/PluginsEnumerations.h"

#include <dcmtk/dcmdata/dcelem.h>
#include <dcmtk/dcmdata/dcdeftag.h>

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
    std::auto_ptr<ParsedDicomFile> f(o.Clone());
    if (i > 4)
      o.ReplacePlainString(DICOM_TAG_SERIES_INSTANCE_UID, "coucou");
    m.Apply(*f);
    f->SaveToFile(b);
  }
}


TEST(DicomModification, Anonymization)
{
  ASSERT_EQ(DICOM_TAG_PATIENT_NAME, FromDcmtkBridge::ParseTag("PatientName"));

  const DicomTag privateTag(0x0045, 0x0010);
  const DicomTag privateTag2(FromDcmtkBridge::ParseTag("0031-1020"));
  ASSERT_TRUE(privateTag.IsPrivate());
  ASSERT_TRUE(privateTag2.IsPrivate());
  ASSERT_EQ(0x0031, privateTag2.GetGroup());
  ASSERT_EQ(0x1020, privateTag2.GetElement());

  std::string s;
  ParsedDicomFile o(true);
  o.ReplacePlainString(DICOM_TAG_PATIENT_NAME, "coucou");
  ASSERT_FALSE(o.GetTagValue(s, privateTag));
  o.Insert(privateTag, "private tag", false);
  ASSERT_TRUE(o.GetTagValue(s, privateTag));
  ASSERT_STREQ("private tag", s.c_str());

  ASSERT_FALSE(o.GetTagValue(s, privateTag2));
  ASSERT_THROW(o.Replace(privateTag2, std::string("hello"), false, DicomReplaceMode_ThrowIfAbsent), OrthancException);
  ASSERT_FALSE(o.GetTagValue(s, privateTag2));
  o.Replace(privateTag2, std::string("hello"), false, DicomReplaceMode_IgnoreIfAbsent);
  ASSERT_FALSE(o.GetTagValue(s, privateTag2));
  o.Replace(privateTag2, std::string("hello"), false, DicomReplaceMode_InsertIfAbsent);
  ASSERT_TRUE(o.GetTagValue(s, privateTag2));
  ASSERT_STREQ("hello", s.c_str());
  o.ReplacePlainString(privateTag2, "hello world");
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
  o.SaveToFile("UnitTestsResults/png1.dcm");

  // Red dot, without alpha channel
  s = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAIAAAACDbGyAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH3gUGDTcIn2+8BgAAACJJREFUCNdj/P//PwMjIwME/P/P+J8BBTAxEOL/R9Lx/z8AynoKAXOeiV8AAAAASUVORK5CYII=";
  o.EmbedContent(s);
  o.SaveToFile("UnitTestsResults/png2.dcm");

  // Check box in Graylevel8
  s = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAAAAAA6mKC9AAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH3gUGDDcB53FulQAAAElJREFUGNNtj0sSAEEEQ1+U+185s1CtmRkblQ9CZldsKHJDk6DLGLJa6chjh0ooQmpjXMM86zPwydGEj6Ed/UGykkEM8X+p3u8/8LcOJIWLGeMAAAAASUVORK5CYII=";
  o.EmbedContent(s);
  //o.ReplacePlainString(DICOM_TAG_SOP_CLASS_UID, UID_DigitalXRayImageStorageForProcessing);
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
  ASSERT_FALSE(GetDicomEncoding(e, "ISO 2022 IR 87"));   //ASSERT_EQ(Encoding_JapaneseKanji, e);
  ASSERT_FALSE(GetDicomEncoding(e, "ISO 2022 IR 159"));  //ASSERT_EQ(Encoding_JapaneseKanjiSupplementary, e);
  ASSERT_FALSE(GetDicomEncoding(e, "ISO 2022 IR 149"));  //ASSERT_EQ(Encoding_Korean, e);

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


TEST(FromDcmtkBridge, ValueRepresentationConversions)
{
  ASSERT_EQ(1, ValueRepresentation_ApplicationEntity);
  ASSERT_EQ(1, OrthancPluginValueRepresentation_AE);

  for (int i = ValueRepresentation_ApplicationEntity;
       i <= ValueRepresentation_NotSupported; i++)
  {
    ValueRepresentation vr = static_cast<ValueRepresentation>(i);

    if (vr == ValueRepresentation_NotSupported)
    {
      ASSERT_THROW(ToDcmtkBridge::Convert(vr), OrthancException);
      ASSERT_THROW(Plugins::Convert(vr), OrthancException);
    }
    else if (vr == ValueRepresentation_OtherDouble || 
             vr == ValueRepresentation_OtherLong ||
             vr == ValueRepresentation_UniversalResource ||
             vr == ValueRepresentation_UnlimitedCharacters)
    {
      // These VR are not supported as of DCMTK 3.6.0
      ASSERT_THROW(ToDcmtkBridge::Convert(vr), OrthancException);
      ASSERT_EQ(OrthancPluginValueRepresentation_UN, Plugins::Convert(vr));
    }
    else
    {
      ASSERT_EQ(vr, FromDcmtkBridge::Convert(ToDcmtkBridge::Convert(vr)));

      OrthancPluginValueRepresentation plugins = Plugins::Convert(vr);
      ASSERT_EQ(vr, Plugins::Convert(plugins));
    }
  }

  for (int i = OrthancPluginValueRepresentation_AE;
       i <= OrthancPluginValueRepresentation_UT; i++)
  {
    OrthancPluginValueRepresentation plugins = static_cast<OrthancPluginValueRepresentation>(i);
    ValueRepresentation orthanc = Plugins::Convert(plugins);
    ASSERT_EQ(plugins, Plugins::Convert(orthanc));
  }
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


namespace Orthanc
{
  // Namespace for the "FRIEND_TEST()" directive in "FromDcmtkBridge" to apply:
  // https://github.com/google/googletest/blob/master/googletest/docs/AdvancedGuide.md#private-class-members
  TEST(FromDcmtkBridge, FromJson)
  {
    std::auto_ptr<DcmElement> element;

    {
      Json::Value a;
      a = "Hello";
      element.reset(FromDcmtkBridge::FromJson(DICOM_TAG_PATIENT_NAME, a, false, Encoding_Utf8));

      Json::Value b;
      std::set<DicomTag> ignoreTagLength;
      ignoreTagLength.insert(DICOM_TAG_PATIENT_ID);

      FromDcmtkBridge::ElementToJson(b, *element, DicomToJsonFormat_Short,
                                     DicomToJsonFlags_Default, 0, Encoding_Ascii, ignoreTagLength);
      ASSERT_TRUE(b.isMember("0010,0010"));
      ASSERT_EQ("Hello", b["0010,0010"].asString());

      FromDcmtkBridge::ElementToJson(b, *element, DicomToJsonFormat_Short,
                                     DicomToJsonFlags_Default, 3, Encoding_Ascii, ignoreTagLength);
      ASSERT_TRUE(b["0010,0010"].isNull()); // "Hello" has more than 3 characters

      FromDcmtkBridge::ElementToJson(b, *element, DicomToJsonFormat_Full,
                                     DicomToJsonFlags_Default, 3, Encoding_Ascii, ignoreTagLength);
      ASSERT_TRUE(b["0010,0010"].isObject());
      ASSERT_EQ("PatientName", b["0010,0010"]["Name"].asString());
      ASSERT_EQ("TooLong", b["0010,0010"]["Type"].asString());
      ASSERT_TRUE(b["0010,0010"]["Value"].isNull());

      ignoreTagLength.insert(DICOM_TAG_PATIENT_NAME);
      FromDcmtkBridge::ElementToJson(b, *element, DicomToJsonFormat_Short,
                                     DicomToJsonFlags_Default, 3, Encoding_Ascii, ignoreTagLength);
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
      std::set<DicomTag> ignoreTagLength;
      FromDcmtkBridge::ElementToJson(b, *element, DicomToJsonFormat_Short,
                                     DicomToJsonFlags_Default, 0, Encoding_Ascii, ignoreTagLength);
      ASSERT_EQ("Hello", b["0010,0010"].asString());
    }

    {
      Json::Value a = Json::arrayValue;
      CreateSampleJson(a);
      element.reset(FromDcmtkBridge::FromJson(REFERENCED_STUDY_SEQUENCE, a, true, Encoding_Utf8));

      {
        Json::Value b;
        std::set<DicomTag> ignoreTagLength;
        FromDcmtkBridge::ElementToJson(b, *element, DicomToJsonFormat_Short,
                                       DicomToJsonFlags_Default, 0, Encoding_Ascii, ignoreTagLength);
        ASSERT_EQ(Json::arrayValue, b["0008,1110"].type());
        ASSERT_EQ(2u, b["0008,1110"].size());
      
        Json::Value::ArrayIndex i = (b["0008,1110"][0]["0010,0010"].asString() == "Hello") ? 0 : 1;

        ASSERT_EQ(3u, b["0008,1110"][i].size());
        ASSERT_EQ(2u, b["0008,1110"][1 - i].size());
        ASSERT_EQ(b["0008,1110"][i]["0010,0010"].asString(), "Hello");
        ASSERT_EQ(b["0008,1110"][i]["0010,0020"].asString(), "World");
        ASSERT_EQ(b["0008,1110"][i]["0008,1030"].asString(), "Toto");
        ASSERT_EQ(b["0008,1110"][1 - i]["0010,0010"].asString(), "Hello2");
        ASSERT_EQ(b["0008,1110"][1 - i]["0010,0020"].asString(), "World2");
      }

      {
        Json::Value b;
        std::set<DicomTag> ignoreTagLength;
        FromDcmtkBridge::ElementToJson(b, *element, DicomToJsonFormat_Full,
                                       DicomToJsonFlags_Default, 0, Encoding_Ascii, ignoreTagLength);

        Json::Value c;
        ServerToolbox::SimplifyTags(c, b, DicomToJsonFormat_Human);

        a[1]["PatientName"] = "Hello2";  // To remove the Data URI Scheme encoding
        ASSERT_EQ(0, c["ReferencedStudySequence"].compare(a));
      }
    }
  }
}


TEST(ParsedDicomFile, InsertReplaceStrings)
{
  ParsedDicomFile f(true);

  f.Insert(DICOM_TAG_PATIENT_NAME, "World", false);
  ASSERT_THROW(f.Insert(DICOM_TAG_PATIENT_ID, "Hello", false), OrthancException);  // Already existing tag
  f.ReplacePlainString(DICOM_TAG_SOP_INSTANCE_UID, "Toto");  // (*)
  f.ReplacePlainString(DICOM_TAG_SOP_CLASS_UID, "Tata");  // (**)

  std::string s;
  ASSERT_FALSE(f.LookupTransferSyntax(s));

  ASSERT_THROW(f.Replace(DICOM_TAG_ACCESSION_NUMBER, std::string("Accession"),
                         false, DicomReplaceMode_ThrowIfAbsent), OrthancException);
  f.Replace(DICOM_TAG_ACCESSION_NUMBER, std::string("Accession"), false, DicomReplaceMode_IgnoreIfAbsent);
  ASSERT_FALSE(f.GetTagValue(s, DICOM_TAG_ACCESSION_NUMBER));
  f.Replace(DICOM_TAG_ACCESSION_NUMBER, std::string("Accession"), false, DicomReplaceMode_InsertIfAbsent);
  ASSERT_TRUE(f.GetTagValue(s, DICOM_TAG_ACCESSION_NUMBER));
  ASSERT_EQ(s, "Accession");
  f.Replace(DICOM_TAG_ACCESSION_NUMBER, std::string("Accession2"), false, DicomReplaceMode_IgnoreIfAbsent);
  ASSERT_TRUE(f.GetTagValue(s, DICOM_TAG_ACCESSION_NUMBER));
  ASSERT_EQ(s, "Accession2");
  f.Replace(DICOM_TAG_ACCESSION_NUMBER, std::string("Accession3"), false, DicomReplaceMode_ThrowIfAbsent);
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
    f.DatasetToJson(b, DicomToJsonFormat_Full, DicomToJsonFlags_Default, 0);

    Json::Value c;
    ServerToolbox::SimplifyTags(c, b, DicomToJsonFormat_Human);

    ASSERT_EQ(0, c["ReferencedPatientSequence"].compare(a));
    ASSERT_NE(0, c["ReferencedStudySequence"].compare(a));  // Because Data URI Scheme decoding was enabled
  }

  a = "data:application/octet-stream;base64,VGF0YQ==";   // echo -n "Tata" | base64 
  f.Replace(DICOM_TAG_SOP_INSTANCE_UID, a, false, DicomReplaceMode_InsertIfAbsent);  // (*)
  f.Replace(DICOM_TAG_SOP_CLASS_UID, a, true, DicomReplaceMode_InsertIfAbsent);  // (**)

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
        ASSERT_EQ(testEncodings[i], f.GetEncoding());
      }

      Json::Value s = Toolbox::ConvertToUtf8(testEncodingsEncoded[i], testEncodings[i]);
      f.Replace(DICOM_TAG_PATIENT_NAME, s, false, DicomReplaceMode_InsertIfAbsent);

      Json::Value v;
      f.DatasetToJson(v, DicomToJsonFormat_Human, DicomToJsonFlags_Default, 0);
      ASSERT_EQ(v["PatientName"].asString(), std::string(testEncodingsExpected[i]));
    }
  }
}


TEST(ParsedDicomFile, ToJsonFlags1)
{
  FromDcmtkBridge::RegisterDictionaryTag(DicomTag(0x7053, 0x1000), ValueRepresentation_PersonName, "MyPrivateTag", 1, 1, "");
  FromDcmtkBridge::RegisterDictionaryTag(DicomTag(0x7050, 0x1000), ValueRepresentation_PersonName, "Declared public tag", 1, 1, "");

  ParsedDicomFile f(true);
  f.Insert(DicomTag(0x7050, 0x1000), "Some public tag", false);  // Even group => public tag
  f.Insert(DicomTag(0x7052, 0x1000), "Some unknown tag", false);  // Even group => public, unknown tag
  f.Insert(DicomTag(0x7053, 0x1000), "Some private tag", false);  // Odd group => private tag

  Json::Value v;
  f.DatasetToJson(v, DicomToJsonFormat_Short, DicomToJsonFlags_None, 0);
  ASSERT_EQ(Json::objectValue, v.type());
  ASSERT_EQ(6u, v.getMemberNames().size());
  ASSERT_FALSE(v.isMember("7052,1000"));
  ASSERT_FALSE(v.isMember("7053,1000"));
  ASSERT_TRUE(v.isMember("7050,1000"));
  ASSERT_EQ(Json::stringValue, v["7050,1000"].type());
  ASSERT_EQ("Some public tag", v["7050,1000"].asString());

  f.DatasetToJson(v, DicomToJsonFormat_Short, static_cast<DicomToJsonFlags>(DicomToJsonFlags_IncludePrivateTags | DicomToJsonFlags_ConvertBinaryToNull), 0);
  ASSERT_EQ(Json::objectValue, v.type());
  ASSERT_EQ(7u, v.getMemberNames().size());
  ASSERT_FALSE(v.isMember("7052,1000"));
  ASSERT_TRUE(v.isMember("7050,1000"));
  ASSERT_TRUE(v.isMember("7053,1000"));
  ASSERT_EQ("Some public tag", v["7050,1000"].asString());
  ASSERT_EQ(Json::nullValue, v["7053,1000"].type());

  f.DatasetToJson(v, DicomToJsonFormat_Short, DicomToJsonFlags_IncludePrivateTags, 0);
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

  f.DatasetToJson(v, DicomToJsonFormat_Short, static_cast<DicomToJsonFlags>(DicomToJsonFlags_IncludeUnknownTags | DicomToJsonFlags_ConvertBinaryToNull), 0);
  ASSERT_EQ(Json::objectValue, v.type());
  ASSERT_EQ(7u, v.getMemberNames().size());
  ASSERT_TRUE(v.isMember("7050,1000"));
  ASSERT_TRUE(v.isMember("7052,1000"));
  ASSERT_FALSE(v.isMember("7053,1000"));
  ASSERT_EQ("Some public tag", v["7050,1000"].asString());
  ASSERT_EQ(Json::nullValue, v["7052,1000"].type());

  f.DatasetToJson(v, DicomToJsonFormat_Short, static_cast<DicomToJsonFlags>(DicomToJsonFlags_IncludeUnknownTags), 0);
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

  f.DatasetToJson(v, DicomToJsonFormat_Short, static_cast<DicomToJsonFlags>(DicomToJsonFlags_IncludeUnknownTags | DicomToJsonFlags_IncludePrivateTags | DicomToJsonFlags_ConvertBinaryToNull), 0);
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
  f.Insert(DICOM_TAG_PIXEL_DATA, "Pixels", false);

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
  ASSERT_EQ("Pixels", v["7fe0,0010"].asString());  

  f.DatasetToJson(v, DicomToJsonFormat_Short, DicomToJsonFlags_IncludePixelData, 0);
  ASSERT_EQ(Json::objectValue, v.type());
  ASSERT_EQ(6u, v.getMemberNames().size());
  ASSERT_TRUE(v.isMember("7fe0,0010"));  
  ASSERT_EQ(Json::stringValue, v["7fe0,0010"].type());
  std::string mime, content;
  ASSERT_TRUE(Toolbox::DecodeDataUriScheme(mime, content, v["7fe0,0010"].asString()));
  ASSERT_EQ("application/octet-stream", mime);
  ASSERT_EQ("Pixels", content);
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
    std::auto_ptr<ParsedDicomFile> dicom
      (ParsedDicomFile::CreateFromJson(v, static_cast<DicomFromJsonFlags>(DicomFromJsonFlags_GenerateIdentifiers)));

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
    std::auto_ptr<ParsedDicomFile> dicom
      (ParsedDicomFile::CreateFromJson(v, static_cast<DicomFromJsonFlags>(DicomFromJsonFlags_GenerateIdentifiers)));

    Json::Value vv;
    dicom->DatasetToJson(vv, DicomToJsonFormat_Human, static_cast<DicomToJsonFlags>(DicomToJsonFlags_IncludePixelData), 0);

    std::string mime, content;
    ASSERT_TRUE(Toolbox::DecodeDataUriScheme(mime, content, vv["PixelData"].asString()));
    ASSERT_EQ("application/octet-stream", mime);
    ASSERT_EQ(5u * 5u * 3u /* the red dot is 5x5 pixels in RGB24 */ + 1 /* for padding */, content.size());
  }


  {
    std::auto_ptr<ParsedDicomFile> dicom
      (ParsedDicomFile::CreateFromJson(v, static_cast<DicomFromJsonFlags>(DicomFromJsonFlags_DecodeDataUriScheme)));

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
  static const char* PATH = "UnitTestsResults/PatternGrayscale8.dcm";

  Orthanc::Image image(Orthanc::PixelFormat_Grayscale8, 256, 256, false);

  for (int y = 0; y < 256; y++)
  {
    uint8_t *p = reinterpret_cast<uint8_t*>(image.GetRow(y));
    for (int x = 0; x < 256; x++, p++)
    {
      *p = y;
    }
  }

  Orthanc::ImageAccessor r = image.GetRegion(32, 32, 64, 192);
  Orthanc::ImageProcessing::Set(r, 0); 
  r = image.GetRegion(160, 32, 64, 192);
  Orthanc::ImageProcessing::Set(r, 255); 

  {
    ParsedDicomFile f(true);
    f.ReplacePlainString(DICOM_TAG_SOP_CLASS_UID, "1.2.840.10008.5.1.4.1.1.7");
    f.ReplacePlainString(DICOM_TAG_STUDY_INSTANCE_UID, "1.2.276.0.7230010.3.1.2.2831176407.321.1458901422.884998");
    f.ReplacePlainString(DICOM_TAG_PATIENT_ID, "ORTHANC");
    f.ReplacePlainString(DICOM_TAG_PATIENT_NAME, "Orthanc");
    f.ReplacePlainString(DICOM_TAG_STUDY_DESCRIPTION, "Patterns");
    f.ReplacePlainString(DICOM_TAG_SERIES_DESCRIPTION, "Grayscale8");
    f.EmbedImage(image);

    f.SaveToFile(PATH);
  }

  {
    std::string s;
    Orthanc::SystemToolbox::ReadFile(s, PATH);
    Orthanc::ParsedDicomFile f(s);
    
    std::auto_ptr<Orthanc::ImageAccessor> decoded(Orthanc::DicomImageDecoder::Decode(f, 0));
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
  static const char* PATH = "UnitTestsResults/PatternRGB24.dcm";

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

  {
    ParsedDicomFile f(true);
    f.ReplacePlainString(DICOM_TAG_SOP_CLASS_UID, "1.2.840.10008.5.1.4.1.1.7");
    f.ReplacePlainString(DICOM_TAG_STUDY_INSTANCE_UID, "1.2.276.0.7230010.3.1.2.2831176407.321.1458901422.884998");
    f.ReplacePlainString(DICOM_TAG_PATIENT_ID, "ORTHANC");
    f.ReplacePlainString(DICOM_TAG_PATIENT_NAME, "Orthanc");
    f.ReplacePlainString(DICOM_TAG_STUDY_DESCRIPTION, "Patterns");
    f.ReplacePlainString(DICOM_TAG_SERIES_DESCRIPTION, "RGB24");
    f.EmbedImage(image);

    f.SaveToFile(PATH);
  }

  {
    std::string s;
    Orthanc::SystemToolbox::ReadFile(s, PATH);
    Orthanc::ParsedDicomFile f(s);
    
    std::auto_ptr<Orthanc::ImageAccessor> decoded(Orthanc::DicomImageDecoder::Decode(f, 0));
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
  static const char* PATH = "UnitTestsResults/PatternGrayscale16.dcm";

  Orthanc::Image image(Orthanc::PixelFormat_Grayscale16, 256, 256, false);

  uint16_t v = 0;
  for (int y = 0; y < 256; y++)
  {
    uint16_t *p = reinterpret_cast<uint16_t*>(image.GetRow(y));
    for (int x = 0; x < 256; x++, v++, p++)
    {
      *p = htole16(v);   // Orthanc uses Little-Endian transfer syntax to encode images
    }
  }

  Orthanc::ImageAccessor r = image.GetRegion(32, 32, 64, 192);
  Orthanc::ImageProcessing::Set(r, 0); 
  r = image.GetRegion(160, 32, 64, 192);
  Orthanc::ImageProcessing::Set(r, 65535); 

  {
    ParsedDicomFile f(true);
    f.ReplacePlainString(DICOM_TAG_SOP_CLASS_UID, "1.2.840.10008.5.1.4.1.1.7");
    f.ReplacePlainString(DICOM_TAG_STUDY_INSTANCE_UID, "1.2.276.0.7230010.3.1.2.2831176407.321.1458901422.884998");
    f.ReplacePlainString(DICOM_TAG_PATIENT_ID, "ORTHANC");
    f.ReplacePlainString(DICOM_TAG_PATIENT_NAME, "Orthanc");
    f.ReplacePlainString(DICOM_TAG_STUDY_DESCRIPTION, "Patterns");
    f.ReplacePlainString(DICOM_TAG_SERIES_DESCRIPTION, "Grayscale16");
    f.EmbedImage(image);

    f.SaveToFile(PATH);
  }

  {
    std::string s;
    Orthanc::SystemToolbox::ReadFile(s, PATH);
    Orthanc::ParsedDicomFile f(s);
    
    std::auto_ptr<Orthanc::ImageAccessor> decoded(Orthanc::DicomImageDecoder::Decode(f, 0));
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
  static const char* PATH = "UnitTestsResults/PatternSignedGrayscale16.dcm";

  Orthanc::Image image(Orthanc::PixelFormat_SignedGrayscale16, 256, 256, false);

  int16_t v = -32768;
  for (int y = 0; y < 256; y++)
  {
    int16_t *p = reinterpret_cast<int16_t*>(image.GetRow(y));
    for (int x = 0; x < 256; x++, v++, p++)
    {
      *p = htole16(v);   // Orthanc uses Little-Endian transfer syntax to encode images
    }
  }

  Orthanc::ImageAccessor r = image.GetRegion(32, 32, 64, 192);
  Orthanc::ImageProcessing::Set(r, -32768); 
  r = image.GetRegion(160, 32, 64, 192);
  Orthanc::ImageProcessing::Set(r, 32767); 

  {
    ParsedDicomFile f(true);
    f.ReplacePlainString(DICOM_TAG_SOP_CLASS_UID, "1.2.840.10008.5.1.4.1.1.7");
    f.ReplacePlainString(DICOM_TAG_STUDY_INSTANCE_UID, "1.2.276.0.7230010.3.1.2.2831176407.321.1458901422.884998");
    f.ReplacePlainString(DICOM_TAG_PATIENT_ID, "ORTHANC");
    f.ReplacePlainString(DICOM_TAG_PATIENT_NAME, "Orthanc");
    f.ReplacePlainString(DICOM_TAG_STUDY_DESCRIPTION, "Patterns");
    f.ReplacePlainString(DICOM_TAG_SERIES_DESCRIPTION, "SignedGrayscale16");
    f.EmbedImage(image);

    f.SaveToFile(PATH);
  }

  {
    std::string s;
    Orthanc::SystemToolbox::ReadFile(s, PATH);
    Orthanc::ParsedDicomFile f(s);
    
    std::auto_ptr<Orthanc::ImageAccessor> decoded(Orthanc::DicomImageDecoder::Decode(f, 0));
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



static void CheckEncoding(const ParsedDicomFile& dicom,
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
    ParsedDicomFile dicom(m);
    ASSERT_EQ(1u, dicom.GetDcmtkObject().getDataset()->card());
    CheckEncoding(dicom, Encoding_Ascii);
  }

  {
    DicomMap m;
    ParsedDicomFile dicom(m, Encoding_Latin4);
    ASSERT_EQ(1u, dicom.GetDcmtkObject().getDataset()->card());
    CheckEncoding(dicom, Encoding_Latin4);
  }

  {
    DicomMap m;
    m.SetValue(DICOM_TAG_SPECIFIC_CHARACTER_SET, "ISO_IR 148", false);
    ParsedDicomFile dicom(m);
    ASSERT_EQ(1u, dicom.GetDcmtkObject().getDataset()->card());
    CheckEncoding(dicom, Encoding_Latin5);
  }

  {
    DicomMap m;
    m.SetValue(DICOM_TAG_SPECIFIC_CHARACTER_SET, "ISO_IR 148", false);
    ParsedDicomFile dicom(m, Encoding_Latin1);
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
        std::string decoded = Toolbox::ConvertToUtf8(encoded, testEncodings[i]);
        ASSERT_STREQ(testEncodingsExpected[i], decoded.c_str());

        if (testEncodings[i] != Encoding_Chinese)
        {
          // A specific source string is used in "EncodingTests.py" to
          // test against Chinese, it is normal that it does not correspond to UTF8

          std::string encoded = Toolbox::ConvertToUtf8(Toolbox::ConvertFromUtf8(utf8, testEncodings[i]), testEncodings[i]);
          ASSERT_STREQ(testEncodingsExpected[i], encoded.c_str());
        }
      }


      Json::Value v;

      {
        DicomMap m;
        m.SetValue(DICOM_TAG_PATIENT_NAME, testEncodingsExpected[i], false);

        ParsedDicomFile dicom(m, testEncodings[i]);
    
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

        ParsedDicomFile dicom(m, testEncodings[i]);

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

      ParsedDicomFile dicom(m, Encoding_Utf8);
      ASSERT_EQ(Encoding_Utf8, dicom.GetEncoding());
      ASSERT_TRUE(dicom.GetTagValue(tag, DICOM_TAG_PATIENT_NAME));
      ASSERT_EQ(tag, testEncodingsExpected[i]);

      {
        Json::Value v;
        dicom.DatasetToJson(v, DicomToJsonFormat_Human, DicomToJsonFlags_Default, 0);
        ASSERT_STREQ(v["SpecificCharacterSet"].asCString(), "ISO_IR 192");
        ASSERT_STREQ(v["PatientName"].asCString(), testEncodingsExpected[i]);
      }

      dicom.ChangeEncoding(testEncodings[i]);

      ASSERT_EQ(testEncodings[i], dicom.GetEncoding());
      
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

    ParsedDicomFile d(m, Encoding_Latin3 /* default encoding */);
    ASSERT_EQ(Encoding_Latin3, d.GetEncoding());
  }
  
  {
    // Valid encoding, "ISO_IR 13" is Japanese
    DicomMap m;
    m.SetValue(DICOM_TAG_SPECIFIC_CHARACTER_SET, "ISO_IR 13", false);
    m.SetValue(DICOM_TAG_PATIENT_NAME, "HELLO", false);

    ParsedDicomFile d(m, Encoding_Latin3 /* default encoding */);
    ASSERT_EQ(Encoding_Japanese, d.GetEncoding());
  }
  
  {
    // Invalid value for an encoding ("nope" is not in the DICOM standard)
    DicomMap m;
    m.SetValue(DICOM_TAG_SPECIFIC_CHARACTER_SET, "nope", false);
    m.SetValue(DICOM_TAG_PATIENT_NAME, "HELLO", false);

    ASSERT_THROW(ParsedDicomFile d(m, Encoding_Latin3), OrthancException);
  }
  
  {
    // Invalid encoding, as provided as a binary string
    DicomMap m;
    m.SetValue(DICOM_TAG_SPECIFIC_CHARACTER_SET, "ISO_IR 13", true);
    m.SetValue(DICOM_TAG_PATIENT_NAME, "HELLO", false);

    ASSERT_THROW(ParsedDicomFile d(m, Encoding_Latin3), OrthancException);
  }
  
  {
    // Encoding provided as an empty string, fallback to default encoding
    // In Orthanc <= 1.3.1, this test was throwing an exception
    DicomMap m;
    m.SetValue(DICOM_TAG_SPECIFIC_CHARACTER_SET, "", false);
    m.SetValue(DICOM_TAG_PATIENT_NAME, "HELLO", false);

    ParsedDicomFile d(m, Encoding_Latin3 /* default encoding */);
    ASSERT_EQ(Encoding_Latin3, d.GetEncoding());
  }
}
