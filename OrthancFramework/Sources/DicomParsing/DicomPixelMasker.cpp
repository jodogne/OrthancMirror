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
#include "../Logging.h"

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
  static const char* KEY_REGION_2D = "2D";
  static const char* KEY_REGION_3D = "3D";
  static const char* KEY_ORIGIN = "Origin";
  static const char* KEY_END = "End";
  static const char* KEY_TARGET_SERIES = "TargetSeries";
  static const char* KEY_TARGET_INSTANCES = "TargetInstances";

  DicomPixelMasker::DicomPixelMasker()
  {
  }

  DicomPixelMasker::~DicomPixelMasker()
  {
    for (std::list<BaseRegion*>::iterator it = regions_.begin(); it != regions_.end(); ++it)
    {
      delete *it;
    }
  }

  DicomPixelMasker::BaseRegion::BaseRegion() :
    mode_(DicomPixelMaskerMode_Undefined),
    fillValue_(0),
    filterWidth_(0)
  {
  }

  DicomPixelMasker::Region2D::Region2D(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2) :
    x1_(x1),
    y1_(y1),
    x2_(x2),
    y2_(y2)
  {
  }

  DicomPixelMasker::Region3D::Region3D(double x1, double y1, double z1, double x2, double y2, double z2) :
    x1_(x1),
    y1_(y1),
    z1_(z1),
    x2_(x2),
    y2_(y2),
    z2_(z2)
  {
  }

  bool DicomPixelMasker::BaseRegion::IsTargeted(const ParsedDicomFile& file) const
  {
    DicomInstanceHasher hasher = file.GetHasher();
    const std::string& seriesId = hasher.HashSeries();
    const std::string& instanceId = hasher.HashInstance();

    if (targetSeries_.size() > 0 && targetSeries_.find(seriesId) == targetSeries_.end())
    {
      return false;
    }

    if (targetInstances_.size() > 0 && targetInstances_.find(instanceId) == targetInstances_.end())
    {
      return false;
    }

    return true;
  }

  bool DicomPixelMasker::Region2D::GetPixelMaskArea(unsigned int& x1, unsigned int& y1, unsigned int& x2, unsigned int& y2, const ParsedDicomFile& file, unsigned int frameIndex) const
  {
    if (IsTargeted(file))
    {
      x1 = x1_;
      y1 = y1_;
      x2 = x2_;
      y2 = y2_;
      return true;
    }

    return false;
  }

  static void GetDoubleVector(std::vector<double>& target, const ParsedDicomFile& file, const DicomTag& tag, size_t expectedSize)
  {
    target.clear();

    std::string str;
    if (!file.GetTagValue(str, tag))
    {
      throw OrthancException(ErrorCode_InexistentTag, "Unable to perform 3D -> 2D conversion, missing tag" + tag.Format());
    }

    std::vector<std::string> strVector;
    Toolbox::SplitString(strVector, str, '\\');

    if (strVector.size() != expectedSize)
    {
      throw OrthancException(ErrorCode_InexistentTag, "Unable to perform 3D -> 2D conversion, tag " + tag.Format() + " length is invalid");
    }

    for (size_t i = 0; i < strVector.size(); ++i)
    {
      try
      {
        target.push_back(boost::lexical_cast<double>(strVector[i]));
      }
      catch (boost::bad_lexical_cast&)
      {
        throw OrthancException(ErrorCode_InexistentTag, "Unable to perform 3D -> 2D conversion, tag " + tag.Format() + " contains invalid value " + strVector[i]);
      }
    }
  }

  bool DicomPixelMasker::Region3D::GetPixelMaskArea(unsigned int& x1, unsigned int& y1, unsigned int& x2, unsigned int& y2, const ParsedDicomFile& file, unsigned int frameIndex) const
  {
    if (IsTargeted(file))
    {
      std::vector<double> imagePositionPatient;
      std::vector<double> imageOrientationPatient;
      std::vector<double> pixelSpacing;

      GetDoubleVector(imagePositionPatient, file, DICOM_TAG_IMAGE_POSITION_PATIENT, 3);
      GetDoubleVector(imageOrientationPatient, file, DICOM_TAG_IMAGE_ORIENTATION_PATIENT, 6);
      GetDoubleVector(pixelSpacing, file, DICOM_TAG_PIXEL_SPACING, 2);

      // note: To simplify, for the z, we only check that imagePositionPatient is between the authorized z values.
      //       This won't be perfectly true for weird images with slices that are not parallel but let's wait for someone to complain ...
      if (imagePositionPatient[2] < std::min(z1_, z2_) || 
          imagePositionPatient[2] > std::max(z1_, z2_))
      {
        return false;
      }
      
      double deltaX1 = x1_ - imagePositionPatient[0];
      double deltaY1 = y1_ - imagePositionPatient[1];
      double deltaZ1 = z1_ - imagePositionPatient[2];
      double deltaX2 = x2_ - imagePositionPatient[0];
      double deltaY2 = y2_ - imagePositionPatient[1];
      double deltaZ2 = z2_ - imagePositionPatient[2];

      double ix1 = (deltaX1 * imageOrientationPatient[0] + deltaY1 * imageOrientationPatient[1] + deltaZ1 * imageOrientationPatient[2]) / pixelSpacing[0];
      double iy1 = (deltaX1 * imageOrientationPatient[3] + deltaY1 * imageOrientationPatient[4] + deltaZ1 * imageOrientationPatient[5]) / pixelSpacing[1];
      double ix2 = (deltaX2 * imageOrientationPatient[0] + deltaY2 * imageOrientationPatient[1] + deltaZ2 * imageOrientationPatient[2]) / pixelSpacing[0];
      double iy2 = (deltaX2 * imageOrientationPatient[3] + deltaY2 * imageOrientationPatient[4] + deltaZ2 * imageOrientationPatient[5]) / pixelSpacing[1];

      std::string strRows;
      std::string strColumns;

      if (!file.GetTagValue(strRows, DICOM_TAG_ROWS) || !file.GetTagValue(strColumns, DICOM_TAG_COLUMNS))
      {
        throw OrthancException(ErrorCode_InexistentTag, "Unable to perform 3D -> 2D conversion, missing ROWS or COLUMNS tag");
      }

      // clip on image size
      double rows = boost::lexical_cast<double>(strRows);
      double columns = boost::lexical_cast<double>(strColumns);

      x1 = static_cast<unsigned int>(std::max(0.0, std::min(ix1, ix2)));
      y1 = static_cast<unsigned int>(std::max(0.0, std::min(iy1, iy2)));
      x2 = static_cast<unsigned int>(std::min(columns, std::max(ix1, ix2)));
      y2 = static_cast<unsigned int>(std::min(rows, std::max(iy1, iy2)));

      return true;
    }
    
    return false;
  }

  void DicomPixelMasker::Apply(ParsedDicomFile& toModify)
  {
    for (std::list<BaseRegion*>::const_iterator itr = regions_.begin(); itr != regions_.end(); ++itr)
    {
      const BaseRegion* r = *itr;

      for (unsigned int i = 0; i < toModify.GetFramesCount(); ++i)
      {
        unsigned int x1, y1, x2, y2;

        if (r->GetPixelMaskArea(x1, y1, x2, y2, toModify, i))
        {
          ImageAccessor imageRegion;
          toModify.GetRawFrame(i)->GetRegion(imageRegion, x1, y1, x2 - x1, y2 - y1);

          if (r->GetMode() == DicomPixelMaskerMode_MeanFilter)
          {
            ImageProcessing::MeanFilter(imageRegion, r->GetFilterWidth(), r->GetFilterWidth());
          }
          else if (r->GetMode() == DicomPixelMaskerMode_Fill)
          {
            ImageProcessing::Set(imageRegion, r->GetFillValue());
          }
        }
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

          std::unique_ptr<BaseRegion> region;
          
          if (regionJson.isMember(KEY_REGION_TYPE) && regionJson[KEY_REGION_TYPE].isString())
          {
            if (regionJson[KEY_REGION_TYPE].asString() == KEY_REGION_2D)
            {
              if (regionJson.isMember(KEY_ORIGIN) && regionJson[KEY_ORIGIN].isArray() && regionJson[KEY_ORIGIN].size() == 2 &&
                  regionJson.isMember(KEY_END) && regionJson[KEY_END].isArray() && regionJson[KEY_END].size() == 2)
              {
                unsigned int x = regionJson[KEY_ORIGIN][0].asUInt();
                unsigned int y = regionJson[KEY_ORIGIN][1].asUInt();
                unsigned int width = regionJson[KEY_END][0].asUInt() - x;
                unsigned int height = regionJson[KEY_END][1].asUInt() - y;
                
                region.reset(new Region2D(x, y, width, height));
              }
              else
              {
                throw OrthancException(ErrorCode_BadFileFormat, "2D Region: invalid coordinates");
              }

            }
            else if (regionJson[KEY_REGION_TYPE].asString() == KEY_REGION_3D)
            {
              if (regionJson.isMember(KEY_ORIGIN) && regionJson[KEY_ORIGIN].isArray() && regionJson[KEY_ORIGIN].size() == 3 &&
                  regionJson.isMember(KEY_END) && regionJson[KEY_END].isArray() && regionJson[KEY_END].size() == 3)
              {
                double x1 = regionJson[KEY_ORIGIN][0].asDouble();
                double y1 = regionJson[KEY_ORIGIN][1].asDouble();
                double z1 = regionJson[KEY_ORIGIN][2].asDouble();
                double x2 = regionJson[KEY_END][0].asDouble();
                double y2 = regionJson[KEY_END][1].asDouble();
                double z2 = regionJson[KEY_END][2].asDouble();
                
                region.reset(new Region3D(x1, y1, z1, x2, y2, z2));
              }
              else
              {
                throw OrthancException(ErrorCode_BadFileFormat, "2D Region: invalid coordinates");
              }
            }
            else
            {
              throw OrthancException(ErrorCode_BadFileFormat, std::string(KEY_REGION_TYPE) + " unrecognized value '" + regionJson[KEY_REGION_TYPE].asString() +"'");
            }
          }

          if (regionJson.isMember(KEY_MASK_TYPE) && regionJson[KEY_MASK_TYPE].isString())
          {
            if (regionJson[KEY_MASK_TYPE].asString() == KEY_MASK_TYPE_FILL)
            {
              if (regionJson.isMember(KEY_FILL_VALUE) && regionJson[KEY_FILL_VALUE].isInt())
              {
                region->SetFillValue(regionJson[KEY_FILL_VALUE].asInt());
              }
            }
            else if (regionJson[KEY_MASK_TYPE].asString() == KEY_MASK_TYPE_MEAN_FILTER)
            {
              if (regionJson.isMember(KEY_FILTER_WIDTH) && regionJson[KEY_FILTER_WIDTH].isUInt())
              {
                region->SetMeanFilter(regionJson[KEY_FILTER_WIDTH].asUInt());
              }
            }
            else
            {
              throw OrthancException(ErrorCode_BadFileFormat, std::string(KEY_MASK_TYPE) + " should be '" + KEY_MASK_TYPE_FILL +"' or '" + KEY_MASK_TYPE_MEAN_FILTER + "'.");
            }
          }

          if (regionJson.isMember(KEY_TARGET_SERIES) && regionJson[KEY_TARGET_SERIES].isArray())
          {
            std::set<std::string> targetSeries;
            SerializationToolbox::ReadSetOfStrings(targetSeries, regionJson, KEY_TARGET_SERIES);
            region->SetTargetSeries(targetSeries);
          }

          if (regionJson.isMember(KEY_TARGET_INSTANCES) && regionJson[KEY_TARGET_INSTANCES].isArray())
          {
            std::set<std::string> targetInstances;
            SerializationToolbox::ReadSetOfStrings(targetInstances, regionJson, KEY_TARGET_INSTANCES);
            region->SetTargetInstances(targetInstances);
          }

          regions_.push_back(region.release());
        }
      }
    }

  }
}
