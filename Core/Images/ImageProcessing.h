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


#pragma once

#include "ImageAccessor.h"
#include <vector>

#include <stdint.h>
#include <algorithm>

namespace Orthanc
{
  namespace ImageProcessing
  {
    class ImagePoint
    {
      int32_t x_;
      int32_t y_;
      
     public:
      ImagePoint(int32_t x, int32_t y)
        : x_(x),
          y_(y)
      {
      }

      int32_t GetX() const {return x_;}

      int32_t GetY() const {return y_;}

      void Set(int32_t x, int32_t y)
      {
        x_ = x;
        y_ = y;
      }

      void ClipTo(int32_t minX, int32_t maxX, int32_t minY, int32_t maxY)
      {
        x_ = std::max(minX, std::min(maxX, x_));
        y_ = std::max(minY, std::min(maxY, y_));
      }

      double GetDistanceTo(const ImagePoint& other) const;

      double GetDistanceToLine(double a, double b, double c) const; // where ax + by + c = 0 is the equation of the line
    };

    void Copy(ImageAccessor& target,
              const ImageAccessor& source);

    void Convert(ImageAccessor& target,
                 const ImageAccessor& source);

    void ApplyWindowing_Deprecated(ImageAccessor& target,
                                   const ImageAccessor& source,
                                   float windowCenter,
                                   float windowWidth,
                                   float rescaleSlope,
                                   float rescaleIntercept,
                                   bool invert);

    void Set(ImageAccessor& image,
             int64_t value);

    void Set(ImageAccessor& image,
             uint8_t red,
             uint8_t green,
             uint8_t blue,
             uint8_t alpha);

    void Set(ImageAccessor& image,
             uint8_t red,
             uint8_t green,
             uint8_t blue,
             ImageAccessor& alpha);

    void ShiftRight(ImageAccessor& target,
                    unsigned int shift);

    void ShiftLeft(ImageAccessor& target,
                   unsigned int shift);

    void GetMinMaxIntegerValue(int64_t& minValue,
                               int64_t& maxValue,
                               const ImageAccessor& image);

    void GetMinMaxFloatValue(float& minValue,
                             float& maxValue,
                             const ImageAccessor& image);

    void AddConstant(ImageAccessor& image,
                     int64_t value);

    // "useRound" is expensive
    void MultiplyConstant(ImageAccessor& image,
                          float factor,
                          bool useRound);

    // Computes "(x + offset) * scaling" inplace. "useRound" is expensive.
    void ShiftScale(ImageAccessor& image,
                    float offset,
                    float scaling,
                    bool useRound);

    void ShiftScale(ImageAccessor& target,
                    const ImageAccessor& source,
                    float offset,
                    float scaling,
                    bool useRound);

    void Invert(ImageAccessor& image);

    void Invert(ImageAccessor& image, int64_t maxValue);

    void DrawLineSegment(ImageAccessor& image,
                         int x0,
                         int y0,
                         int x1,
                         int y1,
                         int64_t value);

    void DrawLineSegment(ImageAccessor& image,
                         int x0,
                         int y0,
                         int x1,
                         int y1,
                         uint8_t red,
                         uint8_t green,
                         uint8_t blue,
                         uint8_t alpha);

    void FillPolygon(ImageAccessor& image,
                     const std::vector<ImagePoint>& points,
                     int64_t value);

    void Resize(ImageAccessor& target,
                const ImageAccessor& source);

    ImageAccessor* Halve(const ImageAccessor& source,
                         bool forceMinimalPitch);

    void FlipX(ImageAccessor& image);

    void FlipY(ImageAccessor& image);

    void SeparableConvolution(ImageAccessor& image /* inplace */,
                              const std::vector<float>& horizontal,
                              size_t horizontalAnchor,
                              const std::vector<float>& vertical,
                              size_t verticalAnchor);

    void SmoothGaussian5x5(ImageAccessor& image);

    void FitSize(ImageAccessor& target,
                 const ImageAccessor& source);
    
    ImageAccessor* FitSize(const ImageAccessor& source,
                           unsigned int width,
                           unsigned int height);
  }
}
