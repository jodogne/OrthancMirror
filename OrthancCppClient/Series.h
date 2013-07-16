/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2013 Medical Physics Department, CHU of Liege,
 * Belgium
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
  class Series : public Orthanc::IDynamicObject, private Orthanc::ArrayFilledByThreads::IFiller
  {
  private:
    enum Status3DImage
    {
      Status3DImage_NotTested,
      Status3DImage_True,
      Status3DImage_False
    };

    const OrthancConnection& connection_;
    std::string id_;
    Json::Value series_;
    Orthanc::ArrayFilledByThreads  instances_;
    Status3DImage status_;
  
    void Check3DImage();

    bool Is3DImageInternal();

    void ReadSeries();

    virtual size_t GetFillerSize()
    {
      return series_["Instances"].size();
    }

    virtual Orthanc::IDynamicObject* GetFillerItem(size_t index);

    void Load3DImage(void* target,
                     Orthanc::PixelFormat format,
                     size_t lineStride,
                     size_t stackStride,
                     Orthanc::ThreadedCommandProcessor::IListener* listener);

  public:
    Series(const OrthancConnection& connection,
           const std::string& id);

    void Reload()
    {
      instances_.Reload();
    }

    bool Is3DImage();

    unsigned int GetInstanceCount();

    Instance& GetInstance(unsigned int index);

    const std::string& GetId() const
    {
      return id_;
    }

    std::string GetUrl() const;

    unsigned int GetWidth();

    unsigned int GetHeight();

    void GetVoxelSize(float& sizeX, float& sizeY, float& sizeZ);  

    std::string GetMainDicomTag(const char* tag, 
                                const char* defaultValue) const;

    void Load3DImage(void* target,
                     Orthanc::PixelFormat format,
                     size_t lineStride,
                     size_t stackStride,
                     Orthanc::ThreadedCommandProcessor::IListener& listener)
    {
      Load3DImage(target, format, lineStride, stackStride, &listener);
    }

    void Load3DImage(void* target,
                     Orthanc::PixelFormat format,
                     size_t lineStride,
                     size_t stackStride)
    {
      Load3DImage(target, format, lineStride, stackStride, NULL);
    }
  };
}
