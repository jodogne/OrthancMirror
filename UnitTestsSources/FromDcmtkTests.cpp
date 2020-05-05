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
#include <dcmtk/dcmjpeg/djrploss.h>   // for DJ_RPLossy
#include <dcmtk/dcmjpeg/djrplol.h>    // for DJ_RPLossless


#if !defined(ORTHANC_ENABLE_DCMTK_JPEG)
#  error Macro ORTHANC_ENABLE_DCMTK_JPEG must be defined
#endif

#if !defined(ORTHANC_ENABLE_DCMTK_JPEG_LOSSLESS)
#  error Macro ORTHANC_ENABLE_DCMTK_JPEG_LOSSLESS must be defined
#endif



namespace Orthanc
{
  class IParsedDicomImage : public boost::noncopyable
  {
  public:
    virtual ~IParsedDicomImage()
    {
    }

    virtual DicomTransferSyntax GetTransferSyntax() = 0;

    virtual std::string GetSopClassUid() = 0;

    virtual std::string GetSopInstanceUid() = 0;

    virtual unsigned int GetFramesCount() = 0;

    // Can return NULL, for compressed transfer syntaxes
    virtual ImageAccessor* GetUncompressedFrame(unsigned int frame) = 0;
    
    virtual void GetCompressedFrame(std::string& target,
                                    unsigned int frame) = 0;
    
    virtual void WriteToMemoryBuffer(std::string& target) = 0;
  };


  class IDicomImageReader : public boost::noncopyable
  {
  public:
    virtual ~IDicomImageReader()
    {
    }

    virtual IParsedDicomImage* Read(const void* data,
                                    size_t size) = 0;

    virtual IParsedDicomImage* Transcode(const void* data,
                                         size_t size,
                                         DicomTransferSyntax syntax,
                                         bool allowNewSopInstanceUid) = 0;
  };


  class DcmtkImageReader : public IDicomImageReader
  {
  private:
    class Image : public IParsedDicomImage
    {
    private:
      std::unique_ptr<DcmFileFormat>    dicom_;
      std::unique_ptr<DicomFrameIndex>  index_;
      DicomTransferSyntax               transferSyntax_;
      std::string                       sopClassUid_;
      std::string                       sopInstanceUid_;

      static std::string GetStringTag(DcmDataset& dataset,
                                      const DcmTagKey& tag)
      {
        const char* value = NULL;

        if (!dataset.findAndGetString(tag, value).good() ||
            value == NULL)
        {
          throw OrthancException(ErrorCode_BadFileFormat,
                                 "Missing SOP class/instance UID in DICOM instance");
        }
        else
        {
          return std::string(value);
        }
      }

    public:
      Image(DcmFileFormat* dicom,
            DicomTransferSyntax syntax) :
        dicom_(dicom),
        transferSyntax_(syntax)
      {
        if (dicom == NULL ||
            dicom_->getDataset() == NULL)
        {
          throw OrthancException(ErrorCode_NullPointer);
        }

        DcmDataset& dataset = *dicom_->getDataset();
        index_.reset(new DicomFrameIndex(dataset));

        sopClassUid_ = GetStringTag(dataset, DCM_SOPClassUID);
        sopInstanceUid_ = GetStringTag(dataset, DCM_SOPInstanceUID);
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

      virtual void WriteToMemoryBuffer(std::string& target) ORTHANC_OVERRIDE
      {
        assert(dicom_.get() != NULL);
        if (!FromDcmtkBridge::SaveToMemoryBuffer(target, *dicom_, transferSyntax_))
        {
          throw OrthancException(ErrorCode_InternalError,
                                 "Cannot write the DICOM instance to a memory buffer");
        }
      }

      virtual ImageAccessor* GetUncompressedFrame(unsigned int frame) ORTHANC_OVERRIDE
      {
        assert(dicom_.get() != NULL &&
               dicom_->getDataset() != NULL);
        return DicomImageDecoder::Decode(*dicom_->getDataset(), frame);
      }

      virtual void GetCompressedFrame(std::string& target,
                                      unsigned int frame) ORTHANC_OVERRIDE
      {
        assert(index_.get() != NULL);
        index_->GetRawFrame(target, frame);
      }
    };

    unsigned int lossyQuality_;

    static DicomTransferSyntax DetectTransferSyntax(DcmFileFormat& dicom)
    {
      if (dicom.getDataset() == NULL)
      {
        throw OrthancException(ErrorCode_InternalError);
      }
        
      DcmDataset& dataset = *dicom.getDataset();

      E_TransferSyntax xfer = dataset.getCurrentXfer();
      if (xfer == EXS_Unknown)
      {
        dataset.updateOriginalXfer();
        xfer = dataset.getCurrentXfer();
        if (xfer == EXS_Unknown)
        {
          throw OrthancException(ErrorCode_BadFileFormat,
                                 "Cannot determine the transfer syntax of the DICOM instance");
        }
      }

      DicomTransferSyntax syntax;
      if (FromDcmtkBridge::LookupOrthancTransferSyntax(syntax, xfer))
      {
        return syntax;
      }
      else
      {
        throw OrthancException(
          ErrorCode_BadFileFormat,
          "Unsupported transfer syntax: " + boost::lexical_cast<std::string>(xfer));
      }
    }


    static uint16_t GetBitsStored(DcmFileFormat& dicom)
    {
      if (dicom.getDataset() == NULL)
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      uint16_t bitsStored;
      if (dicom.getDataset()->findAndGetUint16(DCM_BitsStored, bitsStored).good())
      {
        return bitsStored;
      }
      else
      {
        throw OrthancException(ErrorCode_BadFileFormat,
                               "Missing \"Bits Stored\" tag in DICOM instance");
      }      
    }
    
      
  public:
    DcmtkImageReader() :
      lossyQuality_(90)
    {
    }

    void SetLossyQuality(unsigned int quality)
    {
      if (quality <= 0 ||
          quality > 100)
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
      else
      {
        lossyQuality_ = quality;
      }
    }

    unsigned int GetLossyQuality() const
    {
      return lossyQuality_;
    }

    virtual IParsedDicomImage* Read(const void* data,
                                    size_t size)
    {
      std::unique_ptr<DcmFileFormat> dicom(FromDcmtkBridge::LoadFromMemoryBuffer(data, size));
      if (dicom.get() == NULL)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }

      DicomTransferSyntax transferSyntax = DetectTransferSyntax(*dicom);

      return new Image(dicom.release(), transferSyntax);
    }

    virtual IParsedDicomImage* Transcode(const void* data,
                                         size_t size,
                                         DicomTransferSyntax syntax,
                                         bool allowNewSopInstanceUid)
    {
      std::unique_ptr<DcmFileFormat> dicom(FromDcmtkBridge::LoadFromMemoryBuffer(data, size));
      if (dicom.get() == NULL)
      {
        throw OrthancException(ErrorCode_BadFileFormat);
      }

      const uint16_t bitsStored = GetBitsStored(*dicom);

      if (syntax == DetectTransferSyntax(*dicom))
      {
        // No transcoding is needed
        return new Image(dicom.release(), syntax);
      }
      
      if (syntax == DicomTransferSyntax_LittleEndianImplicit &&
          FromDcmtkBridge::Transcode(*dicom, DicomTransferSyntax_LittleEndianImplicit, NULL))
      {
        return new Image(dicom.release(), syntax);
      }

      if (syntax == DicomTransferSyntax_LittleEndianExplicit &&
          FromDcmtkBridge::Transcode(*dicom, DicomTransferSyntax_LittleEndianExplicit, NULL))
      {
        return new Image(dicom.release(), syntax);
      }
      
      if (syntax == DicomTransferSyntax_BigEndianExplicit &&
          FromDcmtkBridge::Transcode(*dicom, DicomTransferSyntax_BigEndianExplicit, NULL))
      {
        return new Image(dicom.release(), syntax);
      }

      if (syntax == DicomTransferSyntax_DeflatedLittleEndianExplicit &&
          FromDcmtkBridge::Transcode(*dicom, DicomTransferSyntax_DeflatedLittleEndianExplicit, NULL))
      {
        return new Image(dicom.release(), syntax);
      }

#if ORTHANC_ENABLE_JPEG == 1
      if (syntax == DicomTransferSyntax_JPEGProcess1 &&
          allowNewSopInstanceUid &&
          bitsStored == 8)
      {
        DJ_RPLossy rpLossy(lossyQuality_);
        
        if (FromDcmtkBridge::Transcode(*dicom, DicomTransferSyntax_JPEGProcess1, &rpLossy))
        {
          return new Image(dicom.release(), syntax);
        }
      }
#endif
      
#if ORTHANC_ENABLE_JPEG == 1
      if (syntax == DicomTransferSyntax_JPEGProcess2_4 &&
          allowNewSopInstanceUid &&
          bitsStored <= 12)
      {
        DJ_RPLossy rpLossy(lossyQuality_);
        if (FromDcmtkBridge::Transcode(*dicom, DicomTransferSyntax_JPEGProcess2_4, &rpLossy))
        {
          return new Image(dicom.release(), syntax);
        }
      }
#endif

      //LOG(INFO) << "Unable to transcode DICOM image using the built-in reader";
      return NULL;
    }
  };
  

  
  class IDicomTranscoder1 : public boost::noncopyable
  {
  public:
    virtual ~IDicomTranscoder1()
    {
    }

    virtual DcmFileFormat& GetDicom() = 0;

    virtual DicomTransferSyntax GetTransferSyntax() = 0;

    virtual std::string GetSopClassUid() = 0;

    virtual std::string GetSopInstanceUid() = 0;

    virtual unsigned int GetFramesCount() = 0;

    virtual ImageAccessor* DecodeFrame(unsigned int frame) = 0;

    virtual void GetCompressedFrame(std::string& target,
                                    unsigned int frame) = 0;

    // NB: Transcoding can change the value of "GetSopInstanceUid()"
    // and "GetTransferSyntax()" if lossy compression is applied
    virtual bool Transcode(std::string& target,
                           DicomTransferSyntax syntax,
                           bool allowNewSopInstanceUid) = 0;

    virtual void WriteToMemoryBuffer(std::string& target) = 0;
  };


  class DcmtkTranscoder2 : public IDicomTranscoder1
  {
  private:
    std::unique_ptr<DcmFileFormat>    dicom_;
    std::unique_ptr<DicomFrameIndex>  index_;
    DicomTransferSyntax               transferSyntax_;
    std::string                       sopClassUid_;
    std::string                       sopInstanceUid_;
    uint16_t                          bitsStored_;
    unsigned int                      lossyQuality_;

    static std::string GetStringTag(DcmDataset& dataset,
                                    const DcmTagKey& tag)
    {
      const char* value = NULL;

      if (!dataset.findAndGetString(tag, value).good() ||
          value == NULL)
      {
        throw OrthancException(ErrorCode_BadFileFormat,
                               "Missing SOP class/instance UID in DICOM instance");
      }
      else
      {
        return std::string(value);
      }
    }

    void Setup(DcmFileFormat* dicom)
    {
      lossyQuality_ = 90;
      
      dicom_.reset(dicom);
      
      if (dicom == NULL ||
          dicom_->getDataset() == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }

      DcmDataset& dataset = *dicom_->getDataset();
      index_.reset(new DicomFrameIndex(dataset));

      E_TransferSyntax xfer = dataset.getCurrentXfer();
      if (xfer == EXS_Unknown)
      {
        dataset.updateOriginalXfer();
        xfer = dataset.getCurrentXfer();
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

      if (!dataset.findAndGetUint16(DCM_BitsStored, bitsStored_).good())
      {
        throw OrthancException(ErrorCode_BadFileFormat,
                               "Missing \"Bits Stored\" tag in DICOM instance");
      }      

      sopClassUid_ = GetStringTag(dataset, DCM_SOPClassUID);
      sopInstanceUid_ = GetStringTag(dataset, DCM_SOPInstanceUID);
    }
    
  public:
    DcmtkTranscoder2(DcmFileFormat* dicom)  // Takes ownership
    {
      Setup(dicom);
    }

    DcmtkTranscoder2(const void* dicom,
                    size_t size)
    {
      Setup(FromDcmtkBridge::LoadFromMemoryBuffer(dicom, size));
    }

    void SetLossyQuality(unsigned int quality)
    {
      if (quality <= 0 ||
          quality > 100)
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
      else
      {
        lossyQuality_ = quality;
      }
    }

    unsigned int GetLossyQuality() const
    {
      return lossyQuality_;
    }

    unsigned int GetBitsStored() const
    {
      return bitsStored_;
    }

    virtual DcmFileFormat& GetDicom()
    {
      assert(dicom_ != NULL);
      return *dicom_;
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

    virtual void WriteToMemoryBuffer(std::string& target) ORTHANC_OVERRIDE
    {
      if (!FromDcmtkBridge::SaveToMemoryBuffer(target, *dicom_))
      {
        throw OrthancException(ErrorCode_InternalError,
                               "Cannot write the DICOM instance to a memory buffer");
      }
    }

    virtual ImageAccessor* DecodeFrame(unsigned int frame) ORTHANC_OVERRIDE
    {
      assert(dicom_->getDataset() != NULL);
      return DicomImageDecoder::Decode(*dicom_->getDataset(), frame);
    }

    virtual void GetCompressedFrame(std::string& target,
                                    unsigned int frame) ORTHANC_OVERRIDE
    {
      index_->GetRawFrame(target, frame);
    }

    virtual bool Transcode(std::string& target,
                           DicomTransferSyntax syntax,
                           bool allowNewSopInstanceUid) ORTHANC_OVERRIDE
    {
      assert(dicom_ != NULL &&
             dicom_->getDataset() != NULL);
      
      if (syntax == GetTransferSyntax())
      {
        printf("NO TRANSCODING\n");
        
        // No change in the transfer syntax => simply serialize the current dataset
        WriteToMemoryBuffer(target);
        return true;
      }
      
      printf(">> %d\n", bitsStored_);

      if (syntax == DicomTransferSyntax_LittleEndianImplicit &&
          FromDcmtkBridge::Transcode(*dicom_, syntax, NULL) &&
          FromDcmtkBridge::SaveToMemoryBuffer(target, *dicom_, syntax))
      {
        transferSyntax_ = DicomTransferSyntax_LittleEndianImplicit;
        return true;
      }

      if (syntax == DicomTransferSyntax_LittleEndianExplicit &&
          FromDcmtkBridge::Transcode(*dicom_, syntax, NULL) &&
          FromDcmtkBridge::SaveToMemoryBuffer(target, *dicom_, syntax))
      {
        transferSyntax_ = DicomTransferSyntax_LittleEndianExplicit;
        return true;
      }
      
      if (syntax == DicomTransferSyntax_BigEndianExplicit &&
          FromDcmtkBridge::Transcode(*dicom_, syntax, NULL) &&
          FromDcmtkBridge::SaveToMemoryBuffer(target, *dicom_, syntax))
      {
        transferSyntax_ = DicomTransferSyntax_BigEndianExplicit;
        return true;
      }

      if (syntax == DicomTransferSyntax_DeflatedLittleEndianExplicit &&
          FromDcmtkBridge::Transcode(*dicom_, syntax, NULL) &&
          FromDcmtkBridge::SaveToMemoryBuffer(target, *dicom_, syntax))
      {
        transferSyntax_ = DicomTransferSyntax_DeflatedLittleEndianExplicit;
        return true;
      }

#if ORTHANC_ENABLE_JPEG == 1
      if (syntax == DicomTransferSyntax_JPEGProcess1 &&
          allowNewSopInstanceUid &&
          GetBitsStored() == 8)
      {
        DJ_RPLossy rpLossy(lossyQuality_);
        
        if (FromDcmtkBridge::Transcode(*dicom_, syntax, &rpLossy) &&
            FromDcmtkBridge::SaveToMemoryBuffer(target, *dicom_, syntax))
        {
          transferSyntax_ = DicomTransferSyntax_JPEGProcess1;
          sopInstanceUid_ = GetStringTag(*dicom_->getDataset(), DCM_SOPInstanceUID);
          return true;
        }
      }
#endif
      
#if ORTHANC_ENABLE_JPEG == 1
      if (syntax == DicomTransferSyntax_JPEGProcess2_4 &&
          allowNewSopInstanceUid &&
          GetBitsStored() <= 12)
      {
        DJ_RPLossy rpLossy(lossyQuality_);
        if (FromDcmtkBridge::Transcode(*dicom_, syntax, &rpLossy) &&
            FromDcmtkBridge::SaveToMemoryBuffer(target, *dicom_, syntax))
        {
          transferSyntax_ = DicomTransferSyntax_JPEGProcess2_4;
          sopInstanceUid_ = GetStringTag(*dicom_->getDataset(), DCM_SOPInstanceUID);
          return true;
        }
      }
#endif

      return false;
    }
  };
}




#include <boost/filesystem.hpp>


static void TestFile(const std::string& path)
{
  static unsigned int count = 0;
  count++;
  

  printf("** %s\n", path.c_str());

  std::string s;
  SystemToolbox::ReadFile(s, path);

  Orthanc::DcmtkTranscoder2 transcoder(s.c_str(), s.size());

  /*if (transcoder.GetBitsStored() != 8)  // TODO
    return; */

  {
    char buf[1024];
    sprintf(buf, "/tmp/source-%06d.dcm", count);
    printf(">> %s\n", buf);
    Orthanc::SystemToolbox::WriteFile(s, buf);
  }

  printf("[%s] [%s] [%s] %d %d\n", GetTransferSyntaxUid(transcoder.GetTransferSyntax()),
         transcoder.GetSopClassUid().c_str(), transcoder.GetSopInstanceUid().c_str(),
         transcoder.GetFramesCount(), transcoder.GetTransferSyntax());

  for (size_t i = 0; i < transcoder.GetFramesCount(); i++)
  {
    std::string f;
    transcoder.GetCompressedFrame(f, i);

    if (i == 0)
    {
      char buf[1024];
      sprintf(buf, "/tmp/frame-%06d.raw", count);
      printf(">> %s\n", buf);
      Orthanc::SystemToolbox::WriteFile(f, buf);
    }
  }

  {
    std::string t;
    transcoder.WriteToMemoryBuffer(t);

    Orthanc::DcmtkTranscoder2 transcoder2(t.c_str(), t.size());
    printf(">> %d %d ; %lu bytes\n", transcoder.GetTransferSyntax(), transcoder2.GetTransferSyntax(), t.size());
  }

  {
    std::string a = transcoder.GetSopInstanceUid();
    DicomTransferSyntax b = transcoder.GetTransferSyntax();
    
    DicomTransferSyntax syntax = DicomTransferSyntax_JPEGProcess2_4;
    //DicomTransferSyntax syntax = DicomTransferSyntax_LittleEndianExplicit;

    std::string t;
    bool ok = transcoder.Transcode(t, syntax, true);
    printf("Transcoding: %d\n", ok);

    if (ok)
    {
      printf("[%s] => [%s]\n", a.c_str(), transcoder.GetSopInstanceUid().c_str());
      printf("[%s] => [%s]\n", GetTransferSyntaxUid(b),
             GetTransferSyntaxUid(transcoder.GetTransferSyntax()));
      
      {
        char buf[1024];
        sprintf(buf, "/tmp/transcoded-%06d.dcm", count);
        printf(">> %s\n", buf);
        Orthanc::SystemToolbox::WriteFile(t, buf);
      }

      Orthanc::DcmtkTranscoder2 transcoder2(t.c_str(), t.size());
      printf("  => transcoded transfer syntax %d ; %lu bytes\n", transcoder2.GetTransferSyntax(), t.size());
    }
  }
  
  printf("\n");
}

TEST(Toto, DISABLED_Transcode)
{
  //OFLog::configure(OFLogger::DEBUG_LOG_LEVEL);

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
  }

  if (0)
  {
    TestFile("/home/jodogne/Subversion/orthanc-tests/Database/Multiframe.dcm");
    TestFile("/home/jodogne/Subversion/orthanc-tests/Database/Issue44/Monochrome1-Jpeg.dcm");
  }

  if (0)
  {
    TestFile("/home/jodogne/Subversion/orthanc-tests/Database/TransferSyntaxes/1.2.840.10008.1.2.1.dcm");
  }
}


TEST(Toto, DISABLED_Transcode2)
{
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

      DcmtkImageReader reader;

      {
        std::unique_ptr<IParsedDicomImage> image(
          reader.Read(source.c_str(), source.size()));
        ASSERT_TRUE(image.get() != NULL);
        ASSERT_EQ(a, image->GetTransferSyntax());

        std::string target;
        image->WriteToMemoryBuffer(target);
      }

      for (int j = 0; j <= DicomTransferSyntax_XML; j++)
      {
        DicomTransferSyntax b = (DicomTransferSyntax) j;
        //if (a == b) continue;

        std::unique_ptr<IParsedDicomImage> image(
          reader.Transcode(source.c_str(), source.size(), b, true));
        if (image.get() != NULL)
        {
          printf("[%s] -> [%s]\n", GetTransferSyntaxUid(a), GetTransferSyntaxUid(b));

          std::string target;
          image->WriteToMemoryBuffer(target);

          char buf[1024];
          sprintf(buf, "/tmp/%s-%s.dcm", GetTransferSyntaxUid(a), GetTransferSyntaxUid(b));
          
          SystemToolbox::WriteFile(target, buf);
        }
        else if (a != DicomTransferSyntax_JPEG2000 &&
                 a != DicomTransferSyntax_JPEG2000LosslessOnly)
        {
          ASSERT_TRUE(b != DicomTransferSyntax_LittleEndianImplicit &&
                      b != DicomTransferSyntax_LittleEndianExplicit &&
                      b != DicomTransferSyntax_BigEndianExplicit &&
                      b != DicomTransferSyntax_DeflatedLittleEndianExplicit);
        }
      }
    }
  }
}


#include "../Core/DicomNetworking/DicomAssociation.h"
#include "../Core/DicomNetworking/DicomControlUserConnection.h"
#include "../Core/DicomNetworking/DicomStoreUserConnection.h"

TEST(Toto, DISABLED_DicomAssociation)
{
  DicomAssociationParameters params;
  params.SetLocalApplicationEntityTitle("ORTHANC");
  params.SetRemoteApplicationEntityTitle("PACS");
  params.SetRemotePort(2001);

#if 0
  DicomAssociation assoc;
  assoc.ProposeGenericPresentationContext(UID_StorageCommitmentPushModelSOPClass);
  assoc.ProposeGenericPresentationContext(UID_VerificationSOPClass);
  assoc.ProposePresentationContext(UID_ComputedRadiographyImageStorage,
                                   DicomTransferSyntax_JPEGProcess1);
  assoc.ProposePresentationContext(UID_ComputedRadiographyImageStorage,
                                   DicomTransferSyntax_JPEGProcess2_4);
  assoc.ProposePresentationContext(UID_ComputedRadiographyImageStorage,
                                   DicomTransferSyntax_JPEG2000);
  
  assoc.Open(params);

  int presID = ASC_findAcceptedPresentationContextID(&assoc.GetDcmtkAssociation(), UID_ComputedRadiographyImageStorage);
  printf(">> %d\n", presID);
    
  std::map<DicomTransferSyntax, uint8_t> pc;
  printf(">> %d\n", assoc.LookupAcceptedPresentationContext(pc, UID_ComputedRadiographyImageStorage));
  
  for (std::map<DicomTransferSyntax, uint8_t>::const_iterator
         it = pc.begin(); it != pc.end(); ++it)
  {
    printf("[%s] => %d\n", GetTransferSyntaxUid(it->first), it->second);
  }
#else
  {
    DicomControlUserConnection assoc(params);

    try
    {
      printf(">> %d\n", assoc.Echo());
    }
    catch (OrthancException&)
    {
    }
  }
    
  params.SetRemoteApplicationEntityTitle("PACS");
  params.SetRemotePort(2000);

  {
    DicomControlUserConnection assoc(params);
    printf(">> %d\n", assoc.Echo());
  }

#endif
}

static void TestTranscode(DicomStoreUserConnection& scu,
                          const std::string& sopClassUid,
                          DicomTransferSyntax transferSyntax)
{
  std::set<DicomTransferSyntax> accepted;

  scu.LookupTranscoding(accepted, sopClassUid, transferSyntax);
  if (accepted.empty())
  {
    throw OrthancException(ErrorCode_NetworkProtocol,
                           "The SOP class is not supported by the remote modality");
  }

  {
    unsigned int count = 0;
    for (std::set<DicomTransferSyntax>::const_iterator
           it = accepted.begin(); it != accepted.end(); ++it)
    {
      LOG(INFO) << "available for transcoding " << (count++) << ": " << sopClassUid
                << " / " << GetTransferSyntaxUid(*it);
    }
  }
  
  if (accepted.find(transferSyntax) != accepted.end())
  {
    printf("**** OK, without transcoding !! [%s]\n", GetTransferSyntaxUid(transferSyntax));
  }
  else
  {
    // Transcoding - only in Orthanc >= 1.7.0

    const DicomTransferSyntax uncompressed[] = {
      DicomTransferSyntax_LittleEndianImplicit,  // Default transfer syntax
      DicomTransferSyntax_LittleEndianExplicit,
      DicomTransferSyntax_BigEndianExplicit
    };

    bool found = false;
    for (size_t i = 0; i < 3; i++)
    {
      if (accepted.find(uncompressed[i]) != accepted.end())
      {
        printf("**** TRANSCODING to %s\n", GetTransferSyntaxUid(uncompressed[i]));
        found = true;
        break;
      }
    }

    if (!found)
    {
      printf("**** KO KO KO\n");
    }
  }
}


TEST(Toto, DISABLED_Store)
{
  DicomAssociationParameters params;
  params.SetLocalApplicationEntityTitle("ORTHANC");
  params.SetRemoteApplicationEntityTitle("STORESCP");
  params.SetRemotePort(2000);

  DicomStoreUserConnection assoc(params);
  assoc.RegisterStorageClass(UID_MRImageStorage, DicomTransferSyntax_JPEGProcess1);
  assoc.RegisterStorageClass(UID_MRImageStorage, DicomTransferSyntax_JPEGProcess2_4);
  //assoc.RegisterStorageClass(UID_MRImageStorage, DicomTransferSyntax_LittleEndianExplicit);

  //assoc.SetUncompressedSyntaxesProposed(false);  // Necessary for transcoding
  assoc.SetCommonClassesProposed(false);
  assoc.SetRetiredBigEndianProposed(true);
  TestTranscode(assoc, UID_MRImageStorage, DicomTransferSyntax_LittleEndianExplicit);
  TestTranscode(assoc, UID_MRImageStorage, DicomTransferSyntax_JPEG2000);
  TestTranscode(assoc, UID_MRImageStorage, DicomTransferSyntax_JPEG2000);
}


TEST(Toto, DISABLED_Store2)
{
  DicomAssociationParameters params;
  params.SetLocalApplicationEntityTitle("ORTHANC");
  params.SetRemoteApplicationEntityTitle("STORESCP");
  params.SetRemotePort(2000);

  DicomStoreUserConnection assoc(params);
  //assoc.SetCommonClassesProposed(false);
  assoc.SetRetiredBigEndianProposed(true);

  std::string s;
  Orthanc::SystemToolbox::ReadFile(s, "/tmp/i/" + std::string(GetTransferSyntaxUid(DicomTransferSyntax_BigEndianExplicit)) +".dcm");

  std::string c, i;
  assoc.Store(c, i, s.c_str(), s.size());
  printf("[%s] [%s]\n", c.c_str(), i.c_str());
}


namespace Orthanc
{
  class IDicomTranscoder : public boost::noncopyable
  {
  public:
    virtual ~IDicomTranscoder()
    {
    }

    /**
     * Transcoding flavor that creates a new parsed DICOM file. A
     * "std::set<>" is used to give the possible plugin the
     * possibility to do a single parsing for all the possible
     * transfer syntaxes.
     **/
    virtual DcmFileFormat* Transcode(const void* buffer,
                                     size_t size,
                                     const std::set<DicomTransferSyntax>& allowedSyntaxes,
                                     bool allowNewSopInstanceUid) = 0;

    /**
     * In-place transcoding. This method is used first during
     * C-STORE. It can return "false" if inplace is not supported, in
     * which case the "Transcode()" method should be used.
     **/
    virtual bool InplaceTranscode(DcmFileFormat& dicom,
                                  const std::set<DicomTransferSyntax>& allowedSyntaxes,
                                  bool allowNewSopInstanceUid) = 0;

    /**
     * Important: Transcoding over the DICOM protocol is only
     * implemented towards uncompressed transfer syntaxes.
     **/
    static void Store(std::string& sopClassUid /* out */,
                      std::string& sopInstanceUid /* out */,
                      DicomStoreUserConnection& connection,
                      IDicomTranscoder& transcoder,
                      const void* buffer,
                      size_t size,
                      const std::string& moveOriginatorAET,
                      uint16_t moveOriginatorID)
    {
      std::unique_ptr<DcmFileFormat> dicom(FromDcmtkBridge::LoadFromMemoryBuffer(buffer, size));
      if (dicom.get() == NULL ||
          dicom->getDataset() == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }

      DicomTransferSyntax inputSyntax;
      connection.LookupParameters(sopClassUid, sopInstanceUid, inputSyntax, *dicom);

      std::set<DicomTransferSyntax> accepted;
      connection.LookupTranscoding(accepted, sopClassUid, inputSyntax);

      if (accepted.find(inputSyntax) != accepted.end())
      {
        // No need for transcoding
        connection.Store(sopClassUid, sopInstanceUid, *dicom, moveOriginatorAET, moveOriginatorID);
      }
      else
      {
        // Transcoding is needed
        std::set<DicomTransferSyntax> uncompressedSyntaxes;

        if (accepted.find(DicomTransferSyntax_LittleEndianImplicit) != accepted.end())
        {
          uncompressedSyntaxes.insert(DicomTransferSyntax_LittleEndianImplicit);
        }

        if (accepted.find(DicomTransferSyntax_LittleEndianExplicit) != accepted.end())
        {
          uncompressedSyntaxes.insert(DicomTransferSyntax_LittleEndianExplicit);
        }

        if (accepted.find(DicomTransferSyntax_BigEndianExplicit) != accepted.end())
        {
          uncompressedSyntaxes.insert(DicomTransferSyntax_BigEndianExplicit);
        }

        std::unique_ptr<DcmFileFormat> transcoded;

        if (transcoder.InplaceTranscode(*dicom, uncompressedSyntaxes, false))
        {
          // In-place transcoding is supported
          transcoded.reset(dicom.release());
        }
        else
        {
          transcoded.reset(transcoder.Transcode(buffer, size, uncompressedSyntaxes, false));
        }

        // WARNING: The "dicom" variable must not be used below this
        // point. The "sopInstanceUid" might also have changed (if
        // using lossy compression).
        
        if (transcoded == NULL ||
            transcoded->getDataset() == NULL)
        {
          throw OrthancException(
            ErrorCode_NotImplemented,
            "Cannot transcode from \"" + std::string(GetTransferSyntaxUid(inputSyntax)) +
            "\" to an uncompressed syntax for modality: " +
            connection.GetParameters().GetRemoteModality().GetApplicationEntityTitle());
        }
        else
        {
          DicomTransferSyntax transcodedSyntax;

          // Sanity check
          if (!FromDcmtkBridge::LookupOrthancTransferSyntax(transcodedSyntax, *transcoded) ||
              accepted.find(transcodedSyntax) == accepted.end())
          {
            throw OrthancException(ErrorCode_InternalError);
          }
          else
          {
            connection.Store(sopClassUid, sopInstanceUid, *transcoded, moveOriginatorAET, moveOriginatorID);
          }
        }
      }
    }

    static void Store(std::string& sopClassUid /* out */,
                      std::string& sopInstanceUid /* out */,
                      DicomStoreUserConnection& connection,
                      IDicomTranscoder& transcoder,
                      const void* buffer,
                      size_t size)
    {
      Store(sopClassUid, sopInstanceUid, connection, transcoder,
            buffer, size, "", 0 /* Not a C-MOVE */);
    }
  };


  class DcmtkTranscoder : public IDicomTranscoder
  {
  private:
    unsigned int  lossyQuality_;
    
    static uint16_t GetBitsStored(DcmDataset& dataset)
    {
      uint16_t bitsStored;
      if (dataset.findAndGetUint16(DCM_BitsStored, bitsStored).good())
      {
        return bitsStored;
      }
      else
      {
        throw OrthancException(ErrorCode_BadFileFormat,
                               "Missing \"Bits Stored\" tag in DICOM instance");
      }      
    }

  public:
    DcmtkTranscoder() :
      lossyQuality_(90)
    {
    }

    void SetLossyQuality(unsigned int quality)
    {
      if (quality <= 0 ||
          quality > 100)
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
      else
      {
        lossyQuality_ = quality;
      }
    }

    unsigned int GetLossyQuality() const
    {
      return lossyQuality_;
    }
    
    virtual DcmFileFormat* Transcode(const void* buffer,
                                     size_t size,
                                     const std::set<DicomTransferSyntax>& allowedSyntaxes,
                                     bool allowNewSopInstanceUid)
    {
      std::unique_ptr<DcmFileFormat> dicom(FromDcmtkBridge::LoadFromMemoryBuffer(buffer, size));

      if (dicom.get() == NULL)
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      if (InplaceTranscode(*dicom, allowedSyntaxes, allowNewSopInstanceUid))
      {
        return dicom.release();
      }
      else
      {
        return NULL;
      }
    }

    virtual bool InplaceTranscode(DcmFileFormat& dicom,
                                  const std::set<DicomTransferSyntax>& allowedSyntaxes,
                                  bool allowNewSopInstanceUid)
    {
      if (dicom.getDataset() == NULL)
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      DicomTransferSyntax syntax;
      if (!FromDcmtkBridge::LookupOrthancTransferSyntax(syntax, dicom))
      {
        throw OrthancException(ErrorCode_BadFileFormat,
                               "Cannot determine the transfer syntax");
      }

      const uint16_t bitsStored = GetBitsStored(*dicom.getDataset());

      if (allowedSyntaxes.find(syntax) != allowedSyntaxes.end())
      {
        // No transcoding is needed
        return true;
      }
      
      if (allowedSyntaxes.find(DicomTransferSyntax_LittleEndianImplicit) != allowedSyntaxes.end() &&
          FromDcmtkBridge::Transcode(dicom, DicomTransferSyntax_LittleEndianImplicit, NULL))
      {
        return true;
      }

      if (allowedSyntaxes.find(DicomTransferSyntax_LittleEndianExplicit) != allowedSyntaxes.end() &&
          FromDcmtkBridge::Transcode(dicom, DicomTransferSyntax_LittleEndianExplicit, NULL))
      {
        return true;
      }
      
      if (allowedSyntaxes.find(DicomTransferSyntax_BigEndianExplicit) != allowedSyntaxes.end() &&
          FromDcmtkBridge::Transcode(dicom, DicomTransferSyntax_BigEndianExplicit, NULL))
      {
        return true;
      }

      if (allowedSyntaxes.find(DicomTransferSyntax_DeflatedLittleEndianExplicit) != allowedSyntaxes.end() &&
          FromDcmtkBridge::Transcode(dicom, DicomTransferSyntax_DeflatedLittleEndianExplicit, NULL))
      {
        return true;
      }

#if ORTHANC_ENABLE_JPEG == 1
      if (allowedSyntaxes.find(DicomTransferSyntax_JPEGProcess1) != allowedSyntaxes.end() &&
          allowNewSopInstanceUid &&
          bitsStored == 8)
      {
        DJ_RPLossy rpLossy(lossyQuality_);
        
        if (FromDcmtkBridge::Transcode(dicom, DicomTransferSyntax_JPEGProcess1, &rpLossy))
        {
          return true;
        }
      }
#endif
      
#if ORTHANC_ENABLE_JPEG == 1
      if (allowedSyntaxes.find(DicomTransferSyntax_JPEGProcess2_4) != allowedSyntaxes.end() &&
          allowNewSopInstanceUid &&
          bitsStored <= 12)
      {
        DJ_RPLossy rpLossy(lossyQuality_);
        if (FromDcmtkBridge::Transcode(dicom, DicomTransferSyntax_JPEGProcess2_4, &rpLossy))
        {
          return true;
        }
      }
#endif

      return false;
    }
  };
}


TEST(Toto, DISABLED_Transcode3)
{
  DicomAssociationParameters p;
  p.SetRemotePort(2000);

  DcmtkTranscoder transcoder;
  
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

      DicomStoreUserConnection scu(p);
      scu.SetCommonClassesProposed(false);
      scu.SetRetiredBigEndianProposed(true);

      std::string c, i;
      IDicomTranscoder::Store(c, i, scu, transcoder, source.c_str(), source.size());
    }
  }
}


#endif
