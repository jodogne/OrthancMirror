/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
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


#pragma once

#include "../OrthancFramework.h"

#include "ImageAccessor.h"

#include <vector>
#include <stdint.h>
#include <boost/noncopyable.hpp>

namespace Orthanc
{
  class ORTHANC_PUBLIC ImageProcessing : public boost::noncopyable
  {
  public:
    class ORTHANC_PUBLIC ImagePoint
    {
    private:
      int32_t x_;
      int32_t y_;
      
    public:
      ImagePoint(int32_t x,
                 int32_t y);

      int32_t GetX() const;

      int32_t GetY() const;

      void Set(int32_t x, int32_t y);

      void ClipTo(int32_t minX,
                  int32_t maxX,
                  int32_t minY,
                  int32_t maxY);

      double GetDistanceTo(const ImagePoint& other) const;

      double GetDistanceToLine(double a,
                               double b,
                               double c) const; // where ax + by + c = 0 is the equation of the line
    };

    static void Copy(ImageAccessor& target,
                     const ImageAccessor& source);

    static void Convert(ImageAccessor& target,
                        const ImageAccessor& source);

    static void ApplyWindowing_Deprecated(ImageAccessor& target,
                                          const ImageAccessor& source,
                                          float windowCenter,
                                          float windowWidth,
                                          float rescaleSlope,
                                          float rescaleIntercept,
                                          bool invert);

    static void Set(ImageAccessor& image,
                    int64_t value);

    static void Set(ImageAccessor& image,
                    uint8_t red,
                    uint8_t green,
                    uint8_t blue,
                    uint8_t alpha);

    static void Set(ImageAccessor& image,
                    uint8_t red,
                    uint8_t green,
                    uint8_t blue,
                    ImageAccessor& alpha);

    static void ShiftRight(ImageAccessor& target,
                           unsigned int shift);

    static void ShiftLeft(ImageAccessor& target,
                          unsigned int shift);

    static void GetMinMaxIntegerValue(int64_t& minValue,
                                      int64_t& maxValue,
                                      const ImageAccessor& image);

    static void GetMinMaxFloatValue(float& minValue,
                                    float& maxValue,
                                    const ImageAccessor& image);

    static void AddConstant(ImageAccessor& image,
                            int64_t value);

    // "useRound" is expensive
    static void MultiplyConstant(ImageAccessor& image,
                                 float factor,
                                 bool useRound);

    // Computes "(x + offset) * scaling" inplace. "useRound" is expensive.
    static void ShiftScale(ImageAccessor& image,
                           float offset,
                           float scaling,
                           bool useRound);

    static void ShiftScale(ImageAccessor& target,
                           const ImageAccessor& source,
                           float offset,
                           float scaling,
                           bool useRound);

    // Computes "x * scaling + offset" inplace. "useRound" is expensive.
    static void ShiftScale2(ImageAccessor& image,
                            float offset,
                            float scaling,
                            bool useRound);

    static void ShiftScale2(ImageAccessor& target,
                            const ImageAccessor& source,
                            float offset,
                            float scaling,
                            bool useRound);

    static void Invert(ImageAccessor& image);

    static void Invert(ImageAccessor& image, int64_t maxValue);

    static void DrawLineSegment(ImageAccessor& image,
                                int x0,
                                int y0,
                                int x1,
                                int y1,
                                int64_t value);

    static void DrawLineSegment(ImageAccessor& image,
                                int x0,
                                int y0,
                                int x1,
                                int y1,
                                uint8_t red,
                                uint8_t green,
                                uint8_t blue,
                                uint8_t alpha);

    static void FillPolygon(ImageAccessor& image,
                            const std::vector<ImagePoint>& points,
                            int64_t value);

    static void Resize(ImageAccessor& target,
                       const ImageAccessor& source);

    static ImageAccessor* Halve(const ImageAccessor& source,
                                bool forceMinimalPitch);

    static void FlipX(ImageAccessor& image);

    static void FlipY(ImageAccessor& image);

    static void SeparableConvolution(ImageAccessor& image /* inplace */,
                                     const std::vector<float>& horizontal,
                                     size_t horizontalAnchor,
                                     const std::vector<float>& vertical,
                                     size_t verticalAnchor,
                                     bool useRound /* this is expensive */);

    static void SmoothGaussian5x5(ImageAccessor& image,
                                  bool useRound /* this is expensive */);

    static void FitSize(ImageAccessor& target,
                        const ImageAccessor& source);

    // Resize the image to the given width/height. The resized image
    // occupies the entire canvas (aspect ratio is not preserved).
    static ImageAccessor* FitSize(const ImageAccessor& source,
                                  unsigned int width,
                                  unsigned int height);

    // Resize an image, but keeps its original aspect ratio. Zeros are
    // added around the image to reach the specified size.
    static ImageAccessor* FitSizeKeepAspectRatio(const ImageAccessor& source,
                                                 unsigned int width,
                                                 unsigned int height);

    // https://en.wikipedia.org/wiki/YCbCr#JPEG_conversion
    static void ConvertJpegYCbCrToRgb(ImageAccessor& image /* inplace */);

    static void SwapEndianness(ImageAccessor& image /* inplace */);
  };
}
