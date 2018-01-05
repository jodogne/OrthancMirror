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


#include "GdcmImageDecoder.h"

#include "OrthancImageWrapper.h"

#include <gdcmImageReader.h>
#include <gdcmImageApplyLookupTable.h>
#include <gdcmImageChangePlanarConfiguration.h>
#include <gdcmImageChangePhotometricInterpretation.h>
#include <stdexcept>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/device/array.hpp>


namespace OrthancPlugins
{
  struct GdcmImageDecoder::PImpl
  {
    const void*           dicom_;
    size_t                size_;

    gdcm::ImageReader reader_;
    std::auto_ptr<gdcm::ImageApplyLookupTable> lut_;
    std::auto_ptr<gdcm::ImageChangePhotometricInterpretation> photometric_;
    std::auto_ptr<gdcm::ImageChangePlanarConfiguration> interleaved_;
    std::string decoded_;

    PImpl(const void* dicom,
          size_t size) :
      dicom_(dicom),
      size_(size)
    {
    }


    const gdcm::DataSet& GetDataSet() const
    {
      return reader_.GetFile().GetDataSet();
    }


    const gdcm::Image& GetImage() const
    {
      if (interleaved_.get() != NULL)
      {
        return interleaved_->GetOutput();
      }

      if (lut_.get() != NULL)
      {
        return lut_->GetOutput();
      }

      if (photometric_.get() != NULL)
      {
        return photometric_->GetOutput();
      }

      return reader_.GetImage();
    }


    void Decode()
    {
      // Change photometric interpretation or apply LUT, if required
      {
        const gdcm::Image& image = GetImage();
        if (image.GetPixelFormat().GetSamplesPerPixel() == 1 &&
            image.GetPhotometricInterpretation() == gdcm::PhotometricInterpretation::PALETTE_COLOR)
        {
          lut_.reset(new gdcm::ImageApplyLookupTable());
          lut_->SetInput(image);
          if (!lut_->Apply())
          {
            throw std::runtime_error( "GDCM cannot apply the lookup table");
          }
        }
        else if (image.GetPixelFormat().GetSamplesPerPixel() == 1)
        {
          if (image.GetPhotometricInterpretation() != gdcm::PhotometricInterpretation::MONOCHROME1 &&
              image.GetPhotometricInterpretation() != gdcm::PhotometricInterpretation::MONOCHROME2)
          {
            photometric_.reset(new gdcm::ImageChangePhotometricInterpretation());
            photometric_->SetInput(image);
            photometric_->SetPhotometricInterpretation(gdcm::PhotometricInterpretation::MONOCHROME2);
            if (!photometric_->Change() ||
                GetImage().GetPhotometricInterpretation() != gdcm::PhotometricInterpretation::MONOCHROME2)
            {
              throw std::runtime_error("GDCM cannot change the photometric interpretation");
            }
          }      
        }
        else 
        {
          if (image.GetPixelFormat().GetSamplesPerPixel() == 3 &&
              image.GetPhotometricInterpretation() != gdcm::PhotometricInterpretation::RGB &&
              image.GetPhotometricInterpretation() != gdcm::PhotometricInterpretation::YBR_FULL &&
              (image.GetTransferSyntax() != gdcm::TransferSyntax::JPEG2000Lossless ||
               image.GetPhotometricInterpretation() != gdcm::PhotometricInterpretation::YBR_RCT))
          {
            photometric_.reset(new gdcm::ImageChangePhotometricInterpretation());
            photometric_->SetInput(image);
            photometric_->SetPhotometricInterpretation(gdcm::PhotometricInterpretation::RGB);
            if (!photometric_->Change() ||
                GetImage().GetPhotometricInterpretation() != gdcm::PhotometricInterpretation::RGB)
            {
              throw std::runtime_error("GDCM cannot change the photometric interpretation");
            }
          }
        }
      }

      // Possibly convert planar configuration to interleaved
      {
        const gdcm::Image& image = GetImage();
        if (image.GetPlanarConfiguration() != 0 && 
            image.GetPixelFormat().GetSamplesPerPixel() != 1)
        {
          interleaved_.reset(new gdcm::ImageChangePlanarConfiguration());
          interleaved_->SetInput(image);
          if (!interleaved_->Change() ||
              GetImage().GetPlanarConfiguration() != 0)
          {
            throw std::runtime_error("GDCM cannot change the planar configuration to interleaved");
          }
        }
      }
    }
  };

  GdcmImageDecoder::GdcmImageDecoder(const void* dicom,
                                     size_t size) :
    pimpl_(new PImpl(dicom, size))
  {
    // Setup a stream to the memory buffer
    using namespace boost::iostreams;
    basic_array_source<char> source(reinterpret_cast<const char*>(dicom), size);
    stream<basic_array_source<char> > stream(source);

    // Parse the DICOM instance using GDCM
    pimpl_->reader_.SetStream(stream);
    if (!pimpl_->reader_.Read())
    {
      throw std::runtime_error("Bad file format");
    }

    pimpl_->Decode();
  }


  OrthancPluginPixelFormat GdcmImageDecoder::GetFormat() const
  {
    const gdcm::Image& image = pimpl_->GetImage();

    if (image.GetPixelFormat().GetSamplesPerPixel() == 1 &&
        (image.GetPhotometricInterpretation() == gdcm::PhotometricInterpretation::MONOCHROME1 ||
         image.GetPhotometricInterpretation() == gdcm::PhotometricInterpretation::MONOCHROME2))
    {
      switch (image.GetPixelFormat())
      {
        case gdcm::PixelFormat::UINT16:
          return OrthancPluginPixelFormat_Grayscale16;

        case gdcm::PixelFormat::INT16:
          return OrthancPluginPixelFormat_SignedGrayscale16;

        case gdcm::PixelFormat::UINT8:
          return OrthancPluginPixelFormat_Grayscale8;

        default:
          throw std::runtime_error("Unsupported pixel format");
      }
    }
    else if (image.GetPixelFormat().GetSamplesPerPixel() == 3 &&
             (image.GetPhotometricInterpretation() == gdcm::PhotometricInterpretation::RGB ||
              image.GetPhotometricInterpretation() == gdcm::PhotometricInterpretation::YBR_FULL ||
              image.GetPhotometricInterpretation() == gdcm::PhotometricInterpretation::YBR_RCT))
    {
      switch (image.GetPixelFormat())
      {
        case gdcm::PixelFormat::UINT8:
          return OrthancPluginPixelFormat_RGB24;

        case gdcm::PixelFormat::UINT16:
#if ORTHANC_PLUGINS_VERSION_IS_ABOVE(1, 3, 1)
          return OrthancPluginPixelFormat_RGB48;
#else
          throw std::runtime_error("RGB48 pixel format is only supported if compiled against Orthanc SDK >= 1.3.1");
#endif
          
        default:
          break;
      }      
    }

    throw std::runtime_error("Unsupported pixel format");
  }


  unsigned int GdcmImageDecoder::GetWidth() const
  {
    return pimpl_->GetImage().GetColumns();
  }


  unsigned int GdcmImageDecoder::GetHeight() const
  {
    return pimpl_->GetImage().GetRows();
  }

  
  unsigned int GdcmImageDecoder::GetFramesCount() const
  {
    return pimpl_->GetImage().GetDimension(2);
  }


  size_t GdcmImageDecoder::GetBytesPerPixel(OrthancPluginPixelFormat format)
  {
    switch (format)
    {
      case OrthancPluginPixelFormat_Grayscale8:
        return 1;

      case OrthancPluginPixelFormat_Grayscale16:
      case OrthancPluginPixelFormat_SignedGrayscale16:
        return 2;

      case OrthancPluginPixelFormat_RGB24:
        return 3;

#if ORTHANC_PLUGINS_VERSION_IS_ABOVE(1, 3, 1)
      case OrthancPluginPixelFormat_RGB48:
        return 6;
#endif

      default:
        throw std::runtime_error("Unsupport pixel format");
    }
  }

  static void ConvertYbrToRgb(uint8_t rgb[3],
                              const uint8_t ybr[3])
  {
    // http://dicom.nema.org/medical/dicom/current/output/chtml/part03/sect_C.7.6.3.html#sect_C.7.6.3.1.2
    // https://en.wikipedia.org/wiki/YCbCr#JPEG_conversion
    
    const float Y  = ybr[0];
    const float Cb = ybr[1];
    const float Cr = ybr[2];

    const float result[3] = {
      Y                             + 1.402f    * (Cr - 128.0f),
      Y - 0.344136f * (Cb - 128.0f) - 0.714136f * (Cr - 128.0f),
      Y + 1.772f    * (Cb - 128.0f)
    };

    for (uint8_t i = 0; i < 3 ; i++)
    {
      if (result[i] < 0)
      {
        rgb[i] = 0;
      }
      else if (result[i] > 255)
      {
        rgb[i] = 255;
      }
      else
      {
        rgb[i] = static_cast<uint8_t>(result[i]);
      }
    }    
  }

  
  static void FixPhotometricInterpretation(OrthancImageWrapper& image,
                                           gdcm::PhotometricInterpretation interpretation)
  {
    switch (interpretation)
    {
      case gdcm::PhotometricInterpretation::RGB:
        return;

      case gdcm::PhotometricInterpretation::YBR_FULL:
      {
        // Fix for Osimis issue WVB-319: Some images are not loading in US_MF

        uint32_t width = image.GetWidth();
        uint32_t height = image.GetHeight();
        uint32_t pitch = image.GetPitch();
        uint8_t* buffer = reinterpret_cast<uint8_t*>(image.GetBuffer());
        
        if (image.GetFormat() != OrthancPluginPixelFormat_RGB24 ||
            pitch < 3 * width)
        {
          throw std::runtime_error("Internal error");
        }

        for (uint32_t y = 0; y < height; y++)
        {
          uint8_t* p = buffer + y * pitch;
          for (uint32_t x = 0; x < width; x++, p += 3)
          {
            const uint8_t ybr[3] = { p[0], p[1], p[2] };
            uint8_t rgb[3];
            ConvertYbrToRgb(rgb, ybr);
            p[0] = rgb[0];
            p[1] = rgb[1];
            p[2] = rgb[2];
          }
        }

        return;
      }

      default:
        throw std::runtime_error("Unsupported output photometric interpretation");
    }    
  }


  OrthancPluginImage* GdcmImageDecoder::Decode(OrthancPluginContext* context,
                                               unsigned int frameIndex) const
  {
    unsigned int frames = GetFramesCount();
    unsigned int width = GetWidth();
    unsigned int height = GetHeight();
    OrthancPluginPixelFormat format = GetFormat();
    size_t bpp = GetBytesPerPixel(format);

    if (frameIndex >= frames)
    {
      throw std::runtime_error("Inexistent frame index");
    }

    std::string& decoded = pimpl_->decoded_;
    OrthancImageWrapper target(context, format, width, height);

    if (width == 0 ||
        height == 0)
    {
      return target.Release();
    }

    if (decoded.empty())
    {
      decoded.resize(pimpl_->GetImage().GetBufferLength());
      pimpl_->GetImage().GetBuffer(&decoded[0]);
    }

    const void* sourceBuffer = &decoded[0];

    if (target.GetPitch() == bpp * width &&
        frames == 1)
    {
      assert(decoded.size() == target.GetPitch() * target.GetHeight());      
      memcpy(target.GetBuffer(), sourceBuffer, decoded.size());
    }
    else 
    {
      size_t targetPitch = target.GetPitch();
      size_t sourcePitch = width * bpp;

      const char* a = &decoded[sourcePitch * height * frameIndex];
      char* b = target.GetBuffer();

      for (uint32_t y = 0; y < height; y++)
      {
        memcpy(b, a, sourcePitch);
        a += sourcePitch;
        b += targetPitch;
      }
    }
    
    FixPhotometricInterpretation(target, pimpl_->GetImage().GetPhotometricInterpretation());
                                 
    return target.Release();
  }
}
