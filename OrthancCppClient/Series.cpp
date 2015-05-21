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


#include "../Core/PrecompiledHeaders.h"
#include "Series.h"

#include "OrthancConnection.h"

#include <set>
#include <boost/lexical_cast.hpp>

namespace OrthancClient
{
  namespace
  {
    class SliceLocator
    {
    private:
      float normal_[3];

    public:
      SliceLocator(Instance& someSlice)
      {
        /**
         * Compute the slice normal from Image Orientation Patient.
         * http://nipy.sourceforge.net/nibabel/dicom/dicom_orientation.html#dicom-z-from-slice
         * http://dicomiseasy.blogspot.be/2013/06/getting-oriented-using-image-plane.html
         * http://www.itk.org/pipermail/insight-users/2003-September/004762.html
         **/

        std::vector<float> cosines;
        someSlice.SplitVectorOfFloats(cosines, "ImageOrientationPatient");  // 0020-0037

        if (cosines.size() != 6)
        {
          throw OrthancClientException(Orthanc::ErrorCode_BadFileFormat);
        }

        normal_[0] = cosines[1] * cosines[5] - cosines[2] * cosines[4];
        normal_[1] = cosines[2] * cosines[3] - cosines[0] * cosines[5];
        normal_[2] = cosines[0] * cosines[4] - cosines[1] * cosines[3];
      }


      /**
       * Compute the distance of some slice along the slice normal.
       **/
      float ComputeSliceLocation(Instance& instance) const
      {
        std::vector<float> ipp;
        instance.SplitVectorOfFloats(ipp, "ImagePositionPatient");  // 0020-0032
        if (ipp.size() != 3)
        {
          throw OrthancClientException(Orthanc::ErrorCode_BadFileFormat);
        }

        float dist = 0;

        for (int i = 0; i < 3; i++)
        {
          dist += normal_[i] * ipp[i];
        }

        return dist;
      }
    };

    class ImageDownloadCommand : public Orthanc::ICommand
    {
    private:
      Orthanc::PixelFormat format_;
      Orthanc::ImageExtractionMode mode_;
      Instance& instance_;
      void* target_;
      size_t lineStride_;

    public:
      ImageDownloadCommand(Instance& instance, 
                           Orthanc::PixelFormat format,
                           Orthanc::ImageExtractionMode mode,
                           void* target,
                           size_t lineStride) :
        format_(format),
        mode_(mode),
        instance_(instance),
        target_(target),
        lineStride_(lineStride)
      {
        instance_.SetImageExtractionMode(mode);
      }

      virtual bool Execute()
      {
        using namespace Orthanc;

        unsigned int width = instance_.GetHeight();

        for (unsigned int y = 0; y < instance_.GetHeight(); y++)
        {
          uint8_t* p = reinterpret_cast<uint8_t*>(target_) + y * lineStride_;

          if (instance_.GetPixelFormat() == format_)
          {
            memcpy(p, instance_.GetBuffer(y), GetBytesPerPixel(instance_.GetPixelFormat()) * instance_.GetWidth());
          }
          else if (instance_.GetPixelFormat() == PixelFormat_Grayscale8 &&
                   format_ == PixelFormat_RGB24)
          {
            const uint8_t* s = reinterpret_cast<const uint8_t*>(instance_.GetBuffer(y));
            for (unsigned int x = 0; x < width; x++, s++, p += 3)
            {
              p[0] = *s;
              p[1] = *s;
              p[2] = *s;
            }
          }
          else
          {
            throw OrthancClientException(ErrorCode_NotImplemented);
          }
        }

        // Do not keep the image in memory, as we are loading 3D images
        instance_.DiscardImage();

        return true;
      }
    };


    class ProgressToFloatListener : public Orthanc::ThreadedCommandProcessor::IListener
    {
    private:
      float* target_;

    public:
      ProgressToFloatListener(float* target) : target_(target)
      {
      }

      virtual void SignalProgress(unsigned int current,
                                  unsigned int total)
      {
        if (total == 0)
        {
          *target_ = 0;
        }
        else
        {
          *target_ = static_cast<float>(current) / static_cast<float>(total);
        }
      }

      virtual void SignalSuccess(unsigned int total)
      {
        *target_ = 1;
      }

      virtual void SignalFailure()
      {
        *target_ = 0;
      }

      virtual void SignalCancel()
      {
        *target_ = 0;
      }
    };

  }


  void Series::Check3DImage()
  {
    if (!Is3DImage())
    {
      throw OrthancClientException(Orthanc::ErrorCode_NotImplemented);
    }
  }

  bool Series::Is3DImageInternal()
  {
    try
    {
      if (GetInstanceCount() == 0)
      {
        // Empty image, use some default value (should never happen)
        voxelSizeX_ = 1;
        voxelSizeY_ = 1;
        voxelSizeZ_ = 1;
        sliceThickness_ = 1;

        return true;
      }

      // Choose a reference slice
      Instance& reference = GetInstance(0);

      // Check that all the child instances share the same 3D parameters
      for (unsigned int i = 0; i < GetInstanceCount(); i++)
      {
        Instance& i2 = GetInstance(i);

        if (std::string(reference.GetTagAsString("Columns")) != std::string(i2.GetTagAsString("Columns")) ||
            std::string(reference.GetTagAsString("Rows")) != std::string(i2.GetTagAsString("Rows")) ||
            std::string(reference.GetTagAsString("ImageOrientationPatient")) != std::string(i2.GetTagAsString("ImageOrientationPatient")) ||
            std::string(reference.GetTagAsString("SliceThickness")) != std::string(i2.GetTagAsString("SliceThickness")) ||
            std::string(reference.GetTagAsString("PixelSpacing")) != std::string(i2.GetTagAsString("PixelSpacing")))
        {
          return false;
        }              
      }


      // Extract X/Y voxel size and slice thickness
      std::string s = GetInstance(0).GetTagAsString("PixelSpacing");  // 0028-0030
      size_t pos = s.find('\\');
      assert(pos != std::string::npos);
      std::string sy = s.substr(0, pos);
      std::string sx = s.substr(pos + 1);

      try
      {
        voxelSizeX_ = boost::lexical_cast<float>(sx);
        voxelSizeY_ = boost::lexical_cast<float>(sy);
      }
      catch (boost::bad_lexical_cast)
      {
        throw OrthancClientException(Orthanc::ErrorCode_BadFileFormat);
      }

      sliceThickness_ = GetInstance(0).GetTagAsFloat("SliceThickness");  // 0018-0050


      // Compute the location of each slice to extract the voxel size along Z
      voxelSizeZ_ = std::numeric_limits<float>::infinity();

      SliceLocator locator(reference);
      float referenceSliceLocation = locator.ComputeSliceLocation(reference);

      std::set<float> l;
      for (unsigned int i = 0; i < GetInstanceCount(); i++)
      {
        float location = locator.ComputeSliceLocation(GetInstance(i));
        float distanceToReferenceSlice = fabs(location - referenceSliceLocation);

        l.insert(location);

        if (distanceToReferenceSlice > std::numeric_limits<float>::epsilon() &&
            distanceToReferenceSlice < voxelSizeZ_)
        {
          voxelSizeZ_ = distanceToReferenceSlice;
        }
      }


      // Make sure that 2 slices do not share the same Z location
      return l.size() == GetInstanceCount();
    }
    catch (OrthancClientException)
    {
      return false;
    }
  }

  void Series::ReadSeries()
  {
    Orthanc::HttpClient client(connection_.GetHttpClient());

    client.SetUrl(std::string(connection_.GetOrthancUrl()) + "/series/" + id_);
    Json::Value v;
    if (!client.Apply(series_))
    {
      throw OrthancClientException(Orthanc::ErrorCode_NetworkProtocol);
    }
  }

  Orthanc::IDynamicObject* Series::GetFillerItem(size_t index)
  {
    Json::Value::ArrayIndex tmp = static_cast<Json::Value::ArrayIndex>(index);
    std::string id = series_["Instances"][tmp].asString();
    return new Instance(connection_, id.c_str());
  }

  Series::Series(const OrthancConnection& connection,
                 const char* id) :
    connection_(connection),
    id_(id),
    instances_(*this)
  {
    ReadSeries();
    status_ = Status3DImage_NotTested;
    url_ = std::string(connection_.GetOrthancUrl()) + "/series/" + id_;

    voxelSizeX_ = 0;
    voxelSizeY_ = 0;
    voxelSizeZ_ = 0;
    sliceThickness_ = 0;

    instances_.SetThreadCount(connection.GetThreadCount());
  }


  bool Series::Is3DImage()
  {
    if (status_ == Status3DImage_NotTested)
    {
      status_ = Is3DImageInternal() ? Status3DImage_True : Status3DImage_False;
    }

    return status_ == Status3DImage_True;
  }

  unsigned int Series::GetInstanceCount()
  {
    return instances_.GetSize();
  }

  Instance& Series::GetInstance(unsigned int index)
  {
    return dynamic_cast<Instance&>(instances_.GetItem(index));
  }

  unsigned int Series::GetWidth()
  {
    Check3DImage();

    if (GetInstanceCount() == 0)
      return 0;
    else
      return GetInstance(0).GetTagAsInt("Columns");
  }

  unsigned int Series::GetHeight()
  {
    Check3DImage();

    if (GetInstanceCount() == 0)
      return 0;
    else
      return GetInstance(0).GetTagAsInt("Rows");
  }

  const char* Series::GetMainDicomTag(const char* tag, const char* defaultValue) const
  {
    if (series_["MainDicomTags"].isMember(tag))
    {
      return series_["MainDicomTags"][tag].asCString();
    }
    else
    {
      return defaultValue;
    }
  }


  
  void Series::Load3DImageInternal(void* target,
                                   Orthanc::PixelFormat format,
                                   size_t lineStride,
                                   size_t stackStride,
                                   Orthanc::ThreadedCommandProcessor::IListener* listener)
  {
    using namespace Orthanc;

    // Choose the extraction mode, depending on the format of the
    // target image.

    uint8_t bytesPerPixel;
    ImageExtractionMode mode;

    switch (format)
    {
      case PixelFormat_RGB24:
        bytesPerPixel = 3;
        mode = ImageExtractionMode_Preview;
        break;

      case PixelFormat_Grayscale8:
        bytesPerPixel = 1;
        mode = ImageExtractionMode_UInt8;  // Preview ???
        break; 

      case PixelFormat_Grayscale16:
        bytesPerPixel = 2;
        mode = ImageExtractionMode_UInt16;
        break;

      case PixelFormat_SignedGrayscale16:
        bytesPerPixel = 2;
        mode = ImageExtractionMode_UInt16;
        format = PixelFormat_Grayscale16;
        break;

      default:
        throw OrthancClientException(ErrorCode_NotImplemented);
    }


    // Check that the target image is properly sized
    unsigned int sx = GetWidth();
    unsigned int sy = GetHeight();

    if (lineStride < sx * bytesPerPixel ||
        stackStride < sx * sy * bytesPerPixel)
    {
      throw OrthancClientException(ErrorCode_BadRequest);
    }

    if (sx == 0 || sy == 0 || GetInstanceCount() == 0)
    {
      // Empty image, nothing to do
      if (listener)
        listener->SignalSuccess(0);
      return;
    }


    /**
     * Order the stacks according to their distance along the slice
     * normal (using the "Image Position Patient" tag). This works
     * even if the "SliceLocation" tag is absent.
     **/
    SliceLocator locator(GetInstance(0));

    typedef std::map<float, Instance*> Instances;
    Instances instances;
    for (unsigned int i = 0; i < GetInstanceCount(); i++)
    {
      float dist = locator.ComputeSliceLocation(GetInstance(i));
      instances[dist] = &GetInstance(i);
    }

    if (instances.size() != GetInstanceCount())
    {
      // Several instances have the same Z coordinate
      throw OrthancClientException(ErrorCode_NotImplemented);
    }


    // Submit the download of each stack as a set of commands
    ThreadedCommandProcessor processor(connection_.GetThreadCount());

    if (listener != NULL)
    {
      processor.SetListener(*listener);
    }

    uint8_t* stackTarget = reinterpret_cast<uint8_t*>(target);
    for (Instances::iterator it = instances.begin(); it != instances.end(); ++it)
    {
      processor.Post(new ImageDownloadCommand(*it->second, format, mode, stackTarget, lineStride));
      stackTarget += stackStride;
    }


    // Wait for all the stacks to be downloaded
    if (!processor.Join())
    {
      throw OrthancClientException(ErrorCode_NetworkProtocol);
    }
  }

  float Series::GetVoxelSizeX()
  {
    Check3DImage();   // Is3DImageInternal() will compute the voxel sizes
    return voxelSizeX_;
  }

  float Series::GetVoxelSizeY()
  {
    Check3DImage();   // Is3DImageInternal() will compute the voxel sizes
    return voxelSizeY_;
  }

  float Series::GetVoxelSizeZ()
  {
    Check3DImage();   // Is3DImageInternal() will compute the voxel sizes
    return voxelSizeZ_;
  }

  float Series::GetSliceThickness()
  {
    Check3DImage();   // Is3DImageInternal() will compute the voxel sizes
    return sliceThickness_;
  }

  void Series::Load3DImage(void* target,
                           Orthanc::PixelFormat format,
                           int64_t lineStride,
                           int64_t stackStride,
                           float* progress)
  {
    ProgressToFloatListener listener(progress);
    Load3DImageInternal(target, format, static_cast<size_t>(lineStride), 
                        static_cast<size_t>(stackStride), &listener);
  }
}
