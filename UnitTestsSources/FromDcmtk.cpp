#include "gtest/gtest.h"

#include "../OrthancServer/FromDcmtkBridge.h"
#include "../OrthancServer/OrthancInitialization.h"
#include "../OrthancServer/DicomModification.h"
#include "../Core/OrthancException.h"
#include "../Core/ImageFormats/ImageBuffer.h"
#include "../Core/ImageFormats/PngReader.h"
#include "../Core/ImageFormats/PngWriter.h"

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


#include <dcmtk/dcmdata/dcuid.h>

TEST(DicomModification, Png)
{
  // Red dot in http://en.wikipedia.org/wiki/Data_URI_scheme (RGBA image)
  std::string s = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==";

  std::string m, c;
  Toolbox::DecodeDataUriScheme(m, c, s);

  ASSERT_EQ("image/png", m);
  ASSERT_EQ(116, c.size());

  std::string cc;
  Toolbox::DecodeBase64(cc, c);
  PngReader reader;
  reader.ReadFromMemory(cc);

  ASSERT_EQ(5, reader.GetHeight());
  ASSERT_EQ(5, reader.GetWidth());
  ASSERT_EQ(PixelFormat_RGBA32, reader.GetFormat());

  ParsedDicomFile o;
  o.EmbedImage(s);
  o.SaveToFile("UnitTestsResults/png1.dcm");

  // Red dot, without alpha channel
  s = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAIAAAACDbGyAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH3gUGDTcIn2+8BgAAACJJREFUCNdj/P//PwMjIwME/P/P+J8BBTAxEOL/R9Lx/z8AynoKAXOeiV8AAAAASUVORK5CYII=";
  o.EmbedImage(s);
  o.SaveToFile("UnitTestsResults/png2.dcm");

  // Check box in Graylevel8
  s = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAAAAAA6mKC9AAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH3gUGDDcB53FulQAAAElJREFUGNNtj0sSAEEEQ1+U+185s1CtmRkblQ9CZldsKHJDk6DLGLJa6chjh0ooQmpjXMM86zPwydGEj6Ed/UGykkEM8X+p3u8/8LcOJIWLGeMAAAAASUVORK5CYII=";
  o.EmbedImage(s);
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
