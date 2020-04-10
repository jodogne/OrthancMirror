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
#include <dcmtk/dcmjpeg/djrploss.h>   // for DJ_RPLossy
#include <dcmtk/dcmjpeg/djrplol.h>    // for DJ_RPLossless


namespace Orthanc
{
  class IDicomTranscoder : public boost::noncopyable
  {
  public:
    virtual ~IDicomTranscoder()
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
                           std::set<DicomTransferSyntax> syntaxes,
                           bool allowNewSopInstanceUid) = 0;

    virtual void WriteToMemoryBuffer(std::string& target) = 0;
  };


  class DcmtkTranscoder : public IDicomTranscoder
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

      if (!dataset.findAndGetUint16(DCM_BitsStored, bitsStored_).good())
      {
        throw OrthancException(ErrorCode_BadFileFormat,
                               "Missing \"Bits Stored\" tag in DICOM instance");
      }      

      sopClassUid_ = GetStringTag(dataset, DCM_SOPClassUID);
      sopInstanceUid_ = GetStringTag(dataset, DCM_SOPInstanceUID);
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
                           std::set<DicomTransferSyntax> syntaxes,
                           bool allowNewSopInstanceUid) ORTHANC_OVERRIDE
    {
      assert(dicom_ != NULL &&
             dicom_->getDataset() != NULL);
      
      if (syntaxes.find(GetTransferSyntax()) != syntaxes.end())
      {
        printf("NO TRANSCODING\n");
        
        // No change in the transfer syntax => simply serialize the current dataset
        WriteToMemoryBuffer(target);
        return true;
      }
      
      printf(">> %d\n", bitsStored_);

      DJ_RPLossy rpLossy(lossyQuality_);

      if (syntaxes.find(DicomTransferSyntax_LittleEndianImplicit) != syntaxes.end() &&
          FromDcmtkBridge::Transcode(target, *dicom_, DicomTransferSyntax_LittleEndianImplicit, NULL))
      {
        transferSyntax_ = DicomTransferSyntax_LittleEndianImplicit;
        return true;
      }
      else if (syntaxes.find(DicomTransferSyntax_LittleEndianExplicit) != syntaxes.end() &&
               FromDcmtkBridge::Transcode(target, *dicom_, DicomTransferSyntax_LittleEndianExplicit, NULL))
      {
        transferSyntax_ = DicomTransferSyntax_LittleEndianExplicit;
        return true;
      }
      else if (syntaxes.find(DicomTransferSyntax_BigEndianExplicit) != syntaxes.end() &&
               FromDcmtkBridge::Transcode(target, *dicom_, DicomTransferSyntax_BigEndianExplicit, NULL))
      {
        transferSyntax_ = DicomTransferSyntax_BigEndianExplicit;
        return true;
      }
      else if (syntaxes.find(DicomTransferSyntax_JPEGProcess1) != syntaxes.end() &&
               allowNewSopInstanceUid &&
               GetBitsStored() == 8 &&
               FromDcmtkBridge::Transcode(target, *dicom_, DicomTransferSyntax_JPEGProcess1, &rpLossy))
      {
        transferSyntax_ = DicomTransferSyntax_JPEGProcess1;
        sopInstanceUid_ = GetStringTag(*dicom_->getDataset(), DCM_SOPInstanceUID);
        return true;
      }
      else if (syntaxes.find(DicomTransferSyntax_JPEGProcess2_4) != syntaxes.end() &&
               allowNewSopInstanceUid &&
               GetBitsStored() <= 12 &&
               FromDcmtkBridge::Transcode(target, *dicom_, DicomTransferSyntax_JPEGProcess2_4, &rpLossy))
      {
        transferSyntax_ = DicomTransferSyntax_JPEGProcess2_4;
        sopInstanceUid_ = GetStringTag(*dicom_->getDataset(), DCM_SOPInstanceUID);
        return true;
      }
      else
      {
        return false;
      }
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


#include <boost/filesystem.hpp>


static void TestFile(const std::string& path)
{
  static unsigned int count = 0;
  count++;
  

  printf("** %s\n", path.c_str());

  std::string s;
  SystemToolbox::ReadFile(s, path);

  Orthanc::DcmtkTranscoder transcoder(s.c_str(), s.size());

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

    Orthanc::DcmtkTranscoder transcoder2(t.c_str(), t.size());
    printf(">> %d %d ; %lu bytes\n", transcoder.GetTransferSyntax(), transcoder2.GetTransferSyntax(), t.size());
  }

  {
    std::string a = transcoder.GetSopInstanceUid();
    DicomTransferSyntax b = transcoder.GetTransferSyntax();
    
    std::set<DicomTransferSyntax> syntaxes;
    syntaxes.insert(DicomTransferSyntax_JPEGProcess2_4);
    //syntaxes.insert(DicomTransferSyntax_LittleEndianExplicit);

    std::string t;
    bool ok = transcoder.Transcode(t, syntaxes, true);
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

      Orthanc::DcmtkTranscoder transcoder2(t.c_str(), t.size());
      printf("  => transcoded transfer syntax %d ; %lu bytes\n", transcoder2.GetTransferSyntax(), t.size());
    }
  }
  
  printf("\n");
}

TEST(Toto, DISABLED_Transcode)
{
  //OFLog::configure(OFLogger::DEBUG_LOG_LEVEL);

  if (0)
  {
    std::string s;
    //SystemToolbox::ReadFile(s, "/home/jodogne/Subversion/orthanc-tests/Database/TransferSyntaxes/1.2.840.10008.1.2.4.50.dcm");
    //SystemToolbox::ReadFile(s, "/home/jodogne/DICOM/Alain.dcm");
    //SystemToolbox::ReadFile(s, "/home/jodogne/Subversion/orthanc-tests/Database/Brainix/Epi/IM-0001-0002.dcm");
    SystemToolbox::ReadFile(s, "/home/jodogne/Subversion/orthanc-tests/Database/TransferSyntaxes/1.2.840.10008.1.2.1.dcm");

    std::unique_ptr<DcmFileFormat> dicom(FromDcmtkBridge::LoadFromMemoryBuffer(s.c_str(), s.size()));

    // less /home/jodogne/Downloads/dcmtk-3.6.4/dcmdata/include/dcmtk/dcmdata/dcxfer.h
    printf(">> %d\n", dicom->getDataset()->getOriginalXfer());  // => 4 == EXS_JPEGProcess1

    const DcmRepresentationParameter *p;

#if 0
    E_TransferSyntax target = EXS_LittleEndianExplicit;
    p = NULL;
#elif 0
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



#ifdef _WIN32
/**
 * "The maximum length, in bytes, of the string returned in the buffer 
 * pointed to by the name parameter is dependent on the namespace provider,
 * but this string must be 256 bytes or less.
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms738527(v=vs.85).aspx
 **/
#  define HOST_NAME_MAX 256
#  include <winsock.h>
#endif 


#if !defined(HOST_NAME_MAX) && defined(_POSIX_HOST_NAME_MAX)
/**
 * TO IMPROVE: "_POSIX_HOST_NAME_MAX is only the minimum value that
 * HOST_NAME_MAX can ever have [...] Therefore you cannot allocate an
 * array of size _POSIX_HOST_NAME_MAX, invoke gethostname() and expect
 * that the result will fit."
 * http://lists.gnu.org/archive/html/bug-gnulib/2009-08/msg00128.html
 **/
#define HOST_NAME_MAX _POSIX_HOST_NAME_MAX
#endif


#include "../Core/DicomNetworking/RemoteModalityParameters.h"


#include <dcmtk/dcmnet/diutil.h>  // For dcmConnectionTimeout()



namespace Orthanc
{
  // By default, the timeout for client DICOM connections is set to 10 seconds
  static boost::mutex  defaultTimeoutMutex_;
  static uint32_t defaultTimeout_ = 10;


  class DicomAssociationParameters
  {
  private:
    std::string           localAet_;
    std::string           remoteAet_;
    std::string           remoteHost_;
    uint16_t              remotePort_;
    ModalityManufacturer  manufacturer_;
    uint32_t              timeout_;

    void ReadDefaultTimeout()
    {
      boost::mutex::scoped_lock lock(defaultTimeoutMutex_);
      timeout_ = defaultTimeout_;
    }

  public:
    DicomAssociationParameters() :
      localAet_("STORESCU"),
      remoteAet_("ANY-SCP"),
      remoteHost_("127.0.0.1"),
      remotePort_(104),
      manufacturer_(ModalityManufacturer_Generic)
    {
      ReadDefaultTimeout();
    }
    
    DicomAssociationParameters(const std::string& localAet,
                               const RemoteModalityParameters& remote) :
      localAet_(localAet),
      remoteAet_(remote.GetApplicationEntityTitle()),
      remoteHost_(remote.GetHost()),
      remotePort_(remote.GetPortNumber()),
      manufacturer_(remote.GetManufacturer()),
      timeout_(defaultTimeout_)
    {
      ReadDefaultTimeout();
    }
    
    const std::string& GetLocalApplicationEntityTitle() const
    {
      return localAet_;
    }

    const std::string& GetRemoteApplicationEntityTitle() const
    {
      return remoteAet_;
    }

    const std::string& GetRemoteHost() const
    {
      return remoteHost_;
    }

    uint16_t GetRemotePort() const
    {
      return remotePort_;
    }

    ModalityManufacturer GetRemoteManufacturer() const
    {
      return manufacturer_;
    }

    void SetLocalApplicationEntityTitle(const std::string& aet)
    {
      localAet_ = aet;
    }

    void SetRemoteApplicationEntityTitle(const std::string& aet)
    {
      remoteAet_ = aet;
    }

    void SetRemoteHost(const std::string& host)
    {
      if (host.size() > HOST_NAME_MAX - 10)
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange,
                               "Invalid host name (too long): " + host);
      }

      remoteHost_ = host;
    }

    void SetRemotePort(uint16_t port)
    {
      remotePort_ = port;
    }

    void SetRemoteManufacturer(ModalityManufacturer manufacturer)
    {
      manufacturer_ = manufacturer;
    }

    void SetRemoteModality(const RemoteModalityParameters& parameters)
    {
      SetRemoteApplicationEntityTitle(parameters.GetApplicationEntityTitle());
      SetRemoteHost(parameters.GetHost());
      SetRemotePort(parameters.GetPortNumber());
      SetRemoteManufacturer(parameters.GetManufacturer());
    }

    bool IsEqual(const DicomAssociationParameters& other) const
    {
      return (localAet_ == other.localAet_ &&
              remoteAet_ == other.remoteAet_ &&
              remoteHost_ == other.remoteHost_ &&
              remotePort_ == other.remotePort_ &&
              manufacturer_ == other.manufacturer_);
    }

    void SetTimeout(uint32_t seconds)
    {
      timeout_ = seconds;
    }

    uint32_t GetTimeout() const
    {
      return timeout_;
    }

    bool HasTimeout() const
    {
      return timeout_ != 0;
    }
    
    static void SetDefaultTimeout(uint32_t seconds)
    {
      LOG(INFO) << "Default timeout for DICOM connections if Orthanc acts as SCU (client): " 
                << seconds << " seconds (0 = no timeout)";

      {
        boost::mutex::scoped_lock lock(defaultTimeoutMutex_);
        defaultTimeout_ = seconds;
      }
    }

    void CheckCondition(const OFCondition& cond,
                        const std::string& command) const
    {
      if (cond.bad())
      {
        // Reformat the error message from DCMTK by turning multiline
        // errors into a single line
      
        std::string s(cond.text());
        std::string info;
        info.reserve(s.size());

        bool isMultiline = false;
        for (size_t i = 0; i < s.size(); i++)
        {
          if (s[i] == '\r')
          {
            // Ignore
          }
          else if (s[i] == '\n')
          {
            if (isMultiline)
            {
              info += "; ";
            }
            else
            {
              info += " (";
              isMultiline = true;
            }
          }
          else
          {
            info.push_back(s[i]);
          }
        }

        if (isMultiline)
        {
          info += ")";
        }

        throw OrthancException(ErrorCode_NetworkProtocol,
                               "DicomUserConnection - " + command + " to AET \"" +
                               GetRemoteApplicationEntityTitle() + "\": " + info);
      }
    }
  };
  

  static void FillSopSequence(DcmDataset& dataset,
                              const DcmTagKey& tag,
                              const std::vector<std::string>& sopClassUids,
                              const std::vector<std::string>& sopInstanceUids,
                              const std::vector<StorageCommitmentFailureReason>& failureReasons,
                              bool hasFailureReasons)
  {
    assert(sopClassUids.size() == sopInstanceUids.size() &&
           (hasFailureReasons ?
            failureReasons.size() == sopClassUids.size() :
            failureReasons.empty()));

    if (sopInstanceUids.empty())
    {
      // Add an empty sequence
      if (!dataset.insertEmptyElement(tag).good())
      {
        throw OrthancException(ErrorCode_InternalError);
      }
    }
    else
    {
      for (size_t i = 0; i < sopClassUids.size(); i++)
      {
        std::unique_ptr<DcmItem> item(new DcmItem);
        if (!item->putAndInsertString(DCM_ReferencedSOPClassUID, sopClassUids[i].c_str()).good() ||
            !item->putAndInsertString(DCM_ReferencedSOPInstanceUID, sopInstanceUids[i].c_str()).good() ||
            (hasFailureReasons &&
             !item->putAndInsertUint16(DCM_FailureReason, failureReasons[i]).good()) ||
            !dataset.insertSequenceItem(tag, item.release()).good())
        {
          throw OrthancException(ErrorCode_InternalError);
        }
      }
    }
  }                              


  class DicomAssociation : public boost::noncopyable
  {
  private:
    // This is the maximum number of presentation context IDs (the
    // number of odd integers between 1 and 255)
    // http://dicom.nema.org/medical/dicom/2019e/output/chtml/part08/sect_9.3.2.2.html
    static const size_t MAX_PROPOSED_PRESENTATIONS = 128;
    
    struct ProposedPresentationContext
    {
      std::string                    sopClassUid_;
      std::set<DicomTransferSyntax>  transferSyntaxes_;
    };

    typedef std::map<std::string, std::map<DicomTransferSyntax, uint8_t> >  AcceptedPresentationContexts;

    DicomAssociationRole                      role_;
    bool                                      isOpen_;
    std::vector<ProposedPresentationContext>  proposed_;
    AcceptedPresentationContexts              accepted_;
    T_ASC_Network*                            net_;
    T_ASC_Parameters*                         params_;
    T_ASC_Association*                        assoc_;

    void Initialize()
    {
      role_ = DicomAssociationRole_Default;
      isOpen_ = false;
      net_ = NULL; 
      params_ = NULL;
      assoc_ = NULL;      

      // Must be after "isOpen_ = false"
      ClearPresentationContexts();
    }

    void CheckConnecting(const DicomAssociationParameters& parameters,
                         const OFCondition& cond)
    {
      try
      {
        parameters.CheckCondition(cond, "connecting");
      }
      catch (OrthancException&)
      {
        CloseInternal();
        throw;
      }
    }
    
    void CloseInternal()
    {
      if (assoc_ != NULL)
      {
        ASC_releaseAssociation(assoc_);
        ASC_destroyAssociation(&assoc_);
        assoc_ = NULL;
        params_ = NULL;
      }
      else
      {
        if (params_ != NULL)
        {
          ASC_destroyAssociationParameters(&params_);
          params_ = NULL;
        }
      }

      if (net_ != NULL)
      {
        ASC_dropNetwork(&net_);
        net_ = NULL;
      }

      accepted_.clear();
      isOpen_ = false;
    }

    void AddAccepted(const std::string& sopClassUid,
                     DicomTransferSyntax syntax,
                     uint8_t presentationContextId)
    {
      AcceptedPresentationContexts::iterator found = accepted_.find(sopClassUid);

      if (found == accepted_.end())
      {
        std::map<DicomTransferSyntax, uint8_t> syntaxes;
        syntaxes[syntax] = presentationContextId;
        accepted_[sopClassUid] = syntaxes;
      }      
      else
      {
        if (found->second.find(syntax) != found->second.end())
        {
          LOG(WARNING) << "The same transfer syntax ("
                       << GetTransferSyntaxUid(syntax)
                       << ") was accepted twice for the same SOP class UID ("
                       << sopClassUid << ")";
        }
        else
        {
          found->second[syntax] = presentationContextId;
        }
      }
    }

  public:
    DicomAssociation()
    {
      Initialize();
    }

    ~DicomAssociation()
    {
      try
      {
        Close();
      }
      catch (OrthancException&)
      {
        // Don't throw exception in destructors
      }
    }

    bool IsOpen() const
    {
      return isOpen_;
    }

    void SetRole(DicomAssociationRole role)
    {
      if (role_ != role)
      {
        Close();
        role_ = role;
      }
    }

    void ClearPresentationContexts()
    {
      Close();
      proposed_.clear();
      proposed_.reserve(MAX_PROPOSED_PRESENTATIONS);
    }

    void Open(const DicomAssociationParameters& parameters)
    {
      if (isOpen_)
      {
        return;  // Already open
      }
      
      // Timeout used during association negociation and ASC_releaseAssociation()
      uint32_t acseTimeout = parameters.GetTimeout();
      if (acseTimeout == 0)
      {
        /**
         * Timeout is disabled. Global timeout (seconds) for
         * connecting to remote hosts.  Default value is -1 which
         * selects infinite timeout, i.e. blocking connect().
         **/
        dcmConnectionTimeout.set(-1);
        acseTimeout = 10;
      }
      else
      {
        dcmConnectionTimeout.set(acseTimeout);
      }
      
      T_ASC_SC_ROLE dcmtkRole;
      switch (role_)
      {
        case DicomAssociationRole_Default:
          dcmtkRole = ASC_SC_ROLE_DEFAULT;
          break;

        case DicomAssociationRole_Scu:
          dcmtkRole = ASC_SC_ROLE_SCU;
          break;

        case DicomAssociationRole_Scp:
          dcmtkRole = ASC_SC_ROLE_SCP;
          break;

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }

      assert(net_ == NULL &&
             params_ == NULL &&
             assoc_ == NULL);

      if (proposed_.empty())
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls,
                               "No presentation context was proposed");
      }

      LOG(INFO) << "Opening a DICOM SCU connection from AET \""
                << parameters.GetLocalApplicationEntityTitle() 
                << "\" to AET \"" << parameters.GetRemoteApplicationEntityTitle()
                << "\" on host " << parameters.GetRemoteHost()
                << ":" << parameters.GetRemotePort() 
                << " (manufacturer: " << EnumerationToString(parameters.GetRemoteManufacturer()) << ")";

      CheckConnecting(parameters, ASC_initializeNetwork(NET_REQUESTOR, 0, /*opt_acse_timeout*/ acseTimeout, &net_));
      CheckConnecting(parameters, ASC_createAssociationParameters(&params_, /*opt_maxReceivePDULength*/ ASC_DEFAULTMAXPDU));

      // Set this application's title and the called application's title in the params
      CheckConnecting(parameters, ASC_setAPTitles(
                        params_, parameters.GetLocalApplicationEntityTitle().c_str(),
                        parameters.GetRemoteApplicationEntityTitle().c_str(), NULL));

      // Set the network addresses of the local and remote entities
      char localHost[HOST_NAME_MAX];
      gethostname(localHost, HOST_NAME_MAX - 1);

      char remoteHostAndPort[HOST_NAME_MAX];

#ifdef _MSC_VER
      _snprintf
#else
        snprintf
#endif
        (remoteHostAndPort, HOST_NAME_MAX - 1, "%s:%d",
         parameters.GetRemoteHost().c_str(), parameters.GetRemotePort());

      CheckConnecting(parameters, ASC_setPresentationAddresses(params_, localHost, remoteHostAndPort));

      // Set various options
      CheckConnecting(parameters, ASC_setTransportLayerType(params_, /*opt_secureConnection*/ false));

      // Setup the list of proposed presentation contexts
      unsigned int presentationContextId = 1;
      for (size_t i = 0; i < proposed_.size(); i++)
      {
        assert(presentationContextId <= 255);
        const char* sopClassUid = proposed_[i].sopClassUid_.c_str();

        const std::set<DicomTransferSyntax>& source = proposed_[i].transferSyntaxes_;
          
        std::vector<const char*> transferSyntaxes;
        transferSyntaxes.reserve(source.size());
          
        for (std::set<DicomTransferSyntax>::const_iterator
               it = source.begin(); it != source.end(); ++it)
        {
          transferSyntaxes.push_back(GetTransferSyntaxUid(*it));
        }

        assert(!transferSyntaxes.empty());
        CheckConnecting(parameters, ASC_addPresentationContext(
                          params_, presentationContextId, sopClassUid,
                          &transferSyntaxes[0], transferSyntaxes.size(), dcmtkRole));

        presentationContextId += 2;
      }

      // Do the association
      CheckConnecting(parameters, ASC_requestAssociation(net_, params_, &assoc_));
      isOpen_ = true;

      // Inspect the accepted transfer syntaxes
      LST_HEAD **l = &params_->DULparams.acceptedPresentationContext;
      if (*l != NULL)
      {
        DUL_PRESENTATIONCONTEXT* pc = (DUL_PRESENTATIONCONTEXT*) LST_Head(l);
        LST_Position(l, (LST_NODE*)pc);
        while (pc)
        {
          if (pc->result == ASC_P_ACCEPTANCE)
          {
            DicomTransferSyntax transferSyntax;
            if (LookupTransferSyntax(transferSyntax, pc->acceptedTransferSyntax))
            {
              AddAccepted(pc->abstractSyntax, transferSyntax, pc->presentationContextID);
            }
            else
            {
              LOG(WARNING) << "Unknown transfer syntax received from AET \""
                           << parameters.GetRemoteApplicationEntityTitle()
                           << "\": " << pc->acceptedTransferSyntax;
            }
          }
            
          pc = (DUL_PRESENTATIONCONTEXT*) LST_Next(l);
        }
      }

      if (accepted_.empty())
      {
        throw OrthancException(ErrorCode_NoPresentationContext,
                               "Unable to negotiate a presentation context with AET \"" +
                               parameters.GetRemoteApplicationEntityTitle() + "\"");
      }
    }

    void Close()
    {
      if (isOpen_)
      {
        CloseInternal();
      }
    }

    bool LookupAcceptedPresentationContext(std::map<DicomTransferSyntax, uint8_t>& target,
                                           const std::string& sopClassUid) const
    {
      if (!IsOpen())
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls, "Connection not opened");
      }
      
      AcceptedPresentationContexts::const_iterator found = accepted_.find(sopClassUid);

      if (found == accepted_.end())
      {
        return false;
      }
      else
      {
        target = found->second;
        return true;
      }
    }

    void ProposeGenericPresentationContext(const std::string& sopClassUid)
    {
      std::set<DicomTransferSyntax> ts;
      ts.insert(DicomTransferSyntax_LittleEndianImplicit);
      ts.insert(DicomTransferSyntax_LittleEndianExplicit);
      ts.insert(DicomTransferSyntax_BigEndianExplicit);
      ProposePresentationContext(sopClassUid, ts);
    }

    void ProposePresentationContext(const std::string& sopClassUid,
                                    DicomTransferSyntax transferSyntax)
    {
      std::set<DicomTransferSyntax> ts;
      ts.insert(transferSyntax);
      ProposePresentationContext(sopClassUid, ts);
    }

    size_t GetRemainingPropositions() const
    {
      assert(proposed_.size() <= MAX_PROPOSED_PRESENTATIONS);
      return MAX_PROPOSED_PRESENTATIONS - proposed_.size();
    }

    void ProposePresentationContext(const std::string& sopClassUid,
                                    const std::set<DicomTransferSyntax>& transferSyntaxes)
    {
      if (transferSyntaxes.empty())
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange,
                               "No transfer syntax provided");
      }
      
      if (proposed_.size() >= MAX_PROPOSED_PRESENTATIONS)
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange,
                               "Too many proposed presentation contexts");
      }
      
      if (IsOpen())
      {
        Close();
      }

      ProposedPresentationContext context;
      context.sopClassUid_ = sopClassUid;
      context.transferSyntaxes_ = transferSyntaxes;

      proposed_.push_back(context);
    }

    T_ASC_Association& GetDcmtkAssociation() const
    {
      if (isOpen_)
      {
        assert(assoc_ != NULL);
        return *assoc_;
      }
      else
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls,
                               "The connection is not open");
      }
    }

    T_ASC_Network& GetDcmtkNetwork() const
    {
      if (isOpen_)
      {
        assert(net_ != NULL);
        return *net_;
      }
      else
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls,
                               "The connection is not open");
      }
    }


    static void ReportStorageCommitment(const DicomAssociationParameters& parameters,
                                        const std::string& transactionUid,
                                        const std::vector<std::string>& sopClassUids,
                                        const std::vector<std::string>& sopInstanceUids,
                                        const std::vector<StorageCommitmentFailureReason>& failureReasons)
    {
      if (sopClassUids.size() != sopInstanceUids.size() ||
          sopClassUids.size() != failureReasons.size())
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange);
      }
    

      std::vector<std::string> successSopClassUids, successSopInstanceUids, failedSopClassUids, failedSopInstanceUids;
      std::vector<StorageCommitmentFailureReason> failedReasons;

      successSopClassUids.reserve(sopClassUids.size());
      successSopInstanceUids.reserve(sopClassUids.size());
      failedSopClassUids.reserve(sopClassUids.size());
      failedSopInstanceUids.reserve(sopClassUids.size());
      failedReasons.reserve(sopClassUids.size());

      for (size_t i = 0; i < sopClassUids.size(); i++)
      {
        switch (failureReasons[i])
        {
          case StorageCommitmentFailureReason_Success:
            successSopClassUids.push_back(sopClassUids[i]);
            successSopInstanceUids.push_back(sopInstanceUids[i]);
            break;

          case StorageCommitmentFailureReason_ProcessingFailure:
          case StorageCommitmentFailureReason_NoSuchObjectInstance:
          case StorageCommitmentFailureReason_ResourceLimitation:
          case StorageCommitmentFailureReason_ReferencedSOPClassNotSupported:
          case StorageCommitmentFailureReason_ClassInstanceConflict:
          case StorageCommitmentFailureReason_DuplicateTransactionUID:
            failedSopClassUids.push_back(sopClassUids[i]);
            failedSopInstanceUids.push_back(sopInstanceUids[i]);
            failedReasons.push_back(failureReasons[i]);
            break;

          default:
          {
            char buf[16];
            sprintf(buf, "%04xH", failureReasons[i]);
            throw OrthancException(ErrorCode_ParameterOutOfRange,
                                   "Unsupported failure reason for storage commitment: " + std::string(buf));
          }
        }
      }
    
      DicomAssociation association;

      {
        std::set<DicomTransferSyntax> transferSyntaxes;
        transferSyntaxes.insert(DicomTransferSyntax_LittleEndianExplicit);
        transferSyntaxes.insert(DicomTransferSyntax_LittleEndianImplicit);

        association.SetRole(DicomAssociationRole_Scp);
        association.ProposePresentationContext(UID_StorageCommitmentPushModelSOPClass,
                                               transferSyntaxes);
      }
      
      association.Open(parameters);

      /**
       * N-EVENT-REPORT
       * http://dicom.nema.org/medical/dicom/2019a/output/chtml/part04/sect_J.3.3.html
       * http://dicom.nema.org/medical/dicom/2019a/output/chtml/part07/chapter_10.html#table_10.1-1
       *
       * Status code:
       * http://dicom.nema.org/medical/dicom/2019a/output/chtml/part07/chapter_10.html#sect_10.1.1.1.8
       **/

      /**
       * Send the "EVENT_REPORT_RQ" request
       **/

      LOG(INFO) << "Reporting modality \""
                << parameters.GetRemoteApplicationEntityTitle()
                << "\" about storage commitment transaction: " << transactionUid
                << " (" << successSopClassUids.size() << " successes, " 
                << failedSopClassUids.size() << " failures)";
      const DIC_US messageId = association.GetDcmtkAssociation().nextMsgID++;
      
      {
        T_DIMSE_Message message;
        memset(&message, 0, sizeof(message));
        message.CommandField = DIMSE_N_EVENT_REPORT_RQ;

        T_DIMSE_N_EventReportRQ& content = message.msg.NEventReportRQ;
        content.MessageID = messageId;
        strncpy(content.AffectedSOPClassUID, UID_StorageCommitmentPushModelSOPClass, DIC_UI_LEN);
        strncpy(content.AffectedSOPInstanceUID, UID_StorageCommitmentPushModelSOPInstance, DIC_UI_LEN);
        content.DataSetType = DIMSE_DATASET_PRESENT;

        DcmDataset dataset;
        if (!dataset.putAndInsertString(DCM_TransactionUID, transactionUid.c_str()).good())
        {
          throw OrthancException(ErrorCode_InternalError);
        }

        {
          std::vector<StorageCommitmentFailureReason> empty;
          FillSopSequence(dataset, DCM_ReferencedSOPSequence, successSopClassUids,
                          successSopInstanceUids, empty, false);
        }

        // http://dicom.nema.org/medical/dicom/2019a/output/chtml/part04/sect_J.3.3.html
        if (failedSopClassUids.empty())
        {
          content.EventTypeID = 1;  // "Storage Commitment Request Successful"
        }
        else
        {
          content.EventTypeID = 2;  // "Storage Commitment Request Complete - Failures Exist"

          // Failure reason
          // http://dicom.nema.org/medical/dicom/2019a/output/chtml/part03/sect_C.14.html#sect_C.14.1.1
          FillSopSequence(dataset, DCM_FailedSOPSequence, failedSopClassUids,
                          failedSopInstanceUids, failedReasons, true);
        }

        int presID = ASC_findAcceptedPresentationContextID(
          &association.GetDcmtkAssociation(), UID_StorageCommitmentPushModelSOPClass);
        if (presID == 0)
        {
          throw OrthancException(ErrorCode_NetworkProtocol, "Storage commitment - "
                                 "Unable to send N-EVENT-REPORT request to AET: " +
                                 parameters.GetRemoteApplicationEntityTitle());
        }

        if (!DIMSE_sendMessageUsingMemoryData(
              &association.GetDcmtkAssociation(), presID, &message, NULL /* status detail */,
              &dataset, NULL /* callback */, NULL /* callback context */,
              NULL /* commandSet */).good())
        {
          throw OrthancException(ErrorCode_NetworkProtocol);
        }
      }

      /**
       * Read the "EVENT_REPORT_RSP" response
       **/

      {
        T_ASC_PresentationContextID presID = 0;
        T_DIMSE_Message message;

        if (!DIMSE_receiveCommand(&association.GetDcmtkAssociation(),
                                  (parameters.HasTimeout() ? DIMSE_NONBLOCKING : DIMSE_BLOCKING),
                                  parameters.GetTimeout(), &presID, &message,
                                  NULL /* no statusDetail */).good() ||
            message.CommandField != DIMSE_N_EVENT_REPORT_RSP)
        {
          throw OrthancException(ErrorCode_NetworkProtocol, "Storage commitment - "
                                 "Unable to read N-EVENT-REPORT response from AET: " +
                                 parameters.GetRemoteApplicationEntityTitle());
        }

        const T_DIMSE_N_EventReportRSP& content = message.msg.NEventReportRSP;
        if (content.MessageIDBeingRespondedTo != messageId ||
            !(content.opts & O_NEVENTREPORT_AFFECTEDSOPCLASSUID) ||
            !(content.opts & O_NEVENTREPORT_AFFECTEDSOPINSTANCEUID) ||
            //(content.opts & O_NEVENTREPORT_EVENTTYPEID) ||  // Pedantic test - The "content.EventTypeID" is not used by Orthanc
            std::string(content.AffectedSOPClassUID) != UID_StorageCommitmentPushModelSOPClass ||
            std::string(content.AffectedSOPInstanceUID) != UID_StorageCommitmentPushModelSOPInstance ||
            content.DataSetType != DIMSE_DATASET_NULL)
        {
          throw OrthancException(ErrorCode_NetworkProtocol, "Storage commitment - "
                                 "Badly formatted N-EVENT-REPORT response from AET: " +
                                 parameters.GetRemoteApplicationEntityTitle());
        }

        if (content.DimseStatus != 0 /* success */)
        {
          throw OrthancException(ErrorCode_NetworkProtocol, "Storage commitment - "
                                 "The request cannot be handled by remote AET: " +
                                 parameters.GetRemoteApplicationEntityTitle());
        }
      }

      association.Close();
    }
      
    static void RequestStorageCommitment(const DicomAssociationParameters& parameters,
                                         const std::string& transactionUid,
                                         const std::vector<std::string>& sopClassUids,
                                         const std::vector<std::string>& sopInstanceUids)
    {
      if (sopClassUids.size() != sopInstanceUids.size())
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange);
      }

      for (size_t i = 0; i < sopClassUids.size(); i++)
      {
        if (sopClassUids[i].empty() ||
            sopInstanceUids[i].empty())
        {
          throw OrthancException(ErrorCode_ParameterOutOfRange,
                                 "The SOP class/instance UIDs cannot be empty, found: \"" +
                                 sopClassUids[i] + "\" / \"" + sopInstanceUids[i] + "\"");
        }
      }

      if (transactionUid.size() < 5 ||
          transactionUid.substr(0, 5) != "2.25.")
      {
        throw OrthancException(ErrorCode_ParameterOutOfRange);
      }

      DicomAssociation association;

      {
        std::set<DicomTransferSyntax> transferSyntaxes;
        transferSyntaxes.insert(DicomTransferSyntax_LittleEndianExplicit);
        transferSyntaxes.insert(DicomTransferSyntax_LittleEndianImplicit);
      
        association.SetRole(DicomAssociationRole_Default);
        association.ProposePresentationContext(UID_StorageCommitmentPushModelSOPClass,
                                               transferSyntaxes);
      }
      
      association.Open(parameters);
      
      /**
       * N-ACTION
       * http://dicom.nema.org/medical/dicom/2019a/output/chtml/part04/sect_J.3.2.html
       * http://dicom.nema.org/medical/dicom/2019a/output/chtml/part07/chapter_10.html#table_10.1-4
       *
       * Status code:
       * http://dicom.nema.org/medical/dicom/2019a/output/chtml/part07/chapter_10.html#sect_10.1.1.1.8
       **/

      /**
       * Send the "N_ACTION_RQ" request
       **/

      LOG(INFO) << "Request to modality \""
                << parameters.GetRemoteApplicationEntityTitle()
                << "\" about storage commitment for " << sopClassUids.size()
                << " instances, with transaction UID: " << transactionUid;
      const DIC_US messageId = association.GetDcmtkAssociation().nextMsgID++;
      
      {
        T_DIMSE_Message message;
        memset(&message, 0, sizeof(message));
        message.CommandField = DIMSE_N_ACTION_RQ;

        T_DIMSE_N_ActionRQ& content = message.msg.NActionRQ;
        content.MessageID = messageId;
        strncpy(content.RequestedSOPClassUID, UID_StorageCommitmentPushModelSOPClass, DIC_UI_LEN);
        strncpy(content.RequestedSOPInstanceUID, UID_StorageCommitmentPushModelSOPInstance, DIC_UI_LEN);
        content.ActionTypeID = 1;  // "Request Storage Commitment"
        content.DataSetType = DIMSE_DATASET_PRESENT;

        DcmDataset dataset;
        if (!dataset.putAndInsertString(DCM_TransactionUID, transactionUid.c_str()).good())
        {
          throw OrthancException(ErrorCode_InternalError);
        }

        {
          std::vector<StorageCommitmentFailureReason> empty;
          FillSopSequence(dataset, DCM_ReferencedSOPSequence, sopClassUids, sopInstanceUids, empty, false);
        }
          
        int presID = ASC_findAcceptedPresentationContextID(
          &association.GetDcmtkAssociation(), UID_StorageCommitmentPushModelSOPClass);
        if (presID == 0)
        {
          throw OrthancException(ErrorCode_NetworkProtocol, "Storage commitment - "
                                 "Unable to send N-ACTION request to AET: " +
                                 parameters.GetRemoteApplicationEntityTitle());
        }

        if (!DIMSE_sendMessageUsingMemoryData(
              &association.GetDcmtkAssociation(), presID, &message, NULL /* status detail */,
              &dataset, NULL /* callback */, NULL /* callback context */,
              NULL /* commandSet */).good())
        {
          throw OrthancException(ErrorCode_NetworkProtocol);
        }
      }

      /**
       * Read the "N_ACTION_RSP" response
       **/

      {
        T_ASC_PresentationContextID presID = 0;
        T_DIMSE_Message message;
        
        if (!DIMSE_receiveCommand(&association.GetDcmtkAssociation(),
                                  (parameters.HasTimeout() ? DIMSE_NONBLOCKING : DIMSE_BLOCKING),
                                  parameters.GetTimeout(), &presID, &message,
                                  NULL /* no statusDetail */).good() ||
            message.CommandField != DIMSE_N_ACTION_RSP)
        {
          throw OrthancException(ErrorCode_NetworkProtocol, "Storage commitment - "
                                 "Unable to read N-ACTION response from AET: " +
                                 parameters.GetRemoteApplicationEntityTitle());
        }

        const T_DIMSE_N_ActionRSP& content = message.msg.NActionRSP;
        if (content.MessageIDBeingRespondedTo != messageId ||
            !(content.opts & O_NACTION_AFFECTEDSOPCLASSUID) ||
            !(content.opts & O_NACTION_AFFECTEDSOPINSTANCEUID) ||
            //(content.opts & O_NACTION_ACTIONTYPEID) ||  // Pedantic test - The "content.ActionTypeID" is not used by Orthanc
            std::string(content.AffectedSOPClassUID) != UID_StorageCommitmentPushModelSOPClass ||
            std::string(content.AffectedSOPInstanceUID) != UID_StorageCommitmentPushModelSOPInstance ||
            content.DataSetType != DIMSE_DATASET_NULL)
        {
          throw OrthancException(ErrorCode_NetworkProtocol, "Storage commitment - "
                                 "Badly formatted N-ACTION response from AET: " +
                                 parameters.GetRemoteApplicationEntityTitle());
        }

        if (content.DimseStatus != 0 /* success */)
        {
          throw OrthancException(ErrorCode_NetworkProtocol, "Storage commitment - "
                                 "The request cannot be handled by remote AET: " +
                                 parameters.GetRemoteApplicationEntityTitle());
        }
      }

      association.Close();
    }
  };



  static void TestAndCopyTag(DicomMap& result,
                             const DicomMap& source,
                             const DicomTag& tag)
  {
    if (!source.HasTag(tag))
    {
      throw OrthancException(ErrorCode_BadRequest);
    }
    else
    {
      result.SetValue(tag, source.GetValue(tag));
    }
  }


  namespace
  {
    struct FindPayload
    {
      DicomFindAnswers* answers;
      const char*       level;
      bool              isWorklist;
    };
  }


  static void FindCallback(
    /* in */
    void *callbackData,
    T_DIMSE_C_FindRQ *request,      /* original find request */
    int responseCount,
    T_DIMSE_C_FindRSP *response,    /* pending response received */
    DcmDataset *responseIdentifiers /* pending response identifiers */
    )
  {
    FindPayload& payload = *reinterpret_cast<FindPayload*>(callbackData);

    if (responseIdentifiers != NULL)
    {
      if (payload.isWorklist)
      {
        ParsedDicomFile answer(*responseIdentifiers);
        payload.answers->Add(answer);
      }
      else
      {
        DicomMap m;
        FromDcmtkBridge::ExtractDicomSummary(m, *responseIdentifiers);
        
        if (!m.HasTag(DICOM_TAG_QUERY_RETRIEVE_LEVEL))
        {
          m.SetValue(DICOM_TAG_QUERY_RETRIEVE_LEVEL, payload.level, false);
        }

        payload.answers->Add(m);
      }
    }
  }


  static void NormalizeFindQuery(DicomMap& fixedQuery,
                                 ResourceType level,
                                 const DicomMap& fields)
  {
    std::set<DicomTag> allowedTags;

    // WARNING: Do not add "break" or reorder items in this switch-case!
    switch (level)
    {
      case ResourceType_Instance:
        DicomTag::AddTagsForModule(allowedTags, DicomModule_Instance);

      case ResourceType_Series:
        DicomTag::AddTagsForModule(allowedTags, DicomModule_Series);

      case ResourceType_Study:
        DicomTag::AddTagsForModule(allowedTags, DicomModule_Study);

      case ResourceType_Patient:
        DicomTag::AddTagsForModule(allowedTags, DicomModule_Patient);
        break;

      default:
        throw OrthancException(ErrorCode_InternalError);
    }

    switch (level)
    {
      case ResourceType_Patient:
        allowedTags.insert(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_STUDIES);
        allowedTags.insert(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_SERIES);
        allowedTags.insert(DICOM_TAG_NUMBER_OF_PATIENT_RELATED_INSTANCES);
        break;

      case ResourceType_Study:
        allowedTags.insert(DICOM_TAG_MODALITIES_IN_STUDY);
        allowedTags.insert(DICOM_TAG_NUMBER_OF_STUDY_RELATED_SERIES);
        allowedTags.insert(DICOM_TAG_NUMBER_OF_STUDY_RELATED_INSTANCES);
        allowedTags.insert(DICOM_TAG_SOP_CLASSES_IN_STUDY);
        break;

      case ResourceType_Series:
        allowedTags.insert(DICOM_TAG_NUMBER_OF_SERIES_RELATED_INSTANCES);
        break;

      default:
        break;
    }

    allowedTags.insert(DICOM_TAG_SPECIFIC_CHARACTER_SET);

    DicomArray query(fields);
    for (size_t i = 0; i < query.GetSize(); i++)
    {
      const DicomTag& tag = query.GetElement(i).GetTag();
      if (allowedTags.find(tag) == allowedTags.end())
      {
        LOG(WARNING) << "Tag not allowed for this C-Find level, will be ignored: " << tag;
      }
      else
      {
        fixedQuery.SetValue(tag, query.GetElement(i).GetValue());
      }
    }
  }



  static ParsedDicomFile* ConvertQueryFields(const DicomMap& fields,
                                             ModalityManufacturer manufacturer)
  {
    // Fix outgoing C-Find requests issue for Syngo.Via and its
    // solution was reported by Emsy Chan by private mail on
    // 2015-06-17. According to Robert van Ommen (2015-11-30), the
    // same fix is required for Agfa Impax. This was generalized for
    // generic manufacturer since it seems to affect PhilipsADW,
    // GEWAServer as well:
    // https://bitbucket.org/sjodogne/orthanc/issues/31/

    switch (manufacturer)
    {
      case ModalityManufacturer_GenericNoWildcardInDates:
      case ModalityManufacturer_GenericNoUniversalWildcard:
      {
        std::unique_ptr<DicomMap> fix(fields.Clone());

        std::set<DicomTag> tags;
        fix->GetTags(tags);

        for (std::set<DicomTag>::const_iterator it = tags.begin(); it != tags.end(); ++it)
        {
          // Replace a "*" wildcard query by an empty query ("") for
          // "date" or "all" value representations depending on the
          // type of manufacturer.
          if (manufacturer == ModalityManufacturer_GenericNoUniversalWildcard ||
              (manufacturer == ModalityManufacturer_GenericNoWildcardInDates &&
               FromDcmtkBridge::LookupValueRepresentation(*it) == ValueRepresentation_Date))
          {
            const DicomValue* value = fix->TestAndGetValue(*it);

            if (value != NULL && 
                !value->IsNull() &&
                value->GetContent() == "*")
            {
              fix->SetValue(*it, "", false);
            }
          }
        }

        return new ParsedDicomFile(*fix, GetDefaultDicomEncoding(), false /* be strict */);
      }

      default:
        return new ParsedDicomFile(fields, GetDefaultDicomEncoding(), false /* be strict */);
    }
  }



  class DicomControlUserConnection : public boost::noncopyable
  {
  private:
    DicomAssociationParameters  parameters_;
    DicomAssociation            association_;

    void SetupPresentationContexts()
    {
      association_.ProposeGenericPresentationContext(UID_VerificationSOPClass);
      association_.ProposeGenericPresentationContext(UID_FINDPatientRootQueryRetrieveInformationModel);
      association_.ProposeGenericPresentationContext(UID_FINDStudyRootQueryRetrieveInformationModel);
      association_.ProposeGenericPresentationContext(UID_MOVEStudyRootQueryRetrieveInformationModel);
      association_.ProposeGenericPresentationContext(UID_FINDModalityWorklistInformationModel);
    }

    void FindInternal(DicomFindAnswers& answers,
                      DcmDataset* dataset,
                      const char* sopClass,
                      bool isWorklist,
                      const char* level)
    {
      assert(isWorklist ^ (level != NULL));

      association_.Open(parameters_);

      FindPayload payload;
      payload.answers = &answers;
      payload.level = level;
      payload.isWorklist = isWorklist;

      // Figure out which of the accepted presentation contexts should be used
      int presID = ASC_findAcceptedPresentationContextID(
        &association_.GetDcmtkAssociation(), sopClass);
      if (presID == 0)
      {
        throw OrthancException(ErrorCode_DicomFindUnavailable,
                               "Remote AET is " + parameters_.GetRemoteApplicationEntityTitle());
      }

      T_DIMSE_C_FindRQ request;
      memset(&request, 0, sizeof(request));
      request.MessageID = association_.GetDcmtkAssociation().nextMsgID++;
      strncpy(request.AffectedSOPClassUID, sopClass, DIC_UI_LEN);
      request.Priority = DIMSE_PRIORITY_MEDIUM;
      request.DataSetType = DIMSE_DATASET_PRESENT;

      T_DIMSE_C_FindRSP response;
      DcmDataset* statusDetail = NULL;

#if DCMTK_VERSION_NUMBER >= 364
      int responseCount;
#endif

      OFCondition cond = DIMSE_findUser(
        &association_.GetDcmtkAssociation(), presID, &request, dataset,
#if DCMTK_VERSION_NUMBER >= 364
        responseCount,
#endif
        FindCallback, &payload,
        /*opt_blockMode*/ (parameters_.HasTimeout() ? DIMSE_NONBLOCKING : DIMSE_BLOCKING),
        /*opt_dimse_timeout*/ parameters_.GetTimeout(),
        &response, &statusDetail);
    
      if (statusDetail)
      {
        delete statusDetail;
      }

      parameters_.CheckCondition(cond, "C-FIND");

    
      /**
       * New in Orthanc 1.6.0: Deal with failures during C-FIND.
       * http://dicom.nema.org/medical/dicom/current/output/chtml/part04/sect_C.4.html#table_C.4-1
       **/
    
      if (response.DimseStatus != 0x0000 &&  // Success
          response.DimseStatus != 0xFF00 &&  // Pending - Matches are continuing 
          response.DimseStatus != 0xFF01)    // Pending - Matches are continuing 
      {
        char buf[16];
        sprintf(buf, "%04X", response.DimseStatus);

        if (response.DimseStatus == STATUS_FIND_Failed_UnableToProcess)
        {
          throw OrthancException(ErrorCode_NetworkProtocol,
                                 HttpStatus_422_UnprocessableEntity,
                                 "C-FIND SCU to AET \"" +
                                 parameters_.GetRemoteApplicationEntityTitle() +
                                 "\" has failed with DIMSE status 0x" + buf +
                                 " (unable to process - invalid query ?)");
        }
        else
        {
          throw OrthancException(ErrorCode_NetworkProtocol, "C-FIND SCU to AET \"" +
                                 parameters_.GetRemoteApplicationEntityTitle() +
                                 "\" has failed with DIMSE status 0x" + buf);
        }
      }
    }

    void MoveInternal(const std::string& targetAet,
                      ResourceType level,
                      const DicomMap& fields)
    {
      association_.Open(parameters_);

      std::unique_ptr<ParsedDicomFile> query(
        ConvertQueryFields(fields, parameters_.GetRemoteManufacturer()));
      DcmDataset* dataset = query->GetDcmtkObject().getDataset();

      const char* sopClass = UID_MOVEStudyRootQueryRetrieveInformationModel;
      switch (level)
      {
        case ResourceType_Patient:
          DU_putStringDOElement(dataset, DCM_QueryRetrieveLevel, "PATIENT");
          break;

        case ResourceType_Study:
          DU_putStringDOElement(dataset, DCM_QueryRetrieveLevel, "STUDY");
          break;

        case ResourceType_Series:
          DU_putStringDOElement(dataset, DCM_QueryRetrieveLevel, "SERIES");
          break;

        case ResourceType_Instance:
          DU_putStringDOElement(dataset, DCM_QueryRetrieveLevel, "IMAGE");
          break;

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }

      // Figure out which of the accepted presentation contexts should be used
      int presID = ASC_findAcceptedPresentationContextID(&association_.GetDcmtkAssociation(), sopClass);
      if (presID == 0)
      {
        throw OrthancException(ErrorCode_DicomMoveUnavailable,
                               "Remote AET is " + parameters_.GetRemoteApplicationEntityTitle());
      }

      T_DIMSE_C_MoveRQ request;
      memset(&request, 0, sizeof(request));
      request.MessageID = association_.GetDcmtkAssociation().nextMsgID++;
      strncpy(request.AffectedSOPClassUID, sopClass, DIC_UI_LEN);
      request.Priority = DIMSE_PRIORITY_MEDIUM;
      request.DataSetType = DIMSE_DATASET_PRESENT;
      strncpy(request.MoveDestination, targetAet.c_str(), DIC_AE_LEN);

      T_DIMSE_C_MoveRSP response;
      DcmDataset* statusDetail = NULL;
      DcmDataset* responseIdentifiers = NULL;
      OFCondition cond = DIMSE_moveUser(
        &association_.GetDcmtkAssociation(), presID, &request, dataset, NULL, NULL,
        /*opt_blockMode*/ (parameters_.HasTimeout() ? DIMSE_NONBLOCKING : DIMSE_BLOCKING),
        /*opt_dimse_timeout*/ parameters_.GetTimeout(),
        &association_.GetDcmtkNetwork(), NULL, NULL,
        &response, &statusDetail, &responseIdentifiers);

      if (statusDetail)
      {
        delete statusDetail;
      }

      if (responseIdentifiers)
      {
        delete responseIdentifiers;
      }

      parameters_.CheckCondition(cond, "C-MOVE");

    
      /**
       * New in Orthanc 1.6.0: Deal with failures during C-MOVE.
       * http://dicom.nema.org/medical/dicom/current/output/chtml/part04/sect_C.4.2.html#table_C.4-2
       **/
    
      if (response.DimseStatus != 0x0000 &&  // Success
          response.DimseStatus != 0xFF00)    // Pending - Sub-operations are continuing
      {
        char buf[16];
        sprintf(buf, "%04X", response.DimseStatus);

        if (response.DimseStatus == STATUS_MOVE_Failed_UnableToProcess)
        {
          throw OrthancException(ErrorCode_NetworkProtocol,
                                 HttpStatus_422_UnprocessableEntity,
                                 "C-MOVE SCU to AET \"" +
                                 parameters_.GetRemoteApplicationEntityTitle() +
                                 "\" has failed with DIMSE status 0x" + buf +
                                 " (unable to process - resource not found ?)");
        }
        else
        {
          throw OrthancException(ErrorCode_NetworkProtocol, "C-MOVE SCU to AET \"" +
                                 parameters_.GetRemoteApplicationEntityTitle() +
                                 "\" has failed with DIMSE status 0x" + buf);
        }
      }
    }
    
  public:
    DicomControlUserConnection()
    {
      SetupPresentationContexts();
    }
    
    DicomControlUserConnection(const DicomAssociationParameters& params) :
      parameters_(params)
    {
      SetupPresentationContexts();
    }
    
    void SetParameters(const DicomAssociationParameters& params)
    {
      if (!parameters_.IsEqual(params))
      {
        association_.Close();
        parameters_ = params;
      }
    }

    const DicomAssociationParameters& GetParameters() const
    {
      return parameters_;
    }

    bool Echo()
    {
      association_.Open(parameters_);

      DIC_US status;
      parameters_.CheckCondition(
        DIMSE_echoUser(&association_.GetDcmtkAssociation(),
                       association_.GetDcmtkAssociation().nextMsgID++, 
                       /*opt_blockMode*/ (parameters_.HasTimeout() ? DIMSE_NONBLOCKING : DIMSE_BLOCKING),
                       /*opt_dimse_timeout*/ parameters_.GetTimeout(),
                       &status, NULL),
        "C-ECHO");
      
      return status == STATUS_Success;
    }


    void Find(DicomFindAnswers& result,
              ResourceType level,
              const DicomMap& originalFields,
              bool normalize)
    {
      std::unique_ptr<ParsedDicomFile> query;

      if (normalize)
      {
        DicomMap fields;
        NormalizeFindQuery(fields, level, originalFields);
        query.reset(ConvertQueryFields(fields, parameters_.GetRemoteManufacturer()));
      }
      else
      {
        query.reset(new ParsedDicomFile(originalFields,
                                        GetDefaultDicomEncoding(),
                                        false /* be strict */));
      }
    
      DcmDataset* dataset = query->GetDcmtkObject().getDataset();

      const char* clevel = NULL;
      const char* sopClass = NULL;

      switch (level)
      {
        case ResourceType_Patient:
          clevel = "PATIENT";
          DU_putStringDOElement(dataset, DCM_QueryRetrieveLevel, "PATIENT");
          sopClass = UID_FINDPatientRootQueryRetrieveInformationModel;
          break;

        case ResourceType_Study:
          clevel = "STUDY";
          DU_putStringDOElement(dataset, DCM_QueryRetrieveLevel, "STUDY");
          sopClass = UID_FINDStudyRootQueryRetrieveInformationModel;
          break;

        case ResourceType_Series:
          clevel = "SERIES";
          DU_putStringDOElement(dataset, DCM_QueryRetrieveLevel, "SERIES");
          sopClass = UID_FINDStudyRootQueryRetrieveInformationModel;
          break;

        case ResourceType_Instance:
          clevel = "IMAGE";
          DU_putStringDOElement(dataset, DCM_QueryRetrieveLevel, "IMAGE");
          sopClass = UID_FINDStudyRootQueryRetrieveInformationModel;
          break;

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }


      const char* universal;
      if (parameters_.GetRemoteManufacturer() == ModalityManufacturer_GE)
      {
        universal = "*";
      }
      else
      {
        universal = "";
      }      
    

      // Add the expected tags for this query level.
      // WARNING: Do not reorder or add "break" in this switch-case!
      switch (level)
      {
        case ResourceType_Instance:
          if (!dataset->tagExists(DCM_SOPInstanceUID))
          {
            DU_putStringDOElement(dataset, DCM_SOPInstanceUID, universal);
          }

        case ResourceType_Series:
          if (!dataset->tagExists(DCM_SeriesInstanceUID))
          {
            DU_putStringDOElement(dataset, DCM_SeriesInstanceUID, universal);
          }

        case ResourceType_Study:
          if (!dataset->tagExists(DCM_AccessionNumber))
          {
            DU_putStringDOElement(dataset, DCM_AccessionNumber, universal);
          }

          if (!dataset->tagExists(DCM_StudyInstanceUID))
          {
            DU_putStringDOElement(dataset, DCM_StudyInstanceUID, universal);
          }

        case ResourceType_Patient:
          if (!dataset->tagExists(DCM_PatientID))
          {
            DU_putStringDOElement(dataset, DCM_PatientID, universal);
          }
        
          break;

        default:
          throw OrthancException(ErrorCode_ParameterOutOfRange);
      }

      assert(clevel != NULL && sopClass != NULL);
      FindInternal(result, dataset, sopClass, false, clevel);
    }
    

    void Move(const std::string& targetAet,
              ResourceType level,
              const DicomMap& findResult)
    {
      DicomMap move;
      switch (level)
      {
        case ResourceType_Patient:
          TestAndCopyTag(move, findResult, DICOM_TAG_PATIENT_ID);
          break;

        case ResourceType_Study:
          TestAndCopyTag(move, findResult, DICOM_TAG_STUDY_INSTANCE_UID);
          break;

        case ResourceType_Series:
          TestAndCopyTag(move, findResult, DICOM_TAG_STUDY_INSTANCE_UID);
          TestAndCopyTag(move, findResult, DICOM_TAG_SERIES_INSTANCE_UID);
          break;

        case ResourceType_Instance:
          TestAndCopyTag(move, findResult, DICOM_TAG_STUDY_INSTANCE_UID);
          TestAndCopyTag(move, findResult, DICOM_TAG_SERIES_INSTANCE_UID);
          TestAndCopyTag(move, findResult, DICOM_TAG_SOP_INSTANCE_UID);
          break;

        default:
          throw OrthancException(ErrorCode_InternalError);
      }

      MoveInternal(targetAet, level, move);
    }


    void Move(const std::string& targetAet,
              const DicomMap& findResult)
    {
      if (!findResult.HasTag(DICOM_TAG_QUERY_RETRIEVE_LEVEL))
      {
        throw OrthancException(ErrorCode_InternalError);
      }

      const std::string tmp = findResult.GetValue(DICOM_TAG_QUERY_RETRIEVE_LEVEL).GetContent();
      ResourceType level = StringToResourceType(tmp.c_str());

      Move(targetAet, level, findResult);
    }


    void MovePatient(const std::string& targetAet,
                     const std::string& patientId)
    {
      DicomMap query;
      query.SetValue(DICOM_TAG_PATIENT_ID, patientId, false);
      MoveInternal(targetAet, ResourceType_Patient, query);
    }

    void MoveStudy(const std::string& targetAet,
                   const std::string& studyUid)
    {
      DicomMap query;
      query.SetValue(DICOM_TAG_STUDY_INSTANCE_UID, studyUid, false);
      MoveInternal(targetAet, ResourceType_Study, query);
    }

    void MoveSeries(const std::string& targetAet,
                    const std::string& studyUid,
                    const std::string& seriesUid)
    {
      DicomMap query;
      query.SetValue(DICOM_TAG_STUDY_INSTANCE_UID, studyUid, false);
      query.SetValue(DICOM_TAG_SERIES_INSTANCE_UID, seriesUid, false);
      MoveInternal(targetAet, ResourceType_Series, query);
    }

    void MoveInstance(const std::string& targetAet,
                      const std::string& studyUid,
                      const std::string& seriesUid,
                      const std::string& instanceUid)
    {
      DicomMap query;
      query.SetValue(DICOM_TAG_STUDY_INSTANCE_UID, studyUid, false);
      query.SetValue(DICOM_TAG_SERIES_INSTANCE_UID, seriesUid, false);
      query.SetValue(DICOM_TAG_SOP_INSTANCE_UID, instanceUid, false);
      MoveInternal(targetAet, ResourceType_Instance, query);
    }


    void FindWorklist(DicomFindAnswers& result,
                      ParsedDicomFile& query)
    {
      DcmDataset* dataset = query.GetDcmtkObject().getDataset();
      const char* sopClass = UID_FINDModalityWorklistInformationModel;

      FindInternal(result, dataset, sopClass, true, NULL);
    }
  };


  class DicomStorageUserConnection : public boost::noncopyable
  {
  private:
    std::unique_ptr<DicomAssociation>  association_;

  public:
  };
}


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
  DicomControlUserConnection assoc(params);

  try
  {
    printf(">> %d\n", assoc.Echo());
  }
  catch (OrthancException&)
  {
  }

  params.SetRemoteApplicationEntityTitle("PACS");
  params.SetRemotePort(2000);
  assoc.SetParameters(params);
  printf(">> %d\n", assoc.Echo());

#endif
}


#endif
