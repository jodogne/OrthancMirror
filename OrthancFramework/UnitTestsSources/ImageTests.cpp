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

#include "../Sources/Images/Font.h"
#include "../Sources/Images/Image.h"
#include "../Sources/Images/ImageProcessing.h"
#include "../Sources/Images/JpegReader.h"
#include "../Sources/Images/JpegWriter.h"
#include "../Sources/Images/PngReader.h"
#include "../Sources/Images/PngWriter.h"
#include "../Sources/Images/PamReader.h"
#include "../Sources/Images/PamWriter.h"
#include "../Sources/Toolbox.h"

#if ORTHANC_SANDBOXED != 1
#  include "../Sources/SystemToolbox.h"
#  include "../Sources/TemporaryFile.h"
#endif

#include <stdint.h>


TEST(PngWriter, ColorPattern)
{
  Orthanc::PngWriter w;
  unsigned int width = 17;
  unsigned int height = 61;
  unsigned int pitch = width * 3;

  std::vector<uint8_t> image(height * pitch);
  for (unsigned int y = 0; y < height; y++)
  {
    uint8_t *p = &image[0] + y * pitch;
    for (unsigned int x = 0; x < width; x++, p += 3)
    {
      p[0] = (y % 3 == 0) ? 255 : 0;
      p[1] = (y % 3 == 1) ? 255 : 0;
      p[2] = (y % 3 == 2) ? 255 : 0;
    }
  }

  Orthanc::ImageAccessor accessor;
  accessor.AssignReadOnly(Orthanc::PixelFormat_RGB24, width, height, pitch, &image[0]);

  std::string f;

#if ORTHANC_SANDBOXED == 1
  Orthanc::IImageWriter::WriteToMemory(w, f, accessor);
#else
  Orthanc::IImageWriter::WriteToFile(w, "UnitTestsResults/ColorPattern.png", accessor);
  Orthanc::SystemToolbox::ReadFile(f, "UnitTestsResults/ColorPattern.png");
#endif

  std::string md5;
  Orthanc::Toolbox::ComputeMD5(md5, f);
  ASSERT_EQ("604e785f53c99cae6ea4584870b2c41d", md5);
}

TEST(PngWriter, Color16Pattern)
{
  Orthanc::PngWriter w;
  unsigned int width = 17;
  unsigned int height = 61;
  unsigned int pitch = width * 8;

  std::vector<uint8_t> image(height * pitch);
  for (unsigned int y = 0; y < height; y++)
  {
    uint8_t *p = &image[0] + y * pitch;
    for (unsigned int x = 0; x < width; x++, p += 8)
    {
      p[0] = (y % 8 == 0) ? 255 : 0;
      p[1] = (y % 8 == 1) ? 255 : 0;
      p[2] = (y % 8 == 2) ? 255 : 0;
      p[3] = (y % 8 == 3) ? 255 : 0;
      p[4] = (y % 8 == 4) ? 255 : 0;
      p[5] = (y % 8 == 5) ? 255 : 0;
      p[6] = (y % 8 == 6) ? 255 : 0;
      p[7] = (y % 8 == 7) ? 255 : 0;
    }
  }

  Orthanc::ImageAccessor accessor;
  accessor.AssignReadOnly(Orthanc::PixelFormat_RGBA64, width, height, pitch, &image[0]);

  std::string f;

#if ORTHANC_SANDBOXED == 1
  Orthanc::IImageWriter::WriteToMemory(w, f, accessor);
#else
  Orthanc::IImageWriter::WriteToFile(w, "UnitTestsResults/Color16Pattern.png", accessor);
  Orthanc::SystemToolbox::ReadFile(f, "UnitTestsResults/Color16Pattern.png");
#endif

  std::string md5;
  Orthanc::Toolbox::ComputeMD5(md5, f);
  ASSERT_EQ("1cca552b6bd152b6fdab35c4a9f02c2a", md5);
}


TEST(PngWriter, Gray8Pattern)
{
  Orthanc::PngWriter w;
  int width = 17;
  int height = 256;
  int pitch = width;

  std::vector<uint8_t> image(height * pitch);
  for (int y = 0; y < height; y++)
  {
    uint8_t *p = &image[0] + y * pitch;
    for (int x = 0; x < width; x++, p++)
    {
      *p = y;
    }
  }

  Orthanc::ImageAccessor accessor;
  accessor.AssignReadOnly(Orthanc::PixelFormat_Grayscale8, width, height, pitch, &image[0]);

  std::string f;
  
#if ORTHANC_SANDBOXED == 1
  Orthanc::IImageWriter::WriteToMemory(w, f, accessor);
#else
  Orthanc::IImageWriter::WriteToFile(w, "UnitTestsResults/Gray8Pattern.png", accessor);
  Orthanc::SystemToolbox::ReadFile(f, "UnitTestsResults/Gray8Pattern.png");
#endif

  std::string md5;
  Orthanc::Toolbox::ComputeMD5(md5, f);
  ASSERT_EQ("5a9b98bea3d0a6d983980cc38bfbcdb3", md5);
}

TEST(PngWriter, Gray16Pattern)
{
  Orthanc::PngWriter w;
  int width = 256;
  int height = 256;
  int pitch = width * 2 + 16;

  std::vector<uint8_t> image(height * pitch);

  int v = 0;
  for (int y = 0; y < height; y++)
  {
    uint16_t *p = reinterpret_cast<uint16_t*>(&image[0] + y * pitch);
    for (int x = 0; x < width; x++, p++, v++)
    {
      *p = v;
    }
  }

  Orthanc::ImageAccessor accessor;
  accessor.AssignReadOnly(Orthanc::PixelFormat_Grayscale16, width, height, pitch, &image[0]);

  std::string f;
  
#if ORTHANC_SANDBOXED == 1
  Orthanc::IImageWriter::WriteToMemory(w, f, accessor);
#else
  Orthanc::IImageWriter::WriteToFile(w, "UnitTestsResults/Gray16Pattern.png", accessor);
  Orthanc::SystemToolbox::ReadFile(f, "UnitTestsResults/Gray16Pattern.png");
#endif

  std::string md5;
  Orthanc::Toolbox::ComputeMD5(md5, f);
  ASSERT_EQ("0785866a08bf0a02d2eeff87f658571c", md5);
}

TEST(PngWriter, EndToEnd)
{
  Orthanc::PngWriter w;
  unsigned int width = 256;
  unsigned int height = 256;
  unsigned int pitch = width * 2 + 16;

  std::vector<uint8_t> image(height * pitch);

  int v = 0;
  for (unsigned int y = 0; y < height; y++)
  {
    uint16_t *p = reinterpret_cast<uint16_t*>(&image[0] + y * pitch);
    for (unsigned int x = 0; x < width; x++, p++, v++)
    {
      *p = v;
    }
  }

  Orthanc::ImageAccessor accessor;
  accessor.AssignReadOnly(Orthanc::PixelFormat_Grayscale16, width, height, pitch, &image[0]);

  std::string s;
  Orthanc::IImageWriter::WriteToMemory(w, s, accessor);

  {
    Orthanc::PngReader r;
    r.ReadFromMemory(s);

    ASSERT_EQ(r.GetFormat(), Orthanc::PixelFormat_Grayscale16);
    ASSERT_EQ(r.GetWidth(), width);
    ASSERT_EQ(r.GetHeight(), height);

    v = 0;
    for (unsigned int y = 0; y < height; y++)
    {
      const uint16_t *p = reinterpret_cast<const uint16_t*>((const uint8_t*) r.GetConstBuffer() + y * r.GetPitch());
      ASSERT_EQ(p, r.GetConstRow(y));
      for (unsigned int x = 0; x < width; x++, p++, v++)
      {
        ASSERT_EQ(*p, v);
      }
    }
  }

#if ORTHANC_SANDBOXED != 1
  {
    Orthanc::TemporaryFile tmp;
    tmp.Write(s);

    Orthanc::PngReader r2;
    r2.ReadFromFile(tmp.GetPath());

    ASSERT_EQ(r2.GetFormat(), Orthanc::PixelFormat_Grayscale16);
    ASSERT_EQ(r2.GetWidth(), width);
    ASSERT_EQ(r2.GetHeight(), height);

    v = 0;
    for (unsigned int y = 0; y < height; y++)
    {
      const uint16_t *p = reinterpret_cast<const uint16_t*>((const uint8_t*) r2.GetConstBuffer() + y * r2.GetPitch());
      ASSERT_EQ(p, r2.GetConstRow(y));
      for (unsigned int x = 0; x < width; x++, p++, v++)
      {
        ASSERT_EQ(*p, v);
      }
    }
  }
#endif
}




TEST(JpegWriter, Basic)
{
  std::string s;

  {
    Orthanc::Image img(Orthanc::PixelFormat_Grayscale8, 16, 16, false);
    for (unsigned int y = 0, value = 0; y < img.GetHeight(); y++)
    {
      uint8_t* p = reinterpret_cast<uint8_t*>(img.GetRow(y));
      for (unsigned int x = 0; x < img.GetWidth(); x++, p++)
      {
        *p = value++;
      }
    }

    Orthanc::JpegWriter w;
    Orthanc::IImageWriter::WriteToMemory(w, s, img);

#if ORTHANC_SANDBOXED != 1
    Orthanc::IImageWriter::WriteToFile(w, "UnitTestsResults/hello.jpg", img);
    Orthanc::SystemToolbox::WriteFile(s, "UnitTestsResults/hello2.jpg");

    std::string t;
    Orthanc::SystemToolbox::ReadFile(t, "UnitTestsResults/hello.jpg");
    ASSERT_EQ(s.size(), t.size());
    ASSERT_EQ(0, memcmp(s.c_str(), t.c_str(), s.size()));
#endif
  }

  {
    Orthanc::JpegReader r1;
    r1.ReadFromMemory(s);
    ASSERT_EQ(16u, r1.GetWidth());
    ASSERT_EQ(16u, r1.GetHeight());

#if ORTHANC_SANDBOXED != 1
    Orthanc::JpegReader r2;
    r2.ReadFromFile("UnitTestsResults/hello.jpg");
    ASSERT_EQ(16u, r2.GetWidth());
    ASSERT_EQ(16u, r2.GetHeight());
#endif

    unsigned int value = 0;
    for (unsigned int y = 0; y < r1.GetHeight(); y++)
    {
      const uint8_t* p1 = reinterpret_cast<const uint8_t*>(r1.GetConstRow(y));
#if ORTHANC_SANDBOXED != 1
      const uint8_t* p2 = reinterpret_cast<const uint8_t*>(r2.GetConstRow(y));
#endif
      for (unsigned int x = 0; x < r1.GetWidth(); x++, value++)
      {
        ASSERT_TRUE(*p1 == value ||
                    *p1 == value - 1 ||
                    *p1 == value + 1);  // Be tolerant to differences of +-1

#if ORTHANC_SANDBOXED != 1
        ASSERT_EQ(*p1, *p2);
        p2++;
#endif

        p1++;
      }
    }
  }
}


TEST(PamWriter, ColorPattern)
{
  Orthanc::PamWriter w;
  unsigned int width = 17;
  unsigned int height = 61;
  unsigned int pitch = width * 3;

  std::vector<uint8_t> image(height * pitch);
  for (unsigned int y = 0; y < height; y++)
  {
    uint8_t *p = &image[0] + y * pitch;
    for (unsigned int x = 0; x < width; x++, p += 3)
    {
      p[0] = (y % 3 == 0) ? 255 : 0;
      p[1] = (y % 3 == 1) ? 255 : 0;
      p[2] = (y % 3 == 2) ? 255 : 0;
    }
  }

  Orthanc::ImageAccessor accessor;
  accessor.AssignReadOnly(Orthanc::PixelFormat_RGB24, width, height, pitch, &image[0]);

  std::string f;

#if ORTHANC_SANDBOXED == 1
  Orthanc::IImageWriter::WriteToMemory(w, f, accessor);
#else
  Orthanc::IImageWriter::WriteToFile(w, "UnitTestsResults/ColorPattern.pam", accessor);
  Orthanc::SystemToolbox::ReadFile(f, "UnitTestsResults/ColorPattern.pam");
#endif

  std::string md5;
  Orthanc::Toolbox::ComputeMD5(md5, f);
  ASSERT_EQ("81a3441754e88969ebbe53e69891e841", md5);
}

TEST(PamWriter, Gray8Pattern)
{
  Orthanc::PamWriter w;
  int width = 17;
  int height = 256;
  int pitch = width;

  std::vector<uint8_t> image(height * pitch);
  for (int y = 0; y < height; y++)
  {
    uint8_t *p = &image[0] + y * pitch;
    for (int x = 0; x < width; x++, p++)
    {
      *p = y;
    }
  }

  Orthanc::ImageAccessor accessor;
  accessor.AssignReadOnly(Orthanc::PixelFormat_Grayscale8, width, height, pitch, &image[0]);

  std::string f;

#if ORTHANC_SANDBOXED == 1
  Orthanc::IImageWriter::WriteToMemory(w, f, accessor);
#else
  Orthanc::IImageWriter::WriteToFile(w, "UnitTestsResults/Gray8Pattern.pam", accessor);
  Orthanc::SystemToolbox::ReadFile(f, "UnitTestsResults/Gray8Pattern.pam");
#endif
  
  std::string md5;
  Orthanc::Toolbox::ComputeMD5(md5, f);
  ASSERT_EQ("7873c408d26a9d11dd1c1de5e69cc0a3", md5);
}

TEST(PamWriter, Gray16Pattern)
{
  Orthanc::PamWriter w;
  int width = 256;
  int height = 256;
  int pitch = width * 2 + 16;

  std::vector<uint8_t> image(height * pitch);

  int v = 0;
  for (int y = 0; y < height; y++)
  {
    uint16_t *p = reinterpret_cast<uint16_t*>(&image[0] + y * pitch);
    for (int x = 0; x < width; x++, p++, v++)
    {
      *p = v;
    }
  }

  Orthanc::ImageAccessor accessor;
  accessor.AssignReadOnly(Orthanc::PixelFormat_Grayscale16, width, height, pitch, &image[0]);

  std::string f;

#if ORTHANC_SANDBOXED == 1
  Orthanc::IImageWriter::WriteToMemory(w, f, accessor);
#else
  Orthanc::IImageWriter::WriteToFile(w, "UnitTestsResults/Gray16Pattern.pam", accessor);
  Orthanc::SystemToolbox::ReadFile(f, "UnitTestsResults/Gray16Pattern.pam");
#endif

  std::string md5;
  Orthanc::Toolbox::ComputeMD5(md5, f);
  ASSERT_EQ("b268772bf28f3b2b8520ff21c5e3dcb6", md5);
}

TEST(PamWriter, EndToEnd)
{
  Orthanc::PamWriter w;
  unsigned int width = 256;
  unsigned int height = 256;
  unsigned int pitch = width * 2 + 16;

  std::vector<uint8_t> image(height * pitch);

  int v = 0;
  for (unsigned int y = 0; y < height; y++)
  {
    uint16_t *p = reinterpret_cast<uint16_t*>(&image[0] + y * pitch);
    for (unsigned int x = 0; x < width; x++, p++, v++)
    {
      *p = v;
    }
  }

  Orthanc::ImageAccessor accessor;
  accessor.AssignReadOnly(Orthanc::PixelFormat_Grayscale16, width, height, pitch, &image[0]);

  std::string s;
  Orthanc::IImageWriter::WriteToMemory(w, s, accessor);

  {
    Orthanc::PamReader r(true);
    r.ReadFromMemory(s);

    ASSERT_EQ(r.GetFormat(), Orthanc::PixelFormat_Grayscale16);
    ASSERT_EQ(r.GetWidth(), width);
    ASSERT_EQ(r.GetHeight(), height);

    v = 0;
    for (unsigned int y = 0; y < height; y++)
    {
      const uint16_t *p = reinterpret_cast<const uint16_t*>
        ((const uint8_t*) r.GetConstBuffer() + y * r.GetPitch());
      ASSERT_EQ(p, r.GetConstRow(y));
      for (unsigned int x = 0; x < width; x++, p++, v++)
      {
        ASSERT_EQ(v, *p);
      }
    }
  }

  {
    // true means "enforce alignment by using a temporary buffer"
    Orthanc::PamReader r(true);
    r.ReadFromMemory(s);

    ASSERT_EQ(r.GetFormat(), Orthanc::PixelFormat_Grayscale16);
    ASSERT_EQ(r.GetWidth(), width);
    ASSERT_EQ(r.GetHeight(), height);

    v = 0;
    for (unsigned int y = 0; y < height; y++)
    {
      const uint16_t* p = reinterpret_cast<const uint16_t*>
        ((const uint8_t*)r.GetConstBuffer() + y * r.GetPitch());
      ASSERT_EQ(p, r.GetConstRow(y));
      for (unsigned int x = 0; x < width; x++, p++, v++)
      {
        ASSERT_EQ(v, *p);
      }
    }
  }

#if ORTHANC_SANDBOXED != 1
  {
    Orthanc::TemporaryFile tmp;
    tmp.Write(s);

    Orthanc::PamReader r2(true);
    r2.ReadFromFile(tmp.GetPath());

    ASSERT_EQ(r2.GetFormat(), Orthanc::PixelFormat_Grayscale16);
    ASSERT_EQ(r2.GetWidth(), width);
    ASSERT_EQ(r2.GetHeight(), height);

    v = 0;
    for (unsigned int y = 0; y < height; y++)
    {
      const uint16_t *p = reinterpret_cast<const uint16_t*>
        ((const uint8_t*) r2.GetConstBuffer() + y * r2.GetPitch());
      ASSERT_EQ(p, r2.GetConstRow(y));
      for (unsigned int x = 0; x < width; x++, p++, v++)
      {
        ASSERT_EQ(*p, v);
      }
    }
  }
#endif

#if ORTHANC_SANDBOXED != 1
  {
    Orthanc::TemporaryFile tmp;
    tmp.Write(s);

    // true means "enforce alignment by using a temporary buffer"
    Orthanc::PamReader r2(true);
    r2.ReadFromFile(tmp.GetPath());

    ASSERT_EQ(r2.GetFormat(), Orthanc::PixelFormat_Grayscale16);
    ASSERT_EQ(r2.GetWidth(), width);
    ASSERT_EQ(r2.GetHeight(), height);

    v = 0;
    for (unsigned int y = 0; y < height; y++)
    {
      const uint16_t* p = reinterpret_cast<const uint16_t*>
        ((const uint8_t*)r2.GetConstBuffer() + y * r2.GetPitch());
      ASSERT_EQ(p, r2.GetConstRow(y));
      for (unsigned int x = 0; x < width; x++, p++, v++)
      {
        ASSERT_EQ(*p, v);
      }
    }
  }
#endif
}


TEST(PngWriter, Gray16Then8)
{
  Orthanc::Image image16(Orthanc::PixelFormat_Grayscale16, 32, 32, false);
  Orthanc::Image image8(Orthanc::PixelFormat_Grayscale8, 32, 32, false);

  memset(image16.GetBuffer(), 0, image16.GetHeight() * image16.GetPitch());
  memset(image8.GetBuffer(), 0, image8.GetHeight() * image8.GetPitch());

  {
    Orthanc::PamWriter w;
    std::string s;
    Orthanc::IImageWriter::WriteToMemory(w, s, image16);
    Orthanc::IImageWriter::WriteToMemory(w, s, image8);  // No problem here
  }

  {
    Orthanc::PamWriter w;
    std::string s;
    Orthanc::IImageWriter::WriteToMemory(w, s, image8);
    Orthanc::IImageWriter::WriteToMemory(w, s, image16);  // No problem here
  }

  {
    Orthanc::PngWriter w;
    std::string s;
    Orthanc::IImageWriter::WriteToMemory(w, s, image8);
    Orthanc::IImageWriter::WriteToMemory(w, s, image16);  // No problem here
  }  

  {
    // The following call leads to "Invalid read of size 1" in Orthanc <= 1.9.2
    Orthanc::PngWriter w;
    std::string s;
    Orthanc::IImageWriter::WriteToMemory(w, s, image16);
    Orthanc::IImageWriter::WriteToMemory(w, s, image8);  // Problem here
  }  
}
