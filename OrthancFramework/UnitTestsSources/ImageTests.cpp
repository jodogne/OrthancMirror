/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2020 Osimis S.A., Belgium
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
#include "../Sources/SystemToolbox.h"
#include "../Sources/Toolbox.h"
#include "../Sources/TemporaryFile.h"

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

  w.WriteToFile("UnitTestsResults/ColorPattern.png", accessor);

  std::string f, md5;
  Orthanc::SystemToolbox::ReadFile(f, "UnitTestsResults/ColorPattern.png");
  Orthanc::Toolbox::ComputeMD5(md5, f);
  ASSERT_EQ("604e785f53c99cae6ea4584870b2c41d", md5);
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

  w.WriteToFile("UnitTestsResults/Gray8Pattern.png", accessor);

  std::string f, md5;
  Orthanc::SystemToolbox::ReadFile(f, "UnitTestsResults/Gray8Pattern.png");
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
  w.WriteToFile("UnitTestsResults/Gray16Pattern.png", accessor);

  std::string f, md5;
  Orthanc::SystemToolbox::ReadFile(f, "UnitTestsResults/Gray16Pattern.png");
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
  w.WriteToMemory(s, accessor);

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
    w.WriteToFile("UnitTestsResults/hello.jpg", img);

    w.WriteToMemory(s, img);
    Orthanc::SystemToolbox::WriteFile(s, "UnitTestsResults/hello2.jpg");

    std::string t;
    Orthanc::SystemToolbox::ReadFile(t, "UnitTestsResults/hello.jpg");
    ASSERT_EQ(s.size(), t.size());
    ASSERT_EQ(0, memcmp(s.c_str(), t.c_str(), s.size()));
  }

  {
    Orthanc::JpegReader r1, r2;
    r1.ReadFromFile("UnitTestsResults/hello.jpg");
    ASSERT_EQ(16u, r1.GetWidth());
    ASSERT_EQ(16u, r1.GetHeight());

    r2.ReadFromMemory(s);
    ASSERT_EQ(16u, r2.GetWidth());
    ASSERT_EQ(16u, r2.GetHeight());

    for (unsigned int y = 0; y < r1.GetHeight(); y++)
    {
      const uint8_t* p1 = reinterpret_cast<const uint8_t*>(r1.GetConstRow(y));
      const uint8_t* p2 = reinterpret_cast<const uint8_t*>(r2.GetConstRow(y));
      for (unsigned int x = 0; x < r1.GetWidth(); x++)
      {
        ASSERT_EQ(*p1, *p2);
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

  w.WriteToFile("UnitTestsResults/ColorPattern.pam", accessor);

  std::string f, md5;
  Orthanc::SystemToolbox::ReadFile(f, "UnitTestsResults/ColorPattern.pam");
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

  w.WriteToFile("UnitTestsResults/Gray8Pattern.pam", accessor);

  std::string f, md5;
  Orthanc::SystemToolbox::ReadFile(f, "UnitTestsResults/Gray8Pattern.pam");
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
  w.WriteToFile("UnitTestsResults/Gray16Pattern.pam", accessor);

  std::string f, md5;
  Orthanc::SystemToolbox::ReadFile(f, "UnitTestsResults/Gray16Pattern.pam");
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
  w.WriteToMemory(s, accessor);

  {
    Orthanc::PamReader r;
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

  {
    Orthanc::TemporaryFile tmp;
    tmp.Write(s);

    Orthanc::PamReader r2;
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

}
