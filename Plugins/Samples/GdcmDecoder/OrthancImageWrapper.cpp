/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017 Osimis, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include "OrthancImageWrapper.h"

#include <stdexcept>

namespace OrthancPlugins
{
  OrthancImageWrapper::OrthancImageWrapper(OrthancPluginContext* context,
                                           OrthancPluginPixelFormat format,
                                           uint32_t width,
                                           uint32_t height) :
    context_(context)
  {
    image_ = OrthancPluginCreateImage(context_, format, width, height);
    if (image_ == NULL)
    {
      throw std::runtime_error("Cannot create an image");
    }
  }


  OrthancImageWrapper::OrthancImageWrapper(OrthancPluginContext* context,
                                           OrthancPluginImage* image) :
    context_(context),
    image_(image)
  {
    if (image_ == NULL)
    {
      throw std::runtime_error("Invalid image returned by the core of Orthanc");
    }
  }



  OrthancImageWrapper::~OrthancImageWrapper()
  {
    if (image_ != NULL)
    {
      OrthancPluginFreeImage(context_, image_);
    }
  }


  OrthancPluginImage* OrthancImageWrapper::Release()
  {
    OrthancPluginImage* tmp = image_;
    image_ = NULL;
    return tmp;
  }


  uint32_t OrthancImageWrapper::GetWidth()
  {
    return OrthancPluginGetImageWidth(context_, image_);
  }


  uint32_t OrthancImageWrapper::GetHeight()
  {
    return OrthancPluginGetImageHeight(context_, image_);
  }


  uint32_t OrthancImageWrapper::GetPitch()
  {
    return OrthancPluginGetImagePitch(context_, image_);
  }


  OrthancPluginPixelFormat OrthancImageWrapper::GetFormat()
  {
    return OrthancPluginGetImagePixelFormat(context_, image_);
  }


  char* OrthancImageWrapper::GetBuffer()
  {
    return reinterpret_cast<char*>(OrthancPluginGetImageBuffer(context_, image_));
  }
}
