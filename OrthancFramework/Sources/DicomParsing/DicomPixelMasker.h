/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
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

#include "ParsedDicomFile.h"
#include "../Images/ImageProcessing.h"

#include <set>


namespace Orthanc
{
  enum DicomPixelMaskerMode
  {
    DicomPixelMaskerMode_Fill,
    DicomPixelMaskerMode_MeanFilter,

    DicomPixelMaskerMode_Undefined
  };

  class ORTHANC_PUBLIC DicomPixelMasker : public boost::noncopyable
  {
    class BaseRegion
    {
      DicomPixelMaskerMode    mode_;
      int32_t                 fillValue_;     // pixel value
      uint32_t                filterWidth_;   // filter width
      std::set<std::string>   targetSeries_;
      std::set<std::string>   targetInstances_;

    protected:
      bool IsTargeted(const ParsedDicomFile& file) const;
      BaseRegion();

    public:
      
      virtual ~BaseRegion()
      {
      }

      virtual bool GetPixelMaskArea(unsigned int& x1, unsigned int& y1, unsigned int& x2, unsigned int& y2, const ParsedDicomFile& file, unsigned int frameIndex) const = 0;

      DicomPixelMaskerMode GetMode() const
      {
        return mode_;
      }

      int32_t GetFillValue() const
      {
        assert(mode_ == DicomPixelMaskerMode_Fill);
        return fillValue_;
      }

      int32_t GetFilterWidth() const
      {
        assert(mode_ == DicomPixelMaskerMode_MeanFilter);
        return filterWidth_;
      }

      void SetFillValue(int32_t value)
      {
        mode_ = DicomPixelMaskerMode_Fill;
        fillValue_ = value;
      }

      void SetMeanFilter(uint32_t value)
      {
        mode_ = DicomPixelMaskerMode_MeanFilter;
        filterWidth_ = value;
      }

      void SetTargetSeries(const std::set<std::string> targetSeries)
      {
        targetSeries_ = targetSeries;
      }

      void SetTargetInstances(const std::set<std::string> targetInstances)
      {
        targetInstances_ = targetInstances;
      }
    };

    class Region2D : public BaseRegion
    {
      unsigned int            x1_;
      unsigned int            y1_;
      unsigned int            x2_;
      unsigned int            y2_;

    public:
      Region2D(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2);
      
      virtual bool GetPixelMaskArea(unsigned int& x1, unsigned int& y1, unsigned int& x2, unsigned int& y2, const ParsedDicomFile& file, unsigned int frameIndex) const ORTHANC_OVERRIDE;
    };

    class Region3D : public BaseRegion
    {
      double             x1_;
      double             y1_;
      double             z1_;
      double             x2_;
      double             y2_;
      double             z2_;

    public:
      Region3D(double x1, double y1, double z1, double x2, double y2, double z2);

      virtual bool GetPixelMaskArea(unsigned int& x1, unsigned int& y1, unsigned int& x2, unsigned int& y2, const ParsedDicomFile& file, unsigned int frameIndex) const ORTHANC_OVERRIDE;
    };

  private:
    std::list<BaseRegion*>   regions_;

  public:
    DicomPixelMasker();
    
    ~DicomPixelMasker();

    void Apply(ParsedDicomFile& toModify);

    void ParseRequest(const Json::Value& request);
  };
}
