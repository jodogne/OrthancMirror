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
#include "../Core/DicomNetworking/DicomFindAnswers.h"
#include "../Core/DicomParsing/DicomModification.h"
#include "../Core/DicomParsing/DicomWebJsonVisitor.h"
#include "../Core/DicomParsing/FromDcmtkBridge.h"
#include "../Core/DicomParsing/Internals/DicomImageDecoder.h"
#include "../Core/DicomParsing/ToDcmtkBridge.h"
#include "../Core/Endianness.h"
#include "../Core/Images/Image.h"
#include "../Core/Images/ImageBuffer.h"
#include "../Core/Images/ImageProcessing.h"
#include "../Core/Images/PngReader.h"
#include "../Core/Images/PngWriter.h"
#include "../Core/OrthancException.h"
#include "../Core/SystemToolbox.h"
#include "../OrthancServer/ServerToolbox.h"
#include "../Plugins/Engine/PluginsEnumerations.h"
#include "../Resources/EncodingTests.h"

#include <dcmtk/dcmdata/dcelem.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <boost/algorithm/string/predicate.hpp>

#if ORTHANC_ENABLE_PUGIXML == 1
#  include <pugixml.hpp>
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
    o.SaveToFile("UnitTestsResults/png4.dcm");
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


TEST(FromDcmtkBridge, ValueRepresentationConversions)
{
#if ORTHANC_ENABLE_PLUGINS == 1
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
#endif
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
    std::unique_ptr<DcmElement> element;

    {
      Json::Value a;
      a = "Hello";
      element.reset(FromDcmtkBridge::FromJson(DICOM_TAG_PATIENT_NAME, a, false, Encoding_Utf8, ""));

      Json::Value b;
      std::set<DicomTag> ignoreTagLength;
      ignoreTagLength.insert(DICOM_TAG_PATIENT_ID);

      FromDcmtkBridge::ElementToJson(b, *element, DicomToJsonFormat_Short,
                                     DicomToJsonFlags_Default, 0, Encoding_Ascii, false, ignoreTagLength);
      ASSERT_TRUE(b.isMember("0010,0010"));
      ASSERT_EQ("Hello", b["0010,0010"].asString());

      FromDcmtkBridge::ElementToJson(b, *element, DicomToJsonFormat_Short,
                                     DicomToJsonFlags_Default, 3, Encoding_Ascii, false, ignoreTagLength);
      ASSERT_TRUE(b["0010,0010"].isNull()); // "Hello" has more than 3 characters

      FromDcmtkBridge::ElementToJson(b, *element, DicomToJsonFormat_Full,
                                     DicomToJsonFlags_Default, 3, Encoding_Ascii, false, ignoreTagLength);
      ASSERT_TRUE(b["0010,0010"].isObject());
      ASSERT_EQ("PatientName", b["0010,0010"]["Name"].asString());
      ASSERT_EQ("TooLong", b["0010,0010"]["Type"].asString());
      ASSERT_TRUE(b["0010,0010"]["Value"].isNull());

      ignoreTagLength.insert(DICOM_TAG_PATIENT_NAME);
      FromDcmtkBridge::ElementToJson(b, *element, DicomToJsonFormat_Short,
                                     DicomToJsonFlags_Default, 3, Encoding_Ascii, false, ignoreTagLength);
      ASSERT_EQ("Hello", b["0010,0010"].asString());
    }

    {
      Json::Value a;
      a = "Hello";
      // Cannot assign a string to a sequence
      ASSERT_THROW(element.reset(FromDcmtkBridge::FromJson(REFERENCED_STUDY_SEQUENCE, a, false, Encoding_Utf8, "")), OrthancException);
    }

    {
      Json::Value a = Json::arrayValue;
      a.append("Hello");
      // Cannot assign an array to a string
      ASSERT_THROW(element.reset(FromDcmtkBridge::FromJson(DICOM_TAG_PATIENT_NAME, a, false, Encoding_Utf8, "")), OrthancException);
    }

    {
      Json::Value a;
      a = "data:application/octet-stream;base64,SGVsbG8=";  // echo -n "Hello" | base64
      element.reset(FromDcmtkBridge::FromJson(DICOM_TAG_PATIENT_NAME, a, true, Encoding_Utf8, ""));

      Json::Value b;
      std::set<DicomTag> ignoreTagLength;
      FromDcmtkBridge::ElementToJson(b, *element, DicomToJsonFormat_Short,
                                     DicomToJsonFlags_Default, 0, Encoding_Ascii, false, ignoreTagLength);
      ASSERT_EQ("Hello", b["0010,0010"].asString());
    }

    {
      Json::Value a = Json::arrayValue;
      CreateSampleJson(a);
      element.reset(FromDcmtkBridge::FromJson(REFERENCED_STUDY_SEQUENCE, a, true, Encoding_Utf8, ""));

      {
        Json::Value b;
        std::set<DicomTag> ignoreTagLength;
        FromDcmtkBridge::ElementToJson(b, *element, DicomToJsonFormat_Short,
                                       DicomToJsonFlags_Default, 0, Encoding_Ascii, false, ignoreTagLength);
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
                                       DicomToJsonFlags_Default, 0, Encoding_Ascii, false, ignoreTagLength);

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

  f.Insert(DICOM_TAG_PATIENT_NAME, "World", false, "");
  ASSERT_THROW(f.Insert(DICOM_TAG_PATIENT_ID, "Hello", false, ""), OrthancException);  // Already existing tag
  f.ReplacePlainString(DICOM_TAG_SOP_INSTANCE_UID, "Toto");  // (*)
  f.ReplacePlainString(DICOM_TAG_SOP_CLASS_UID, "Tata");  // (**)

  std::string s;
  ASSERT_FALSE(f.LookupTransferSyntax(s));

  ASSERT_THROW(f.Replace(DICOM_TAG_ACCESSION_NUMBER, std::string("Accession"),
                         false, DicomReplaceMode_ThrowIfAbsent, ""), OrthancException);
  f.Replace(DICOM_TAG_ACCESSION_NUMBER, std::string("Accession"), false, DicomReplaceMode_IgnoreIfAbsent, "");
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
    ServerToolbox::SimplifyTags(c, b, DicomToJsonFormat_Human);

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
  f.Insert(DICOM_TAG_PIXEL_DATA, "Pixels", false, "");

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

  Orthanc::ImageAccessor r;

  image.GetRegion(r, 32, 32, 64, 192);
  Orthanc::ImageProcessing::Set(r, 0);
  
  image.GetRegion(r, 160, 32, 64, 192);
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
    
    std::unique_ptr<Orthanc::ImageAccessor> decoded(Orthanc::DicomImageDecoder::Decode(f, 0));
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
    
    std::unique_ptr<Orthanc::ImageAccessor> decoded(Orthanc::DicomImageDecoder::Decode(f, 0));
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

  Orthanc::ImageAccessor r;
  
  image.GetRegion(r, 32, 32, 64, 192);
  Orthanc::ImageProcessing::Set(r, 0);
  
  image.GetRegion(r, 160, 32, 64, 192);
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
    
    std::unique_ptr<Orthanc::ImageAccessor> decoded(Orthanc::DicomImageDecoder::Decode(f, 0));
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

  Orthanc::ImageAccessor r;
  image.GetRegion(r, 32, 32, 64, 192);
  Orthanc::ImageProcessing::Set(r, -32768);
  
  image.GetRegion(r, 160, 32, 64, 192);
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
    
    std::unique_ptr<Orthanc::ImageAccessor> decoded(Orthanc::DicomImageDecoder::Decode(f, 0));
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

          std::string encoded = Toolbox::ConvertToUtf8(Toolbox::ConvertFromUtf8(utf8, testEncodings[i]), testEncodings[i], false);
          ASSERT_STREQ(testEncodingsExpected[i], encoded.c_str());
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

    ASSERT_THROW(ParsedDicomFile d(m, Encoding_Latin3, false), OrthancException);
  }
  
  {
    // Invalid encoding, as provided as a binary string
    DicomMap m;
    m.SetValue(DICOM_TAG_SPECIFIC_CHARACTER_SET, "ISO_IR 13", true);
    m.SetValue(DICOM_TAG_PATIENT_NAME, "HELLO", false);

    ASSERT_THROW(ParsedDicomFile d(m, Encoding_Latin3, false), OrthancException);
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




#if ORTHANC_ENABLE_DCMTK_TRANSCODING == 1

#include "../Core/DicomParsing/Internals/DicomFrameIndex.h"

#include <dcmtk/dcmdata/dcostrmb.h>
#include <dcmtk/dcmdata/dcpixel.h>
#include <dcmtk/dcmdata/dcpxitem.h>


namespace Orthanc
{
  class IDicomTranscoder : public boost::noncopyable
  {
  public:
    virtual ~IDicomTranscoder()
    {
    }

    virtual DicomTransferSyntax GetTransferSyntax() = 0;

    virtual std::string GetSopClassUid() = 0;

    virtual std::string GetSopInstanceUid() = 0;

    virtual unsigned int GetFramesCount() = 0;

    virtual ImageAccessor* DecodeFrame(unsigned int frame) = 0;

    virtual void GetCompressedFrame(std::string& target,
                                    unsigned int frame) = 0;

    virtual IDicomTranscoder* Transcode(std::set<DicomTransferSyntax> syntaxes,
                                        bool allowNewSopInstanceUid) = 0;
  };


  class DcmtkTranscoder : public IDicomTranscoder
  {
  private:
    std::unique_ptr<DcmFileFormat>    dicom_;
    std::unique_ptr<DicomFrameIndex>  index_;
    DicomTransferSyntax               transferSyntax_;
    std::string                       sopClassUid_;
    std::string                       sopInstanceUid_;

    void Setup(DcmFileFormat* dicom)
    {
      dicom_.reset(dicom);
      
      if (dicom == NULL ||
          dicom_->getDataset() == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }

      DcmDataset& dataset = *dicom_->getDataset();
      index_.reset(new DicomFrameIndex(dataset));

      E_TransferSyntax xfer = dataset.getOriginalXfer();
      if (xfer == EXS_Unknown)
      {
        dataset.updateOriginalXfer();
        xfer = dataset.getOriginalXfer();
        if (xfer == EXS_Unknown)
        {
          throw OrthancException(ErrorCode_BadFileFormat,
                                 "Cannot determine the transfer syntax of the DICOM instance");
        }
      }

      if (!FromDcmtkBridge::LookupOrthancTransferSyntax(transferSyntax_, xfer))
      {
        throw OrthancException(
          ErrorCode_BadFileFormat,
          "Unsupported transfer syntax: " + boost::lexical_cast<std::string>(xfer));
      }

      const char* a = NULL;
      const char* b = NULL;

      if (!dataset.findAndGetString(DCM_SOPClassUID, a).good() ||
          !dataset.findAndGetString(DCM_SOPInstanceUID, b).good() ||
          a == NULL ||
          b == NULL)
      {
        throw OrthancException(ErrorCode_BadFileFormat,
                               "Missing SOP class/instance UID in DICOM instance");
      }

      sopClassUid_.assign(a);
      sopInstanceUid_.assign(b);
    }
    
  public:
    DcmtkTranscoder(DcmFileFormat* dicom)  // Takes ownership
    {
      Setup(dicom);
    }

    DcmtkTranscoder(const void* dicom,
                    size_t size)
    {
      Setup(FromDcmtkBridge::LoadFromMemoryBuffer(dicom, size));
    }

    virtual DicomTransferSyntax GetTransferSyntax() ORTHANC_OVERRIDE
    {
      return transferSyntax_;
    }

    virtual std::string GetSopClassUid() ORTHANC_OVERRIDE
    {
      return sopClassUid_;
    }
    
    virtual std::string GetSopInstanceUid() ORTHANC_OVERRIDE
    {
      return sopInstanceUid_;
    }

    virtual unsigned int GetFramesCount() ORTHANC_OVERRIDE
    {
      return index_->GetFramesCount();
    }

    virtual ImageAccessor* DecodeFrame(unsigned int frame) ORTHANC_OVERRIDE
    {
      assert(dicom_->getDataset() != NULL);
      return DicomImageDecoder::Decode(*dicom_->getDataset(), frame);
    }

    virtual void GetCompressedFrame(std::string& target,
                                    unsigned int frame) ORTHANC_OVERRIDE
    {
#if 1
      index_->GetRawFrame(target, frame);
      printf("%d: %d\n", frame, target.size());
#endif

#if 1
      assert(dicom_->getDataset() != NULL);
      DcmDataset& dataset = *dicom_->getDataset();
      
      DcmPixelSequence* pixelSequence = FromDcmtkBridge::GetPixelSequence(dataset);

      if (pixelSequence != NULL &&
          frame == 0 &&
          pixelSequence->card() != GetFramesCount() + 1)
      {
        printf("COMPRESSED\n");
        
        // Check out "djcodecd.cc"
        
        printf("%d fragments\n", pixelSequence->card());
        
        // Skip the first fragment, that is the offset table
        for (unsigned long i = 1; ;i++)
        {
          DcmPixelItem *fragment = NULL;
          if (pixelSequence->getItem(fragment, i).good())
          {
            printf("fragment %d %d\n", i, fragment->getLength());
          }
          else
          {
            break;
          }
        }
      }
#endif
    }

    virtual IDicomTranscoder* Transcode(std::set<DicomTransferSyntax> syntaxes,
                                        bool allowNewSopInstanceUid) ORTHANC_OVERRIDE
    {
      throw OrthancException(ErrorCode_NotImplemented);
    }
  };
}




static bool Transcode(std::string& buffer,
                      DcmDataset& dataSet,
                      E_TransferSyntax xfer)
{
  // Determine the transfer syntax which shall be used to write the
  // information to the file. We always switch to the Little Endian
  // syntax, with explicit length.

  // http://support.dcmtk.org/docs/dcxfer_8h-source.html


  /**
   * Note that up to Orthanc 0.7.1 (inclusive), the
   * "EXS_LittleEndianExplicit" was always used to save the DICOM
   * dataset into memory. We now keep the original transfer syntax
   * (if available).
   **/
  //E_TransferSyntax xfer = dataSet.getOriginalXfer();
  if (xfer == EXS_Unknown)
  {
    // No information about the original transfer syntax: This is
    // most probably a DICOM dataset that was read from memory.
    xfer = EXS_LittleEndianExplicit;
  }

  E_EncodingType encodingType = /*opt_sequenceType*/ EET_ExplicitLength;

  // Create the meta-header information
  DcmFileFormat ff(&dataSet);
  ff.validateMetaInfo(xfer);
  ff.removeInvalidGroups();

  // Create a memory buffer with the proper size
  {
    const uint32_t estimatedSize = ff.calcElementLength(xfer, encodingType);  // (*)
    buffer.resize(estimatedSize);
  }

  DcmOutputBufferStream ob(&buffer[0], buffer.size());

  // Fill the memory buffer with the meta-header and the dataset
  ff.transferInit();
  OFCondition c = ff.write(ob, xfer, encodingType, NULL,
                           /*opt_groupLength*/ EGL_recalcGL,
                           /*opt_paddingType*/ EPD_withoutPadding);
  ff.transferEnd();

  if (c.good())
  {
    // The DICOM file is successfully written, truncate the target
    // buffer if its size was overestimated by (*)
    ob.flush();

    size_t effectiveSize = static_cast<size_t>(ob.tell());
    if (effectiveSize < buffer.size())
    {
      buffer.resize(effectiveSize);
    }

    return true;
  }
  else
  {
    // Error
    buffer.clear();
    return false;
  }
}

#include "dcmtk/dcmjpeg/djrploss.h"  /* for DJ_RPLossy */
#include "dcmtk/dcmjpeg/djrplol.h"   /* for DJ_RPLossless */

#include <boost/filesystem.hpp>


static void TestFile(const std::string& path)
{
  printf("** %s\n", path.c_str());

  std::string s;
  SystemToolbox::ReadFile(s, path);

  Orthanc::DcmtkTranscoder transcoder(s.c_str(), s.size());

  printf("[%s] [%s] [%s] %d\n", GetTransferSyntaxUid(transcoder.GetTransferSyntax()),
         transcoder.GetSopClassUid().c_str(), transcoder.GetSopInstanceUid().c_str(),
         transcoder.GetFramesCount());

  for (size_t i = 0; i < transcoder.GetFramesCount(); i++)
  {
    std::string f;
    transcoder.GetCompressedFrame(f, i);

    if (i == 0)
    {
      static unsigned int i = 0;
      char buf[1024];
      sprintf(buf, "/tmp/frame-%06d.dcm", i++);
      printf(">> %s\n", buf);
      Orthanc::SystemToolbox::WriteFile(f, buf);
    }
  }

  printf("\n");
}

TEST(Toto, Transcode)
{
  if (0)
  {
    OFLog::configure(OFLogger::DEBUG_LOG_LEVEL);

    std::string s;
    //SystemToolbox::ReadFile(s, "/home/jodogne/Subversion/orthanc-tests/Database/TransferSyntaxes/1.2.840.10008.1.2.4.50.dcm");
    //SystemToolbox::ReadFile(s, "/home/jodogne/DICOM/Alain.dcm");
    SystemToolbox::ReadFile(s, "/home/jodogne/Subversion/orthanc-tests/Database/Brainix/Epi/IM-0001-0002.dcm");

    std::unique_ptr<DcmFileFormat> dicom(FromDcmtkBridge::LoadFromMemoryBuffer(s.c_str(), s.size()));

    // less /home/jodogne/Downloads/dcmtk-3.6.4/dcmdata/include/dcmtk/dcmdata/dcxfer.h
    printf(">> %d\n", dicom->getDataset()->getOriginalXfer());  // => 4 == EXS_JPEGProcess1

    const DcmRepresentationParameter *p;

#if 0
    E_TransferSyntax target = EXS_LittleEndianExplicit;
    p = NULL;
#elif 1
    E_TransferSyntax target = EXS_JPEGProcess14SV1;  
    DJ_RPLossless rp_lossless(6, 0);
    p = &rp_lossless;
#else
    E_TransferSyntax target = EXS_JPEGProcess1;
    DJ_RPLossy rp_lossy(90);  // quality
    p = &rp_lossy;
#endif 
  
    ASSERT_TRUE(dicom->getDataset()->chooseRepresentation(target, p).good());
    ASSERT_TRUE(dicom->getDataset()->canWriteXfer(target));

    std::string t;
    ASSERT_TRUE(Transcode(t, *dicom->getDataset(), target));

    SystemToolbox::WriteFile(s, "source.dcm");
    SystemToolbox::WriteFile(t, "target.dcm");
  }

  if (1)
  {
    const char* const PATH = "/home/jodogne/Subversion/orthanc-tests/Database/TransferSyntaxes";
    
    for (boost::filesystem::directory_iterator it(PATH);
         it != boost::filesystem::directory_iterator(); ++it)
    {
      if (boost::filesystem::is_regular_file(it->status()))
      {
        TestFile(it->path().string());
      }
    }

    TestFile("/home/jodogne/Subversion/orthanc-tests/Database/Multiframe.dcm");
    TestFile("/home/jodogne/Subversion/orthanc-tests/Database/Issue44/Monochrome1-Jpeg.dcm");
  }
}

#endif
