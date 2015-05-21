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


#pragma once

#include "Instance.h"

#include "../Core/MultiThreading/ArrayFilledByThreads.h"
#include "../Core/MultiThreading/ThreadedCommandProcessor.h"

namespace OrthancClient
{
  /**
   * {summary}{Connection to a series stored in %Orthanc.}
   * {description}{This class encapsulates a connection to a series
   * from a remote instance of %Orthanc.}
   **/
  class LAAW_API Series :
    public Orthanc::IDynamicObject, 
    private Orthanc::ArrayFilledByThreads::IFiller
  {
  private:
    enum Status3DImage
    {
      Status3DImage_NotTested,
      Status3DImage_True,
      Status3DImage_False
    };

    const OrthancConnection& connection_;
    std::string id_, url_;
    Json::Value series_;
    Orthanc::ArrayFilledByThreads  instances_;
    Status3DImage status_;

    float voxelSizeX_;
    float voxelSizeY_;
    float voxelSizeZ_;
    float sliceThickness_;

    void Check3DImage();

    bool Is3DImageInternal();

    void ReadSeries();

    virtual size_t GetFillerSize()
    {
      return series_["Instances"].size();
    }

    virtual Orthanc::IDynamicObject* GetFillerItem(size_t index);

    void Load3DImageInternal(void* target,
                             Orthanc::PixelFormat format,
                             size_t lineStride,
                             size_t stackStride,
                             Orthanc::ThreadedCommandProcessor::IListener* listener);

  public:
    /**
     * {summary}{Create a connection to some series.}
     * {param}{connection The remote instance of %Orthanc.}
     * {param}{id The %Orthanc identifier of the series.}
     **/
    Series(const OrthancConnection& connection,
           const char* id);

     /**
     * {summary}{Reload the instances of this series.}
     * {description}{This method will reload the list of the instances of this series. Pay attention to the fact that the instances that have been previously returned by GetInstance() will be invalidated.}
     **/
    void Reload()
    {
      instances_.Reload();
    }

    /**
     * {summary}{Return the number of instances for this series.}
     * {returns}{The number of instances.}
     **/
    uint32_t GetInstanceCount();
    
    /**
     * {summary}{Get some instance of this series.}
     * {description}{This method will return an object that contains information about some instance. The instances are indexed by a number between 0 (inclusive) and the result of GetInstanceCount() (exclusive).}
     * {param}{index The index of the instance of interest.}
     * {returns}{The instance.}
     **/
    Instance& GetInstance(uint32_t index);

    /**
     * {summary}{Get the %Orthanc identifier of this series.}
     * {returns}{The identifier.}
     **/
    const char* GetId() const
    {
      return id_.c_str();
    }

    /**
     * {summary}{Returns the URL to this series.}
     * {returns}{The URL.}
     **/
    const char* GetUrl() const
    {
      return url_.c_str();
    }

   
    /**
     * {summary}{Get the value of one of the main DICOM tags for this series.}
     * {param}{tag The name of the tag of interest ("Modality", "Manufacturer", "SeriesDate", "SeriesDescription", "SeriesInstanceUID"...).}
     * {param}{defaultValue The default value to be returned if this tag does not exist.}
     * {returns}{The value of the tag.}
     **/
    const char* GetMainDicomTag(const char* tag, 
                                const char* defaultValue) const;

    /**
     * {summary}{Test whether this series encodes a 3D image that can be downloaded from %Orthanc.}
     * {returns}{"true" if and only if this is a 3D image.}
     **/
    bool Is3DImage();

    /**
     * {summary}{Get the width of the 3D image.}
     * {description}{Get the width of the 3D image (i.e. along the X-axis). This call is only valid if this series corresponds to a 3D image.}
     * {returns}{The width.}
     **/
    uint32_t GetWidth();

    /**
     * {summary}{Get the height of the 3D image.}
     * {description}{Get the height of the 3D image (i.e. along the Y-axis). This call is only valid if this series corresponds to a 3D image.}
     * {returns}{The height.}
     **/
    uint32_t GetHeight();

    /**
     * {summary}{Get the physical size of a voxel along the X-axis.}
     * {description}{Get the physical size of a voxel along the X-axis. This call is only valid if this series corresponds to a 3D image.}
     * {returns}{The voxel size.}
     **/
    float GetVoxelSizeX();

    /**
     * {summary}{Get the physical size of a voxel along the Y-axis.}
     * {description}{Get the physical size of a voxel along the Y-axis. This call is only valid if this series corresponds to a 3D image.}
     * {returns}{The voxel size.}
     **/
    float GetVoxelSizeY();

    /**
     * {summary}{Get the physical size of a voxel along the Z-axis.}
     * {description}{Get the physical size of a voxel along the Z-axis. This call is only valid if this series corresponds to a 3D image.}
     * {returns}{The voxel size.}
     **/
    float GetVoxelSizeZ();

    /**
     * {summary}{Get the slice thickness.}
     * {description}{Get the slice thickness. This call is only valid if this series corresponds to a 3D image.}
     * {returns}{The slice thickness.}
     **/
    float GetSliceThickness();

    LAAW_API_INTERNAL void Load3DImage(void* target,
                                       Orthanc::PixelFormat format,
                                       int64_t lineStride,
                                       int64_t stackStride,
                                       Orthanc::ThreadedCommandProcessor::IListener& listener)
    {
      Load3DImageInternal(target, format, static_cast<size_t>(lineStride), 
                          static_cast<size_t>(stackStride), &listener);
    }

    /**
     * {summary}{Load the 3D image into a memory buffer.}
     * {description}{Load the 3D image into a memory buffer. This call is only valid if this series corresponds to a 3D image. The "target" buffer must be wide enough to store all the voxels of the image.}
     * {param}{target The target memory buffer.}
     * {param}{format The memory layout of the voxels.}
     * {param}{lineStride The number of bytes between two lines in the target memory buffer.}
     * {param}{stackStride The number of bytes between two 2D slices in the target memory buffer.}
     **/
    void Load3DImage(void* target,
                     Orthanc::PixelFormat format,
                     int64_t lineStride,
                     int64_t stackStride)
    {
      Load3DImageInternal(target, format, static_cast<size_t>(lineStride),
                          static_cast<size_t>(stackStride), NULL);
    }

    /**
     * {summary}{Load the 3D image into a memory buffer.}
     * {description}{Load the 3D image into a memory buffer. This call is only valid if this series corresponds to a 3D image. The "target" buffer must be wide enough to store all the voxels of the image. This method will also update a progress indicator to monitor the loading of the image.}
     * {param}{target The target memory buffer.}
     * {param}{format The memory layout of the voxels.}
     * {param}{lineStride The number of bytes between two lines in the target memory buffer.}
     * {param}{stackStride The number of bytes between two 2D slices in the target memory buffer.}
     * {param}{progress A pointer to a floating-point number that is continuously updated by the download threads to reflect the percentage of completion (between 0 and 1). This value can be read from a separate thread.}
     **/
    void Load3DImage(void* target,
                     Orthanc::PixelFormat format,
                     int64_t lineStride,
                     int64_t stackStride,
                     float* progress);
  };
}
