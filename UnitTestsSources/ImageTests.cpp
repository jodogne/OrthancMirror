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

#include <stdint.h>
#include "../Core/ImageFormats/ImageBuffer.h"
#include "../Core/ImageFormats/PngReader.h"
#include "../Core/ImageFormats/PngWriter.h"
#include "../Core/ImageFormats/JpegWriter.h"
#include "../Core/Toolbox.h"
#include "../Core/Uuid.h"


TEST(PngWriter, ColorPattern)
{
  Orthanc::PngWriter w;
  int width = 17;
  int height = 61;
  int pitch = width * 3;

  std::vector<uint8_t> image(height * pitch);
  for (int y = 0; y < height; y++)
  {
    uint8_t *p = &image[0] + y * pitch;
    for (int x = 0; x < width; x++, p += 3)
    {
      p[0] = (y % 3 == 0) ? 255 : 0;
      p[1] = (y % 3 == 1) ? 255 : 0;
      p[2] = (y % 3 == 2) ? 255 : 0;
    }
  }

  w.WriteToFile("UnitTestsResults/ColorPattern.png", width, height, pitch, Orthanc::PixelFormat_RGB24, &image[0]);

  std::string f, md5;
  Orthanc::Toolbox::ReadFile(f, "UnitTestsResults/ColorPattern.png");
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

  w.WriteToFile("UnitTestsResults/Gray8Pattern.png", width, height, pitch, Orthanc::PixelFormat_Grayscale8, &image[0]);

  std::string f, md5;
  Orthanc::Toolbox::ReadFile(f, "UnitTestsResults/Gray8Pattern.png");
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

  w.WriteToFile("UnitTestsResults/Gray16Pattern.png", width, height, pitch, Orthanc::PixelFormat_Grayscale16, &image[0]);

  std::string f, md5;
  Orthanc::Toolbox::ReadFile(f, "UnitTestsResults/Gray16Pattern.png");
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

  std::string s;
  w.WriteToMemory(s, width, height, pitch, Orthanc::PixelFormat_Grayscale16, &image[0]);

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
    Orthanc::Toolbox::TemporaryFile tmp;
    Orthanc::Toolbox::WriteFile(s, tmp.GetPath());

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
  Orthanc::ImageBuffer img(16, 16, Orthanc::PixelFormat_Grayscale8);
  Orthanc::ImageAccessor accessor = img.GetAccessor();
  for (unsigned int y = 0, value = 0; y < img.GetHeight(); y++)
  {
    uint8_t* p = reinterpret_cast<uint8_t*>(accessor.GetRow(y));
    for (unsigned int x = 0; x < img.GetWidth(); x++, p++)
    {
      *p = value++;
    }
  }

  Orthanc::JpegWriter w;
  w.WriteToFile("UnitTestsResults/hello.jpg", accessor);

  std::string s;
  w.WriteToMemory(s, accessor);
  Orthanc::Toolbox::WriteFile(s, "UnitTestsResults/hello2.jpg");
}
