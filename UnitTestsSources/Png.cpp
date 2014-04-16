#include "gtest/gtest.h"

#include <stdint.h>
#include "../Core/FileFormats/PngReader.h"
#include "../Core/FileFormats/PngWriter.h"
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

  w.WriteToFile("ColorPattern.png", width, height, pitch, Orthanc::PixelFormat_RGB24, &image[0]);

  std::string f, md5;
  Orthanc::Toolbox::ReadFile(f, "ColorPattern.png");
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

  w.WriteToFile("Gray8Pattern.png", width, height, pitch, Orthanc::PixelFormat_Grayscale8, &image[0]);

  std::string f, md5;
  Orthanc::Toolbox::ReadFile(f, "Gray8Pattern.png");
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

  w.WriteToFile("Gray16Pattern.png", width, height, pitch, Orthanc::PixelFormat_Grayscale16, &image[0]);

  std::string f, md5;
  Orthanc::Toolbox::ReadFile(f, "Gray16Pattern.png");
  Orthanc::Toolbox::ComputeMD5(md5, f);
  ASSERT_EQ("0785866a08bf0a02d2eeff87f658571c", md5);
}

TEST(PngWriter, EndToEnd)
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

  std::string s;
  w.WriteToMemory(s, width, height, pitch, Orthanc::PixelFormat_Grayscale16, &image[0]);

  {
    Orthanc::PngReader r;
    r.ReadFromMemory(s);

    ASSERT_EQ(r.GetFormat(), Orthanc::PixelFormat_Grayscale16);
    ASSERT_EQ(r.GetWidth(), width);
    ASSERT_EQ(r.GetHeight(), height);

    v = 0;
    for (int y = 0; y < height; y++)
    {
      uint16_t *p = reinterpret_cast<uint16_t*>((uint8_t*) r.GetBuffer() + y * r.GetPitch());
      ASSERT_EQ(p, r.GetBuffer(y));
      for (int x = 0; x < width; x++, p++, v++)
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
    for (int y = 0; y < height; y++)
    {
      uint16_t *p = reinterpret_cast<uint16_t*>((uint8_t*) r2.GetBuffer() + y * r2.GetPitch());
      ASSERT_EQ(p, r2.GetBuffer(y));
      for (int x = 0; x < width; x++, p++, v++)
      {
        ASSERT_EQ(*p, v);
      }
    }
  }
}
