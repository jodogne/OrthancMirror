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
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#pragma once

#include <orthanc/OrthancCPlugin.h>
#include <stdint.h>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

namespace OrthancPlugins
{
  class GdcmImageDecoder : public boost::noncopyable
  {
  private:
    struct PImpl;
    boost::shared_ptr<PImpl> pimpl_;
  
  public:
    GdcmImageDecoder(const void* dicom,
                     size_t size);

    OrthancPluginPixelFormat GetFormat() const;

    unsigned int GetWidth() const;

    unsigned int GetHeight() const;

    unsigned int GetFramesCount() const;

    static size_t GetBytesPerPixel(OrthancPluginPixelFormat format);

    OrthancPluginImage* Decode(OrthancPluginContext* context,
                               unsigned int frameIndex) const;
  };
}
