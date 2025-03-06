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

#include <list>


namespace Orthanc
{
  enum DicomPixelMaskerMode
  {
    DicomPixelMaskerMode_Fill,
    DicomPixelMaskerMode_MeanFilter
  };

  class ORTHANC_PUBLIC DicomPixelMasker : public boost::noncopyable
  {
    struct Region
    {
      unsigned int            x_;
      unsigned int            y_;
      unsigned int            width_;
      unsigned int            height_;
      DicomPixelMaskerMode    mode_;
      int32_t                 fillValue_;  // pixel value
      uint32_t                filterWidth_;  // filter width
      std::list<std::string>  targetSeries_;

      Region() :
        x_(0),
        y_(0),
        width_(0),
        height_(0),
        mode_(DicomPixelMaskerMode_Fill),
        fillValue_(0),
        filterWidth_(0)
      {
      }
    };

  private:
    std::list<Region>   regions_;

  public:
    DicomPixelMasker();

    void Apply(ParsedDicomFile& toModify);

    void ParseRequest(const Json::Value& request);
  };
}
