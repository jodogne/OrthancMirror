/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2018 Osimis S.A., Belgium
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


#include "GdcmDecoderCache.h"

#include "OrthancImageWrapper.h"

namespace OrthancPlugins
{
  std::string GdcmDecoderCache::ComputeMd5(OrthancPluginContext* context,
                                           const void* dicom,
                                           size_t size)
  {
    std::string result;

    char* md5 = OrthancPluginComputeMd5(context, dicom, size);

    if (md5 == NULL)
    {
      throw std::runtime_error("Cannot compute MD5 hash");
    }

    bool ok = false;
    try
    {
      result.assign(md5);
      ok = true;
    }
    catch (...)
    {
    }

    OrthancPluginFreeString(context, md5);

    if (!ok)
    {
      throw std::runtime_error("Not enough memory");
    }
    else
    {    
      return result;
    }
  }


  OrthancImageWrapper* GdcmDecoderCache::Decode(OrthancPluginContext* context,
                                                const void* dicom,
                                                const uint32_t size,
                                                uint32_t frameIndex)
  {
    std::string md5 = ComputeMd5(context, dicom, size);

    // First check whether the previously decoded image is the same
    // as this one
    {
      boost::mutex::scoped_lock lock(mutex_);

      if (decoder_.get() != NULL &&
          size_ == size &&
          md5_ == md5)
      {
        // This is the same image: Reuse the previous decoding
        return new OrthancImageWrapper(context, decoder_->Decode(context, frameIndex));
      }
    }

    // This is not the same image
    std::auto_ptr<GdcmImageDecoder> decoder(new GdcmImageDecoder(dicom, size));
    std::auto_ptr<OrthancImageWrapper> image(new OrthancImageWrapper(context, decoder->Decode(context, frameIndex)));

    {
      // Cache the newly created decoder for further use
      boost::mutex::scoped_lock lock(mutex_);
      decoder_ = decoder;
      size_ = size;
      md5_ = md5;
    }

    return image.release();
  }
}
