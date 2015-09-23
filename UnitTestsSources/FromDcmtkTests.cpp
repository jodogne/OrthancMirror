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
#include "../Core/OrthancException.h"
#include "../Core/Images/ImageBuffer.h"
#include "../Core/Images/PngReader.h"
#include "../Core/Images/PngWriter.h"
#include "../Core/Uuid.h"
#include "../Resources/EncodingTests.h"

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
  o.Insert(privateTag, "private tag");
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

    int v = 0;
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
    std::cout << EnumerationToString(testEncodings[i]) << std::endl;
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
    std::cout << EnumerationToString(testEncodings[i]) << std::endl;
    std::string dicom;

    {
      ParsedDicomFile f;
      f.SetEncoding(testEncodings[i]);
      f.Insert(DICOM_TAG_PATIENT_NAME, testEncodingsEncoded[i]);
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
