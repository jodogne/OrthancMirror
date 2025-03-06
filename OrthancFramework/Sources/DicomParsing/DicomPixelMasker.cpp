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


#include "../PrecompiledHeaders.h"

#include "DicomPixelMasker.h"
#include "../OrthancException.h"
#include "../SerializationToolbox.h"


namespace Orthanc
{
  static const char* KEY_MASK_PIXELS = "MaskPixelData";
  static const char* KEY_MASK_TYPE = "MaskType";
  static const char* KEY_MASK_TYPE_FILL = "Fill";
  static const char* KEY_MASK_TYPE_MEAN_FILTER = "MeanFilter";
  static const char* KEY_FILTER_WIDTH = "FilterWidth";
  static const char* KEY_FILL_VALUE = "FillValue";
  static const char* KEY_REGIONS = "Regions";
  static const char* KEY_REGION_TYPE = "RegionType";
  static const char* KEY_REGION_PIXELS = "Pixels";
  static const char* KEY_REGION_BOUNDING_BOX = "BoundingBox";
  static const char* KEY_ORIGIN = "Origin";
  static const char* KEY_END = "End";
  static const char* KEY_TARGET_SERIES = "TargetSeries";

  DicomPixelMasker::DicomPixelMasker()
  {
  }

  void DicomPixelMasker::Apply(ParsedDicomFile& toModify)
  {
    for (std::list<Region>::const_iterator r = regions_.begin(); r != regions_.end(); ++r)
    {
      ImageAccessor imageRegion;
      toModify.GetRawFrame(0)->GetRegion(imageRegion, r->x_, r->y_, r->width_, r->height_);

      if (r->mode_ == DicomPixelMaskerMode_MeanFilter)
      {
        ImageProcessing::MeanFilter(imageRegion, r->filterWidth_, r->filterWidth_);
      }
      else if (r->mode_ == DicomPixelMaskerMode_Fill)
      {
        ImageProcessing::Set(imageRegion, r->fillValue_);
      }
    }
  }

  void DicomPixelMasker::ParseRequest(const Json::Value& request)
  {
    if (request.isMember(KEY_MASK_PIXELS) && request[KEY_MASK_PIXELS].isObject())
    {
      const Json::Value& maskPixelsJson = request[KEY_MASK_PIXELS];

      if (maskPixelsJson.isMember(KEY_REGIONS) && maskPixelsJson[KEY_REGIONS].isArray())
      {
        const Json::Value& regionsJson = maskPixelsJson[KEY_REGIONS];

        for (Json::ArrayIndex i = 0; i < regionsJson.size(); ++i)
        {
          const Json::Value& regionJson = regionsJson[i];
          Region region;

          if (regionJson.isMember(KEY_MASK_TYPE) && regionJson[KEY_MASK_TYPE].isString())
          {
            if (regionJson[KEY_MASK_TYPE].asString() == KEY_MASK_TYPE_FILL)
            {
              region.mode_ = DicomPixelMaskerMode_Fill;

              if (regionJson.isMember(KEY_FILL_VALUE) && regionJson[KEY_FILL_VALUE].isInt())
              {
                region.fillValue_ = regionJson[KEY_FILL_VALUE].asInt();
              }
            }
            else if (regionJson[KEY_MASK_TYPE].asString() == KEY_MASK_TYPE_MEAN_FILTER)
            {
              region.mode_ = DicomPixelMaskerMode_MeanFilter;

              if (regionJson.isMember(KEY_FILTER_WIDTH) && regionJson[KEY_FILTER_WIDTH].isUInt())
              {
                region.filterWidth_ = regionJson[KEY_FILTER_WIDTH].asUInt();
              }
            }
            else
            {
              throw OrthancException(ErrorCode_BadFileFormat, std::string(KEY_MASK_TYPE) + " should be '" + KEY_MASK_TYPE_FILL +"' or '" + KEY_MASK_TYPE_MEAN_FILTER + "'.");
            }
          }

          if (regionJson.isMember(KEY_TARGET_SERIES) && regionJson[KEY_TARGET_SERIES].isArray())
          {
            SerializationToolbox::ReadListOfStrings(region.targetSeries_, regionJson, KEY_TARGET_SERIES);
          }

          if (regionJson.isMember(KEY_REGION_TYPE) && regionJson[KEY_REGION_TYPE].asString() == KEY_REGION_PIXELS)
          {
            if (regionJson.isMember(KEY_ORIGIN) && regionJson[KEY_ORIGIN].isArray() && regionJson[KEY_ORIGIN].size() == 2 &&
                regionJson.isMember(KEY_END) && regionJson[KEY_END].isArray() && regionJson[KEY_END].size() == 2)
            {
              region.x_ = regionJson[KEY_ORIGIN][0].asUInt();
              region.y_ = regionJson[KEY_ORIGIN][1].asUInt();
              region.width_ = regionJson[KEY_END][0].asUInt() - region.x_;
              region.height_ = regionJson[KEY_END][1].asUInt() - region.y_;
              
              regions_.push_back(region);
            }
          } 
          else if (regionJson.isMember(KEY_REGION_TYPE) && regionJson[KEY_REGION_TYPE].asString() == KEY_REGION_BOUNDING_BOX)
          {
            // TODO
            throw OrthancException(ErrorCode_NotImplemented);
          }
        }

      }

      // TODO: support multiple series + move this 
    }

  }
}
