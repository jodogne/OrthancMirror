/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include <gtest/gtest.h>

#include "../Sources/Compatibility.h"
#include "../Sources/DicomFormat/DicomImageInformation.h"
#include "../Sources/Images/Image.h"
#include "../Sources/Images/ImageProcessing.h"
#include "../Sources/Images/ImageTraits.h"
#include "../Sources/OrthancException.h"

#include <memory>

using namespace Orthanc;


TEST(DicomImageInformation, ExtractPixelFormat1)
{
  // Cardiac/MR*
  DicomMap m;
  m.SetValue(DICOM_TAG_ROWS, "24", false);
  m.SetValue(DICOM_TAG_COLUMNS, "16", false);
  m.SetValue(DICOM_TAG_BITS_ALLOCATED, "16", false);
  m.SetValue(DICOM_TAG_SAMPLES_PER_PIXEL, "1", false);
  m.SetValue(DICOM_TAG_BITS_STORED, "12", false);
  m.SetValue(DICOM_TAG_HIGH_BIT, "11", false);
  m.SetValue(DICOM_TAG_PIXEL_REPRESENTATION, "0", false);
  m.SetValue(DICOM_TAG_PHOTOMETRIC_INTERPRETATION, "MONOCHROME2", false);

  DicomImageInformation info(m);
  PixelFormat format;
  ASSERT_TRUE(info.ExtractPixelFormat(format, false));
  ASSERT_EQ(PixelFormat_Grayscale16, format);
}


TEST(DicomImageInformation, ExtractPixelFormat2)
{
  // Delphine CT
  DicomMap m;
  m.SetValue(DICOM_TAG_ROWS, "24", false);
  m.SetValue(DICOM_TAG_COLUMNS, "16", false);
  m.SetValue(DICOM_TAG_BITS_ALLOCATED, "16", false);
  m.SetValue(DICOM_TAG_SAMPLES_PER_PIXEL, "1", false);
  m.SetValue(DICOM_TAG_BITS_STORED, "16", false);
  m.SetValue(DICOM_TAG_HIGH_BIT, "15", false);
  m.SetValue(DICOM_TAG_PIXEL_REPRESENTATION, "1", false);
  m.SetValue(DICOM_TAG_PHOTOMETRIC_INTERPRETATION, "MONOCHROME2", false);

  DicomImageInformation info(m);
  PixelFormat format;
  ASSERT_TRUE(info.ExtractPixelFormat(format, false));
  ASSERT_EQ(PixelFormat_SignedGrayscale16, format);
}



namespace
{
  template <typename T>
  class TestImageTraits : public ::testing::Test
  {
  private:
    std::unique_ptr<Image>  image_;

  protected:
    virtual void SetUp() ORTHANC_OVERRIDE
    {
      image_.reset(new Image(ImageTraits::PixelTraits::GetPixelFormat(), 7, 9, false));
    }

    virtual void TearDown() ORTHANC_OVERRIDE
    {
      image_.reset(NULL);
    }

  public:
    typedef T ImageTraits;
    
    ImageAccessor& GetImage()
    {
      return *image_;
    }
  };

  template <typename T>
  class TestIntegerImageTraits : public TestImageTraits<T>
  {
  };
}


typedef ::testing::Types<
  ImageTraits<PixelFormat_Grayscale8>,
  ImageTraits<PixelFormat_Grayscale16>,
  ImageTraits<PixelFormat_SignedGrayscale16>
  > IntegerFormats;
TYPED_TEST_CASE(TestIntegerImageTraits, IntegerFormats);

typedef ::testing::Types<
  ImageTraits<PixelFormat_Grayscale8>,
  ImageTraits<PixelFormat_Grayscale16>,
  ImageTraits<PixelFormat_SignedGrayscale16>,
  ImageTraits<PixelFormat_RGB24>,
  ImageTraits<PixelFormat_BGRA32>
  > AllFormats;
TYPED_TEST_CASE(TestImageTraits, AllFormats);


TYPED_TEST(TestImageTraits, SetZero)
{
  ImageAccessor& image = this->GetImage();
  
  memset(image.GetBuffer(), 128, image.GetHeight() * image.GetWidth());

  switch (image.GetFormat())
  {
    case PixelFormat_Grayscale8:
    case PixelFormat_Grayscale16:
    case PixelFormat_SignedGrayscale16:
      ImageProcessing::Set(image, 0);
      break;

    case PixelFormat_RGB24:
    case PixelFormat_BGRA32:
      ImageProcessing::Set(image, 0, 0, 0, 0);
      break;

    default:
      ASSERT_TRUE(0);
  }

  typename TestFixture::ImageTraits::PixelType zero, value;
  TestFixture::ImageTraits::PixelTraits::SetZero(zero);

  for (unsigned int y = 0; y < image.GetHeight(); y++)
  {
    for (unsigned int x = 0; x < image.GetWidth(); x++)
    {
      TestFixture::ImageTraits::GetPixel(value, image, x, y);
      ASSERT_TRUE(TestFixture::ImageTraits::PixelTraits::IsEqual(zero, value));
    }
  }
}


TYPED_TEST(TestIntegerImageTraits, SetZeroFloat)
{
  ImageAccessor& image = this->GetImage();
  
  memset(image.GetBuffer(), 128, image.GetHeight() * image.GetWidth());

  float c = 0.0f;
  for (unsigned int y = 0; y < image.GetHeight(); y++)
  {
    for (unsigned int x = 0; x < image.GetWidth(); x++, c++)
    {
      TestFixture::ImageTraits::SetFloatPixel(image, c, x, y);
    }
  }

  c = 0.0f;
  for (unsigned int y = 0; y < image.GetHeight(); y++)
  {
    for (unsigned int x = 0; x < image.GetWidth(); x++, c++)
    {
      ASSERT_FLOAT_EQ(c, TestFixture::ImageTraits::GetFloatPixel(image, x, y));
    }
  }
}


TYPED_TEST(TestIntegerImageTraits, FillPolygon)
{
  ImageAccessor& image = this->GetImage();

  ImageProcessing::Set(image, 128);

  // draw a triangle
  std::vector<ImageProcessing::ImagePoint> points;
  points.push_back(ImageProcessing::ImagePoint(1,1));
  points.push_back(ImageProcessing::ImagePoint(1,5));
  points.push_back(ImageProcessing::ImagePoint(5,5));

  ImageProcessing::FillPolygon(image, points, 255);

  // outside polygon
  ASSERT_FLOAT_EQ(128, TestFixture::ImageTraits::GetFloatPixel(image, 0, 0));
  ASSERT_FLOAT_EQ(128, TestFixture::ImageTraits::GetFloatPixel(image, 0, 6));
  ASSERT_FLOAT_EQ(128, TestFixture::ImageTraits::GetFloatPixel(image, 6, 6));
  ASSERT_FLOAT_EQ(128, TestFixture::ImageTraits::GetFloatPixel(image, 6, 0));

  ASSERT_FLOAT_EQ(255, TestFixture::ImageTraits::GetFloatPixel(image, 1, 1));
  ASSERT_FLOAT_EQ(255, TestFixture::ImageTraits::GetFloatPixel(image, 1, 2));
  ASSERT_FLOAT_EQ(255, TestFixture::ImageTraits::GetFloatPixel(image, 1, 5));
  ASSERT_FLOAT_EQ(255, TestFixture::ImageTraits::GetFloatPixel(image, 2, 4));
  ASSERT_FLOAT_EQ(255, TestFixture::ImageTraits::GetFloatPixel(image, 5, 5));
}

TYPED_TEST(TestIntegerImageTraits, FillPolygonLargerThanImage)
{
  ImageAccessor& image = this->GetImage();

  ImageProcessing::Set(image, 0);

  std::vector<ImageProcessing::ImagePoint> points;
  points.push_back(ImageProcessing::ImagePoint(0, 0));
  points.push_back(ImageProcessing::ImagePoint(image.GetWidth(),0));
  points.push_back(ImageProcessing::ImagePoint(image.GetWidth(),image.GetHeight()));
  points.push_back(ImageProcessing::ImagePoint(0,image.GetHeight()));

  ImageProcessing::FillPolygon(image, points, 255);

  for (unsigned int y = 0; y < image.GetHeight(); y++)
  {
    for (unsigned int x = 0; x < image.GetWidth(); x++)
    {
      ASSERT_FLOAT_EQ(255, TestFixture::ImageTraits::GetFloatPixel(image, x, y));
    }
  }
}

TYPED_TEST(TestIntegerImageTraits, FillPolygonFullImage)
{
  ImageAccessor& image = this->GetImage();

  ImageProcessing::Set(image, 0);

  std::vector<ImageProcessing::ImagePoint> points;
  points.push_back(ImageProcessing::ImagePoint(0, 0));
  points.push_back(ImageProcessing::ImagePoint(image.GetWidth() - 1,0));
  points.push_back(ImageProcessing::ImagePoint(image.GetWidth() - 1,image.GetHeight() - 1));
  points.push_back(ImageProcessing::ImagePoint(0,image.GetHeight() - 1));

  ImageProcessing::FillPolygon(image, points, 255);

  ASSERT_FLOAT_EQ(255, TestFixture::ImageTraits::GetFloatPixel(image, 0, 0));
  ASSERT_FLOAT_EQ(255, TestFixture::ImageTraits::GetFloatPixel(image, image.GetWidth() - 1, image.GetHeight() - 1));
}




static void SetGrayscale8Pixel(ImageAccessor& image,
                               unsigned int x,
                               unsigned int y,
                               uint8_t value)
{
  ImageTraits<PixelFormat_Grayscale8>::SetPixel(image, value, x, y);
}

static bool TestGrayscale8Pixel(const ImageAccessor& image,
                                unsigned int x,
                                unsigned int y,
                                uint8_t value)
{
  PixelTraits<PixelFormat_Grayscale8>::PixelType p;
  ImageTraits<PixelFormat_Grayscale8>::GetPixel(p, image, x, y);
  if (p != value) printf("%d %d\n", p, value);
  return p == value;
}

static void SetGrayscale16Pixel(ImageAccessor& image,
                                unsigned int x,
                                unsigned int y,
                                uint16_t value)
{
  ImageTraits<PixelFormat_Grayscale16>::SetPixel(image, value, x, y);
}

static bool TestGrayscale16Pixel(const ImageAccessor& image,
                                 unsigned int x,
                                 unsigned int y,
                                 uint16_t value)
{
  PixelTraits<PixelFormat_Grayscale16>::PixelType p;
  ImageTraits<PixelFormat_Grayscale16>::GetPixel(p, image, x, y);
  if (p != value) printf("%d %d\n", p, value);
  return p == value;
}

static void SetSignedGrayscale16Pixel(ImageAccessor& image,
                                      unsigned int x,
                                      unsigned int y,
                                      int16_t value)
{
  ImageTraits<PixelFormat_SignedGrayscale16>::SetPixel(image, value, x, y);
}

static bool TestSignedGrayscale16Pixel(const ImageAccessor& image,
                                       unsigned int x,
                                       unsigned int y,
                                       int16_t value)
{
  PixelTraits<PixelFormat_SignedGrayscale16>::PixelType p;
  ImageTraits<PixelFormat_SignedGrayscale16>::GetPixel(p, image, x, y);
  if (p != value) printf("%d %d\n", p, value);
  return p == value;
}

static void SetRGB24Pixel(ImageAccessor& image,
                          unsigned int x,
                          unsigned int y,
                          uint8_t red,
                          uint8_t green,
                          uint8_t blue)
{
  PixelTraits<PixelFormat_RGB24>::PixelType p;
  p.red_ = red;
  p.green_ = green;
  p.blue_ = blue;
  ImageTraits<PixelFormat_RGB24>::SetPixel(image, p, x, y);
}

static bool TestRGB24Pixel(const ImageAccessor& image,
                           unsigned int x,
                           unsigned int y,
                           uint8_t red,
                           uint8_t green,
                           uint8_t blue)
{
  PixelTraits<PixelFormat_RGB24>::PixelType p;
  ImageTraits<PixelFormat_RGB24>::GetPixel(p, image, x, y);
  bool ok = (p.red_ == red &&
             p.green_ == green &&
             p.blue_ == blue);
  if (!ok) printf("%d,%d,%d  %d,%d,%d\n", p.red_, p.green_, p.blue_, red, green, blue);
  return ok;
}


TEST(ImageProcessing, FlipGrayscale8)
{
  {
    Image image(PixelFormat_Grayscale8, 0, 0, false);
    ImageProcessing::FlipX(image);
    ImageProcessing::FlipY(image);
  }

  {
    Image image(PixelFormat_Grayscale8, 1, 1, false);
    SetGrayscale8Pixel(image, 0, 0, 128);
    ImageProcessing::FlipX(image);
    ImageProcessing::FlipY(image);
    ASSERT_TRUE(TestGrayscale8Pixel(image, 0, 0, 128));
  }

  {
    Image image(PixelFormat_Grayscale8, 3, 2, false);
    SetGrayscale8Pixel(image, 0, 0, 10);
    SetGrayscale8Pixel(image, 1, 0, 20);
    SetGrayscale8Pixel(image, 2, 0, 30);
    SetGrayscale8Pixel(image, 0, 1, 40);
    SetGrayscale8Pixel(image, 1, 1, 50);
    SetGrayscale8Pixel(image, 2, 1, 60);

    ImageProcessing::FlipX(image);
    ASSERT_TRUE(TestGrayscale8Pixel(image, 0, 0, 30));
    ASSERT_TRUE(TestGrayscale8Pixel(image, 1, 0, 20));
    ASSERT_TRUE(TestGrayscale8Pixel(image, 2, 0, 10));
    ASSERT_TRUE(TestGrayscale8Pixel(image, 0, 1, 60));
    ASSERT_TRUE(TestGrayscale8Pixel(image, 1, 1, 50));
    ASSERT_TRUE(TestGrayscale8Pixel(image, 2, 1, 40));

    ImageProcessing::FlipY(image);
    ASSERT_TRUE(TestGrayscale8Pixel(image, 0, 0, 60));
    ASSERT_TRUE(TestGrayscale8Pixel(image, 1, 0, 50));
    ASSERT_TRUE(TestGrayscale8Pixel(image, 2, 0, 40));
    ASSERT_TRUE(TestGrayscale8Pixel(image, 0, 1, 30));
    ASSERT_TRUE(TestGrayscale8Pixel(image, 1, 1, 20));
    ASSERT_TRUE(TestGrayscale8Pixel(image, 2, 1, 10));
  }
}



TEST(ImageProcessing, FlipRGB24)
{
  Image image(PixelFormat_RGB24, 2, 2, false);
  SetRGB24Pixel(image, 0, 0, 10, 100, 110);
  SetRGB24Pixel(image, 1, 0, 20, 100, 110);
  SetRGB24Pixel(image, 0, 1, 30, 100, 110);
  SetRGB24Pixel(image, 1, 1, 40, 100, 110);

  ImageProcessing::FlipX(image);
  ASSERT_TRUE(TestRGB24Pixel(image, 0, 0, 20, 100, 110));
  ASSERT_TRUE(TestRGB24Pixel(image, 1, 0, 10, 100, 110));
  ASSERT_TRUE(TestRGB24Pixel(image, 0, 1, 40, 100, 110));
  ASSERT_TRUE(TestRGB24Pixel(image, 1, 1, 30, 100, 110));

  ImageProcessing::FlipY(image);
  ASSERT_TRUE(TestRGB24Pixel(image, 0, 0, 40, 100, 110));
  ASSERT_TRUE(TestRGB24Pixel(image, 1, 0, 30, 100, 110));
  ASSERT_TRUE(TestRGB24Pixel(image, 0, 1, 20, 100, 110));
  ASSERT_TRUE(TestRGB24Pixel(image, 1, 1, 10, 100, 110));
}


TEST(ImageProcessing, ResizeBasicGrayscale8)
{
  Image source(PixelFormat_Grayscale8, 2, 2, false);
  SetGrayscale8Pixel(source, 0, 0, 10);
  SetGrayscale8Pixel(source, 1, 0, 20);
  SetGrayscale8Pixel(source, 0, 1, 30);
  SetGrayscale8Pixel(source, 1, 1, 40);

  {
    Image target(PixelFormat_Grayscale8, 2, 4, false);
    ImageProcessing::Resize(target, source);
    ASSERT_TRUE(TestGrayscale8Pixel(target, 0, 0, 10));
    ASSERT_TRUE(TestGrayscale8Pixel(target, 1, 0, 20));
    ASSERT_TRUE(TestGrayscale8Pixel(target, 0, 1, 10));
    ASSERT_TRUE(TestGrayscale8Pixel(target, 1, 1, 20));
    ASSERT_TRUE(TestGrayscale8Pixel(target, 0, 2, 30));
    ASSERT_TRUE(TestGrayscale8Pixel(target, 1, 2, 40));
    ASSERT_TRUE(TestGrayscale8Pixel(target, 0, 3, 30));
    ASSERT_TRUE(TestGrayscale8Pixel(target, 1, 3, 40));
  }

  {
    Image target(PixelFormat_Grayscale8, 4, 2, false);
    ImageProcessing::Resize(target, source);
    ASSERT_TRUE(TestGrayscale8Pixel(target, 0, 0, 10));
    ASSERT_TRUE(TestGrayscale8Pixel(target, 1, 0, 10));
    ASSERT_TRUE(TestGrayscale8Pixel(target, 2, 0, 20));
    ASSERT_TRUE(TestGrayscale8Pixel(target, 3, 0, 20));
    ASSERT_TRUE(TestGrayscale8Pixel(target, 0, 1, 30));
    ASSERT_TRUE(TestGrayscale8Pixel(target, 1, 1, 30));
    ASSERT_TRUE(TestGrayscale8Pixel(target, 2, 1, 40));
    ASSERT_TRUE(TestGrayscale8Pixel(target, 3, 1, 40));
  }
}


TEST(ImageProcessing, ResizeBasicRGB24)
{
  Image source(PixelFormat_RGB24, 2, 2, false);
  SetRGB24Pixel(source, 0, 0, 10, 100, 110);
  SetRGB24Pixel(source, 1, 0, 20, 100, 110);
  SetRGB24Pixel(source, 0, 1, 30, 100, 110);
  SetRGB24Pixel(source, 1, 1, 40, 100, 110);

  {
    Image target(PixelFormat_RGB24, 2, 4, false);
    ImageProcessing::Resize(target, source);
    ASSERT_TRUE(TestRGB24Pixel(target, 0, 0, 10, 100, 110));
    ASSERT_TRUE(TestRGB24Pixel(target, 1, 0, 20, 100, 110));
    ASSERT_TRUE(TestRGB24Pixel(target, 0, 1, 10, 100, 110));
    ASSERT_TRUE(TestRGB24Pixel(target, 1, 1, 20, 100, 110));
    ASSERT_TRUE(TestRGB24Pixel(target, 0, 2, 30, 100, 110));
    ASSERT_TRUE(TestRGB24Pixel(target, 1, 2, 40, 100, 110));
    ASSERT_TRUE(TestRGB24Pixel(target, 0, 3, 30, 100, 110));
    ASSERT_TRUE(TestRGB24Pixel(target, 1, 3, 40, 100, 110));
  }

  {
    Image target(PixelFormat_RGB24, 4, 2, false);
    ImageProcessing::Resize(target, source);
    ASSERT_TRUE(TestRGB24Pixel(target, 0, 0, 10, 100, 110));
    ASSERT_TRUE(TestRGB24Pixel(target, 1, 0, 10, 100, 110));
    ASSERT_TRUE(TestRGB24Pixel(target, 2, 0, 20, 100, 110));
    ASSERT_TRUE(TestRGB24Pixel(target, 3, 0, 20, 100, 110));
    ASSERT_TRUE(TestRGB24Pixel(target, 0, 1, 30, 100, 110));
    ASSERT_TRUE(TestRGB24Pixel(target, 1, 1, 30, 100, 110));
    ASSERT_TRUE(TestRGB24Pixel(target, 2, 1, 40, 100, 110));
    ASSERT_TRUE(TestRGB24Pixel(target, 3, 1, 40, 100, 110));
  }
}


TEST(ImageProcessing, ResizeEmptyGrayscale8)
{
  {
    Image source(PixelFormat_Grayscale8, 0, 0, false);
    Image target(PixelFormat_Grayscale8, 2, 2, false);
    ImageProcessing::Resize(target, source);
    ASSERT_TRUE(TestGrayscale8Pixel(target, 0, 0, 0));
    ASSERT_TRUE(TestGrayscale8Pixel(target, 1, 0, 0));
    ASSERT_TRUE(TestGrayscale8Pixel(target, 0, 1, 0));
    ASSERT_TRUE(TestGrayscale8Pixel(target, 1, 1, 0));
  }

  {
    Image source(PixelFormat_Grayscale8, 2, 2, false);
    Image target(PixelFormat_Grayscale8, 0, 0, false);
    ImageProcessing::Resize(target, source);
  }
}


TEST(ImageProcessing, Convolution)
{
  std::vector<float> k1(5, 1);
  std::vector<float> k2(1, 1);

  {
    Image image(PixelFormat_Grayscale8, 1, 1, false);
    SetGrayscale8Pixel(image, 0, 0, 100);    
    ImageProcessing::SeparableConvolution(image, k1, 2, k2, 0, true /* round */);
    ASSERT_TRUE(TestGrayscale8Pixel(image, 0, 0, 100));
    ImageProcessing::SeparableConvolution(image, k1, 2, k1, 2, true /* round */);
    ASSERT_TRUE(TestGrayscale8Pixel(image, 0, 0, 100));
    ImageProcessing::SeparableConvolution(image, k2, 0, k1, 2, true /* round */);
    ASSERT_TRUE(TestGrayscale8Pixel(image, 0, 0, 100));
    ImageProcessing::SeparableConvolution(image, k2, 0, k2, 0, true /* round */);
    ASSERT_TRUE(TestGrayscale8Pixel(image, 0, 0, 100));
  }
  
  {
    Image image(PixelFormat_RGB24, 1, 1, false);
    SetRGB24Pixel(image, 0, 0, 10, 20, 30);    
    ImageProcessing::SeparableConvolution(image, k1, 2, k2, 0, true /* round */);
    ASSERT_TRUE(TestRGB24Pixel(image, 0, 0, 10, 20, 30));
    ImageProcessing::SeparableConvolution(image, k1, 2, k1, 2, true /* round */);
    ASSERT_TRUE(TestRGB24Pixel(image, 0, 0, 10, 20, 30));
    ImageProcessing::SeparableConvolution(image, k2, 0, k1, 2, true /* round */);
    ASSERT_TRUE(TestRGB24Pixel(image, 0, 0, 10, 20, 30));
    ImageProcessing::SeparableConvolution(image, k2, 0, k2, 0, true /* round */);
    ASSERT_TRUE(TestRGB24Pixel(image, 0, 0, 10, 20, 30));
  }

  {  
    Image dirac(PixelFormat_Grayscale8, 9, 1, false);
    ImageProcessing::Set(dirac, 0);
    SetGrayscale8Pixel(dirac, 4, 0, 100);

    {
      std::unique_ptr<ImageAccessor> image(Image::Clone(dirac));
      ImageProcessing::SeparableConvolution(*image, k1, 2, k2, 0, true /* round */);
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 0, 0, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 1, 0, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 2, 0, 20));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 3, 0, 20));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 4, 0, 20));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 5, 0, 20));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 6, 0, 20));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 7, 0, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 8, 0, 0));    
    }

    {
      std::unique_ptr<ImageAccessor> image(Image::Clone(dirac));
      ImageProcessing::SeparableConvolution(*image, k2, 0, k1, 2, true /* round */);
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 0, 0, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 1, 0, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 2, 0, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 3, 0, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 4, 0, 100));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 5, 0, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 6, 0, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 7, 0, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 8, 0, 0));    
    }

    {
      std::unique_ptr<ImageAccessor> image(Image::Clone(dirac));
      ImageProcessing::SeparableConvolution(*image, k2, 0, k2, 0, true /* round */);
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 0, 0, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 1, 0, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 2, 0, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 3, 0, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 4, 0, 100));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 5, 0, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 6, 0, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 7, 0, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 8, 0, 0));    
    }
  }

  {  
    Image dirac(PixelFormat_Grayscale8, 1, 9, false);
    ImageProcessing::Set(dirac, 0);
    SetGrayscale8Pixel(dirac, 0, 4, 100);

    {
      std::unique_ptr<ImageAccessor> image(Image::Clone(dirac));
      ImageProcessing::SeparableConvolution(*image, k2, 0, k1, 2, true /* round */);
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 0, 0, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 0, 1, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 0, 2, 20));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 0, 3, 20));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 0, 4, 20));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 0, 5, 20));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 0, 6, 20));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 0, 7, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 0, 8, 0));    
    }

    {
      std::unique_ptr<ImageAccessor> image(Image::Clone(dirac));
      ImageProcessing::SeparableConvolution(*image, k1, 2, k2, 0, true /* round */);
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 0, 0, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 0, 1, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 0, 2, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 0, 3, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 0, 4, 100));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 0, 5, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 0, 6, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 0, 7, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 0, 8, 0));    
    }

    {
      std::unique_ptr<ImageAccessor> image(Image::Clone(dirac));
      ImageProcessing::SeparableConvolution(*image, k2, 0, k2, 0, true /* round */);
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 0, 0, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 0, 1, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 0, 2, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 0, 3, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 0, 4, 100));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 0, 5, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 0, 6, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 0, 7, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(*image, 0, 8, 0));    
    }
  }

  {
    Image dirac(PixelFormat_RGB24, 9, 1, false);
    ImageProcessing::Set(dirac, 0);
    SetRGB24Pixel(dirac, 4, 0, 100, 120, 140);

    {
      std::unique_ptr<ImageAccessor> image(Image::Clone(dirac));
      ImageProcessing::SeparableConvolution(*image, k1, 2, k2, 0, true /* round */);
      ASSERT_TRUE(TestRGB24Pixel(*image, 0, 0, 0, 0, 0));
      ASSERT_TRUE(TestRGB24Pixel(*image, 1, 0, 0, 0, 0));
      ASSERT_TRUE(TestRGB24Pixel(*image, 2, 0, 20, 24, 28));
      ASSERT_TRUE(TestRGB24Pixel(*image, 3, 0, 20, 24, 28));
      ASSERT_TRUE(TestRGB24Pixel(*image, 4, 0, 20, 24, 28));
      ASSERT_TRUE(TestRGB24Pixel(*image, 5, 0, 20, 24, 28));
      ASSERT_TRUE(TestRGB24Pixel(*image, 6, 0, 20, 24, 28));
      ASSERT_TRUE(TestRGB24Pixel(*image, 7, 0, 0, 0, 0));
      ASSERT_TRUE(TestRGB24Pixel(*image, 8, 0, 0, 0, 0));    
    }

    {
      std::unique_ptr<ImageAccessor> image(Image::Clone(dirac));
      ImageProcessing::SeparableConvolution(*image, k2, 0, k1, 2, true /* round */);
      ASSERT_TRUE(TestRGB24Pixel(*image, 0, 0, 0, 0, 0));
      ASSERT_TRUE(TestRGB24Pixel(*image, 1, 0, 0, 0, 0));
      ASSERT_TRUE(TestRGB24Pixel(*image, 2, 0, 0, 0, 0));
      ASSERT_TRUE(TestRGB24Pixel(*image, 3, 0, 0, 0, 0));
      ASSERT_TRUE(TestRGB24Pixel(*image, 4, 0, 100, 120, 140));
      ASSERT_TRUE(TestRGB24Pixel(*image, 5, 0, 0, 0, 0));
      ASSERT_TRUE(TestRGB24Pixel(*image, 6, 0, 0, 0, 0));
      ASSERT_TRUE(TestRGB24Pixel(*image, 7, 0, 0, 0, 0));
      ASSERT_TRUE(TestRGB24Pixel(*image, 8, 0, 0, 0, 0));    
    }

    {
      std::unique_ptr<ImageAccessor> image(Image::Clone(dirac));
      ImageProcessing::SeparableConvolution(*image, k2, 0, k2, 0, true /* round */);
      ASSERT_TRUE(TestRGB24Pixel(*image, 0, 0, 0, 0, 0));
      ASSERT_TRUE(TestRGB24Pixel(*image, 1, 0, 0, 0, 0));
      ASSERT_TRUE(TestRGB24Pixel(*image, 2, 0, 0, 0, 0));
      ASSERT_TRUE(TestRGB24Pixel(*image, 3, 0, 0, 0, 0));
      ASSERT_TRUE(TestRGB24Pixel(*image, 4, 0, 100, 120, 140));
      ASSERT_TRUE(TestRGB24Pixel(*image, 5, 0, 0, 0, 0));
      ASSERT_TRUE(TestRGB24Pixel(*image, 6, 0, 0, 0, 0));
      ASSERT_TRUE(TestRGB24Pixel(*image, 7, 0, 0, 0, 0));
      ASSERT_TRUE(TestRGB24Pixel(*image, 8, 0, 0, 0, 0));    
    }
  }

  {
    Image dirac(PixelFormat_RGB24, 1, 9, false);
    ImageProcessing::Set(dirac, 0);
    SetRGB24Pixel(dirac, 0, 4, 100, 120, 140);

    {
      std::unique_ptr<ImageAccessor> image(Image::Clone(dirac));
      ImageProcessing::SeparableConvolution(*image, k2, 0, k1, 2, true /* round */);
      ASSERT_TRUE(TestRGB24Pixel(*image, 0, 0, 0, 0, 0));
      ASSERT_TRUE(TestRGB24Pixel(*image, 0, 1, 0, 0, 0));
      ASSERT_TRUE(TestRGB24Pixel(*image, 0, 2, 20, 24, 28));
      ASSERT_TRUE(TestRGB24Pixel(*image, 0, 3, 20, 24, 28));
      ASSERT_TRUE(TestRGB24Pixel(*image, 0, 4, 20, 24, 28));
      ASSERT_TRUE(TestRGB24Pixel(*image, 0, 5, 20, 24, 28));
      ASSERT_TRUE(TestRGB24Pixel(*image, 0, 6, 20, 24, 28));
      ASSERT_TRUE(TestRGB24Pixel(*image, 0, 7, 0, 0, 0));
      ASSERT_TRUE(TestRGB24Pixel(*image, 0, 8, 0, 0, 0));    
    }

    {
      std::unique_ptr<ImageAccessor> image(Image::Clone(dirac));
      ImageProcessing::SeparableConvolution(*image, k1, 2, k2, 0, true /* round */);
      ASSERT_TRUE(TestRGB24Pixel(*image, 0, 0, 0, 0, 0));
      ASSERT_TRUE(TestRGB24Pixel(*image, 0, 1, 0, 0, 0));
      ASSERT_TRUE(TestRGB24Pixel(*image, 0, 2, 0, 0, 0));
      ASSERT_TRUE(TestRGB24Pixel(*image, 0, 3, 0, 0, 0));
      ASSERT_TRUE(TestRGB24Pixel(*image, 0, 4, 100, 120, 140));
      ASSERT_TRUE(TestRGB24Pixel(*image, 0, 5, 0, 0, 0));
      ASSERT_TRUE(TestRGB24Pixel(*image, 0, 6, 0, 0, 0));
      ASSERT_TRUE(TestRGB24Pixel(*image, 0, 7, 0, 0, 0));
      ASSERT_TRUE(TestRGB24Pixel(*image, 0, 8, 0, 0, 0));    
    }

    {
      std::unique_ptr<ImageAccessor> image(Image::Clone(dirac));
      ImageProcessing::SeparableConvolution(*image, k2, 0, k2, 0, true /* round */);
      ASSERT_TRUE(TestRGB24Pixel(*image, 0, 0, 0, 0, 0));
      ASSERT_TRUE(TestRGB24Pixel(*image, 0, 1, 0, 0, 0));
      ASSERT_TRUE(TestRGB24Pixel(*image, 0, 2, 0, 0, 0));
      ASSERT_TRUE(TestRGB24Pixel(*image, 0, 3, 0, 0, 0));
      ASSERT_TRUE(TestRGB24Pixel(*image, 0, 4, 100, 120, 140));
      ASSERT_TRUE(TestRGB24Pixel(*image, 0, 5, 0, 0, 0));
      ASSERT_TRUE(TestRGB24Pixel(*image, 0, 6, 0, 0, 0));
      ASSERT_TRUE(TestRGB24Pixel(*image, 0, 7, 0, 0, 0));
      ASSERT_TRUE(TestRGB24Pixel(*image, 0, 8, 0, 0, 0));    
    }
  }
}


TEST(ImageProcessing, SmoothGaussian5x5)
{
  /**
     Test the point spread function, as can be seen in Octave:
     g1 = [ 1 4 6 4 1 ];
     g1 /= sum(g1);
     g2 = conv2(g1, g1');
     floor(conv2(diag([ 0 0 100 0 0 ]), g2, 'same'))  % red/green channels
     floor(conv2(diag([ 0 0 200 0 0 ]), g2, 'same'))  % blue channel
  **/

  {
    Image image(PixelFormat_Grayscale8, 5, 5, false);
    ImageProcessing::Set(image, 0);
    SetGrayscale8Pixel(image, 2, 2, 100);
    ImageProcessing::SmoothGaussian5x5(image, true /* round */);

    // In Octave: round(conv2([1 4 6 4 1],[1 4 6 4 1]')/256*100)
    ASSERT_TRUE(TestGrayscale8Pixel(image, 0, 0, 0));
    ASSERT_TRUE(TestGrayscale8Pixel(image, 1, 0, 2));
    ASSERT_TRUE(TestGrayscale8Pixel(image, 2, 0, 2));
    ASSERT_TRUE(TestGrayscale8Pixel(image, 3, 0, 2));
    ASSERT_TRUE(TestGrayscale8Pixel(image, 4, 0, 0));
    ASSERT_TRUE(TestGrayscale8Pixel(image, 0, 1, 2));
    ASSERT_TRUE(TestGrayscale8Pixel(image, 1, 1, 6));
    ASSERT_TRUE(TestGrayscale8Pixel(image, 2, 1, 9));
    ASSERT_TRUE(TestGrayscale8Pixel(image, 3, 1, 6));
    ASSERT_TRUE(TestGrayscale8Pixel(image, 4, 1, 2));
    ASSERT_TRUE(TestGrayscale8Pixel(image, 0, 2, 2));
    ASSERT_TRUE(TestGrayscale8Pixel(image, 1, 2, 9));
    ASSERT_TRUE(TestGrayscale8Pixel(image, 2, 2, 14));
    ASSERT_TRUE(TestGrayscale8Pixel(image, 3, 2, 9));
    ASSERT_TRUE(TestGrayscale8Pixel(image, 4, 2, 2));
    ASSERT_TRUE(TestGrayscale8Pixel(image, 0, 3, 2));
    ASSERT_TRUE(TestGrayscale8Pixel(image, 1, 3, 6));
    ASSERT_TRUE(TestGrayscale8Pixel(image, 2, 3, 9));
    ASSERT_TRUE(TestGrayscale8Pixel(image, 3, 3, 6));
    ASSERT_TRUE(TestGrayscale8Pixel(image, 4, 3, 2));
    ASSERT_TRUE(TestGrayscale8Pixel(image, 0, 4, 0));
    ASSERT_TRUE(TestGrayscale8Pixel(image, 1, 4, 2));
    ASSERT_TRUE(TestGrayscale8Pixel(image, 2, 4, 2));
    ASSERT_TRUE(TestGrayscale8Pixel(image, 3, 4, 2));
    ASSERT_TRUE(TestGrayscale8Pixel(image, 4, 4, 0));
  }

  {
    Image image(PixelFormat_RGB24, 5, 5, false);
    ImageProcessing::Set(image, 0);
    SetRGB24Pixel(image, 2, 2, 100, 100, 200);
    ImageProcessing::SmoothGaussian5x5(image, true /* round */);

    // In Octave:
    // R,G = round(conv2([1 4 6 4 1],[1 4 6 4 1]')/256*100)
    // B = round(conv2([1 4 6 4 1],[1 4 6 4 1]')/256*200)
    ASSERT_TRUE(TestRGB24Pixel(image, 0, 0, 0, 0, 1));
    ASSERT_TRUE(TestRGB24Pixel(image, 1, 0, 2, 2, 3));
    ASSERT_TRUE(TestRGB24Pixel(image, 2, 0, 2, 2, 5));
    ASSERT_TRUE(TestRGB24Pixel(image, 3, 0, 2, 2, 3));
    ASSERT_TRUE(TestRGB24Pixel(image, 4, 0, 0, 0, 1));
    ASSERT_TRUE(TestRGB24Pixel(image, 0, 1, 2, 2, 3));
    ASSERT_TRUE(TestRGB24Pixel(image, 1, 1, 6, 6, 13));
    ASSERT_TRUE(TestRGB24Pixel(image, 2, 1, 9, 9, 19));
    ASSERT_TRUE(TestRGB24Pixel(image, 3, 1, 6, 6, 13));
    ASSERT_TRUE(TestRGB24Pixel(image, 4, 1, 2, 2, 3));
    ASSERT_TRUE(TestRGB24Pixel(image, 0, 2, 2, 2, 5));
    ASSERT_TRUE(TestRGB24Pixel(image, 1, 2, 9, 9, 19));
    ASSERT_TRUE(TestRGB24Pixel(image, 2, 2, 14, 14, 28));
    ASSERT_TRUE(TestRGB24Pixel(image, 3, 2, 9, 9, 19));
    ASSERT_TRUE(TestRGB24Pixel(image, 4, 2, 2, 2, 5));
    ASSERT_TRUE(TestRGB24Pixel(image, 0, 3, 2, 2, 3));
    ASSERT_TRUE(TestRGB24Pixel(image, 1, 3, 6, 6, 13));
    ASSERT_TRUE(TestRGB24Pixel(image, 2, 3, 9, 9, 19));
    ASSERT_TRUE(TestRGB24Pixel(image, 3, 3, 6, 6, 13));
    ASSERT_TRUE(TestRGB24Pixel(image, 4, 3, 2, 2, 3));
    ASSERT_TRUE(TestRGB24Pixel(image, 0, 4, 0, 0, 1));
    ASSERT_TRUE(TestRGB24Pixel(image, 1, 4, 2, 2, 3));
    ASSERT_TRUE(TestRGB24Pixel(image, 2, 4, 2, 2, 5));
    ASSERT_TRUE(TestRGB24Pixel(image, 3, 4, 2, 2, 3));
    ASSERT_TRUE(TestRGB24Pixel(image, 4, 4, 0, 0, 1));
  }
}

TEST(ImageProcessing, ApplyWindowingFloatToGrayScale8)
{
  {
    Image image(PixelFormat_Float32, 6, 1, false);
    ImageTraits<PixelFormat_Float32>::SetFloatPixel(image, -5.0f, 0, 0);
    ImageTraits<PixelFormat_Float32>::SetFloatPixel(image, 0.0f, 1, 0);
    ImageTraits<PixelFormat_Float32>::SetFloatPixel(image, 5.0f, 2, 0);
    ImageTraits<PixelFormat_Float32>::SetFloatPixel(image, 10.0f, 3, 0);
    ImageTraits<PixelFormat_Float32>::SetFloatPixel(image, 1000.0f, 4, 0);
    ImageTraits<PixelFormat_Float32>::SetFloatPixel(image, 2.0f, 5, 0);

    {
      Image target(PixelFormat_Grayscale8, 6, 1, false);
      ImageProcessing::ApplyWindowing_Deprecated(target, image, 5.0f, 10.0f, 1.0f, 0.0f, false);

      ASSERT_TRUE(TestGrayscale8Pixel(target, 0, 0, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(target, 1, 0, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(target, 2, 0, 128));
      ASSERT_TRUE(TestGrayscale8Pixel(target, 3, 0, 255));
      ASSERT_TRUE(TestGrayscale8Pixel(target, 4, 0, 255));
      ASSERT_TRUE(TestGrayscale8Pixel(target, 5, 0, 255*2/10));
    }

    {
      Image target(PixelFormat_Grayscale8, 6, 1, false);
      ImageProcessing::ApplyWindowing_Deprecated(target, image, 5.0f, 10.0f, 1.0f, 0.0f, true);

      ASSERT_TRUE(TestGrayscale8Pixel(target, 0, 0, 255));
      ASSERT_TRUE(TestGrayscale8Pixel(target, 1, 0, 255));
      ASSERT_TRUE(TestGrayscale8Pixel(target, 2, 0, 127));
      ASSERT_TRUE(TestGrayscale8Pixel(target, 3, 0, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(target, 4, 0, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(target, 5, 0, 255 - 255*2/10));
    }

    {
      Image target(PixelFormat_Grayscale8, 6, 1, false);
      ImageProcessing::ApplyWindowing_Deprecated(target, image, 5000.0f, 10000.01f, 1000.0f, 0.0f, false);

      ASSERT_TRUE(TestGrayscale8Pixel(target, 0, 0, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(target, 1, 0, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(target, 2, 0, 128));
      ASSERT_TRUE(TestGrayscale8Pixel(target, 3, 0, 255));
      ASSERT_TRUE(TestGrayscale8Pixel(target, 4, 0, 255));
      ASSERT_TRUE(TestGrayscale8Pixel(target, 5, 0, 255*2/10));
    }

    {
      Image target(PixelFormat_Grayscale8, 6, 1, false);
      ImageProcessing::ApplyWindowing_Deprecated(target, image, 5000.0f, 10000.01f, 1000.0f, 0.0f, true);

      ASSERT_TRUE(TestGrayscale8Pixel(target, 0, 0, 255));
      ASSERT_TRUE(TestGrayscale8Pixel(target, 1, 0, 255));
      ASSERT_TRUE(TestGrayscale8Pixel(target, 2, 0, 127));
      ASSERT_TRUE(TestGrayscale8Pixel(target, 3, 0, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(target, 4, 0, 0));
      ASSERT_TRUE(TestGrayscale8Pixel(target, 5, 0, 255 - 256*2/10));
    }

    {
      Image target(PixelFormat_Grayscale8, 6, 1, false);
      ImageProcessing::ApplyWindowing_Deprecated(target, image, 50.0f, 100.1f, 10.0f, 30.0f, false);

      ASSERT_TRUE(TestGrayscale8Pixel(target, 0, 0, 0));  // (-5 * 10) + 30 => pixel value = -20 => 0
      ASSERT_TRUE(TestGrayscale8Pixel(target, 1, 0, 256*30/100));  // ((0 * 10) + 30 => pixel value = 30 => 30%
      ASSERT_TRUE(TestGrayscale8Pixel(target, 2, 0, 256*80/100)); // ((5 * 10) + 30 => pixel value = 80 => 80%
      ASSERT_TRUE(TestGrayscale8Pixel(target, 3, 0, 255)); // ((10 * 10) + 30 => pixel value = 130 => 100%
      ASSERT_TRUE(TestGrayscale8Pixel(target, 4, 0, 255)); // ((1000 * 10) + 30 => pixel value = 10030 => 100%
      ASSERT_TRUE(TestGrayscale8Pixel(target, 5, 0, 128)); // ((2 * 10) + 30 => pixel value = 50 => 50%
    }

  }
}

TEST(ImageProcessing, ApplyWindowingFloatToGrayScale16)
{
  {
    Image image(PixelFormat_Float32, 6, 1, false);
    ImageTraits<PixelFormat_Float32>::SetFloatPixel(image, -5.0f, 0, 0);
    ImageTraits<PixelFormat_Float32>::SetFloatPixel(image, 0.0f, 1, 0);
    ImageTraits<PixelFormat_Float32>::SetFloatPixel(image, 5.0f, 2, 0);
    ImageTraits<PixelFormat_Float32>::SetFloatPixel(image, 10.0f, 3, 0);
    ImageTraits<PixelFormat_Float32>::SetFloatPixel(image, 1000.0f, 4, 0);
    ImageTraits<PixelFormat_Float32>::SetFloatPixel(image, 2.0f, 5, 0);

    {
      Image target(PixelFormat_Grayscale16, 6, 1, false);
      ImageProcessing::ApplyWindowing_Deprecated(target, image, 5.0f, 10.0f, 1.0f, 0.0f, false);

      ASSERT_TRUE(TestGrayscale16Pixel(target, 0, 0, 0));
      ASSERT_TRUE(TestGrayscale16Pixel(target, 1, 0, 0));
      ASSERT_TRUE(TestGrayscale16Pixel(target, 2, 0, 32768));
      ASSERT_TRUE(TestGrayscale16Pixel(target, 3, 0, 65535));
      ASSERT_TRUE(TestGrayscale16Pixel(target, 4, 0, 65535));
      ASSERT_TRUE(TestGrayscale16Pixel(target, 5, 0, 65536*2/10));
    }
  }
}

TEST(ImageProcessing, ApplyWindowingGrayScale8ToGrayScale16)
{
  {
    Image image(PixelFormat_Grayscale8, 5, 1, false);
    SetGrayscale8Pixel(image, 0, 0, 0);
    SetGrayscale8Pixel(image, 1, 0, 2);
    SetGrayscale8Pixel(image, 2, 0, 5);
    SetGrayscale8Pixel(image, 3, 0, 10);
    SetGrayscale8Pixel(image, 4, 0, 255);

    {
      Image target(PixelFormat_Grayscale16, 5, 1, false);
      ImageProcessing::ApplyWindowing_Deprecated(target, image, 5.0f, 10.0f, 1.0f, 0.0f, false);

      ASSERT_TRUE(TestGrayscale16Pixel(target, 0, 0, 0));
      ASSERT_TRUE(TestGrayscale16Pixel(target, 1, 0, 65536*2/10));
      ASSERT_TRUE(TestGrayscale16Pixel(target, 2, 0, 65536*5/10));
      ASSERT_TRUE(TestGrayscale16Pixel(target, 3, 0, 65535));
      ASSERT_TRUE(TestGrayscale16Pixel(target, 4, 0, 65535));
    }
  }
}

TEST(ImageProcessing, ApplyWindowingGrayScale16ToGrayScale16)
{
  {
    Image image(PixelFormat_Grayscale16, 5, 1, false);
    SetGrayscale16Pixel(image, 0, 0, 0);
    SetGrayscale16Pixel(image, 1, 0, 2);
    SetGrayscale16Pixel(image, 2, 0, 5);
    SetGrayscale16Pixel(image, 3, 0, 10);
    SetGrayscale16Pixel(image, 4, 0, 255);

    {
      Image target(PixelFormat_Grayscale16, 5, 1, false);
      ImageProcessing::ApplyWindowing_Deprecated(target, image, 5.0f, 10.0f, 1.0f, 0.0f, false);

      ASSERT_TRUE(TestGrayscale16Pixel(target, 0, 0, 0));
      ASSERT_TRUE(TestGrayscale16Pixel(target, 1, 0, 65536*2/10));
      ASSERT_TRUE(TestGrayscale16Pixel(target, 2, 0, 65536*5/10));
      ASSERT_TRUE(TestGrayscale16Pixel(target, 3, 0, 65535));
      ASSERT_TRUE(TestGrayscale16Pixel(target, 4, 0, 65535));
    }
  }
}


TEST(ImageProcessing, ShiftScaleGrayscale8)
{
  Image image(PixelFormat_Grayscale8, 5, 1, false);
  SetGrayscale8Pixel(image, 0, 0, 0);
  SetGrayscale8Pixel(image, 1, 0, 2);
  SetGrayscale8Pixel(image, 2, 0, 5);
  SetGrayscale8Pixel(image, 3, 0, 10);
  SetGrayscale8Pixel(image, 4, 0, 255);

  ImageProcessing::ShiftScale(image, -1.1f, 1.5f, true);
  ASSERT_TRUE(TestGrayscale8Pixel(image, 0, 0, 0));   // (0 - 1.1) * 1.5 = -1.65 ==> 0
  ASSERT_TRUE(TestGrayscale8Pixel(image, 1, 0, 1));   // (2 - 1.1) * 1.5 = 1.35 => 1
  ASSERT_TRUE(TestGrayscale8Pixel(image, 2, 0, 6));   // (5 - 1.1) * 1.5 = 5.85 => 6
  ASSERT_TRUE(TestGrayscale8Pixel(image, 3, 0, 13));  // (10 - 1.1) * 1.5 = 13.35 => 13
  ASSERT_TRUE(TestGrayscale8Pixel(image, 4, 0, 255));
}


TEST(ImageProcessing, Grayscale8_Identity)
{
  Image image(PixelFormat_Float32, 5, 1, false);
  ImageTraits<PixelFormat_Float32>::SetPixel(image, 0, 0, 0);
  ImageTraits<PixelFormat_Float32>::SetPixel(image, 2.5, 1, 0);
  ImageTraits<PixelFormat_Float32>::SetPixel(image, 5.5, 2, 0);
  ImageTraits<PixelFormat_Float32>::SetPixel(image, 10.5, 3, 0);
  ImageTraits<PixelFormat_Float32>::SetPixel(image, 255.5, 4, 0);

  Image image2(PixelFormat_Grayscale8, 5, 1, false);
  ImageProcessing::ShiftScale(image2, image, 0, 1, false);
  ASSERT_TRUE(TestGrayscale8Pixel(image2, 0, 0, 0));
  ASSERT_TRUE(TestGrayscale8Pixel(image2, 1, 0, 2));
  ASSERT_TRUE(TestGrayscale8Pixel(image2, 2, 0, 5));
  ASSERT_TRUE(TestGrayscale8Pixel(image2, 3, 0, 10));
  ASSERT_TRUE(TestGrayscale8Pixel(image2, 4, 0, 255));
}


TEST(ImageProcessing, ShiftScaleGrayscale16)
{
  Image image(PixelFormat_Grayscale16, 5, 1, false);
  SetGrayscale16Pixel(image, 0, 0, 0);
  SetGrayscale16Pixel(image, 1, 0, 2);
  SetGrayscale16Pixel(image, 2, 0, 5);
  SetGrayscale16Pixel(image, 3, 0, 10);
  SetGrayscale16Pixel(image, 4, 0, 255);

  ImageProcessing::ShiftScale(image, -1.1f, 1.5f, true);
  ASSERT_TRUE(TestGrayscale16Pixel(image, 0, 0, 0));
  ASSERT_TRUE(TestGrayscale16Pixel(image, 1, 0, 1));
  ASSERT_TRUE(TestGrayscale16Pixel(image, 2, 0, 6));
  ASSERT_TRUE(TestGrayscale16Pixel(image, 3, 0, 13));
  ASSERT_TRUE(TestGrayscale16Pixel(image, 4, 0, 381));
}


TEST(ImageProcessing, ShiftScaleSignedGrayscale16)
{
  Image image(PixelFormat_SignedGrayscale16, 5, 1, false);
  SetSignedGrayscale16Pixel(image, 0, 0, 0);
  SetSignedGrayscale16Pixel(image, 1, 0, 2);
  SetSignedGrayscale16Pixel(image, 2, 0, 5);
  SetSignedGrayscale16Pixel(image, 3, 0, 10);
  SetSignedGrayscale16Pixel(image, 4, 0, 255);

  ImageProcessing::ShiftScale(image, -17.1f, 11.5f, true);
  ASSERT_TRUE(TestSignedGrayscale16Pixel(image, 0, 0, -197));
  ASSERT_TRUE(TestSignedGrayscale16Pixel(image, 1, 0, -174));
  ASSERT_TRUE(TestSignedGrayscale16Pixel(image, 2, 0, -139));
  ASSERT_TRUE(TestSignedGrayscale16Pixel(image, 3, 0, -82));
  ASSERT_TRUE(TestSignedGrayscale16Pixel(image, 4, 0, 2736));
}


TEST(ImageProcessing, ShiftScaleSignedGrayscale16_Identity)
{
  Image image(PixelFormat_SignedGrayscale16, 5, 1, false);
  SetSignedGrayscale16Pixel(image, 0, 0, 0);
  SetSignedGrayscale16Pixel(image, 1, 0, 2);
  SetSignedGrayscale16Pixel(image, 2, 0, 5);
  SetSignedGrayscale16Pixel(image, 3, 0, 10);
  SetSignedGrayscale16Pixel(image, 4, 0, 255);

  ImageProcessing::ShiftScale(image, 0, 1, true);
  ASSERT_TRUE(TestSignedGrayscale16Pixel(image, 0, 0, 0));
  ASSERT_TRUE(TestSignedGrayscale16Pixel(image, 1, 0, 2));
  ASSERT_TRUE(TestSignedGrayscale16Pixel(image, 2, 0, 5));
  ASSERT_TRUE(TestSignedGrayscale16Pixel(image, 3, 0, 10));
  ASSERT_TRUE(TestSignedGrayscale16Pixel(image, 4, 0, 255));
}


TEST(ImageProcessing, ShiftFloatBuggy)
{
  // This test failed in Orthanc 1.10.1
  
  Image image(PixelFormat_Float32, 3, 1, false);
  ImageTraits<PixelFormat_Float32>::SetFloatPixel(image, -1.0f, 0, 0);
  ImageTraits<PixelFormat_Float32>::SetFloatPixel(image, 0.0f, 1, 0);
  ImageTraits<PixelFormat_Float32>::SetFloatPixel(image, 1.0f, 2, 0);

  std::unique_ptr<Image> cloned(Image::Clone(image));

  ImageProcessing::ShiftScale2(image, 0, 0.000539, true);
  ASSERT_FLOAT_EQ(-0.000539f, ImageTraits<PixelFormat_Float32>::GetFloatPixel(image, 0, 0));
  ASSERT_FLOAT_EQ(0.0f, ImageTraits<PixelFormat_Float32>::GetFloatPixel(image, 1, 0));
  ASSERT_FLOAT_EQ(0.000539f, ImageTraits<PixelFormat_Float32>::GetFloatPixel(image, 2, 0));

  ImageProcessing::ShiftScale2(*cloned, 0, 0.000539, false);
  ASSERT_FLOAT_EQ(-0.000539f, ImageTraits<PixelFormat_Float32>::GetFloatPixel(*cloned, 0, 0));
  ASSERT_FLOAT_EQ(0.0f, ImageTraits<PixelFormat_Float32>::GetFloatPixel(*cloned, 1, 0));
  ASSERT_FLOAT_EQ(0.000539f, ImageTraits<PixelFormat_Float32>::GetFloatPixel(*cloned, 2, 0));
}


TEST(ImageProcessing, ShiftScale2)
{
  std::vector<float> va;
  va.push_back(0);
  va.push_back(-10);
  va.push_back(5);
  
  std::vector<float> vb;
  vb.push_back(0);
  vb.push_back(-42);
  vb.push_back(42);

  Image source(PixelFormat_Float32, 1, 1, false);
  ImageTraits<PixelFormat_Float32>::SetFloatPixel(source, 10, 0, 0);
  
  for (std::vector<float>::const_iterator a = va.begin(); a != va.end(); ++a)
  {
    for (std::vector<float>::const_iterator b = vb.begin(); b != vb.end(); ++b)
    {
      Image target(PixelFormat_Float32, 1, 1, false);

      ImageProcessing::Copy(target, source);
      ImageProcessing::ShiftScale2(target, *b, *a, false);
      ASSERT_FLOAT_EQ((*a) * 10.0f + (*b),
                      ImageTraits<PixelFormat_Float32>::GetFloatPixel(target, 0, 0));

      ImageProcessing::Copy(target, source);
      ImageProcessing::ShiftScale(target, *b, *a, false);
      ASSERT_FLOAT_EQ((*a) * (10.0f + (*b)),
                      ImageTraits<PixelFormat_Float32>::GetFloatPixel(target, 0, 0));
    }
  }
}


namespace
{
  class PolygonSegments : public ImageProcessing::IPolygonFiller
  {
  private:
    std::vector<int> y_, x1_, x2_;

  public:  
    virtual void Fill(int y,
                      int x1,
                      int x2) ORTHANC_OVERRIDE
    {
      assert(x1 <= x2);
      y_.push_back(y);
      x1_.push_back(x1);
      x2_.push_back(x2);
    }

    size_t GetSize() const
    {
      return y_.size();
    }

    int GetY(size_t i) const
    {
      return y_[i];
    }

    int GetX1(size_t i) const
    {
      return x1_[i];
    }

    int GetX2(size_t i) const
    {
      return x2_[i];
    }
  };
}


static bool LookupSegment(unsigned int& x1,
                          unsigned int& x2,
                          const Orthanc::ImageAccessor& image,
                          unsigned int y)
{
  const uint8_t* p = reinterpret_cast<const uint8_t*>(image.GetConstRow(y));

  bool allZeros = true;
  for (unsigned int i = 0; i < image.GetWidth(); i++)
  {
    if (p[i] == 255)
    {
      allZeros = false;
      break;
    }
    else if (p[i] > 0)
    {
      return false;  // error
    }
  }

  if (allZeros)
  {
    return false;
  }  

  x1 = 0;
  while (p[x1] == 0)
  {
    x1++;
  }

  x2 = image.GetWidth() - 1;
  while (p[x2] == 0)
  {
    x2--;
  }

  for (unsigned int i = x1; i <= x2; i++)
  {
    if (p[i] != 255)
    {
      return false;
    }
  }

  return true;
}


TEST(ImageProcessing, FillPolygon)
{
  {
    // Empty
    std::vector<ImageProcessing::ImagePoint> polygon;

    PolygonSegments segments;
    ImageProcessing::FillPolygon(segments, polygon);
    ASSERT_EQ(0u, segments.GetSize());
  }

  {
    // One point
    std::vector<ImageProcessing::ImagePoint> polygon;
    polygon.push_back(ImageProcessing::ImagePoint(288, 208));

    PolygonSegments segments;
    ImageProcessing::FillPolygon(segments, polygon);
    ASSERT_EQ(0u, segments.GetSize());
  }

  {
    // One horizontal segment
    std::vector<ImageProcessing::ImagePoint> polygon;
    polygon.push_back(ImageProcessing::ImagePoint(10, 100));
    polygon.push_back(ImageProcessing::ImagePoint(50, 100));

    PolygonSegments segments;
    ImageProcessing::FillPolygon(segments, polygon);
    ASSERT_EQ(1u, segments.GetSize());
    ASSERT_EQ(100, segments.GetY(0));
    ASSERT_EQ(10, segments.GetX1(0));
    ASSERT_EQ(50, segments.GetX2(0));
  }

  {
    // Set of horizontal segments
    std::vector<ImageProcessing::ImagePoint> polygon;
    polygon.push_back(ImageProcessing::ImagePoint(10, 100));
    polygon.push_back(ImageProcessing::ImagePoint(20, 100));
    polygon.push_back(ImageProcessing::ImagePoint(30, 100));
    polygon.push_back(ImageProcessing::ImagePoint(50, 100));

    PolygonSegments segments;
    ImageProcessing::FillPolygon(segments, polygon);
    ASSERT_EQ(1u, segments.GetSize());
    ASSERT_EQ(100, segments.GetY(0));
    ASSERT_EQ(10, segments.GetX1(0));
    ASSERT_EQ(50, segments.GetX2(0));
  }

  {
    // Set of vertical segments
    std::vector<ImageProcessing::ImagePoint> polygon;
    polygon.push_back(ImageProcessing::ImagePoint(10, 100));
    polygon.push_back(ImageProcessing::ImagePoint(10, 102));
    polygon.push_back(ImageProcessing::ImagePoint(10, 105));

    PolygonSegments segments;
    ImageProcessing::FillPolygon(segments, polygon);
    ASSERT_EQ(6u, segments.GetSize());
    for (size_t i = 0; i < segments.GetSize(); i++)
    {
      ASSERT_EQ(100 + static_cast<int>(i), segments.GetY(i));
      ASSERT_EQ(10, segments.GetX1(i));
      ASSERT_EQ(10, segments.GetX2(i));
    }
  }

  {
    // One diagonal segment
    std::vector<ImageProcessing::ImagePoint> polygon;
    polygon.push_back(ImageProcessing::ImagePoint(10, 100));
    polygon.push_back(ImageProcessing::ImagePoint(11, 101));
    polygon.push_back(ImageProcessing::ImagePoint(13, 103));

    PolygonSegments segments;
    ImageProcessing::FillPolygon(segments, polygon);
    ASSERT_EQ(4u, segments.GetSize());
    ASSERT_EQ(100, segments.GetY(0));
    ASSERT_EQ(10, segments.GetX1(0));
    ASSERT_EQ(10, segments.GetX2(0));
    ASSERT_EQ(101, segments.GetY(1));
    ASSERT_EQ(11, segments.GetX1(1));
    ASSERT_EQ(11, segments.GetX2(1));
    ASSERT_EQ(102, segments.GetY(2));
    ASSERT_EQ(12, segments.GetX1(2));
    ASSERT_EQ(12, segments.GetX2(2));
    ASSERT_EQ(103, segments.GetY(3));
    ASSERT_EQ(13, segments.GetX1(3));
    ASSERT_EQ(13, segments.GetX2(3));
  }

  {
    // "M" shape
    std::vector<ImageProcessing::ImagePoint> polygon;
    polygon.push_back(ImageProcessing::ImagePoint(5, 5));
    polygon.push_back(ImageProcessing::ImagePoint(7, 7));
    polygon.push_back(ImageProcessing::ImagePoint(9, 5));
    polygon.push_back(ImageProcessing::ImagePoint(9, 8));
    polygon.push_back(ImageProcessing::ImagePoint(5, 8));

    PolygonSegments segments;
    ImageProcessing::FillPolygon(segments, polygon);
    ASSERT_EQ(6u, segments.GetSize());
    ASSERT_EQ(5, segments.GetY(0));  ASSERT_EQ(5, segments.GetX1(0));  ASSERT_EQ(5, segments.GetX2(0));
    ASSERT_EQ(5, segments.GetY(1));  ASSERT_EQ(9, segments.GetX1(1));  ASSERT_EQ(9, segments.GetX2(1));
    ASSERT_EQ(6, segments.GetY(2));  ASSERT_EQ(5, segments.GetX1(2));  ASSERT_EQ(6, segments.GetX2(2));
    ASSERT_EQ(6, segments.GetY(3));  ASSERT_EQ(8, segments.GetX1(3));  ASSERT_EQ(9, segments.GetX2(3));
    ASSERT_EQ(7, segments.GetY(4));  ASSERT_EQ(5, segments.GetX1(4));  ASSERT_EQ(9, segments.GetX2(4));
    ASSERT_EQ(8, segments.GetY(5));  ASSERT_EQ(5, segments.GetX1(5));  ASSERT_EQ(9, segments.GetX2(5));
  }

  {
    // Rectangle
    std::vector<ImageProcessing::ImagePoint> polygon;
    polygon.push_back(ImageProcessing::ImagePoint(10, 50));
    polygon.push_back(ImageProcessing::ImagePoint(200, 50));
    polygon.push_back(ImageProcessing::ImagePoint(200, 100));
    polygon.push_back(ImageProcessing::ImagePoint(10, 100));

    PolygonSegments segments;
    ImageProcessing::FillPolygon(segments, polygon);
    ASSERT_EQ(51u, segments.GetSize());

    for (size_t i = 0; i < segments.GetSize(); i++)
    {
      ASSERT_EQ(50 + static_cast<int>(i), segments.GetY(i));
      ASSERT_EQ(10, segments.GetX1(i));
      ASSERT_EQ(200, segments.GetX2(i));
    }
  }

  {
    // Shape that goes outside of the image on the 4 borders
    std::vector<ImageProcessing::ImagePoint> polygon;
    polygon.push_back(ImageProcessing::ImagePoint(5, -5));
    polygon.push_back(ImageProcessing::ImagePoint(40, 15));
    polygon.push_back(ImageProcessing::ImagePoint(20, 32));
    polygon.push_back(ImageProcessing::ImagePoint(-5, 27));

    Image image(PixelFormat_Grayscale8, 30, 30, false);
    ImageProcessing::Set(image, 0);
    ImageProcessing::FillPolygon(image, polygon, 255);

    unsigned int x1, x2;
    ASSERT_TRUE(LookupSegment(x1, x2, image, 0));   ASSERT_EQ(3u, x1);  ASSERT_EQ(14u, x2);
    ASSERT_TRUE(LookupSegment(x1, x2, image, 1));   ASSERT_EQ(3u, x1);  ASSERT_EQ(16u, x2);
    ASSERT_TRUE(LookupSegment(x1, x2, image, 2));   ASSERT_EQ(2u, x1);  ASSERT_EQ(18u, x2);
    ASSERT_TRUE(LookupSegment(x1, x2, image, 3));   ASSERT_EQ(2u, x1);  ASSERT_EQ(19u, x2);
    ASSERT_TRUE(LookupSegment(x1, x2, image, 4));   ASSERT_EQ(2u, x1);  ASSERT_EQ(21u, x2);
    ASSERT_TRUE(LookupSegment(x1, x2, image, 5));   ASSERT_EQ(1u, x1);  ASSERT_EQ(23u, x2);
    ASSERT_TRUE(LookupSegment(x1, x2, image, 6));   ASSERT_EQ(1u, x1);  ASSERT_EQ(25u, x2);
    ASSERT_TRUE(LookupSegment(x1, x2, image, 7));   ASSERT_EQ(1u, x1);  ASSERT_EQ(26u, x2);
    ASSERT_TRUE(LookupSegment(x1, x2, image, 8));   ASSERT_EQ(0u, x1);  ASSERT_EQ(28u, x2);
    ASSERT_TRUE(LookupSegment(x1, x2, image, 9));   ASSERT_EQ(0u, x1);  ASSERT_EQ(29u, x2);
    ASSERT_TRUE(LookupSegment(x1, x2, image, 10));  ASSERT_EQ(0u, x1);  ASSERT_EQ(29u, x2);
    ASSERT_TRUE(LookupSegment(x1, x2, image, 11));  ASSERT_EQ(0u, x1);  ASSERT_EQ(29u, x2);
    ASSERT_TRUE(LookupSegment(x1, x2, image, 12));  ASSERT_EQ(0u, x1);  ASSERT_EQ(29u, x2);
    ASSERT_TRUE(LookupSegment(x1, x2, image, 13));  ASSERT_EQ(0u, x1);  ASSERT_EQ(29u, x2);
    ASSERT_TRUE(LookupSegment(x1, x2, image, 14));  ASSERT_EQ(0u, x1);  ASSERT_EQ(29u, x2);
    ASSERT_TRUE(LookupSegment(x1, x2, image, 15));  ASSERT_EQ(0u, x1);  ASSERT_EQ(29u, x2);
    ASSERT_TRUE(LookupSegment(x1, x2, image, 16));  ASSERT_EQ(0u, x1);  ASSERT_EQ(29u, x2);
    ASSERT_TRUE(LookupSegment(x1, x2, image, 17));  ASSERT_EQ(0u, x1);  ASSERT_EQ(29u, x2);
    ASSERT_TRUE(LookupSegment(x1, x2, image, 18));  ASSERT_EQ(0u, x1);  ASSERT_EQ(29u, x2);
    ASSERT_TRUE(LookupSegment(x1, x2, image, 19));  ASSERT_EQ(0u, x1);  ASSERT_EQ(29u, x2);
    ASSERT_TRUE(LookupSegment(x1, x2, image, 20));  ASSERT_EQ(0u, x1);  ASSERT_EQ(29u, x2);
    ASSERT_TRUE(LookupSegment(x1, x2, image, 21));  ASSERT_EQ(0u, x1);  ASSERT_EQ(29u, x2);
    ASSERT_TRUE(LookupSegment(x1, x2, image, 22));  ASSERT_EQ(0u, x1);  ASSERT_EQ(29u, x2);
    ASSERT_TRUE(LookupSegment(x1, x2, image, 23));  ASSERT_EQ(0u, x1);  ASSERT_EQ(29u, x2);
    ASSERT_TRUE(LookupSegment(x1, x2, image, 24));  ASSERT_EQ(0u, x1);  ASSERT_EQ(29u, x2);
    ASSERT_TRUE(LookupSegment(x1, x2, image, 25));  ASSERT_EQ(0u, x1);  ASSERT_EQ(29u, x2);
    ASSERT_TRUE(LookupSegment(x1, x2, image, 26));  ASSERT_EQ(0u, x1);  ASSERT_EQ(28u, x2);
    ASSERT_TRUE(LookupSegment(x1, x2, image, 27));  ASSERT_EQ(0u, x1);  ASSERT_EQ(26u, x2);
    ASSERT_TRUE(LookupSegment(x1, x2, image, 28));  ASSERT_EQ(0u, x1);  ASSERT_EQ(25u, x2);
    ASSERT_TRUE(LookupSegment(x1, x2, image, 29));  ASSERT_EQ(5u, x1);  ASSERT_EQ(24u, x2);
  }
}
