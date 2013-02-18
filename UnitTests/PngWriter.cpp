#include "gtest/gtest.h"

#include <stdint.h>
#include "../Core/PngWriter.h"

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
}

TEST(PngWriter, Gray16Pattern)
{
  Orthanc::PngWriter w;
  int width = 256;
  int height = 256;
  int pitch = width * 2 + 17;

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
}
